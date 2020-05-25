#include "ServerInfo.h"

#include <functional>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "version.h"
#include "Logging.h"

#include "../HandlerFactory.h"

using IController = Lichtenstein::Server::API::IController;
using namespace Lichtenstein::Server::API::Controllers;
using json = nlohmann::json;

// register to handler factory
bool ServerInfo::registered = HandlerFactory::registerClass("ServerInfo", 
        ServerInfo::construct);

std::unique_ptr<IController> ServerInfo::construct(Server *srv) {
    return std::make_unique<ServerInfo>(srv);
}


/**
 * Constructs the ServerInfo controller, registering all of its routes.
 */
ServerInfo::ServerInfo(Server *srv) : IController(srv) {
    using namespace std::placeholders;

    // register server information routes
    this->route([this] (auto http) mutable {
        http->Get("/server/version", 
                std::bind(&ServerInfo::getVersion, this, _1, _2));
    });
}



/**
 * Returns the server software version.
 */
void ServerInfo::getVersion(const ReqType &req, ResType &res) {
    // build a static response
    json j;

    j["what"] = "Lichtenstein Server";
    j["info_url"] = "https://github.com/tristanseifert/lichtenstein-server";
    j["version"] = std::string(gVERSION);
    j["git_rev"] = std::string(gVERSION_HASH);

    // send it
    this->respond(j, res);
}

