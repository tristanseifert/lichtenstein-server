#include "ChannelData.h"
#include "proto/WireMessage.h"
#include "../Server.h"
#include "../ServerWorker.h"

#include "db/DataStore.h"

#include "render/Pipeline.h"
#include "render/Framebuffer.h"
#include "render/RGBPixel.h"
#include "render/RGBWPixel.h"

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
    for(const auto& kv : this->subscriptions) {
        const auto token = std::get<4>(kv.second);
        Render::Pipeline::pipeline()->fb->removeObserver(token);
    }
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
    } else if(msg->format != PIX_FORMAT_RGB && msg->format != PIX_FORMAT_RGBW) {
        ack.status = PixelStatus::PIX_INVALID_FORMAT;
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

        this->subscriptions[msg->channel] = SubscriptionInfo(start, msg->length, msg->start, msg->format, token);
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
        const auto token = std::get<4>(this->subscriptions[msg->channel]);
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
void ChannelData::observerFired(int channel) {
    using namespace Render;
    auto fb = Pipeline::pipeline()->fb;

    PixelDataMessage msg;
    
    const auto& [fbStart, length, channelOffset, pixelFormat, token] = this->subscriptions[channel];

    // copy pixel data and convert
    if(pixelFormat == PIX_FORMAT_RGB) {
        std::vector<RGBPixel> buf;
        buf.resize(length);

        fb->copyOut(fbStart, length, buf.data());

        size_t bytes = length * sizeof(RGBPixel);
        msg.pixels.resize(bytes);

        std::byte *ptr = reinterpret_cast<std::byte *>(buf.data());
        std::copy(ptr, ptr + bytes, msg.pixels.begin());
    } else if(pixelFormat == PIX_FORMAT_RGBW) {
        std::vector<RGBWPixel> buf;
        buf.resize(length);

        fb->copyOut(fbStart, length, buf.data());

        size_t bytes = length * sizeof(RGBWPixel);
        msg.pixels.resize(bytes);

        std::byte *ptr = reinterpret_cast<std::byte *>(buf.data());
        std::copy(ptr, ptr + bytes, msg.pixels.begin());
    }

    // fill in the message
    msg.channel = channel;
    msg.offset = channelOffset;

    // send it
    const auto data = cista::serialize<kCistaMode>(msg);
    this->send(MessageEndpoint::PixelData, PixelMessageType::PIX_DATA, 0, data);
}

