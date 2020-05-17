#include "Server.h"

#include <memory>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "../Logging.h"
#include "../ConfigManager.h"

#include "HandlerFactory.h"

using namespace Lichtenstein::Server::API;

// global shared API server; created by the start() call
std::shared_ptr<Server> Server::sharedInstance;


/**
 * Initializes the shared API server.
 */
void Server::start() {
    sharedInstance = std::make_shared<Server>();
}

/**
 * Attempts to cleanly shut down the API server.
 */
void Server::stop() {
    // request termination and release our reference
    sharedInstance->terminate();
    sharedInstance = nullptr;
}



/**
 * Initializes the API server. This sets up the worker thread for handling of
 * requests, and configures the HTTP server.
 */
Server::Server() {
    // load the minification settings
    this->minifyResponses = ConfigManager::getBool("api.minify", true);

    // set up the worker thread
    this->shouldTerminate = false;
    this->worker = std::make_unique<std::thread>(&Server::workerEntry, this);
}
/**
 * Cleans up resources we've allocated. This waits for the worker thread to
 * finish its clean up and exit before returning.
 */
Server::~Server() {
    // make sure to tell the worker to terminate
    if(!this->shouldTerminate) {
        Logging::error("You should call API::Server::terminate() before deleting");
        this->terminate();
    }
    this->worker->join();
}

/**
 * Signals to the worker thread that it should terminate.
 */
void Server::terminate() {
    // catch repeated calls
    if(this->shouldTerminate) {
        Logging::error("Ignoring repeated call of API::Server::terminate()!");
        return;
    }

    // set the termination flag
    Logging::debug("Requesting API server termination");
    this->shouldTerminate = true;

    // if HTTP server exists, kill that
    if(this->http) {
        this->http->stop();
    }
}



/**
 * Entry point for the background request working thread
 */
void Server::workerEntry() {
    // set up the server and routes
    this->allocServer();
    this->listen();

    // clean up
    if(this->shouldTerminate) {
        Logging::debug("API server is shutting down");
    } else {
        Logging::error("API::Server::listen() returned unexpectedly");
    }

    // This will deallocate all handlers
    Logging::trace("Deallocating {} API controllers", this->handlers.size());
    this->handlers.clear();
}

/**
 * Creates a new server instance.
 */
void Server::allocServer() {
    // create the server instance…
    this->http = std::make_shared<httplib::Server>();

    // …then register all handlers.
    HandlerFactory::forEach([this] (const std::string &tag, HandlerFactory::HandlerCtor ctor) mutable {
        this->handlers.push_back(ctor(this));
        Logging::trace("Allocated API controller '{}'", tag);
    });

    // register a request logger and error handler
    this->http->set_logger([this](const auto &req, const auto &res) {
        Logging::trace("API request: {:>7s} {} {}:{} {}", req.method, req.path, 
                req.remote_addr, req.remote_port, res.status);
    });
}

/**
 * Binds the HTTP server to the requested port/IP and starts listening for
 * client requests.
 */
void Server::listen() {
    // read the config
    std::string host = ConfigManager::get("api.listen.address", "127.0.0.1");
    int port = ConfigManager::getUnsigned("api.listen.port", 42000);

    Logging::info("Starting API server: {}:{}", host, port);

    // begin listening
    if(!this->http->listen(host.c_str(), port)) {
        Logging::error("Failed to start API server on {}:{}", host, port);
    }
}
