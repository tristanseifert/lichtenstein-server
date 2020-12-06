/**
 * Default endpoint; only provides the ping function for now.
 */
#ifndef PROTO_SERVER_CONTROLLERS_DEFAULTENDPOINT_H
#define PROTO_SERVER_CONTROLLERS_DEFAULTENDPOINT_H

#include "../IMessageHandler.h"

#include <proto/ProtoMessages.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

#include <uuid.h>

namespace Lichtenstein::Server::Proto::Controllers {
    class DefaultEndpoint: public IMessageHandler {
        using PingReq = Lichtenstein::Proto::MessageTypes::PingRequest;

        public:
            DefaultEndpoint() = delete;
            DefaultEndpoint(ServerWorker *client) : IMessageHandler(client) {};

            ~DefaultEndpoint() {};

        public:
            virtual bool canHandle(uint8_t);
            virtual void handle(ServerWorker*, const Header &, PayloadType &);

        private:
            void handlePing(const Header &, const PingReq &);

        private:
            static std::unique_ptr<IMessageHandler> construct(ServerWorker *);

        private:
            static bool registered;
    };
}

#endif
