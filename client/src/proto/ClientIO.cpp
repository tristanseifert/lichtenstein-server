#include "Client.h"

#include <Format.h>
#include <Logging.h>
#include <ConfigManager.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <stdexcept>
#include <system_error>
#include <algorithm>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Client::Proto;

/**
 * Attempts to connect to the server, establish a DTLS connection, and then
 * authenticate the connection.
 */
void Client::establishConnection() {
    int attempts = 0;
    bool success;

    // TODO: close any existing connections/resources or just assert?

    // try to establish a connection and secure it
    do {
        // connect the socket
        success = this->setUpSocket();
        if(!success) {
            Logging::warn("Client::setUpSocket() failed!");
            goto beach;
        }

        // then, create an SSL context and perform the handshake
        try {
            success = this->setUpSsl();
        } catch(std::exception &) {
            // propagate exceptions, BUT close the socket
            ::close(this->sock);
            this->sock = -1;

            throw;
        }

        if(!success) {
            // close the socket to avoid leaking it
            ::close(this->sock);
            this->sock = -1;

            Logging::warn("Client::setUpSSL() failed!");
        }

beach:;
        if(!success && ++attempts >= kConnectionAttempts) {
            auto what = f("Failed to connect to server in {} attempts",
                    attempts);
            throw std::runtime_error(what);
        }
    } while(!success);

    // then, attempt to authenticate; auth failure is an immediate error
    success = this->authenticate();
    if(!success) {
        throw std::runtime_error("Failed to authenticate");
    }
}

/**
 * Creates the socket connection to the server.
 */
bool Client::setUpSocket() {
    int fd = -1, err;
    bool success = false;
    
    // create and connect a socket
    fd = socket(this->serverAddr.ss_family, SOCK_DGRAM, 0);
    if(fd < 0) {
        throw std::system_error(errno, std::generic_category(),
                "socket() failed");
    }

    auto addr = reinterpret_cast<const struct sockaddr *>(&this->serverAddr);
    err = ::connect(fd, addr, this->serverAddrLen);
    if(err != 0) {
        // abort on failure
        const size_t errStrSz = 128;
        char errStr[errStrSz];
        memset(&errStr, 0, errStrSz);
        strerror_r(errno, errStr, errStrSz);

        Logging::warn("Failed to connect to server: {} ({})", errStr, errno);
        goto beach;
    }

    // socket was successfully set up if we drop down here
    success = true;
    Logging::trace("Connected fd {} to {}", fd, this->serverAddr);

beach:;
    if(!success) {
        if(fd > 0) ::close(fd);
    } else {
        this->sock = fd;
    }

    // return success state
    Logging::trace("setUpSocket(): fd={}, success={}", fd, success);
    return success;
}

/**
 * Sets up the SSL context, a datagram adapter to the OpenSSL logic, and then
 * performs the DTLS handshake.
 */
bool Client::setUpSsl() {
    int err, errType;
    bool success = false;
    SSL *ssl = nullptr;
    BIO *bio = nullptr;

    // create an SSL context and associate the socket with it
    ssl = SSL_new(this->ctx);
    if(!ssl) {
        throw std::runtime_error("SSL_new() failed");
    }

    bio = BIO_new_dgram(this->sock, BIO_CLOSE);

    // this may be unneccesary
    BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &this->serverAddr);

    SSL_set_bio(ssl, bio, bio);

    // set the read timeout
    BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &this->readTimeout);
    Logging::trace("Context {}, bio {} for fd {}", (void*)ssl, (void*)bio,
            this->sock);

    // attempt to perform SSL handshake
    err = SSL_connect(ssl);
    Logging::trace("SSL_connect(): {}", err);

    if(err <= 0) {
        // does the error have detailed information?
        if(err < 0) {
            errType = SSL_get_error(ssl, err);

            // SSL session was closed by server
            if(errType == SSL_ERROR_ZERO_RETURN) {
                Logging::error("Connection closed during SSL_connect()");
                goto beach;
            }
            // Read timed out
            else if(errType == SSL_ERROR_WANT_READ) {
                Logging::error("Timed out during SSL_connect()");
                goto beach;
            }

            // all errors below are fatal so they'll throw. do not leak anything
            SSL_free(ssl);

            // underlying syscall failure
            if(errType == SSL_ERROR_SYSCALL) {
                throw std::system_error(errno, std::generic_category(),
                        "SSL_connect() syscall failure");
            }
            // other error
            else {
                auto what = f("SSL_connect(): err = {}, type = {}", err,
                        errType);
                throw SSLError(what);
            }
        }
    }

    // if we get here, connection succeeded
    success = true;

beach:;
    // ensure we do not leak anything
    if(!success) {
        if(ssl) SSL_free(ssl);
    } else {
        this->ssl = ssl;
        this->bio = bio;

        // the connection is good; so, we should call SSL_shutdown() on close
        this->sslShutdown = true;
    }

    // return the final status
    Logging::trace("connect() state: {} (fd={}, ssl={}, bio={})", success, 
            this->sock, (void*)this->ssl, (void*)this->bio);
    return success;
}


/**
 * Tears down the SSL connection, closing the socket, and releasing all of the
 * OpenSSL resources.
 */
void Client::close() {
    int err;

    // shutdown may take multiple steps
    if(this->sslShutdown) {
        do {
            err = SSL_shutdown(this->ssl);
        } while(err == 0);

        if(err != 1) {
            // shutdown failed to complete
            Logging::warn("Failed to shut down client: {}", err);
        }
    }

    if(this->sock > 0) {
        ::close(this->sock);
        this->sock = -1;
    }

    // release resources
    this->bio = nullptr;

    if(this->ssl) {
        SSL_free(this->ssl);
        this->ssl = nullptr;
    }
}

/**
 * Returns how many bytes are pending to read from the SSL context.
 */
size_t Client::bytesAvailable() {
    int err;
    XASSERT(this->ssl, "SSL context must be set up");

    err = SSL_pending(this->ssl);

    return err;
}
/**
 * Writes data to the SSL connection.
 */
size_t Client::write(const PayloadType &data) {
    int err, errType;

    XASSERT(this->ssl, "SSL context must be set up");

    // perform the write against SSL context
    auto *buf = data.data();
    auto bufSz = data.size();

    err = SSL_write(this->ssl, buf, bufSz);

    if(err <= 0) {
        errType = SSL_get_error(this->ssl, err);

        // on any error, do not perform SSL shutdown since they're all fatal
        this->sslShutdown = false;

        // underlying syscall failure
        if(errType == SSL_ERROR_SYSCALL) {
            throw std::system_error(errno, std::generic_category(),
                    "SSL_write() syscall failure");
        }
        // SSL session was closed by server
        else if(errType == SSL_ERROR_ZERO_RETURN) {
            this->close();
            throw std::runtime_error("Connection closed");
        }
        // other error (fatal)
        else {
            auto what = f("SSL_write(): err = {}, type = {}", err, errType);
            throw SSLError(what);
        }
    }

    // return the number of bytes written
    return err;
}

/**
 * Attempts to read data from the SSL connection, blocking if needed.
 */
size_t Client::read(void *buf, size_t toRead) {
    int err, errType;

    XASSERT(this->ssl, "SSL context must be set up");

    // read into a temporary buffer
    err = SSL_read(this->ssl, buf, toRead);

    if(err <= 0) {
        errType = SSL_get_error(this->ssl, err);

        // no data available to read
        if(errType == SSL_ERROR_WANT_READ) {
            return 0;
        }

        // below errors are fatal, so do not perform SSL shutdown on close
        this->sslShutdown = false;

        // underlying syscall failure
        if(errType == SSL_ERROR_SYSCALL) {
            throw std::system_error(errno, std::generic_category(),
                    "SSL_write() syscall failure");
        }
        // SSL session was closed by server
        else if(errType == SSL_ERROR_ZERO_RETURN) {
            this->close();
            throw std::runtime_error("Connection closed");
        }
        // other error (fatal)
        else {
            auto what = f("SSL_read(): err = {}, type = {}", err, errType);
            throw SSLError(what);
        }
    }

    return err;
}

/**
 * Attempts to read an entire message, by first reading a header, then its
 * entire payload.
 *
 * @return true if header and payload were read, false if read should be
 * retried later.
 */
bool Client::readMessage(Header &outHdr, PayloadType &outPayload) {
    size_t read;

    // try to read the message header
    if(!this->readHeader(outHdr)) {
        return false;
    }

    // validate the version
    if(outHdr.version != kLichtensteinProtoVersion) {
        auto what = f("Invalid protocol version {:02x} (expected {:02x})",
                outHdr.version, kLichtensteinProtoVersion);
        throw std::runtime_error(what);
    }

    // now, attempt to read the payload
    this->readPayload(outHdr, outPayload);

    // if we get here, message was read :)
    return true;
}
/**
 * Reads a message header from the SSL context. This will read all the way up to the start of the
 * payload area.
 */
bool Client::readHeader(struct MessageHeader &outHdr) {
    int err;
    struct MessageHeader hdr;

    // try to read the header
    err = this->read(&hdr, sizeof(hdr));

    if(err == 0) {
        // retry later
        return false;
    } else if(err != sizeof(hdr)) {
        // didn't get expected message length
        auto what = f("readHeader() failed, expected {} bytes, got {}", sizeof(hdr), err);
        throw std::runtime_error(what);
    }

    // the header was read successfully
    hdr.length = ntohs(hdr.length);

    outHdr = hdr;
    return true;
}

/**
 * Reads the entire amount of payload bytes into the given buffer. If the
 * entire payload cannot be read, an exception is thrown.
 */
void Client::readPayload(const struct MessageHeader &header, PayloadType &buf) {
    int err;

    // resize output buffer and try to read message
    buf.resize(header.length);

    err = this->read(buf.data(), header.length);

    if(err == 0) {
        throw std::runtime_error("Failed to read message body");
    } else if(err != header.length) {
        auto what = f("Only read {} of {} payload bytes", err, 
                header.length);
        throw std::runtime_error(what);
    }
}

/**
 * Sends a response message to the client.
 */
void Client::send(MessageEndpoint endpoint, uint8_t type, uint8_t tag, const PayloadType &data) {
    struct MessageHeader hdr;
    memset(&hdr, 0, sizeof(hdr));

    std::vector<unsigned char> send;
    int err;

    // validate parameter values
    if(data.size() > std::numeric_limits<uint16_t>::max()) {
        auto what = f("Message too big ({} bytes, max {})", 
                data.size(), std::numeric_limits<uint16_t>::max());
        throw std::invalid_argument(what);
    }

    send.reserve(data.size() + sizeof(hdr));
    send.resize(sizeof(hdr));

    // build a header in network byte order
    hdr.version = kLichtensteinProtoVersion;
    hdr.length = htons(data.size() & 0xFFFF);
    hdr.endpoint = endpoint;
    hdr.messageType = type;
    hdr.tag = tag;

    auto hdrBytes = reinterpret_cast<unsigned char *>(&hdr);
    std::copy(hdrBytes, hdrBytes+sizeof(hdr), send.begin());

    // append payload to send buffer
    send.insert(send.end(), data.begin(), data.end());

    // go and send it
    err = this->write(send);

    if(err != send.size()) {
        auto what = f("Failed to write {} byte message; only wrote {}",
                send.size(), err);
        throw std::runtime_error(what);
    }
}



/**
 * Initializes a new SSL error object. This includes the ssl library error
 * string alongside the `what` description.
 */
Client::SSLError::SSLError(const std::string what) : 
    std::runtime_error("SSL error") {
    // format our internal string
    std::string libErr;
    getSslErrors(libErr);

    this->whatStr = f("{}: {}", what, libErr);
}

/**
 * Gets the error string from the SSL library
 */
void Client::SSLError::getSslErrors(std::string &out) {
    // print errors into a memory BIO and get a buffer ptr
    BIO *bio = BIO_new(BIO_s_mem());
    ERR_print_errors(bio);

    char *buf = nullptr;
    size_t len = BIO_get_mem_data(bio, &buf);

    // strip the last newline from the string
    out = std::string(buf, len);
    out.erase(std::find_if(out.rbegin(), out.rend(), [](int ch) {
        return !std::iscntrl(ch);
    }).base(), out.end());

    // clean up
    BIO_free(bio);
}

