/**
 * Handles a single client connection to the protocol server. This contains a
 * thread dedicated to servicing that client's requests.
 */
#ifndef PROTO_SERVERWORKER_H
#define PROTO_SERVERWORKER_H

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <functional>

#include <sys/socket.h>

#include <openssl/ssl.h>

namespace Lichtenstein::Server::Proto {
    struct MessageHeader;

    class ServerWorker {
        public:
            ServerWorker() = delete;
            ServerWorker(int, const struct sockaddr_storage &, SSL *);

            ~ServerWorker();

        public:
            /**
             * Signals to the client handler that it should terminate as soon
             * as possible. This is an optimization to terminate SSL sessions
             * before the destructor is run, so it spends significantly less
             * time waiting on the worker to join.
             */
            void signalShutdown() {
                this->shouldTerminate = true;
                this->shutdownCause = 1;
            }

            /**
             * Returns whether the worker thread has terminated. Once this is
             * the case, it is safe to deallocate the worker from any thread
             * without possibly deadlocking.
             */
            bool isDone() const {
                return this->workerDone;
            }

            /**
             * Adds a shutdown handler. The integer argument is negative if
             * the shutdown was an abort.
             */
            void addShutdownHandler(std::function<void(int)> f) {
                this->shutdownHandlers.push_back(f);
            }

        private:
            void main();

            bool readHeader(struct MessageHeader &);

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

            // when set, do not attempt to shut down the connection
            bool skipShutdown = false;
            // the worker thread has finished
            std::atomic_bool workerDone;

            // notification handlers for shutdown
            std::vector<std::function<void(int)>> shutdownHandlers;
            int shutdownCause = 0;
    };
}

#endif
