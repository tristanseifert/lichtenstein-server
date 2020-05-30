/**
 * Lichtenstein protocol client; this handles the persistent DTLS connection to
 * the server, authenticating the connection, and acting as a mux for messages
 * between different endpoints.
 */
#ifndef PROTO_CLIENT_H
#define PROTO_CLIENT_H

#include <cstddef>
#include <vector>
#include <atomic>
#include <memory>
#include <thread>

#include <uuid.h>

namespace Lichtenstein::Client::Proto {
    class Client {
        public:
            // don't call these, it is a Shared Instance(tm)
            Client();
            ~Client();

        public:
            void terminate();

        public:
            static void start();
            static void stop();

            static std::shared_ptr<Client> get() {
                return shared;
            }


        private:
            void workerMain();

            bool connect();
            bool authenticate();

        private:
            uuids::uuid uuid;
            std::vector<std::byte> secret;

            std::atomic_bool run;
            std::unique_ptr<std::thread> worker = nullptr;

        private:
            static std::shared_ptr<Client> shared;

        private:
            static const size_t kConnectionAttempts = 10;
    };
}

#endif
