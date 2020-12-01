#include "MulticastControl.h"
#include "../Syncer.h"

#include "proto/WireMessage.h"
#include "proto/ProtoMessages.h"

#include <Format.h>
#include <Logging.h>

#include <stdexcept>

#include <bitsery/bitsery.h>
#include <bitsery/adapter/buffer.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Proto::MessageTypes;
using namespace Lichtenstein::Server::Proto::Controllers;

using IMessageHandler = Lichtenstein::Server::Proto::IMessageHandler;

using Buffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<Buffer>;

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
        case McastCtrlMessageType::MCC_GET_INFO: {
            McastCtrlGetInfo req;
            auto state = bitsery::quickDeserialization(InputAdapter{payload.begin(), payload.size()}, req);
            if(state.first != bitsery::ReaderError::NoError || !state.second) {
                throw std::runtime_error("failed to deserialize McastCtrlGetInfo");
            }

            this->handleGetInfo(header, req);
            break;
        }
        // get multicast key
        case McastCtrlMessageType::MCC_GET_KEY: {
            McastCtrlGetKey req;
            auto state = bitsery::quickDeserialization(InputAdapter{payload.begin(), payload.size()}, req);
            if(state.first != bitsery::ReaderError::NoError || !state.second) {
                throw std::runtime_error("failed to deserialize McastCtrlGetKey");
            }

            this->handleGetKey(header, req);
            break;
        }
        // acknowledge re-key
        case McastCtrlMessageType::MCC_REKEY_ACK: {
            McastCtrlRekeyAck ack;
            auto state = bitsery::quickDeserialization(InputAdapter{payload.begin(), payload.size()}, ack);
            if(state.first != bitsery::ReaderError::NoError || !state.second) {
                throw std::runtime_error("failed to deserialize McastCtrlRekeyAck");
            }

            this->handleRekeyAck(header, ack);
            break;
        }

        default:
            throw std::runtime_error("Invalid message type");
    }
}



/**
 * Gets information on the multicast connection.
 */
void MulticastControl::handleGetInfo(const Header &hdr, const GetInfoMsg &msg) {
    McastCtrlGetInfoAck info;
    memset(&info, 0, sizeof(info));

    info.status = MCC_SUCCESS;

    // multicast address/port
    info.address = Syncer::shared()->getGroupAddress();
    info.port = Syncer::shared()->getGroupPort();

    // currently used key
    info.keyId = Syncer::shared()->getCurrentKeyId();

    // send message
    Buffer payload;
    auto writtenSize = bitsery::quickSerialization(OutputAdapter{payload}, info);
    payload.resize(writtenSize);

    this->reply(hdr, McastCtrlMessageType::MCC_GET_INFO_ACK, payload);
}

/**
 * Sends to the node info for the given key.
 */
void MulticastControl::handleGetKey(const Header &hdr, const GetKeyMsg &msg) {
    McastCtrlGetKeyAck info;
    memset(&info, 0, sizeof(info));

    // get the key info
    if(!Syncer::shared()->isKeyIdValid(msg.keyId)) {
        info.status = MCC_INVALID_KEY_ID;
        goto send;
    }

    // copy it into place
    info.keyId = msg.keyId;
    info.keyData.type = MCC_KEY_TYPE_CHACHA20_POLY1305;

    {
        const auto key = Syncer::shared()->getKeyData(msg.keyId);
        const auto iv = Syncer::shared()->getIVData(msg.keyId);

        info.keyData.key.resize(key.size());
        for(size_t i = 0; i < key.size(); i++) {
            info.keyData.key[i] = key[i];
        }

        info.keyData.iv.resize(key.size());
        for(size_t i = 0; i < iv.size(); i++) {
            info.keyData.iv[i] = iv[i];
        }
    }

send:;
    // send reply
    Buffer payload;
    auto writtenSize = bitsery::quickSerialization(OutputAdapter{payload}, info);
    payload.resize(writtenSize);

    this->reply(hdr, McastCtrlMessageType::MCC_GET_KEY_ACK, payload);
}

/**
 * Handles a re-key acknowledgement message./
 */
void MulticastControl::handleRekeyAck(const Header &hdr, const RekeyAckMsg &msg) {
    // TODO: something?
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
    msg.keyData.type = MCC_KEY_TYPE_CHACHA20_POLY1305;

    // copy key material
    const auto keyData = Syncer::shared()->getKeyData(msg.keyId);
    msg.keyData.key.resize(keyData.size());
    for(size_t i = 0; i < keyData.size(); i++) {
        msg.keyData.key[i] = keyData[i];
    }

    const auto ivData = Syncer::shared()->getIVData(msg.keyId);
    msg.keyData.iv.resize(ivData.size());
    for(size_t i = 0; i < ivData.size(); i++) {
        msg.keyData.iv[i] = ivData[i];
    }

    // send it
    Buffer payload;
    auto writtenSize = bitsery::quickSerialization(OutputAdapter{payload}, msg);
    payload.resize(writtenSize);

    this->send(MessageEndpoint::MulticastControl, McastCtrlMessageType::MCC_REKEY,
            this->nextTag++, payload);
}
