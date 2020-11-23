/**
 * Implements the node authentication endpoint.
 */
#ifndef PROTO_SERVER_CONTROLLERS_AUTHENTICATION_H
#define PROTO_SERVER_CONTROLLERS_AUTHENTICATION_H

#include "../IMessageHandler.h"

#include <proto/ProtoMessages.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <uuid.h>

namespace Lichtenstein::Server::Proto::Auth {
    class IAuthHandler;
}

namespace Lichtenstein::Server::Proto::Controllers {
    class Authentication: public IMessageHandler {
        using AuthResp = Lichtenstein::Proto::MessageTypes::AuthResponse;
        using AuthReq = Lichtenstein::Proto::MessageTypes::AuthRequest;

        public:
            Authentication() = delete;
            Authentication(ServerWorker *client);

            ~Authentication();

        private:
            /// supported auth methods, in descending preference order
            static const std::vector<std::string> kSupportedMethods;

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
            void handle(ServerWorker*, const Header &, PayloadType &);

        private:
            void handleAuthReq(ServerWorker*, const Header &, const AuthReq *);
            bool updateNodeId(ServerWorker*, const uuids::uuid &);

            void handleAuthResp(ServerWorker *, const Header &, const AuthResp *);

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
