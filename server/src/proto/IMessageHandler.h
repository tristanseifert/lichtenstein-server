/**
 * Defines the interface for a client message handler.
 */
#ifndef PROTO_IMESSAGEHANDLER_H
#define PROTO_IMESSAGEHANDLER_H

#include <proto/WireMessage.h>

#include <cstddef>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <map>
#include <mutex>

#include "ServerWorker.h"


namespace Lichtenstein::Server::Proto {
    class IMessageHandler {
        public:
            using HandlerCtor = std::unique_ptr<IMessageHandler>(*)(ServerWorker *);
            using MapType = std::map<std::string, HandlerCtor>;
            using PayloadType = std::vector<unsigned char>;
            using Header = struct Lichtenstein::Proto::MessageHeader;
            using MessageEndpoint = Lichtenstein::Proto::MessageEndpoint;

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
            virtual void handle(ServerWorker *, const Header &hdr, PayloadType &payload) = 0;

        protected:
            /**
             * Replies to an incoming message. This copies the type and tag
             * values from the provided header, but sends the bytes unmodified.
             */
            void reply(const Header &hdr, const uint8_t type, const PayloadType &data) {
                this->send(hdr.endpoint, type, hdr.tag, data);
            }

            /**
             * Sends a response message to the client.
             */
            void send(const MessageEndpoint endpoint, const uint8_t type, const uint8_t tag, const PayloadType &data);

            /**
             * Asserts that the client is authenticated.
             */
            void requireAuth();

            /**
             * Is the client authenticated?
             */
            bool isClientAuthenticated() const {
                return this->client->isAuthenticated();
            }
            /**
             * If the client is authenticated, get the node id.
             *
             * @note This value is undefined if the client isn't authenticated.
             */
            int getNodeId() const {
                return this->client->getNodeId();
            }

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
