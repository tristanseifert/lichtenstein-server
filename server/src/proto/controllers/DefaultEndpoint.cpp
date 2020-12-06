#include "DefaultEndpoint.h"

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
bool DefaultEndpoint::registered = 
    IMessageHandler::registerClass("DefaultEndpoint", &DefaultEndpoint::construct);

/**
 * Constructor method registered in the message handlers
 */
std::unique_ptr<IMessageHandler> DefaultEndpoint::construct(ServerWorker *w) {
    return std::make_unique<DefaultEndpoint>(w);
}


/**
 * All messages for the default endpoint are handled by us.
 */
bool DefaultEndpoint::canHandle(uint8_t type) {
    return (type == MessageEndpoint::Default);
}

/**
 * Handles client subscribe/unsubscribe requests.
 */
void DefaultEndpoint::handle(ServerWorker *worker, const struct MessageHeader &header, PayloadType &payload) {
    XASSERT(header.endpoint == MessageEndpoint::Default, "invalid message endpoint");
    this->requireAuth();

    // handle the message
    switch(header.messageType) {
        // ping request
        case DefaultMessageType::DEF_PING_REQ: {
            PingRequest req;
            auto state = bitsery::quickDeserialization(InputAdapter{payload.begin(), payload.size()}, req);
            if(state.first != bitsery::ReaderError::NoError || !state.second) {
                throw std::runtime_error("failed to deserialize PingRequest");
            }

            this->handlePing(header, req);
            break;
        }

        default:
            throw std::runtime_error("Invalid message type");
    }
}



/**
 * Gets information on the multicast connection.
 */
void DefaultEndpoint::handlePing(const Header &hdr, const PingReq &msg) {
    PingResponse res;
    memset(&res, 0, sizeof(res));

    // status 0 = success
    res.status = 0;

    // copy the timestamp and sequence number
    res.timestamp = msg.timestamp;
    res.sequence = msg.sequence;

    // send message
    Buffer payload;
    auto writtenSize = bitsery::quickSerialization(OutputAdapter{payload}, res);
    payload.resize(writtenSize);

    this->reply(hdr, DEF_PING_RESP, payload);
}

