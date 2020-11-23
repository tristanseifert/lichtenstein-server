#include "Authentication.h"
#include "proto/WireMessage.h"
#include "../Server.h"
#include "../ServerWorker.h"
#include "../auth/IAuthHandler.h"

#include <Format.h>
#include <Logging.h>
#include "db/DataStore.h"

#include <stdexcept>

#include <cista.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Proto::MessageTypes;
using namespace Lichtenstein::Server::Proto::Controllers;

using IMessageHandler = Lichtenstein::Server::Proto::IMessageHandler;

/**
 * Supported authentication mechanisms, in descending order.
 */
const std::vector<std::string> Authentication::kSupportedMethods = {
    "me.tseifert.lichtenstein.auth.null",
};

/// registers the controller
bool Authentication::registered = 
    IMessageHandler::registerClass("Authentication", 
            &Authentication::construct);

/**
 * Constructor method registered in the message handlers
 */
std::unique_ptr<IMessageHandler> Authentication::construct(ServerWorker *w) {
    return std::make_unique<Authentication>(w);
}



/**
 * Constructs the auth handler.
 */
Authentication::Authentication(ServerWorker *client) : IMessageHandler(client) {
    // clear out state machine
    this->state = Idle;
}

/**
 * Resets the client's auth state and deallocates any auth strategies we've
 * used.
 */
Authentication::~Authentication() {
    this->handler = nullptr;
}



/**
 * We can handle all messages send to the auth endpoint. If the message is 
 * unexpected or corrupt, we'll raise an error during handling.
 */
bool Authentication::canHandle(uint8_t type) {
    return (type == MessageEndpoint::Authentication);
}

/**
 * Handles an authentication message. This is basically a state machine that
 * will step through the separate stages of the auth cycle.
 */
void Authentication::handle(ServerWorker *worker, const struct MessageHeader &header, PayloadType &payload) {
    // process the message based on state
    switch(this->state) {
        // idle; we expect an auth request
        case Idle: {
            if(header.messageType != AuthMessageType::AUTH_REQUEST) {
                throw std::runtime_error("Invalid message type");
            }

            auto request = cista::deserialize<AuthRequest, kCistaMode>(payload);
            this->handleAuthReq(worker, header, request);
            break;
        }

        // parse an authentication response
        case HandleResponse: {
            if(header.messageType != AuthMessageType::AUTH_RESPONSE) {
                throw std::runtime_error("Invalid message type");
            }

            auto response = cista::deserialize<AuthResponse, kCistaMode>(payload);
            this->handleAuthResp(worker, header, response);
            break;
        }

        // Authentication success
        case Authenticated: {
            Logging::error("Received unexpected auth packet {:x}", header.messageType);
            break;
        }

        // shouldn't get here
        default: {
            auto what = f("Invalid state {}", this->state);
            throw std::logic_error(what);
        }
    }
}



/**
 * A client sent an authentication request. Select the strongest algorithm we
 * have in common with the client, initialize the handler, and respond to the
 * client with the required information.
 */
void Authentication::handleAuthReq(ServerWorker *worker, const Header &hdr, const AuthReq *msg) {
    int bestMethod = -1;

    AuthRequestAck ack;
    memset(&ack, 0, sizeof(ack));

    // get node UUID
    auto maybeUuid = uuids::uuid::from_string(msg->nodeId.str());
    if(!maybeUuid.has_value()) {
        throw std::runtime_error("Invalid node id");
    }
    const auto uuid = maybeUuid.value();

    Logging::trace("Auth request from {}", uuids::to_string(uuid));

    // locate a node with this ID
    if(!this->updateNodeId(worker, uuid)) {
        ack.status = AuthStatus::AUTH_INVALID_ID;
        goto nack;
    }

    // find the best supported auth method
    for(const auto method : msg->methods) {
        const std::string str = method.str();

        auto it = std::find(kSupportedMethods.begin(), kSupportedMethods.end(), str);
        if(it != kSupportedMethods.end()) {
            size_t idx = it - kSupportedMethods.begin();

            if(idx < bestMethod || bestMethod == -1) {
                bestMethod = idx;
            }
        }
    }

    if(bestMethod < 0) {
        ack.status = AuthStatus::AUTH_NO_METHODS;
        goto nack;
    }

    // TODO: instantiate appropriate auth handler
    Logging::trace("Using auth method {}: {}", bestMethod, kSupportedMethods[bestMethod]);

    // acknowledge the request
    ack.method = kSupportedMethods[bestMethod];

    {
        const auto ackData = cista::serialize<kCistaMode>(ack);
        this->reply(hdr, AuthMessageType::AUTH_REQUEST_ACK, ackData);
    }
    this->state = HandleResponse;
    return;

    // negative acknowledge
nack:;
    const auto nackData = cista::serialize<kCistaMode>(ack);
    this->reply(hdr, AuthMessageType::AUTH_REQUEST_ACK, nackData);

    throw std::runtime_error("No common auth methods");
}

/**
 * Attempts to find a node with the given UUID in the database. If found, update the caller's node
 * id value appropriately.
 */
bool Authentication::updateNodeId(ServerWorker *worker, const uuids::uuid &uuid) {
    using namespace Lichtenstein::Server::DB;

    Types::Node node;

    // get the node from the data store
    if(!DataStore::db()->getNodeForUuid(uuid, node)) {
        return false;
    }

    // it was found, get its id and update the server worker state
    if(worker->nodeId != -1) {
        Logging::warn("Changing node id! {} {} -> {}", (void *) worker, worker->nodeId, node.id);
    }

    Logging::trace("Node uuid {} -> id {}", uuids::to_string(uuid), node.id);

    return true;
}



/**
 * Validates a client's authentication response.
 */
void Authentication::handleAuthResp(ServerWorker *worker, const Header &hdr, const AuthResp *msg) {
    AuthResponseAck ack;
    memset(&ack, 0, sizeof(ack));

    // validate status
    if(msg->status != AUTH_SUCCESS) {
        Logging::warn("Authentication aborted with status {}", msg->status);

        this->state = Idle;
        throw std::runtime_error("Authentication aborted");
    }

    // TODO: invoke the auth handler

    // authentication succeded. update state
    worker->authenticated = true;
    Logging::info("Authentication state for {}: {}", (void *) worker, worker->isAuthenticated());

    // also, let the client know it's now authenticated
    const auto ackData = cista::serialize<kCistaMode>(ack);
    this->reply(hdr, AuthMessageType::AUTH_RESPONSE_ACK, ackData);
    this->state = Authenticated;
}

