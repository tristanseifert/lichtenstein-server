#include "Client.h"

#include <Format.h>
#include <Logging.h>
#include <ConfigManager.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdexcept>
#include <system_error>

#include <base64.h>

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
    // read node uuid
    auto uuidStr = ConfigManager::get("id.uuid", "");
    if(uuidStr.empty()) {
        throw std::runtime_error("Node UUID (id.uuid) is required");
    }

    auto uuid = uuids::uuid::from_string(uuidStr);
    if(!uuid.has_value()) {
        auto what = f("Couldn't parse uuid string '{}'", uuidStr);
        throw std::runtime_error(what);
    }

    // read node secret
    auto secretStr = ConfigManager::get("id.secret", "");
    if(secretStr.empty()) {
        throw std::runtime_error("Node secret (id.secret) is required");
    }

    auto secretDecoded = base64_decode(secretStr);
    if(secretDecoded.empty()) {
        auto what = f("Couldn't decode base64 string '{}'", secretStr);
        throw std::runtime_error(what);
    }
    else if(secretDecoded.size() < kSecretMinLength) {
        auto what = f("Got {} bytes of node secret; expected at least {}",
                secretDecoded.size(), (const size_t)kSecretMinLength);
        throw std::runtime_error(what);
    }

    auto secretBytes = reinterpret_cast<std::byte *>(secretDecoded.data());
    this->secret.assign(secretBytes, secretBytes+secretDecoded.size());

    // get server address/port and attempt to resolve
    this->serverV4Only = ConfigManager::getBool("remote.server.v4Only", false);

    this->serverHost = ConfigManager::get("remote.server.address", "");
    if(this->serverHost.empty()) {
        throw std::runtime_error("Remote address (remote.server.address) is required");
    }

    this->serverPort = ConfigManager::getUnsigned("remote.server.port", 7420);
    if(this->serverPort > 65535) {
        auto what = f("Invalid remote port {}", this->serverPort);
        throw std::runtime_error(what);
    }

    this->resolve();

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
 * Tries to resolve the server hostname/address into an IP address that we can
 * connect to.
 */
void Client::resolve() {
    int err;
    struct addrinfo hints, *res = nullptr;

    // provide hints to resolver (if we want IPv4 only or all)
    memset(&hints, 0, sizeof(hints));

    if(this->serverV4Only) {
        hints.ai_family = AF_INET;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags |= (AI_ADDRCONFIG | AI_V4MAPPED);

    // attempt to resolve it; then get the first address
    std::string port = f("{}", this->serverPort);

    err = getaddrinfo(this->serverHost.c_str(), port.c_str(), &hints, &res);
    if(err != 0) {
        throw std::system_error(errno, std::generic_category(),
                "getaddrinfo() failed");
    }

    if(!res) {
        auto what = f("Failed to resolve '{}'", this->serverHost);
        throw std::runtime_error(what);
    }

    // just pick the first result
    XASSERT(res->ai_addrlen <= sizeof(this->serverAddr),
            "Invalid address length {}; have space for {}", res->ai_addrlen,
            sizeof(this->serverAddr));
    memcpy(&this->serverAddr, res->ai_addr, res->ai_addrlen);

    Logging::debug("Resolved '{}' -> {}", this->serverHost, this->serverAddr);

    // clean up
done:;
    freeaddrinfo(res);
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
        
        if(!success && attempts > kConnectionAttempts) {
            auto what = f("Failed to connect to server in {} attempts",
                    attempts);
            throw std::runtime_error(what);
        }
    } while(!success);

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
