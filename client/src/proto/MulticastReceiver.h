/**
 * Handles receiving multicast data from the server.
 *
 * This requests on startup the multicast group info from the server. This includes keying info, so
 * we can decrypt the multicast packets.
 */
#ifndef PROTO_MULTICASTRECEIVER_H
#define PROTO_MULTICASTRECEIVER_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>

#include <sys/socket.h>

namespace Lichtenstein::Proto {
    struct MessageHeader;
    struct MulticastMessageHeader;
    class MulticastCrypto;

    namespace MessageTypes {
        struct McastCtrlKeyWrapper;
        struct McastCtrlRekey;
        struct McastCtrlGetKeyAck;
        struct McastDataSyncOutput;
    }
}

namespace Lichtenstein::Client::Proto {
    class Client;

    class MulticastReceiver {
        using Header = struct Lichtenstein::Proto::MessageHeader;
        using McastHeader = struct Lichtenstein::Proto::MulticastMessageHeader;
        using PayloadType = std::vector<unsigned char>;

        using RekeyMsg = Lichtenstein::Proto::MessageTypes::McastCtrlRekey;
        using GetKeyAckMsg = Lichtenstein::Proto::MessageTypes::McastCtrlGetKeyAck;
        using KeyWrap = Lichtenstein::Proto::MessageTypes::McastCtrlKeyWrapper;

        using SyncOutMsg = Lichtenstein::Proto::MessageTypes::McastDataSyncOutput;

        using KeyDataType = std::array<std::byte, (256 / 8)>;
        using IVDataType = std::array<std::byte, (128 / 8)>;

        public:
            MulticastReceiver(Client *);
            ~MulticastReceiver();

        public:
            void stop() {
                this->terminate();
            }

            void setGroupInfo(const std::string &address, const uint16_t port, const uint32_t initialKeyId);

            void handleMessage(const Header &, const PayloadType &);

        private:
            void terminate();

            void initSocket();
            void joinGroup();
            void leaveGroup();

            void initWorker();
            void workerMain();

        private:
            void handleSyncOutput(const McastHeader *, const SyncOutMsg &);

            void handleGetKey(const Header &, const GetKeyAckMsg &);
            void handleRekey(const Header &, const RekeyMsg &);

            void loadKey(const uint32_t, const KeyWrap &, bool = false);
            void sendMcastKeyReq(const uint32_t, uint8_t &);

        private:
            // multicast receive socket
            int socket = -1;

            // work thread
            std::atomic_bool shouldTerminate;
            std::unique_ptr<std::thread> worker = nullptr;


            // Multicast group address and port
            struct sockaddr_storage groupAddr;
            uint16_t groupPort;

            // current key ID
            std::atomic_uint32_t currentKeyId;

            // lock for the keystore
            std::mutex keystoreLock;
            // key store type: key, IV, when generated
            using KeyInfo = std::tuple<KeyDataType, IVDataType>;
            // key store; maps key id -> key info
            std::unordered_map<uint32_t, KeyInfo> keyStore;

            // instance to handle the packet crypto
            std::shared_ptr<Lichtenstein::Proto::MulticastCrypto> cryptor = nullptr;

            // client pointer (used to send key requests)
            Client *client = nullptr;
    };
};

#endif
