/**
 * API request handlers should conform to this interface.
 */
#ifndef API_ICONTROLLER_H
#define API_ICONTROLLER_H

#include <functional>
#include <memory>

#include <nlohmann/json_fwd.hpp>
#include <httplib.h>

namespace Lichtenstein::Server::API {
    class Server;

    class IController {
        protected:
            using ReqType = httplib::Request;
            using ResType = httplib::Response;
            using ReaderType = httplib::ContentReader;
            
            // wrapper around the HTTP server for request registration
            class Router {
                friend class IController;

                using Handler = httplib::Server::Handler;
                using HandlerWithBody = httplib::Server::HandlerWithContentReader;

                public:
                    Router() {} 
                    Router(IController *c) : controller(c) {}

                public:
                    void Get(const char *pattern, Handler handler);
                    void Post(const char *pattern, HandlerWithBody handler);
                    void Put(const char *pattern, HandlerWithBody handler);
                    void Patch(const char *pattern, HandlerWithBody handler);
                    void Delete(const char *pattern, Handler handler);
                    void Options(const char *pattern, Handler handler);

                private:
                    void wrapHandler(const ReqType &, ResType &, Handler);
                    void wrapHandlerBody(const ReqType &, ResType &, 
                            const ReaderType &, HandlerWithBody);

                    void exceptionHandler(const ReqType &, ResType &, 
                        const std::exception &);

                private:
                    IController *controller = nullptr;
            };

        protected:
            using RouteCallback = std::function<void(Router *)>;

        public:
            IController() = delete;
            IController(Server *api) : api(api) {
                this->router.controller = this;
            };
            virtual ~IController() = default;

        protected:
            virtual void route(RouteCallback) final;

            virtual void respond(const nlohmann::json &, ResType &);
            virtual void decode(const ReaderType &, nlohmann::json &); 
        public:
            static void respond(const nlohmann::json &, ResType &, bool);

        protected:
            Server *api = nullptr;

            Router router;
    };
}

#endif
