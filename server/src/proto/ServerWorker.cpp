#include "ServerWorker.h"
#include "Server.h"
#include "IMessageHandler.h"

#include <Format.h>
#include <Logging.h>
#include <ConfigManager.h>

#include <stdexcept>

#include <unistd.h>

#include <openssl/ssl.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Server::Proto;

using SSLError = Server::SSLError;

/**
 * Creates a new server client worker. A thread is created specifically to
 * service this client's messages.
 */
ServerWorker::ServerWorker(int fd, const struct sockaddr_storage &addr, 
        SSL *ssl) : socket(fd), addr(addr), ssl(ssl) {
    // create the worker thread
    this->shouldTerminate = this->workerDone = false;
    this->worker = std::make_unique<std::thread>(&ServerWorker::main, this);
}
/**
 * Allocates all required message handlers.
 */
void ServerWorker::initHandlers() {
    IMessageHandler::forEach([this] (auto tag, auto ctor) {
        this->handlers.push_back(ctor(this));
    });
}

/**
 * When deallocating, force SSL session closure, if not done already, and wait
 * for the worker thread to join.
 */
ServerWorker::~ServerWorker() {
    // ensure we're not executing on the worker thread
    auto thisId = std::this_thread::get_id();
    XASSERT(thisId != this->worker->get_id(), 
            "Cannot destruct ServerWorker() from worker thread");

    Logging::trace("~ServerWorker() {} {}", (void*)this, this->addr);

    // if the termination flag isn't set, force close the SSL session
    if(!this->shouldTerminate) {
        int clientFd = this->socket;
        this->socket = -1;
        this->shutdownCause = Destructor;
        this->shouldTerminate = true;

        if(!this->skipShutdown) {
            SSL_shutdown(this->ssl);
        }
        close(clientFd);
    }

    // wait for the worker thread to join
    this->worker->join();
}



/**
 * Client handler main loop; this continuously waits to receive data from the
 * client, or to be notified to shut down.
 */
void ServerWorker::main() {
    Logging::debug("Starting client worker {}/{}", (void*)this, this->addr);

    // allocate message handlers
    this->initHandlers();

    // main read loop
    while(!this->shouldTerminate) {
        struct MessageHeader header;
        memset(&header, 0, sizeof(header));

        std::vector<unsigned char> payload;

        try {
            // read the message header and try to handle the message type
            if(!this->readHeader(header)) {
                // couldn't get a message but not a fatal error, try again
                goto beach;
            }

            // validate the version
            if(header.version != kLichtensteinProtoVersion) {
                Logging::error("Invalid protocol version {:02x} from client {} (expected {:02x})",
                        header.version, this->addr, kLichtensteinProtoVersion);
                this->shutdownCause = InvalidVersion;
                this->shouldTerminate = true;
                goto beach;
            }

#if 0
            Logging::trace("Message type {:x}:{:x} len {} from client {}", header.endpoint, 
                    header.messageType, header.length, this->addr);
#endif

            // check if we can find a handler
            for(auto &handler : this->handlers) {
                if(handler->canHandle(header.endpoint)) {
                    // if so, read and handle it; then read next message
                    this->readMessage(header, payload);
                    handler->handle(this, header, payload);

                    goto beach;
                }
            }

            // if we get here, no suitable handler found
            Logging::warn("Unsupported message type {:x}:{:x} from {}", header.endpoint, 
                    header.messageType, this->addr);
        } catch(std::exception &e) {
            Logging::error("Exception while processing request from {}: {}",
                    this->addr, e.what());
        }

beach: ;
        // run shutdown handler if graceful termination
        if(this->shouldTerminate) {
            for(auto &handler : this->shutdownHandlers) {
                handler(this->shutdownCause);
            }
        }
    }

    // clean up
    Logging::debug("Shutting down client {}/{}", (void*)this, this->addr);

    this->handlers.clear();
    
    if(this->socket > 0) {
        if(!this->skipShutdown) {
            SSL_shutdown(this->ssl);
        }

        // TODO: wait to get a shutdown alert from the client

        // close the socket
        close(this->socket);
        this->socket = -1;
    }

    // clean up the SSL context
    SSL_free(this->ssl);
    this->ssl = nullptr;

    this->workerDone = true;
}



/**
 * Reads a message header from the SSL context. This will read all the way up
 * to the start of the payload area.
 */
bool ServerWorker::readHeader(struct MessageHeader &outHdr) {
    int err;
    struct MessageHeader buf;
    memset(&buf, 0, sizeof(buf));

    // try to read the header length of bytes from the context
    err = this->readBytes(&buf, sizeof(buf));

    if(err == 0) {
        // retry later
        return false;
    } else if(err != sizeof(buf)) {
        // didn't get expected message length
        auto what = f("readHeader() failed, expected {} bytes, got {}",
                sizeof(buf), err);
        throw std::runtime_error(what);
    }

    // the header was read successfully
    buf.length = ntohs(buf.length);

    outHdr = buf;

    return true;
}

/**
 * Reads the entire amount of payload bytes into the given buffer. If the
 * entire payload cannot be read, an exception is thrown.
 */
void ServerWorker::readMessage(const struct MessageHeader &header, 
        std::vector<unsigned char> &buf) {
    int err;

    // resize output buffer and try to read message
    buf.resize(header.length);

    err = this->readBytes(buf.data(), header.length);

    if(err == 0) {
        throw std::runtime_error("Failed to read message body");
    } else if(err != header.length) {
        auto what = f("Only read {} of {} payload bytes", err, 
                header.length);
        throw std::runtime_error(what);
    }
}



/**
 * General read bytes from client routine. Returned is the total number of
 * bytes read, or 0 in case of a non-fatal error indicating that the read
 * should be retried.
 */
size_t ServerWorker::readBytes(void *buf, size_t numBytes) {
    int err, err2;

    // try to read from SSL context
    err = SSL_read(this->ssl, buf, numBytes);

    // handle errors
    if(err <= 0) {
        err2 = SSL_get_error(this->ssl, err);

        switch(err2) {
            // SSL lib wants to read (e.g. read timeout)
            case SSL_ERROR_WANT_READ:
                return 0;
            // connection was closed by peer
            case SSL_ERROR_ZERO_RETURN:
                this->shouldTerminate = true;
                throw std::runtime_error("Connection closed");
            // syscall error
            case SSL_ERROR_SYSCALL:
                this->skipShutdown = true;
                throw std::system_error(errno, std::generic_category(),
                        "Client read failed");
            // protocol error
            case SSL_ERROR_SSL:
                this->skipShutdown = true;
                this->shouldTerminate = true;
                throw SSLError("SSL_read() failed");
            // other, unknown error
            default:
                auto what = f("Unexpected SSL_read() error {}", err2);
                throw std::runtime_error(what);
        }
    }
    // otherwise, a partial read was accomplished
    else {
        return err;
    }
}

/**
 * General write bytes to client routine. The entire buffer must be written to
 * the client for the call to be treated as a success.
 *
 * The total number of bytes written is returned.
 */
size_t ServerWorker::writeBytes(const void *buf, size_t numBytes) {
    int err, err2;
    
    // attempt writing
    err = SSL_write(this->ssl, buf, numBytes);

    if(err <= 0) {
        err2 = SSL_get_error(this->ssl, err);

        switch(err2) {
            // connection was closed by peer
            case SSL_ERROR_ZERO_RETURN:
                this->shouldTerminate = true;
                throw std::runtime_error("Connection closed");
            // syscall error
            case SSL_ERROR_SYSCALL:
                this->skipShutdown = true;
                throw std::system_error(errno, std::generic_category(),
                        "Client write failed");
            // protocol error
            case SSL_ERROR_SSL:
                this->skipShutdown = true;
                this->shouldTerminate = true;
                throw SSLError("SSL_write() failed");
            // other, unknown error
            default:
                auto what = f("Unexpected SSL_write() error {}", err2);
                throw std::runtime_error(what);
        }

    }
    // if return is positive, that's the number of bytes written
    else {
        return err;
    }
}
