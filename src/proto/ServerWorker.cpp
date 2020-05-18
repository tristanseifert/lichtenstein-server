#include "ServerWorker.h"
#include "Server.h"
#include "SocketTypes+fmt.h"
#include "WireMessage.h"

#include "../Logging.h"
#include "../ConfigManager.h"

#include <stdexcept>

#include <unistd.h>

#include <openssl/ssl.h>
#include <fmt/format.h>

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
        this->shutdownCause = 2;
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

    // main read loop
    while(!this->shouldTerminate) {
        struct MessageHeader header;

        try {
            // read the message header and try to handle the message type
            if(!this->readHeader(header)) {
                // couldn't get a message but not an error, try again
                goto beach;
            }
            Logging::trace("Message type {:x} len {} from client {}",
                    header.type, header.length, this->addr);

            // TODO: deserialize
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

    // try to read the header length of bytes from the context
    err = SSL_read(this->ssl, &buf, sizeof(buf));

    if(err != sizeof(buf)) {
        // didn't read enough bytes?
        if(err >= 1) {
            auto what = fmt::format("readHeader() failed, expected {} bytes, got {}",
                    sizeof(buf), err);
            throw std::runtime_error(what);
        }
        // it's some form of error
        else {
            int err2 = SSL_get_error(this->ssl, err);

            switch(err2) {
                // SSL lib wants to read (e.g. read timeout)
                case SSL_ERROR_WANT_READ:
                    return false;
                // connection was closed by peer
                case SSL_ERROR_ZERO_RETURN:
                    this->shouldTerminate = true;
                    return false;
                // syscall error
                case SSL_ERROR_SYSCALL:
                    this->skipShutdown = true;
                    throw std::system_error(errno, std::generic_category(),
                            "Client read failed");
                // protocol error
                case SSL_ERROR_SSL:
                    this->skipShutdown = true;
                    this->shouldTerminate = true;
                    throw SSLError("SSL_read() for header failed");
                // other, unknown error
                default:
                    auto what = fmt::format("Unexpected SSL_read() error {}", err2);
                    throw std::runtime_error(what);
            }
        }
    }

    // the header was read successfully
    outHdr = buf;

    return true;
}
