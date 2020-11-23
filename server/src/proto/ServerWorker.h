/**
 * Handles a single client connection to the protocol server. This contains a
 * thread dedicated to servicing that client's requests.
 */
#ifndef PROTO_SERVERWORKER_H
#define PROTO_SERVERWORKER_H

#include "proto/WireMessage.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <functional>
#include <cstddef>

#include <sys/socket.h>

#include <openssl/ssl.h>

namespace Lichtenstein::Server::Proto {
    namespace Controllers {
        class Authentication;
    }

    class IMessageHandler;

    class ServerWorker {
        using Header = struct Lichtenstein::Proto::MessageHeader;
        
        friend class IMessageHandler;
        friend class Controllers::Authentication;

        public:
            ServerWorker() = delete;
            ServerWorker(int, const struct sockaddr_storage &, SSL *);

            ~ServerWorker();

        private:
            /**
             * Defines why the client handler terminated. If ORed with 0x8000,
             * that shutdown cause means the client handler can be deallocated
             * at a later time safely, e.g. for garbage collection.
             */
            enum ShutdownType {
                Normal = 0 | 0x8000,
                Signalled = 1,
                Destructor = 2,
                InvalidVersion = 3 | 0x8000,
            };

        public:
            /**
             * Signals to the client handler that it should terminate as soon
             * as possible. This is an optimization to terminate SSL sessions
             * before the destructor is run, so it spends significantly less
             * time waiting on the worker to join.
             */
            void signalShutdown() {
                this->shouldTerminate = true;
                this->shutdownCause = Signalled;
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

            /**
             * Returns the authentication state of the worker.
             */
            bool isAuthenticated() const {
                return this->authenticated;
            }
            /**
             * Gets the node ID served by this worker. If not authenticated,
             * -1 is returned.
             */
            int getNodeId() const {
                if(!this->authenticated) {
                    return -1;
                }

                return this->nodeId;
            }

        private:
            void main();

            bool readHeader(Header &);
            void readMessage(const Header &, std::vector<unsigned char> &);

            void initHandlers();

        private:
            size_t readBytes(void *, size_t);
            size_t writeBytes(const void *, size_t);

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
            ShutdownType shutdownCause = Normal;

            // message handlers, allocated during connection
            std::vector<std::unique_ptr<IMessageHandler>> handlers;

            // whether the client has authenticated, and its node id if so
            bool authenticated = false;
            int nodeId = -1;
    };
}

#endif
