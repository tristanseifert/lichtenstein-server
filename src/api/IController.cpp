#include "IController.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "../Logging.h"
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

void IController::respond(nlohmann::json &j, ResType &r) {
    respond(j, r, this->api->shouldMinify());
}


/**
 * Serializes a JSON object and sets it as the request's response.
 */
void IController::respond(nlohmann::json &j, ResType &r, bool minify) {
    std::string out;

    if(minify) {
        out = j.dump();
    } else {
        out = j.dump(4);
    }

    r.set_content(out, "application/json");
}


/**
 * Attempts to read the request body and parse it as JSON.
 */
void IController::decode(const ReaderType &reader, nlohmann::json &j) {
    // read the entire body string
    std::string body;
    reader([&](const char *data, size_t len) {
        body.append(data, len);
        return true;
    });

    // attempt to parse to json
    j = nlohmann::json::parse(body);
} 

