/**
 * Provides auxiliary support for the multicast data channel used for multi-node synchronization.
 *
 * This mostly involves allowing nodes to discover the group address/port, as well as the keying
 * material used to encrypt the messages.
 */
#ifndef PROTO_SERVER_CONTROLLERS_MULTICASTCONTROL_H
#define PROTO_SERVER_CONTROLLERS_MULTICASTCONTROL_H

#include "../IMessageHandler.h"
#include "../Syncer.h"

#include <proto/ProtoMessages.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

#include <uuid.h>

namespace Lichtenstein::Server::Proto::Controllers {
    class MulticastControl: public IMessageHandler {
        using GetInfoMsg = Lichtenstein::Proto::MessageTypes::McastCtrlGetInfo;
        using RekeyAckMsg = Lichtenstein::Proto::MessageTypes::McastCtrlRekeyAck;
        using GetKeyMsg = Lichtenstein::Proto::MessageTypes::McastCtrlGetKey;

        public:
            MulticastControl() = delete;
            MulticastControl(ServerWorker *client);

            ~MulticastControl();

        public:
            virtual bool canHandle(uint8_t);
            virtual void handle(ServerWorker*, const Header &, PayloadType &);

        private:
            void handleGetInfo(const Header &, const GetInfoMsg *);

            void rekeyCallback(uint32_t newKeyId);
            void sendRekey();

            void observerFired(int subscriptionId);

        private:
            Syncer::ObserverToken observer;

            uint8_t nextTag = 0;

        private:
            static std::unique_ptr<IMessageHandler> construct(ServerWorker *);

        private:
            static bool registered;
    };
}

#endif
