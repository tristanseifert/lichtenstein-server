/**
 * Defines the interface for a client message handler.
 */
#ifndef PROTO_IMESSAGEHANDLER_H
#define PROTO_IMESSAGEHANDLER_H

#include "WireMessage.h"

#include <cstddef>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <map>
#include <mutex>

#include <proto/lichtenstein_v1.capnp.h>

namespace Lichtenstein::Server::Proto {
    class ServerWorker;

    class IMessageHandler {
        public:
            using HandlerCtor = std::unique_ptr<IMessageHandler>(*)(ServerWorker *);
            using MapType = std::map<std::string, HandlerCtor>;
            using PayloadType = std::vector<std::byte>;

        public:
            IMessageHandler() = delete;
            IMessageHandler(ServerWorker *client);

            virtual ~IMessageHandler() {}

        public:
            /**
             * Can we handle a message of this specified type?
             */
            virtual bool canHandle(uint8_t type) = 0;

            /**
             * Handles a client message. This is called immediately after the
             * client handling loop calls canHandle() on this object.
             */
            virtual void handle(struct MessageHeader &hdr,
                    PayloadType &payload) = 0;

        protected:
            /**
             * Attempts to decode an incoming byte buffer into a message shell
             * structure.
             */
            WireTypes::Message::Reader decode(const PayloadType &payload);

            /**
             * Replies to an incoming message. This copies the type and tag
             * values from the provided header, but sends the bytes unmodified.
             */
            void reply(const struct MessageHeader &hdr,
                    const PayloadType &data) {
                this->send(hdr.type, hdr.tag, data);
            }

            /**
             * Sends a response message to the client.
             */
            void send(MessageEndpoint type, uint8_t tag, const PayloadType &data);


        private:
            ServerWorker *client = nullptr;



        public:
            /**
             * Registers a handler class.
             */
            static bool registerClass(const std::string &tag, HandlerCtor ctor);

            /**
             * Iterates over all handlers returning the tag and constructor
             * function reference.
             */
            static void forEach(std::function<void(const std::string&, HandlerCtor)> f);

            /**
             * Prints the contents of the handler registry.
             */
            static void dumpRegistry();

        private:
            static MapType *registrations;
            static std::mutex registerLock;
    };
}

#endif
