/**
 * Handles a single client connection to the protocol server. This contains a
 * thread dedicated to servicing that client's requests.
 */
#ifndef PROTO_SERVERWORKER_H
#define PROTO_SERVERWORKER_H

#include <atomic>
#include <memory>
#include <thread>

#include <sys/socket.h>

#include <openssl/ssl.h>

namespace Lichtenstein::Server::Proto {
    class ServerWorker {
        public:
            ServerWorker() = delete;
            ServerWorker(int, const struct sockaddr_storage &, SSL *);

            ~ServerWorker();

        private:
            void main();

        private:
            // File descriptor for the raw client socket
            int socket = -1;
            // Client address
            struct sockaddr_storage addr;
            // SSL context
            SSL *ssl = nullptr;

            // client handling thread and run flag
            std::atomic_bool shouldTerminate;
            std::unique_ptr<std::thread> worker;
    };
}

#endif
