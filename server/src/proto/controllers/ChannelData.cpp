#include "ChannelData.h"
#include "proto/WireMessage.h"
#include "../Server.h"
#include "../ServerWorker.h"

#include "db/DataStore.h"

#include "render/Pipeline.h"
#include "render/Framebuffer.h"

#include <Format.h>
#include <Logging.h>

#include <stdexcept>

#include <cista.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Proto::MessageTypes;
using namespace Lichtenstein::Server::Proto::Controllers;

using IMessageHandler = Lichtenstein::Server::Proto::IMessageHandler;

/// registers the controller
bool ChannelData::registered = 
    IMessageHandler::registerClass("ChannelData", &ChannelData::construct);

/**
 * Constructor method registered in the message handlers
 */
std::unique_ptr<IMessageHandler> ChannelData::construct(ServerWorker *w) {
    return std::make_unique<ChannelData>(w);
}



/**
 * Constructs the pixel data subscription handler.
 */
ChannelData::ChannelData(ServerWorker *client) : IMessageHandler(client) {

}

/**
 * Remove all observers.
 */
ChannelData::~ChannelData() {

}



/**
 * All messages for the pixel data endpoint are handled by us.
 */
bool ChannelData::canHandle(uint8_t type) {
    return (type == MessageEndpoint::PixelData);
}

/**
 * Handles client subscribe/unsubscribe requests.
 */
void ChannelData::handle(ServerWorker *worker, const struct MessageHeader &header, PayloadType &payload) {
    XASSERT(header.endpoint == MessageEndpoint::PixelData, "invalid message endpoint");
    this->requireAuth();

    // handle the message
    switch(header.messageType) {
        // register observer
        case PixelMessageType::PIX_SUBSCRIBE:
            this->handleSubscribe(header, cista::deserialize<SubscribeMsg, kCistaMode>(payload));
            break;
        // remove observer
        case PixelMessageType::PIX_UNSUBSCRIBE:
            this->handleUnsubscribe(header, cista::deserialize<UnsubscribeMsg, kCistaMode>(payload));
            break;

        default:
            throw std::runtime_error("Invalid message type");
    }
}



/**
 * Handles registering a new subscription for a channel.
 *
 * If a subscription already exists for the channel, the request fails.
 */
void ChannelData::handleSubscribe(const Header &hdr, const SubscribeMsg *msg) {
    using namespace DB;
    using namespace Render;

    DB::Types::NodeChannel channel;

    PixelSubscribeAck ack;
    memset(&ack, 0, sizeof(ack));

    auto fb = Pipeline::pipeline()->fb;

    // get all channels
    const auto channels = DataStore::db()->channelsForNode(this->getNodeId());

    // validate the channel index and length
    if(msg->channel >= channels.size()) {
        ack.status = PixelStatus::PIX_INVALID_CHANNEL;
        goto nack;
    }
    channel = channels[msg->channel];

    if(msg->start >= channel.numPixels) {
        ack.status = PixelStatus::PIX_INVALID_OFFSET;
        goto nack;
    } else if((msg->start + msg->length) >= channel.numPixels) {
        ack.status = PixelStatus::PIX_INVALID_LENGTH;
        goto nack;
    }

    // is there a subscription for this channel already?
    if(this->subscriptions.find(msg->channel) != this->subscriptions.end()) {
        Logging::warn("Attempting duplicate registration for channel {}: range ({}, {})",
                msg->channel, msg->start, msg->length);

        ack.status = PixelStatus::PIX_ALREADY_SUBSCRIBED;
        goto nack;
    }

    // add the observer
    {
        size_t start = msg->start + channel.fbOffset;
        size_t channel = msg->channel;

        auto token = fb->registerObserver(start, msg->length,
                [this, channel](Framebuffer::FrameToken frame) {
            this->observerFired(channel);
        });

        this->subscriptions[msg->channel] = SubscriptionInfo(start, msg->length, msg->format, token);
    }

    // done!
    ack.status = PIX_SUCCESS;
    ack.subscriptionId = 420 + msg->channel; // not currently used

nack:;
    // send the acknowledgement
    const auto ackData = cista::serialize<kCistaMode>(ack);
    this->reply(hdr, PixelMessageType::PIX_SUBSCRIBE_ACK, ackData);
}

/**
 * Removes a single (or all) subscriptions for a channel.
 */
void ChannelData::handleUnsubscribe(const Header &hdr, const UnsubscribeMsg *msg) {
    using namespace DB;
    using namespace Render;

    PixelUnsubscribeAck ack;
    memset(&ack, 0, sizeof(ack));

    auto fb = Pipeline::pipeline()->fb;

    if(this->subscriptions.find(msg->channel) != this->subscriptions.end()) {
        // we're really just interested in the token
        const auto& [fbStart, length, pixelFormat, token] = this->subscriptions[msg->channel];
        fb->removeObserver(token);

        // now, remove it
        ack.subscriptionsRemoved += this->subscriptions.erase(msg->channel);
    }

nack:;
    // send the acknowledgement
    const auto ackData = cista::serialize<kCistaMode>(ack);
    this->reply(hdr, PixelMessageType::PIX_UNSUBSCRIBE_ACK, ackData);

}



/**
 * A framebuffer observer has fired.
 *
 * The required region is copied out of the framebuffer and sent to the client. Pixel format
 * conversion takes place here as well.
 */
void ChannelData::observerFired(int subscriptionId) {
    const auto& [fbStart, length, pixelFormat, token] = this->subscriptions[subscriptionId];

    // copy pixel data and convert

    // send it
}

