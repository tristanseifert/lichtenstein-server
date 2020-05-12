#include "IController.h"

#include "Server.h"

using namespace Lichtenstein::Server::API;

/**
 * Acquires a reference to the HTTPlib server, and passes it to the routing
 * callback.
 *
 * Since this is a plain reference, controllers shouldn't hang on to it beyond
 * the length of the function.
 */
void IController::route(RouteCallback cb) {
    // get a reference to the HTTP server and invoke the routes callback
    auto http = this->api->http;
    cb(http.get());
}
