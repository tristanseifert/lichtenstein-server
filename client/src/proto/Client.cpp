#include "Client.h"

#include <Format.h>
#include <Logging.h>
#include <ConfigManager.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <cstring>
#include <stdexcept>
#include <system_error>
#include <algorithm>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <base64.h>

#include <cista.h>
#include <proto/ProtoMessages.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Client::Proto;

/// singleton client instance
std::shared_ptr<Client> Client::shared = nullptr;

/**
 * Allocates the shared client instance, which will automagically attempt to
 * connect to the server.
 */
void Client::start() {
    XASSERT(shared == nullptr, "Repeated calls to Client::start() not allowed");

    shared = std::make_shared<Client>();
}

/**
 * Tears down the client handler and cleans up associated resources.
 */
void Client::stop() {
    XASSERT(shared != nullptr, "Shared client must be set up");

    shared->terminate();
    shared = nullptr;
}



/**
 * Creates a protocol client. This will set up a work thread that performs all
 * connection and message handling in the background.
 */
Client::Client() {
    // read node uuid
    auto uuidStr = ConfigManager::get("id.uuid", "");
    if(uuidStr.empty()) {
        throw std::runtime_error("Node UUID (id.uuid) is required");
    }

    auto uuid = uuids::uuid::from_string(uuidStr);
    if(!uuid.has_value()) {
        auto what = f("Couldn't parse uuid string '{}'", uuidStr);
        throw std::runtime_error(what);
    }

    this->uuid = uuid.value();
    Logging::trace("Node uuid: {}", uuids::to_string(this->uuid));

    // read node secret
    auto secretStr = ConfigManager::get("id.secret", "");
    if(secretStr.empty()) {
        throw std::runtime_error("Node secret (id.secret) is required");
    }

    auto secretDecoded = base64_decode(secretStr);
    if(secretDecoded.empty()) {
        auto what = f("Couldn't decode base64 string '{}'", secretStr);
        throw std::runtime_error(what);
    }
    else if(secretDecoded.size() < kSecretMinLength) {
        auto what = f("Got {} bytes of node secret; expected at least {}",
                secretDecoded.size(), (const size_t)kSecretMinLength);
        throw std::runtime_error(what);
    }

    auto secretBytes = reinterpret_cast<std::byte *>(secretDecoded.data());
    this->secret.assign(secretBytes, secretBytes+secretDecoded.size());

    // get the read timeout
    this->readTimeout = ConfigManager::getTimeval("remote.recv_timeout", 2);
    Logging::trace("Read timeout is {} seconds", this->readTimeout);

    // get server address/port and attempt to resolve
    this->serverV4Only = ConfigManager::getBool("remote.server.ipv4_only", false);

    this->serverHost = ConfigManager::get("remote.server.address", "");
    if(this->serverHost.empty()) {
        throw std::runtime_error("Remote address (remote.server.address) is required");
    }

    this->serverPort = ConfigManager::getUnsigned("remote.server.port", 7420);
    if(this->serverPort > 65535) {
        auto what = f("Invalid remote port {}", this->serverPort);
        throw std::runtime_error(what);
    }

    this->resolve();

    // create the worker thread
    this->run = true;
    this->worker = std::make_unique<std::thread>(&Client::workerMain, this);
}

/**
 * Cleans up client resources. This will waits for the work thread to join
 * before we can exit.
 */
Client::~Client() {
    // we should've had terminate called before
    if(this->run) {
        Logging::error("No call to Client::terminate() before destruction!");
        this->terminate();
    }

    // wait for worker to finish execution
    this->worker->join();
}

/**
 * Prepared the client for termination by signalling the worker thread.
 */
void Client::terminate() {
    if(!this->run) {
        Logging::error("Ignoring repeated call to Client::terminate()");
        return;
    }

    Logging::debug("Requesting client worker termination");
    this->run = false;
}

/**
 * Tries to resolve the server hostname/address into an IP address that we can
 * connect to.
 */
void Client::resolve() {
    int err;
    struct addrinfo hints, *res = nullptr;

    // provide hints to resolver (if we want IPv4 only or all)
    memset(&hints, 0, sizeof(hints));

    if(this->serverV4Only) {
        hints.ai_family = AF_INET;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags |= (AI_ADDRCONFIG | AI_V4MAPPED);

    // attempt to resolve it; then get the first address
    std::string port = f("{}", this->serverPort);

    err = getaddrinfo(this->serverHost.c_str(), port.c_str(), &hints, &res);
    if(err != 0) {
        throw std::system_error(errno, std::generic_category(),
                "getaddrinfo() failed");
    }

    if(!res) {
        auto what = f("Failed to resolve '{}'", this->serverHost);
        throw std::runtime_error(what);
    }

    // just pick the first result
    XASSERT(res->ai_addrlen <= sizeof(this->serverAddr),
            "Invalid address length {}; have space for {}", res->ai_addrlen,
            sizeof(this->serverAddr));

    memcpy(&this->serverAddr, res->ai_addr, res->ai_addrlen);
    this->serverAddrLen = res->ai_addrlen;

    Logging::debug("Resolved '{}' -> '{}'", this->serverHost, this->serverAddr);

    // clean up
done:;
    freeaddrinfo(res);
}



/**
 * Entry point for the client worker thread.
 */
void Client::workerMain() {
    int attempts, err;
    bool success;

    // initialize an SSL context
    this->ctx = SSL_CTX_new(DTLS_client_method());
    SSL_CTX_set_read_ahead(this->ctx, 1);

    // attempt to connect to the server
connect:;
    this->establishConnection();
    Logging::info("Server connection established");

    // process messages as long as we're supposed to run
    while(this->run) {
        struct MessageHeader header;
        std::vector<unsigned char> payload;

        try {
            // try to read a message and its payload
            if(!this->readMessage(header, payload)) {
                goto beach;
            }

            Logging::trace("Received message: type={:x}:{:x} len={}",
                    header.endpoint, header.messageType, header.length);

            // TODO: handle message
        } catch(std::exception &e) {
            Logging::error("Exception while processing message type {:x}:{:x}: {}",
                    header.endpoint, header.messageType, e.what());
        }

        // prepare for the next round of processing
beach: ;
    }

    // perform clean-up
cleanup: ;
    Logging::debug("Client worker thread is shutting down");

    // attempt to close down the connection
    this->close();

    // clean up the SSL context
    if(this->ctx) {
        SSL_CTX_free(this->ctx);
        this->ctx = nullptr;
    }
}

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
 * Uses our node UUID and shared secret to authenticate the connection.
 *
 * Authentication is implemented by means of a simple state machine. When the function is invoked
 * first, we initialize it and progress through the various states until the node is either
 * successfully authenticated, or an error occurs.
 *
 * @return true if server accepted credentials, false otherwise.
 */
bool Client::authenticate() {
    using namespace MessageTypes;

    uint8_t tag = 0;
    std::string method;

    Header head;
    PayloadType payload;

    // states for the state machine
    enum {
        // Send authentication request
        SEND_REQ,
        // Wait to receive auth request
        READ_REQ_ACK,
        // Perform auth according to selected algorithm and send to server
        SEND_RESPONSE,
        // Await response from server whether auth was a success or not
        READ_AUTH_STATE,
    } state = SEND_REQ;

    // authentication state machine
    while(true) {
        switch(state) {
            // send the auth request
            case SEND_REQ:
                this->authSendRequest(tag);
                state = READ_REQ_ACK;
                break;

            // receive the request ack
            case READ_REQ_ACK: {
                memset(&head, 0, sizeof(head));
                payload.clear();

                if(!this->readMessage(head, payload)) {
                    Logging::error("Failed to read auth message (state {})", state);
                    return false;
                }
                if(head.tag != tag ||
                   head.endpoint != MessageEndpoint::Authentication || 
                   head.messageType != AuthMessageType::AUTH_REQUEST_ACK) {
                    Logging::error("Received unexpected message: tag {}, type {:x}:{:x}", head.tag, 
                                   head.endpoint, head.messageType);
                    break;
                }

                // decode the acknowledgement
                auto ack = cista::deserialize<AuthRequestAck, kCistaMode>(payload);
                if(ack->status != AUTH_SUCCESS) {
                    Logging::error("Authentication failure: {}", ack->status);
                    return false;
                }

                // if success, get the desired method and send response
                method = ack->method;

                Logging::trace("Negotiated auth method: {}", method);
                state = SEND_RESPONSE;
                break;
            }

            // send an authentication response
            case SEND_RESPONSE:
                this->authSendResponse(tag);
                state = READ_AUTH_STATE;
                break;

            // receive the response ack
            case READ_AUTH_STATE: {
                memset(&head, 0, sizeof(head));
                payload.clear();

                if(!this->readMessage(head, payload)) {
                    Logging::error("Failed to read auth message (state {})", state);
                    return false;
                }
                if(head.tag != tag ||
                   head.endpoint != MessageEndpoint::Authentication || 
                   head.messageType != AuthMessageType::AUTH_RESPONSE_ACK) {
                    Logging::error("Received unexpected message: tag {}, type {:x}:{:x}", head.tag, 
                                   head.endpoint, head.messageType);
                    break;
                }

                // decode the acknowledgement
                auto ack = cista::deserialize<AuthResponseAck, kCistaMode>(payload);
                if(ack->status != AUTH_SUCCESS) {
                    Logging::error("Authentication failure: {}", ack->status);
                    return false;
                }

                // if we get here, authentication succeeded
                return true;
            }

            // unexpected state
            default:
                Logging::error("Invalid auth state: {}", state);
                return false;
        }
    }

    // we shouldn't fall down here
    return false;
}

/**
 * Sends the authentication request to the server.
 *
 * @param sentTag Tag of the message sent
 *
 * @throws If an error occurs, an exception is thrown.
 */
void Client::authSendRequest(uint8_t &sentTag) {
    using namespace Lichtenstein::Proto::MessageTypes;

    // build the auth message
    AuthRequest msg;
    memset(&msg, 0, sizeof(msg));

    msg.nodeId = uuids::to_string(this->uuid);
    msg.methods.push_back("me.tseifert.lichtenstein.auth.null");

    // serialize and send
    auto bytes = cista::serialize<kCistaMode>(msg);

    uint8_t tag = this->nextTag++;
    this->send(MessageEndpoint::Authentication, AuthMessageType::AUTH_REQUEST, tag, bytes);
    sentTag = tag;
}

/**
 * Sends to the server the authentication response.
 *
 * This performs the needed computations based on the original data sent by the server to
 * complete authentication.
 *
 * @param sentTag Tag of the message sent
 *
 * @throws If an error occurs, an exception is thrown.
 */
void Client::authSendResponse(uint8_t &sentTag) {
    using namespace Lichtenstein::Proto::MessageTypes;

    // build the auth message
    AuthResponse msg;
    memset(&msg, 0, sizeof(msg));

    // serialize and send
    auto bytes = cista::serialize<kCistaMode>(msg);

    uint8_t tag = this->nextTag++;
    this->send(MessageEndpoint::Authentication, AuthMessageType::AUTH_RESPONSE, tag, bytes);
    sentTag = tag;
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

#if 0
    Logging::trace("Read {} byte message: ep={:x}:{:x}, tag={:x}, payload={}",
            (sizeof(outHdr)+outHdr.length), outHdr.endpoint, outHdr.messageType, outHdr.tag,
            hexdump(outPayload.begin(), outPayload.end()));
#endif

    // if we get here, message was read :)
    return true;
}
/**
 * Reads a message header from the SSL context. This will read all the way up
 * to the start of the payload area.
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
        auto what = f("readHeader() failed, expected {} bytes, got {}",
                sizeof(hdr), err);
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
#if 0
    Logging::trace("Sent {} ({} payload) byte message: type={:x}:{:x}, tag={:x}, payload={}",
            send.size(), data.size(), endpoint, type, tag, hexdump(data.begin(), data.end()));
#endif

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
