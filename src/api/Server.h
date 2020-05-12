/**
 * Control of the server is accomplished through a basic REST-style API, which
 * is provided by this class.
 *
 * See the config file for details on configuring the server.
 */
#ifndef API_SERVER_H
#define API_SERVER_H

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <httplib.h>

namespace Lichtenstein::Server::API {
    class IController;

    class Server {
        friend class IController;

        public:
            static void start();
            static void stop();

            // you should not call this!
            Server();
            virtual ~Server();

        private:
            void terminate();

            void workerEntry();
            void allocServer();
            void listen();

        private:
            std::atomic_bool terminateWorker = false;
            std::unique_ptr<std::thread> worker = nullptr;

            std::shared_ptr<httplib::Server> http = nullptr;

            std::vector<std::unique_ptr<IController>> handlers;
    };
}

#endif
