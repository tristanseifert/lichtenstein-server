#include "Client.h"

#include <Format.h>
#include <Logging.h>
#include <ConfigManager.h>

#include <stdexcept>

using namespace Lichtenstein::Client::Proto;

/// singleton client instance
std::shared_ptr<Client> Client::shared = nullptr;

/**
 * Allocates the shared client instance, which will automagically attempt to
 * connect to the server.
 */
void Client::start() {
    XASSERT(shared == nullptr, "Repeated calls to Client::start() not allowed");

    shared = std::make_shared<Client>();
}

/**
 * Tears down the client handler and cleans up associated resources.
 */
void Client::stop() {
    XASSERT(shared != nullptr, "Shared client must be set up");

    shared->terminate();
    shared = nullptr;
}



/**
 * Creates a protocol client. This will set up a work thread that performs all
 * connection and message handling in the background.
 */
Client::Client() {
    // read all required config
    auto uuidStr = ConfigManager::get("id.uuid", "");
    auto uuid = uuids::uuid::from_string(uuidStr);
    if(!uuid.has_value()) {
        auto what = f("Failed to parse uuid string '{}'", uuidStr);
        throw std::runtime_error(what);
    }


    // create the worker thread
    this->run = true;
    this->worker = std::make_unique<std::thread>(&Client::workerMain, this);
}

/**
 * Cleans up client resources. This will waits for the work thread to join
 * before we can exit.
 */
Client::~Client() {
    // we should've had terminate called before
    if(this->run) {
        Logging::error("No call to Client::terminate() before destruction!");
        this->terminate();
    }

    // wait for worker to finish execution
    this->worker->join();
}

/**
 * Prepared the client for termination by signalling the worker thread.
 */
void Client::terminate() {
    if(!this->run) {
        Logging::error("Ignoring repeated call to Client::terminate()");
        return;
    }

    Logging::debug("Requesting client worker termination");
    this->run = false;
}


/**
 * Entry point for the client worker thread.
 */
void Client::workerMain() {
    int attempts;
    bool success;

    // attempt to connect to the server
connect: ;
    attempts = 0;
    do {
        success = this->connect();
        attempts++;
    } while(!success);
    if(!success && attempts > kConnectionAttempts) {
        auto what = f("Failed to connect to server in {} attempts", attempts);
        throw std::runtime_error(what);
    }

    // then, attempt to authenticate; auth failure is an immediate error
    success = this->authenticate();
    if(!success) {
        throw std::runtime_error("Failed to authenticate");
    }

    // process messages as long as we're supposed to run
    while(this->run) {

    }

    // perform clean-up
    Logging::debug("Client worker thread is shutting down");
}



/**
 * Attempts to open a connection to the server.
 *
 * @return true if connection was successful, false if it was not but should be
 * retrued at a later time.
 */
bool Client::connect() {
    return false;
}

/**
 * Uses our node UUID and shared secret to authenticate the connection.
 *
 * @return true if server accepted credentials, false otherwise.
 */
bool Client::authenticate() {
    return false;
}
