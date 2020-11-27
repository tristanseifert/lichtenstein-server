/**
 * Implements the multicast multi-node synchronization facilities.
 *
 * All messages sent to the multicast group are encrypted, using a shared group key. This key is
 * distributed to all nodes over their encrypted DTLS control channels. The key is rotated
 * periodically; clients should no longer accept old keys after a re-key.
 *
 * All keys are 256 bits with an 128 bit IV; this is intended to be used with ChaCha20-Poly1305.
 */
#ifndef PROTO_SERVER_SYNCER_H
#define PROTO_SERVER_SYNCER_H

#include <cstddef>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <random>
#include <chrono>
#include <vector>
#include <array>
#include <tuple>
#include <functional>
#include <unordered_map>
#include <condition_variable>
#include <queue>

#include <sys/socket.h>

namespace Lichtenstein::Proto {
    class MulticastCrypto;
    struct MulticastMessageHeader;
}

namespace Lichtenstein::Server::Proto::Controllers {
    class MulticastControl;
}

namespace Lichtenstein::Server::Proto {
    class Syncer {
        friend class Controllers::MulticastControl;

        public:
            using ObserverToken = unsigned long long;
            // current key id as arg
            using ObserverFunction = std::function<void(uint32_t)>;

        public:
            static void start();
            static void stop();

            // ya shouldn't be calling these ya hear!!!
            Syncer();
            ~Syncer();

        public:
            /**
             * Returns the global protocol server instance.
             */
            static std::shared_ptr<Syncer> shared() {
                return sharedInstance;
            }

            /// get multicast group address (as a string)
            const std::string getGroupAddress() const;
            /// get multicast group port
            const uint16_t getGroupPort() const {
                return this->groupPort;
            }
            /// get the currently active key id
            const uint32_t getCurrentKeyId() const {
                return this->currentKeyId;
            }

            // forces a rekeying to take place, regardless of the internal timer
            void forceRekey() {
                this->generateKey();
            }

            // frame rendering is complete
            void frameCompleted();

            ObserverToken registerObserver(ObserverFunction const& f);
            void removeObserver(ObserverToken token);

        private:
            using KeyDataType = std::array<std::byte, (256 / 8)>;
            using IVDataType = std::array<std::byte, (128 / 8)>;

            using Header = Lichtenstein::Proto::MulticastMessageHeader;

            // is the given key ID valid? e.g. do we have data for it
            const bool isKeyIdValid(const uint32_t id) const {
                return this->keyStore.contains(id);
            }

            // Get key data for the given key id.
            KeyDataType getKeyData(uint32_t keyId) {
                return std::get<0>(this->keyStore[keyId]);
            }
            // Get IV data for the given key id.
            IVDataType getIVData(uint32_t keyId) {
                return std::get<1>(this->keyStore[keyId]);
            }

        private:
            // work items sent to the work thread
            struct WorkItem {
                enum WorkItemType {
                    // send a "sync output" message
                    SYNC_OUTPUT,
                } type;

                // type-specific data
                union {
                    struct {
                        // nothing
                    } syncOut;
                };
            };

        private:
            void terminate();

            void readGroupAddress();

            void initSocket();
            void joinGroup();
            void leaveGroup();

            void initWorker();
            void workerMain();

            void generateKey();
            void invokeObservers();

            void pushWorkItem(const WorkItem &);
            void handleSyncOutput(const WorkItem &);

            void send(const Header &, const std::vector<std::byte> &payload);

        private:
            static std::shared_ptr<Syncer> sharedInstance;

        private:
            using TimePoint = std::chrono::steady_clock::time_point;

            // Multicast group address and port
            struct sockaddr_storage groupAddr;
            uint16_t groupPort;

            // rekey interval
            std::chrono::seconds rekeyInterval;

            // socket used for sending messages
            int socket = -1;

            // work thread
            std::atomic_bool shouldTerminate;
            std::unique_ptr<std::thread> worker = nullptr;

            // work queue; signal the condition variable
            std::mutex workQueueLock;
            std::condition_variable workQueueCv;

            std::queue<WorkItem> workQueue;

            // random engine for generating observer tokens
            std::default_random_engine observerTokenRandom;
            // observer functions to invoke whenever key material changes
            std::mutex observerLock;
            std::unordered_map<ObserverToken, ObserverFunction> observers;

            // lock protecting key management
            std::mutex keyLock;

            // random engine for generating key ID's
            std::mt19937 keyIdRandom;
            // key IDs we've previously issued
            std::vector<uint32_t> prevKeyIds;

            // current key ID
            std::atomic_uint32_t currentKeyId;
            // time at which the current key was generated
            TimePoint currentKeyBirthday;

            // key store type: key, IV, when generated
            using KeyInfo = std::tuple<KeyDataType, IVDataType, TimePoint>;
            // key store; maps key id -> key info
            std::unordered_map<uint32_t, KeyInfo> keyStore;

            // instance to handle the packet crypto
            std::shared_ptr<Lichtenstein::Proto::MulticastCrypto> cryptor = nullptr;

            // tag of the next multicast message
            uint8_t nextTag = 0;
    };
}

#endif
