/**
 * Implements the channel data endpoint; this allows a node to listen for changes in framebuffer
 * data that it will be outputting.
 *
 * Currently, nodes are limited to one observer per output channel. Repeated subscriptions for the
 * same channel are ignored.
 */
#ifndef PROTO_SERVER_CONTROLLERS_CHANNELDATA_H
#define PROTO_SERVER_CONTROLLERS_CHANNELDATA_H

#include "../IMessageHandler.h"
#include "render/Framebuffer.h"

#include <proto/ProtoMessages.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

#include <uuid.h>

namespace Lichtenstein::Server::Proto::Controllers {
    class ChannelData: public IMessageHandler {
        using SubscribeMsg = Lichtenstein::Proto::MessageTypes::PixelSubscribe;
        using UnsubscribeMsg = Lichtenstein::Proto::MessageTypes::PixelUnsubscribe;
        using AckMsg = Lichtenstein::Proto::MessageTypes::PixelDataMessageAck;
        using Format = Lichtenstein::Proto::MessageTypes::PixelFormat;

        public:
            ChannelData() = delete;
            ChannelData(ServerWorker *client);

            ~ChannelData();

        public:
            virtual bool canHandle(uint8_t);
            virtual void handle(ServerWorker*, const Header &, PayloadType &);

        private:
            void handleSubscribe(const Header &, const SubscribeMsg *);
            void handleUnsubscribe(const Header &, const UnsubscribeMsg *);
            void handleAck(const Header &, const AckMsg *);

            void observerFired(int subscriptionId);

        private:
            // fb start, length, channel offset, pixel format, observer token
            using SubscriptionInfo = std::tuple<size_t, size_t, size_t, Format, Render::Framebuffer::ObserverToken>;

            // channel -> subscription info map
            std::unordered_map<int, SubscriptionInfo> subscriptions;
            // timestamp of the last acknowledgement received from a particular channel
            std::unordered_map<int, std::chrono::steady_clock::time_point> lastAckTime;
            // whether a particular channel is throttled or not
            std::unordered_map<int, bool> throttleMap;

        private:
            static std::unique_ptr<IMessageHandler> construct(ServerWorker *);

        private:
            static bool registered;
    };
}

#endif
