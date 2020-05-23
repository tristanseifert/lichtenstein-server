/**
 * Implements the node authentication endpoint.
 */
#ifndef PROTO_SERVER_CONTROLLERS_AUTHENTICATION_H
#define PROTO_SERVER_CONTROLLERS_AUTHENTICATION_H

#include "../IMessageHandler.h"

namespace Lichtenstein::Server::Proto::Controllers {
    class Authentication: public IMessageHandler {
        public:
            Authentication() = delete;
            Authentication(ServerWorker *client);

            ~Authentication();

        private:
            /**
             * We can handle all messages send to the auth endpoint. If the
             * message is unexpected or corrupt, we'll raise an error during
             * handling.
             */
            bool canHandle(uint8_t);

            /**
             * Passes the given message into the authentication state machine;
             * depending on its current state, this may result in an error.
             */
            void handle(struct MessageHeader &, std::vector<std::byte> &);

        private:
            static std::unique_ptr<IMessageHandler> construct(ServerWorker *);

        private:
            static bool registered;
    };
}

#endif
