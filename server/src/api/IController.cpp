#include "IController.h"
#include "Server.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <Logging.h>

using namespace Lichtenstein::Server::API;

/**
 * Initializes routes. The routing callback gets a pointer to our internal
 * routing wrapper, which allows custom exception handling.
 */
void IController::route(RouteCallback cb) {
    cb(&this->router);
}

void IController::respond(const nlohmann::json &j, ResType &r) {
    respond(j, r, this->api->shouldMinify());
}


/**
 * Serializes a JSON object and sets it as the request's response.
 */
void IController::respond(const nlohmann::json &j, ResType &r, bool minify) {
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



/**
 * Exception handler. This prints the error details to the log, and returns
 * a server error to the client.
 */
void IController::Router::exceptionHandler(const ReqType &req, ResType &res, 
        const std::exception &e) {
    // log details
    Logging::error("API error: {:>7s} {} {}:{} {} - {}", req.method, req.path, 
        req.remote_addr, req.remote_port, res.status, e.what());

    // return a 500 to the client
    res.status = 500;
    nlohmann::json j = {
        {"status", 500}
    };
    this->controller->respond(j, res);
}

/**
 * Wraps a regular non-bodied handler in exception handling.
 */
void IController::Router::wrapHandler(const ReqType &req, ResType &res, 
        Handler handler) {
    try {
        handler(req, res);
    } catch(std::exception &e) {
        this->exceptionHandler(req, res, e);
    }
}

/**
 * Wraps a with body handler in exception handling.
 */
void IController::Router::wrapHandlerBody(const ReqType &req, ResType &res,
        const ReaderType &body, HandlerWithBody handler) {
    try {
        handler(req, res, body);
    } catch(std::exception &e) {
        this->exceptionHandler(req, res, e);
    }
}



/// GET request wrapper
void IController::Router::Get(const char *pattern, Handler handler) {
    XASSERT(handler, "Invalid handler for {}", pattern);

    using namespace std::placeholders;
    auto wrap = std::bind(&IController::Router::wrapHandler, this,_1,_2, 
            handler);
    this->controller->api->http->Get(pattern, wrap);
}

/// POST request wrapper
void IController::Router::Post(const char *pattern, HandlerWithBody handler) {
    XASSERT(handler, "Invalid handler for {}", pattern);

    using namespace std::placeholders;
    auto wrap = std::bind(&IController::Router::wrapHandlerBody, this,_1,_2,_3,
            handler);
    this->controller->api->http->Post(pattern, wrap);
}

/// PUT request wrapper
void IController::Router::Put(const char *pattern, HandlerWithBody handler) {
    XASSERT(handler, "Invalid handler for {}", pattern);

    using namespace std::placeholders;
    auto wrap = std::bind(&IController::Router::wrapHandlerBody, this,_1,_2,_3,
            handler);
    this->controller->api->http->Put(pattern, wrap);
}

/// PATCH request wrapper
void IController::Router::Patch(const char *pattern, HandlerWithBody handler) {
    XASSERT(handler, "Invalid handler for {}", pattern);

    using namespace std::placeholders;
    auto wrap = std::bind(&IController::Router::wrapHandlerBody, this,_1,_2,_3,
            handler);
    this->controller->api->http->Patch(pattern, wrap);
}

/// DELETE request wrapper (without body)
void IController::Router::Delete(const char *pattern, Handler handler) {
    XASSERT(handler, "Invalid handler for {}", pattern);

    using namespace std::placeholders;
    Handler wrap = std::bind(&IController::Router::wrapHandler, this,_1,_2,
            handler);
    this->controller->api->http->Delete(pattern, wrap);
}

// OPTIONS request wrapper
void IController::Router::Options(const char *pattern, Handler handler) {
    XASSERT(handler, "Invalid handler for {}", pattern);

    using namespace std::placeholders;
    auto wrap = std::bind(&IController::Router::wrapHandler, this,_1,_2,
            handler);
    this->controller->api->http->Options(pattern, wrap);
}

