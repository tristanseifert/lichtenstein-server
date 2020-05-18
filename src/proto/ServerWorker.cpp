#include "ServerWorker.h"
#include "SocketTypes+fmt.h"

#include "../Logging.h"
#include "../ConfigManager.h"

#include <unistd.h>

#include <openssl/ssl.h>

using namespace Lichtenstein::Server::Proto;

/**
 * Creates a new server client worker. A thread is created specifically to
 * service this client's messages.
 */
ServerWorker::ServerWorker(int fd, const struct sockaddr_storage &addr, 
        SSL *ssl) : socket(fd), addr(addr), ssl(ssl) {
    // create the worker thread
    this->shouldTerminate = false;
    this->worker = std::make_unique<std::thread>(&ServerWorker::main, this);
}

/**
 * When deallocating, force SSL session closure, if not done already, and wait
 * for the worker thread to join.
 */
ServerWorker::~ServerWorker() {
    // if the termination flag isn't set, force close the SSL session
    if(!this->shouldTerminate) {
        int clientFd = this->socket;
        this->socket = -1;
        this->shouldTerminate = true;

        SSL_shutdown(this->ssl);
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
        // TODO: implement :)
    }

    // clean up
    Logging::debug("Shutting down client {}/{}", (void*)this, this->addr);
    
    if(this->socket > 0) {
        SSL_shutdown(this->ssl);

        // TODO: wait to get a shutdown alert from the client

        // close the socket
        close(this->socket);
        this->socket = -1;
    }

    // clean up the SSL context
    SSL_free(this->ssl);
    this->ssl = nullptr;
}
