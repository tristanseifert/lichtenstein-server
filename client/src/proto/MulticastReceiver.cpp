#include "MulticastReceiver.h"
#include "Client.h"

#include <proto/WireMessage.h>
#include <proto/ProtoMessages.h>
#include <proto/MulticastCrypto.h>

#include <ConfigManager.h>
#include <Logging.h>
#include <Format.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdexcept>

#include <cista.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Client::Proto;

/**
 * Sets up the multicast receiver.
 */
MulticastReceiver::MulticastReceiver(Client *_client) : client(_client) {

}

/**
 * Tears down the multicast receiver, including the work thread and socket.
 */
MulticastReceiver::~MulticastReceiver() {
    // ensure the worker gets the terminate request
    if(!this->shouldTerminate) {
        Logging::error("You should call MulticastReceiver::terminate() before dealloc");
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
void MulticastReceiver::terminate() {
    if(this->shouldTerminate) {
        Logging::error("Ignoring repeated call to Proto::MulticastReceiver::terminate()");
        return;
    }

    // set the termination flag
    Logging::debug("Requesting multicast handler termination");
    this->shouldTerminate = true;
}



/**
 * Sets up the multicast send/receive socket.
 *
 * We currently only send multicast packets, so nothing really special happens here.
 */
void MulticastReceiver::initSocket() {
    int err;

    // create the socket
    err = ::socket(AF_INET, SOCK_DGRAM, 0);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "failed to create mcast socket");
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
void MulticastReceiver::joinGroup() {
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
void MulticastReceiver::leaveGroup() {
    int err;

    // build join request
    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));

    auto *addr4 = reinterpret_cast<const struct sockaddr_in *>(&this->groupAddr);
    mreq.imr_multiaddr.s_addr = addr4->sin_addr.s_addr;

    // remove from all interface (TODO: this should be configurable)
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    err = setsockopt(this->socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    if(err != 0) {
        throw std::system_error(errno, std::generic_category(), "failed to leave multicast group");
    }
}

/**
 * Sets up the server's worker thread.
 */
void MulticastReceiver::initWorker() {
    XASSERT(!this->worker, "Worker must only be initialized once");

    this->shouldTerminate = false;
    this->worker = std::make_unique<std::thread>(&MulticastReceiver::workerMain, this);
}



/**
 * Wait to receive multicast messages.
 */
void MulticastReceiver::workerMain() {
    int err;

    // join multicast group
    this->joinGroup();

    // wait on the multicast socket
    while(!this->shouldTerminate) {
        // timeout (250 msec)
        struct timeval timeout;
        memset(&timeout, 0, sizeof(timeout));

        timeout.tv_sec = 0;
        timeout.tv_usec = (1000 * 250);

        // configure fdset
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(this->socket, &read_fds);

        // wait for packet
        err = select(this->socket, &read_fds, nullptr, nullptr, &timeout);

        if(err == -1) {
            throw std::system_error(errno, std::generic_category(), "select() failed");
        } else if(err == 0) {
            // wait timed out
            continue;
        }

        // try to read a packet
    }

    Logging::debug("Multicast receiver thread is shutting down");

    // leave multicast group
    this->leaveGroup();

    // clean up resources
    ::close(this->socket);
}



/**
 * Sets the multicast group information and initial key; also kicks off a request for that initial
 * key data.
 *
 * The worker thread is started automatically.
 */
void MulticastReceiver::setGroupInfo(const std::string &address, const uint16_t port, const uint32_t initialKeyId) {
    int err;
    uint8_t tag;

    // parse address and port number
    this->groupPort = port;

    memset(&this->groupAddr, 0, sizeof(struct sockaddr_storage));

    auto *addr4 = reinterpret_cast<struct sockaddr_in *>(&this->groupAddr);
    auto *addr6 = reinterpret_cast<struct sockaddr_in6 *>(&this->groupAddr);

    if(address.empty()) {
        throw std::runtime_error("Sync group address missing");
    }

    // can we parse it as an IPv4 address?
    err = inet_pton(AF_INET, address.c_str(), &addr4->sin_addr);

    if(err == 1) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(this->groupPort);
    } else if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "inet_pton(AF_INET) failed");
    }

    // request the key (request will come via `handleMessage`)
    this->currentKeyId = initialKeyId;

    this->sendMcastKeyReq(initialKeyId, tag);

    // set up socket and start the worker thread
    this->initSocket();
    this->initWorker();
}

/**
 * Handles a received message for the multicast control endpoint
 */
void MulticastReceiver::handleMessage(const Header &header, const PayloadType &payload) {
    using namespace MessageTypes;

    XASSERT(header.endpoint == MessageEndpoint::MulticastControl, "Invalid endpoint {:x}",
            header.endpoint);

    switch(header.messageType) {
        // key received
        case McastCtrlMessageType::MCC_GET_KEY_ACK:
            this->handleGetKey(header, cista::deserialize<GetKeyAckMsg, kCistaMode>(payload));
            break;

        // rekeying
        case McastCtrlMessageType::MCC_REKEY:
            this->handleRekey(header, cista::deserialize<RekeyMsg, kCistaMode>(payload));
            break;

        default:
            Logging::error("Unexpected multicast control message type {:x}", header.messageType);
            break;
    }
}

/**
 * Processes responses to "get key" requests.
 */
void MulticastReceiver::handleGetKey(const Header &, const GetKeyAckMsg *msg) {
    using namespace MessageTypes;

    // ensure success
    if(msg->status != MCC_SUCCESS) {
        Logging::error("Failed to get key: {}", msg->status);
        throw std::runtime_error("get key request failed");
    }

    // then load the key
    this->loadKey(msg->keyId, msg->keyData);
}

/**
 * Handles a rekey. This is pretty much the same logic as loading a key, except that we will also
 * change the current key id to the value specified in the rekey message.
 */
void MulticastReceiver::handleRekey(const Header &hdr, const RekeyMsg *msg) {
    using namespace MessageTypes;

    // load key and update the key id
    this->loadKey(msg->keyId, msg->keyData);
    this->currentKeyId = msg->keyId;

    Logging::trace("Rekey request received: new key id {:x}", msg->keyId);

    // acknowledge the re-key
    McastCtrlRekeyAck ack;
    memset(&ack, 0, sizeof(ack));

    ack.status = MCC_SUCCESS;
    ack.keyId = msg->keyId;

    auto bytes = cista::serialize<kCistaMode>(ack);
    this->client->reply(hdr, McastCtrlMessageType::MCC_REKEY_ACK, bytes);
}

/**
 * Loads the given key into the key store.
 */
void MulticastReceiver::loadKey(const uint32_t keyId, const KeyWrap &wrapper) {
    using namespace MessageTypes;

    // ensure we take the keystore lock
    std::lock_guard<std::mutex> lg(this->keystoreLock);

    // we don't allow overwriting keys
    if(this->keyStore.contains(keyId)) {
        Logging::error("Attempt to overwrite key {}", keyId);
        throw std::runtime_error("Overwriting keys is not permitted");
    }

    // ensure key type is supported
    if(wrapper.type != MCC_KEY_TYPE_CHACHA20_POLY1305) {
        Logging::error("Invalid key type: {}", wrapper.type);
        throw std::runtime_error("Unsupported key type");
    }

    // cool, we can store the key now
    KeyDataType key;
    IVDataType iv;

    if(key.size() > wrapper.key.size()) {
        throw std::runtime_error("Key too small");
    }
    else if(iv.size() > wrapper.iv.size()) {
        throw std::runtime_error("IV too small");
    }

    for(size_t i = 0; i < key.size(); i++) {
        key[i] = wrapper.key[i];
    }
    for(size_t i = 0; i < iv.size(); i++) {
        iv[i] = wrapper.iv[i];
    }

    this->keyStore[keyId] = KeyInfo(key, iv);
    Logging::trace("Received key {:x}: key = {}, iv = {}", keyId, hexdump(key.begin(), key.end()),
            hexdump(iv.begin(), iv.end()));
}

/**
 * Sends a request for the multicast key with the given id.
 */
void MulticastReceiver::sendMcastKeyReq(const uint32_t keyId, uint8_t &sentTag) {
    using namespace Lichtenstein::Proto::MessageTypes;

    McastCtrlGetKey msg;
    memset(&msg, 0, sizeof(msg));

    msg.keyId = keyId;

    auto bytes = cista::serialize<kCistaMode>(msg);

    uint8_t tag = this->client->nextTag++;
    this->client->send(MessageEndpoint::MulticastControl, McastCtrlMessageType::MCC_GET_KEY, tag, bytes);
    sentTag = tag;
}

