#include "IMessageHandler.h"
#include "ServerWorker.h"
#include "proto/WireMessage.h"

#include <Format.h>
#include <Logging.h>

#include <limits>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <openssl/ssl.h>

using namespace Lichtenstein::Proto;
using namespace Lichtenstein::Server::Proto;

/// Container for all registered message handlers
IMessageHandler::MapType *IMessageHandler::registrations = nullptr;
/// Lock for the above container
std::mutex IMessageHandler::registerLock;


/**
 * Validates that the client ptr is not null on allocation.
 */
IMessageHandler::IMessageHandler(ServerWorker *client) : client(client) {
    XASSERT(client, "Message handlers must be created with a valid client");
}

/**
 * Sends a response message to the client. This will create a wire message
 * marked with the specified type and containing payload.
 *
 * If there is an error while sending data, an exception is thrown.
 */
void IMessageHandler::send(const MessageEndpoint endpoint, const uint8_t type, const uint8_t tag, 
        const PayloadType &data) {
    struct MessageHeader hdr;
    memset(&hdr, 0, sizeof(hdr));

    std::vector<unsigned char> send;
    int err;

    // validate parameter values
    if(data.size() > std::numeric_limits<uint16_t>::max()) {
        auto what = f("Message too big ({} bytes, max {})", 
                data.size(), std::numeric_limits<uint16_t>::max());
        throw std::invalid_argument(what);
    }

    send.reserve(data.size() + sizeof(hdr));
    send.resize(sizeof(hdr));

    // build a header in network byte order
    hdr.version = kLichtensteinProtoVersion;
    hdr.length = htons(data.size());
    hdr.endpoint = endpoint;
    hdr.messageType = type;
    hdr.tag = tag;

    auto hdrBytes = reinterpret_cast<unsigned char *>(&hdr);
    std::copy(hdrBytes, hdrBytes+sizeof(hdr), send.begin());

    // append the payload and attempt to send
    send.insert(send.end(), data.begin(), data.end());

#if 0
    Logging::trace("Sent {} ({} payload) byte message: type={:x}:{:x}, tag={:x}, payload={}",
            send.size(), data.size(), endpoint, type, tag, hexdump(data.begin(), data.end()));
#endif

    err = this->client->writeBytes(send.data(), send.size());

    if(err != send.size()) {
        auto what = f("Failed to write {} byte message; only wrote {}",
                send.size(), err);
        throw std::runtime_error(what);
    }
}



/**
 * Registers a protocol message handler.
 *
 * @param tag Arbitrary user-defined tag for the controller
 * @param ctor Constructor function for controller
 * @return Whether the class was registered successfully
 */
bool IMessageHandler::registerClass(const std::string &tag, HandlerCtor ctor) {
    // ensure only one thread can be allocating at a time
    std::lock_guard guard(registerLock);

    // allocate the map if needed
    if(!registrations) {
        registrations = new MapType;
    }

    // store the registration if free
    if(auto it = registrations->find(tag); it == registrations->end()) {
        registrations->insert(std::make_pair(tag, ctor));
        return true;
    }

    // someone already registered this tag
    Logging::error("Illegal re-registration of tag '{}'", tag);
    return false;
}

/**
 * Iterates over all registered controllers.
 */
void IMessageHandler::forEach(std::function<void(const std::string&, HandlerCtor)> f) {
    if(!registrations) return;

    for(auto const &[key, ctor] : *registrations) {
        f(key, ctor);
    }
}

/**
 * Dumps all registered functions
 */
void IMessageHandler::dumpRegistry() {
    std::stringstream str;

    if(registrations) {
        for(auto const &[key, func] : *registrations) {
            str << std::setw(20) << std::setfill(' ') << key << std::setw(0);
            str << ": " << func << std::endl;
        } 

        Logging::debug("{} Proto msg handlers registered\n{}", 
            registrations->size(), str.str());
    } else {
        Logging::debug("0 Proto msg handlers registered");
    }
}

/**
 * Throws an exception if the client isn't authenticated.
 */
void IMessageHandler::requireAuth() {
    if(!this->client->isAuthenticated()) {
        throw std::runtime_error("Endpoint requires authentication");
    }
}
