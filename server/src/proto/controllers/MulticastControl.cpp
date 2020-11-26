#include "MulticastControl.h"

#include <proto/WireMessage.h>
#include "../Syncer.h"

#include <Format.h>
#include <Logging.h>

#include <stdexcept>

#include <cista.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Proto::MessageTypes;
using namespace Lichtenstein::Server::Proto::Controllers;

using IMessageHandler = Lichtenstein::Server::Proto::IMessageHandler;

/// registers the controller
bool MulticastControl::registered = 
    IMessageHandler::registerClass("MulticastControl", &MulticastControl::construct);

/**
 * Constructor method registered in the message handlers
 */
std::unique_ptr<IMessageHandler> MulticastControl::construct(ServerWorker *w) {
    return std::make_unique<MulticastControl>(w);
}

/**
 * Registers a rekey observer.
 */
MulticastControl::MulticastControl(ServerWorker *client) : IMessageHandler(client) {
    this->observer = Syncer::shared()->registerObserver([this](uint32_t newKeyId) {
        this->rekeyCallback(newKeyId);
    });
}
/**
 * Removes the observer.
 */
MulticastControl::~MulticastControl() {
    Syncer::shared()->removeObserver(this->observer);
}


/**
 * All messages for the pixel data endpoint are handled by us.
 */
bool MulticastControl::canHandle(uint8_t type) {
    return (type == MessageEndpoint::MulticastControl);
}

/**
 * Handles client subscribe/unsubscribe requests.
 */
void MulticastControl::handle(ServerWorker *worker, const struct MessageHeader &header, PayloadType &payload) {
    XASSERT(header.endpoint == MessageEndpoint::MulticastControl, "invalid message endpoint");
    this->requireAuth();

    // handle the message
    switch(header.messageType) {
        // get multicast configuration
        case McastCtrlMessageType::MCC_GET_INFO:
            this->handleGetInfo(header, cista::deserialize<GetInfoMsg, kCistaMode>(payload));
            break;

        default:
            throw std::runtime_error("Invalid message type");
    }
}



/**
 * Gets information on the multicast connection.
 */
void MulticastControl::handleGetInfo(const Header &hdr, const GetInfoMsg *msg) {
    McastCtrlGetInfoAck info;
    memset(&info, 0, sizeof(info));

    info.status = MCC_SUCCESS;

    // multicast address/port
    info.address = Syncer::shared()->getGroupAddress();
    info.port = Syncer::shared()->getGroupPort();

    // currently used key
    info.keyId = Syncer::shared()->getCurrentKeyId();

    // send message
    const auto ackData = cista::serialize<kCistaMode>(info);
    this->reply(hdr, McastCtrlMessageType::MCC_GET_INFO_ACK, ackData);
}



/**
 * Observer callback indicating a rekey occurred.
 */
void MulticastControl::rekeyCallback(uint32_t newKeyId) {
    // send a rekey notification to the client
    this->sendRekey();
}

/**
 * Sends new key info to the client.
 */
void MulticastControl::sendRekey() {
    McastCtrlRekey msg;
    memset(&msg, 0, sizeof(msg));

    // key type information
    msg.keyId = Syncer::shared()->getCurrentKeyId();
    msg.type = MCC_KEY_TYPE_CHACHA20_POLY1305;

    // copy key material
    const auto keyData = Syncer::shared()->getKeyData(msg.keyId);
    msg.key.reserve(keyData.size());
    for(size_t i = 0; i < keyData.size(); i++) {
        msg.key.push_back(keyData[i]);
    }

    const auto ivData = Syncer::shared()->getIVData(msg.keyId);
    msg.iv.reserve(ivData.size());
    for(size_t i = 0; i < ivData.size(); i++) {
        msg.iv.push_back(ivData[i]);
    }

    // send it
    const auto data = cista::serialize<kCistaMode>(msg);
    this->send(MessageEndpoint::MulticastControl, McastCtrlMessageType::MCC_REKEY,
            this->nextTag++, data);
}
