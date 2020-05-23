/**
 * Defines the interface for a client message handler.
 */
#ifndef PROTO_IMESSAGEHANDLER_H
#define PROTO_IMESSAGEHANDLER_H

#include <cstddef>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <map>
#include <mutex>

namespace Lichtenstein::Server::Proto {
    struct MessageHeader;
    class ServerWorker;

    class IMessageHandler {
        public:
            using HandlerCtor = std::unique_ptr<IMessageHandler>(*)(ServerWorker *);
            using MapType = std::map<std::string, HandlerCtor>;

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
                    std::vector<std::byte> &payload) = 0;

        protected:
            /**
             * Sends a response message to the client.
             */
            void send(uint8_t type, const std::vector<std::byte> &data);

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

        private:
            ServerWorker *client = nullptr;
    };
}

#endif
