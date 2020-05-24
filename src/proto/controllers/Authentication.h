/**
 * Implements the node authentication endpoint.
 */
#ifndef PROTO_SERVER_CONTROLLERS_AUTHENTICATION_H
#define PROTO_SERVER_CONTROLLERS_AUTHENTICATION_H

#include "../IMessageHandler.h"

#include <cstddef>
#include <memory>

namespace Lichtenstein::Server::Proto::Auth {
    class IAuthHandler;
}

namespace Lichtenstein::Server::Proto::Controllers {
    class Authentication: public IMessageHandler {
        public:
            Authentication() = delete;
            Authentication(ServerWorker *client);

            ~Authentication();

        private:
            /**
             * Authentication is implemented as a simple state machine that
             * runs through the states below.
             */
            enum State {
                // freshly constructed, waiting for node to request auth
                Idle,
                // process an auth response from the client
                HandleResponse,
                // authentication was success
                Authenticated,
                // something went wrong authenticating
                Failed,
            };

        public:
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
            void handleAuthReq(const WireTypes::AuthRequest::Reader &);
            void handleAuthRes(const WireTypes::AuthResponse::Reader &);

        private:
            // current state machine state
            State state;

            /// authentication handler
            std::unique_ptr<Auth::IAuthHandler> handler = nullptr;

        private:
            static std::unique_ptr<IMessageHandler> construct(ServerWorker *);

        private:
            static bool registered;
    };
}

#endif
