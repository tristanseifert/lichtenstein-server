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

#include <uuid.h>

namespace Lichtenstein::Server::Proto::Controllers {
    class ChannelData: public IMessageHandler {
        using SubscribeMsg = Lichtenstein::Proto::MessageTypes::PixelSubscribe;
        using UnsubscribeMsg = Lichtenstein::Proto::MessageTypes::PixelUnsubscribe;
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

            void observerFired(int subscriptionId);

        private:
            // fb start, length, channel offset, pixel format, observer token
            using SubscriptionInfo = std::tuple<size_t, size_t, size_t, Format, Render::Framebuffer::ObserverToken>;

            // channel -> subscription info map
            std::unordered_map<int, SubscriptionInfo> subscriptions;

        private:
            static std::unique_ptr<IMessageHandler> construct(ServerWorker *);

        private:
            static bool registered;
    };
}

#endif
