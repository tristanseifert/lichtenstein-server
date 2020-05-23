#include "Authentication.h"
#include "../Server.h"
#include "../WireMessage.h"

#include "../../Logging.h"
#include "../../db/DataStore.h"

#include <stdexcept>

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

}

/**
 * Resets the client's auth state and deallocates any auth strategies we've
 * used.
 */
Authentication::~Authentication() {

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
void Authentication::handle(struct MessageHeader &header,
        std::vector<std::byte> &payload) {
    throw std::runtime_error("Unimplemented");
}

