#include "Syncer.h"

#include <ConfigManager.h>
#include <Format.h>
#include <Logging.h>

#include <proto/WireMessage.h>
#include <proto/ProtoMessages.h>
#include <proto/MulticastCrypto.h>

#include <stdexcept>
#include <cstring>
#include <limits>
#include <algorithm>

#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>

#include <bitsery/bitsery.h>
#include <bitsery/adapter/buffer.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Server::Proto;

using Buffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<Buffer>;

std::shared_ptr<Syncer> Syncer::sharedInstance = nullptr;

/**
 * Starts the server by allocating the shared instance.
 */
void Syncer::start() {
    XASSERT(!sharedInstance, "Syncer already running? ({})", 
            (void*) sharedInstance.get());

    // now, allocate the server
    sharedInstance = std::make_shared<Syncer>();
}

/**
 * Terminates the server.
 */
void Syncer::stop() {
    XASSERT(sharedInstance, "Expected syncer to be running");

    sharedInstance->terminate();
    sharedInstance = nullptr;
}



/**
 * Initializes the protocol server. This will set up the DTLS context and
 * listening thread.
 */
Syncer::Syncer() {
    this->cryptor = std::make_shared<MulticastCrypto>();

    // seed the observer token random generator. the time is ok since these are just tokens
    this->observerTokenRandom.seed(time(nullptr) + 'SYNC');

    // read configuration
    const auto interval = ConfigManager::getUnsigned("server.sync.rekey_interval", 1800);
    if(!interval) {
        throw std::runtime_error("Invalid rekey interval");
    }
    this->rekeyInterval = std::chrono::seconds(interval);

    const auto port = ConfigManager::getUnsigned("server.sync.port", 34567);
    if(port == 0 || port > 65535) {
        auto what = f("Invalid multicast group port: {}", port);
        throw std::runtime_error(what);
    }
    this->groupPort = static_cast<uint16_t>(port);

    this->readGroupAddress();

    // join the multicast group and init the worker process
    this->initSocket();

    Logging::info("Multicast group: {} port {}", this->getGroupAddress(), this->groupPort);
    this->joinGroup();

    this->initWorker();
}

/**
 * Cleans up any resources allocated by the server, including its listening
 * thread and all client connections.
 */
Syncer::~Syncer() {
    // ensure the worker gets the terminate request
    if(!this->shouldTerminate) {
        Logging::error("You should call Proto::Syncer::terminate() before dealloc");
        this->terminate();
    }

    // wait on the worker thread to exit
    this->worker->join();

    // release crypto
    this->cryptor = nullptr;
}

/**
 * Terminates the sync handler
 */
void Syncer::terminate() {
    if(this->shouldTerminate) {
        Logging::error("Ignoring repeated call to Proto::Syncer::terminate()");
        return;
    }

    // set the termination flag
    Logging::debug("Requesting sync handler termination");
    this->shouldTerminate = true;

    // signal worker
    this->workQueueCv.notify_one();
}



/**
 * Sets up the multicast send/receive socket.
 *
 * We currently only send multicast packets, so nothing really special happens here.
 */
void Syncer::initSocket() {
    int err;

    // create the socket
    err = ::socket(AF_INET, SOCK_DGRAM, 0);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "failed to create socket");
    }
    this->socket = err;
}
/**
 * Join our multicast group.
 *
 * We don't care about any received packets, and to send multicast, we technically don't have to
 * be a member of the group. However, doing this will cause the kernel to send the IGMP messages
 * that some really stupid network equipment needs to pass multicast.
 *
 * Plus, at least we'll be prepared when/if we ever need to receive multicast.
 */
void Syncer::joinGroup() {
    int err;

    // build join request
    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));

    auto *addr4 = reinterpret_cast<const struct sockaddr_in *>(&this->groupAddr);
    mreq.imr_multiaddr.s_addr = addr4->sin_addr.s_addr;

    // join on any interface (TODO: this should be configurable)
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    err = setsockopt(this->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    if(err != 0) {
        throw std::system_error(errno, std::generic_category(), "failed to join multicast group");
    }
}
/**
 * Leaves the multicast group.
 */
void Syncer::leaveGroup() {
    int err;
}

/**
 * Sets up the server's worker thread.
 */
void Syncer::initWorker() {
    this->shouldTerminate = false;
    this->worker = std::make_unique<std::thread>(&Syncer::workerMain, this);
}
/**
 * Sync server main thread
 *
 * This handles sending the multicast messages and re-keying.
 */
void Syncer::workerMain() {
    // generate initial key
    this->generateKey();

    // main loop
    while(!this->shouldTerminate) {
        // calculate time until next rekey, and do so immediately if past due
        const auto now = std::chrono::steady_clock::now();
        const auto rekeyAt = this->currentKeyBirthday + this->rekeyInterval;

        const auto diff = rekeyAt - now;

        if(diff.count() < 0) {
            Logging::warn("Rekeying past due, generating new multicast keys");
            this->generateKey();
            continue;
        }

        // wait for work messages until timeout
        std::unique_lock<std::mutex> lock(this->workQueueLock);
        auto signalled = this->workQueueCv.wait_until(lock, rekeyAt);

        // if time out, rekey
        if(signalled == std::cv_status::timeout) {
            Logging::trace("Rekeying timer expired, generating new multicast keys");
            this->generateKey();
        }
        // otherwise, work through all items in the work queue
        else if(signalled == std::cv_status::no_timeout) {
            while(!this->workQueue.empty()) {
                // get the head of the queue and pop it off
                const auto workItem = this->workQueue.front();
                this->workQueue.pop();

                // process it
                switch(workItem.type) {
                    case WorkItem::SYNC_OUTPUT:
                        this->handleSyncOutput(workItem);
                        break;

                    default:
                        Logging::error("Unhandled work item type {}", workItem.type);
                        break;
                }
            }
        }
    }

    Logging::trace("Syncer work thread is exiting");
    Logging::debug("Issued {} total key(s)", this->prevKeyIds.size() + 1);

    // leave multicast groups
    this->leaveGroup();

    // close the socket
    close(this->socket);
    this->socket = -1;
}



/**
 * Converts the group address to a string.
 */
const std::string Syncer::getGroupAddress() const {
    // message buffer (large enough to fit an IPv6)
    char buf[INET6_ADDRSTRLEN];
    memset(&buf, 0, INET6_ADDRSTRLEN);

    // format IPv4
    if(this->groupAddr.ss_family == AF_INET) {
        auto *addr4 = reinterpret_cast<const struct sockaddr_in *>(&this->groupAddr);

        inet_ntop(AF_INET, &addr4->sin_addr, buf, sizeof(buf));
        return std::string(buf);
    } 
    // format IPv6
    else if(this->groupAddr.ss_family == AF_INET6) {
        auto *addr6 = reinterpret_cast<const struct sockaddr_in6 *>(&this->groupAddr);

        inet_ntop(AF_INET6, &addr6->sin6_addr, buf, sizeof(buf));
        return std::string(buf);
    }

    // unknown address family
    throw std::runtime_error("unsupported address family");
}

/**
 * Reads and decodes the group address.
 */
void Syncer::readGroupAddress() {
    int err;

    // clear the address struct
    memset(&this->groupAddr, 0, sizeof(struct sockaddr_storage));

    auto *addr4 = reinterpret_cast<struct sockaddr_in *>(&this->groupAddr);
    auto *addr6 = reinterpret_cast<struct sockaddr_in6 *>(&this->groupAddr);

    // get from config
    const auto address = ConfigManager::get("server.sync.group", "239.42.0.69");

    if(address.empty()) {
        throw std::runtime_error("Sync group address missing");
    }

    // can we parse it as an IPv4 address?
    err = inet_pton(AF_INET, address.c_str(), &addr4->sin_addr);

    if(err == 1) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(this->groupPort);

        return;
    } else if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "inet_pton(AF_INET) failed");
    }

    auto what = f("Failed to parse group address '{}'", address);
    throw std::runtime_error(what);
}



/**
 * Generates a new key.
 *
 * Key data is randomly generated, and then any registered rekey observers are invoked.
 */
void Syncer::generateKey() {
    int err;
    uint32_t keyId = 0;

    // distribution used for generating key IDs 
    std::uniform_int_distribution<ObserverToken> dist(0, std::numeric_limits<uint32_t>::max());

    // generate the key material
    KeyDataType key;
    IVDataType iv;

    err = RAND_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
    if(err != 1) {
        throw std::runtime_error("Failed to generate key data");
    }

    err = RAND_bytes(reinterpret_cast<unsigned char *>(iv.data()), iv.size());
    if(err != 1) {
        throw std::runtime_error("Failed to generate IV data");
    }

    // generate a key id
    while(!keyId) {
        uint32_t temp = dist(this->keyIdRandom);

        if(std::find(this->prevKeyIds.begin(), this->prevKeyIds.end(), temp) == this->prevKeyIds.end()) {
            keyId = temp;
        }
    }

    // put it in the key store and update keying state
    {
        std::lock_guard<std::mutex> lg(this->keyLock);

        const auto birthday = std::chrono::steady_clock::now();
        this->currentKeyBirthday = birthday;

        this->prevKeyIds.push_back(keyId);
        this->keyStore[keyId] = KeyInfo(key, iv, birthday);
        this->currentKeyId = keyId;

        // load the key into the cryptor
        this->cryptor->loadKey(key);
    }

    // invoke observers
    this->invokeObservers();
}

/**
 * Invokes all rekey observers.
 *
 * Note that any calls into the add/remove observer functions are prohibited from inside an
 * observer callback.
 */
void Syncer::invokeObservers() {
    std::lock_guard<std::mutex> lg(this->observerLock);

    for(const auto &item : this->observers) {
        const auto &[token, callback] = item;
        callback(this->currentKeyId);
    }
}
/**
 * Registers a new observer that's invoked when the currently active key changes.
 *
 * @note You cannot call this method from a callback.
 */
Syncer::ObserverToken Syncer::registerObserver(ObserverFunction const& f) {
    std::lock_guard<std::mutex> lg(this->observerLock);
    ObserverToken token;

    std::uniform_int_distribution<ObserverToken> dist(0, std::numeric_limits<ObserverToken>::max());

generate:;
    // generate a token and ensure it's unique
    token = dist(this->observerTokenRandom);

    if(this->observers.find(token) != this->observers.end()) {
        goto generate;
    }

    // once we've got an unique token, insert it to the list of observers
    this->observers[token] = f;

    if(kLogObservers) {
        Logging::trace("Registered multicast rekey callback: {}", token);
    }

    return token;
}
/**
 * Removes an existing observer.
 *
 * @note You cannot call this method from a callback.
 *
 * @throws If there is no such observer, an exception is thrown.
 */
void Syncer::removeObserver(ObserverToken token) {
    std::lock_guard<std::mutex> lg(this->observerLock);

    if(this->observers.erase(token) == 0) {
        throw std::invalid_argument("No observer registered with that token");
    }

    if(kLogObservers) {
        Logging::trace("Removed multicast rekey callback {}", token);
    }
}



/**
 * Signals the worker thread that new work items are available.
 */
void Syncer::pushWorkItem(const WorkItem &item) {
    std::unique_lock<std::mutex> lock(this->workQueueLock);
    this->workQueue.push(item);

    this->workQueueCv.notify_one();
}

/**
 * Sends a sync output message.
 */
void Syncer::handleSyncOutput(const WorkItem &) {
    using namespace Lichtenstein::Proto::MessageTypes;

    std::lock_guard<std::mutex> lg(this->keyLock);

    std::vector<std::byte> payload;
    std::vector<std::byte> plaintext;

    // build the packet header
    MulticastMessageHeader hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.version = kLichtensteinProtoVersion;
    hdr.tag = this->nextTag++;
    hdr.keyId = htonl(this->currentKeyId);
    hdr.endpoint = MessageEndpoint::MulticastData;
    hdr.messageType = McastDataMessageType::MCD_SYNC_OUTPUT;

    // build the packet and encode it
    McastDataSyncOutput msg;
    memset(&msg, 0, sizeof(msg));

    Buffer msgData;
    auto writtenSize = bitsery::quickSerialization(OutputAdapter{msgData}, msg);
    msgData.resize(writtenSize);

    // encrypt the payload
    const auto iv = std::get<1>(this->keyStore[this->currentKeyId]);

    plaintext.resize(msgData.size());
    std::transform(msgData.begin(), msgData.end(), plaintext.begin(), [](auto value) {
        return std::byte(value);
    });

    this->cryptor->encrypt(plaintext, iv, payload);
    XASSERT(payload.size() <= std::numeric_limits<uint16_t>::max(), "Payload too big");

    hdr.length = htons(payload.size());

    // send it
    this->send(hdr, payload);
}



/**
 * Sends the given message as a multicast frame.
 */
void Syncer::send(const MulticastMessageHeader &hdr, const std::vector<std::byte> &payload) {
    int err;

    std::vector<std::byte> buffer;
    buffer.reserve(sizeof(MulticastMessageHeader) + payload.size());

    // combine into one buffer
    buffer.resize(sizeof(MulticastMessageHeader));

    auto hdrBytes = reinterpret_cast<const std::byte *>(&hdr);
    std::copy(hdrBytes, hdrBytes+sizeof(MulticastMessageHeader), buffer.begin());

    buffer.insert(buffer.end(), payload.begin(), payload.end());

    // send the UDP frame
    size_t socklen = 0;
    if(this->groupAddr.ss_family == AF_INET) {
        socklen = sizeof(sockaddr_in);
    } else if(this->groupAddr.ss_family == AF_INET6) {
        socklen = sizeof(sockaddr_in6);
    }
    XASSERT(socklen != 0, "Invalid multicast group family {}", this->groupAddr.ss_family);

    err = sendto(this->socket, buffer.data(), buffer.size(), 0,
                 reinterpret_cast<sockaddr *>(&this->groupAddr), socklen);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "failed to send mcast packet");
    }
}


/**
 * Queue a sync output packet to be sent.
 */
void Syncer::frameCompleted() {
    WorkItem w;
    memset(&w, 0, sizeof(w));

    w.type = WorkItem::SYNC_OUTPUT;

    this->pushWorkItem(w);
}
