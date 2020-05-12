/**
 * API request handlers should conform to this interface.
 */
#ifndef API_ICONTROLLER_H
#define API_ICONTROLLER_H

#include <functional>

#include <nlohmann/json_fwd.hpp>

namespace httplib {
    class Server;
    class Request;
    class Response;
    class ContentReader;
}

namespace Lichtenstein::Server::API {
    class Server;

    class IController {
        protected:
            using ReqType = httplib::Request;
            using ResType = httplib::Response;
            using ReaderType = httplib::ContentReader;
            using RouteCallback = std::function<void(httplib::Server *)>;

        public:
            IController() = delete;
            IController(Server *api) : api(api) {};
            virtual ~IController() = default;

        protected:
            virtual void route(RouteCallback) final;

            virtual void respond(nlohmann::json &, ResType &);
            virtual void decode(const ReaderType &, nlohmann::json &); 
        public:
            static void respond(nlohmann::json &, ResType &, bool);

        protected:
            Server *api = nullptr;
    };
}

#endif
