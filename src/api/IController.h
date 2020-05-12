/**
 * API request handlers should conform to this interface.
 */
#ifndef API_ICONTROLLER_H
#define API_ICONTROLLER_H

#include <functional>

namespace httplib { class Server; }

namespace Lichtenstein::Server::API {
    class Server;

    class IController {
        using RouteCallback = std::function<void(httplib::Server *)>;

        public:
            IController() = delete;
            IController(Server *server) : api(server) {}
            virtual ~IController() = default;

        protected:
            virtual void route(RouteCallback) final;

        protected:
            Server *api = nullptr;
    };
}

#endif
