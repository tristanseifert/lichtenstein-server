#include "Authentication.h"
#include "proto/WireMessage.h"
#include "../Server.h"
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
void Authentication::handle(const struct MessageHeader &header, PayloadType &payload) {
    Logging::trace("Received message {} bytes: {}", payload.size(), hexdump(payload.begin(), payload.end()));

    // process the message based on state
    switch(this->state) {
        // idle; we expect an auth request
        case Idle: {
            if(header.messageType != AuthMessageType::AUTH_REQUEST) {
                throw std::runtime_error("Invalid message type");
            }

            auto request = cista::deserialize<AuthRequest, kCistaMode>(payload);
            this->handleAuthReq(header, request);
            break;
        }

        // parse an authentication response
        /*case HandleResponse: {
            this->handleAuthRes(auth.getResponse());
            break;
        }*/

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
void Authentication::handleAuthReq(const Header &hdr, const AuthReq *msg) {
    AuthRequestAck ack;
    memset(&ack, 0, sizeof(ack));

    // get node UUID
    auto maybeUuid = uuids::uuid::from_string(msg->nodeId.str());
    if(!maybeUuid.has_value()) {
        throw std::runtime_error("Invalid node id");
    }
    const auto uuid = maybeUuid.value();

    Logging::trace("Auth request from {}", uuids::to_string(uuid));

    // find the best supported auth method
    int bestMethod = -1;

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
        goto nack;
    }

    Logging::trace("Using auth method {}: {}", bestMethod, kSupportedMethods[bestMethod]);

    // acknowledge the request
    ack.method = kSupportedMethods[bestMethod];

    {
        const auto ackData = cista::serialize<kCistaMode>(ack);
        this->reply(hdr, AuthMessageType::AUTH_REQUEST_ACK, ackData);
    }
    return;

    // negative acknowledge
nack:;
    ack.status = 1;

    const auto nackData = cista::serialize<kCistaMode>(ack);
    this->reply(hdr, AuthMessageType::AUTH_REQUEST_ACK, nackData);

    throw std::runtime_error("No common auth methods");
}

/**
 * Validates a client's authentication response.
 */
/*void Authentication::handleAuthRes(const AuthRes &msg) {
    throw std::runtime_error("Unimplemented");
}*/

