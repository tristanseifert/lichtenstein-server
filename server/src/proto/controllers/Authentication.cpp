#include "Authentication.h"
#include "proto/WireMessage.h"
#include "../Server.h"
#include "../auth/IAuthHandler.h"

#include <Format.h>
#include <Logging.h>
#include "db/DataStore.h"

#include <stdexcept>

// Cap'n Proto stuff 
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <proto/lichtenstein_v1.capnp.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Server::Proto::Controllers;
using IMessageHandler = Lichtenstein::Server::Proto::IMessageHandler;

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
void Authentication::handle(const struct MessageHeader &header,
        std::vector<std::byte> &payload) {
    using namespace WireTypes;
    // decode the protocol message
    auto msg = this->decode(payload);
    if(msg.getPayload().isNull()) {
        throw std::runtime_error("Payload is null");
    }

    // process the message based on state
    switch(this->state) {
        // idle; we expect an auth request
        case Idle: {
            auto req = msg.getPayload().getAs<AuthRequest>();
            this->handleAuthReq(req);
            break;
        }

        // parse an authentication response
        case HandleResponse: {
            auto res = msg.getPayload().getAs<AuthResponse>();
            this->handleAuthRes(res);
            break;
        }

        // shouldn't get here
        default: {
            auto what = f("Invalid state {}", this->state);
            throw std::logic_error(what);
            break;
        }
    }
}



/**
 * A client sent an authentication request. Select the strongest algorithm we
 * have in common with the client, initialize the handler, and respond to the
 * client with the required information.
 */
void Authentication::handleAuthReq(const WireTypes::AuthRequest::Reader &msg) {
    throw std::runtime_error("Unimplemented");
}

/**
 * Validates a client's authentication response.
 */
void Authentication::handleAuthRes(const WireTypes::AuthResponse::Reader &msg) {
    throw std::runtime_error("Unimplemented");
}

