/**
 * The API handler factory is used as a registry of classes that can respond
 * to REST requests. Controllers will be automagically registered during app
 * load; the server will then instantiate each controller when it begins setup.
 */
#ifndef API_HANDLERFACTORY_H
#define API_HANDLERFACTORY_H

#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <functional>

#include "IController.h"

namespace Lichtenstein::Server::API {
    class Server;

    class HandlerFactory {
        public:
            using HandlerCtor = std::unique_ptr<IController>(*)(Server *);
            using MapType = std::map<std::string, HandlerCtor>;

        public:
            HandlerFactory() = delete;

        public:
            static bool registerClass(const std::string &tag, HandlerCtor ctor);
            static void forEach(std::function<void(const std::string&, HandlerCtor)> f);

            static void dump();

        private:
            static MapType *registrations;
            static std::mutex registerLock;
    };
}

#endif
