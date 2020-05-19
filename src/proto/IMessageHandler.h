/**
 * Defines the interface for a client message handler.
 */
#ifndef PROTO_IMESSAGEHANDLER_H
#define PROTO_IMESSAGEHANDLER_H

#include <cstddef>
#include <vector>

namespace Lichtenstein::Server::Proto {
    struct MessageHeader;
    class ServerWorker;

    class IMessageHandler {
        public:
            IMessageHandler() = delete;
            IMessageHandler(ServerWorker *client);

            virtual ~IMessageHandler() {}

        public:
            /**
             * Can we handle a message of this specified type?
             */
            virtual bool canHandle(uint16_t type) = 0;

            /**
             * Handles a client message. This is called immediately after the
             * client handling loop calls canHandle() on this object.
             */
            virtual void handle(struct MessageHeader &hdr, 
                    std::vector<std::byte> &payload) = 0;

        protected:
            /**
             * Sends a response message to the client.
             */
            void send(uint8_t type, const std::vector<std::byte> &data);

        private:
            ServerWorker *client = nullptr;
    };
}

#endif
