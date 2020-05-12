/**
 * Provides the /server API component; allows getting general information about
 * the server and some limited control.
 */
#ifndef API_CONTROLLERS_SERVERINFO_H
#define API_CONTROLLERS_SERVERINFO_H

#include <memory>

#include <httplib.h>

#include "../IController.h"

namespace Lichtenstein::Server::API::Controllers {
    class ServerInfo: public IController {
        using ReqType = httplib::Request;
        using ResType = httplib::Response;

        public:
            ServerInfo(Server *srv);
            virtual ~ServerInfo() {};

        private:
            void getVersion(const ReqType &req, ResType &res);

        private:
            static std::unique_ptr<IController> construct(Server *);
            static bool registered;
    };
}

#endif
