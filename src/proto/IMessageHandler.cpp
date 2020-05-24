#include "IMessageHandler.h"
#include "ServerWorker.h"
#include "WireMessage.h"

#include "../Logging.h"

#include <limits>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <fmt/format.h>
#include <openssl/ssl.h>

// Cap'n Proto stuff 
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <proto/lichtenstein_v1.capnp.h>

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
 * Attempts to decode a message from the byte buffer. An exception will be
 * thrown if decoding failed.
 */
WireTypes::Message::Reader IMessageHandler::decode(const PayloadType &payload) {
    using namespace WireTypes;
    
    auto data = reinterpret_cast<const capnp::word*>(payload.data());
    auto ptr = kj::arrayPtr(data, payload.size());
    capnp::FlatArrayMessageReader reader(ptr);

    Message::Reader msg = reader.getRoot<Message>();
    
    // ensure that the status was success
    if(msg.getStatus() != 0) {
        // should never get into this case
        if(!msg.hasErrorStr()) {
            auto what = fmt::format("Message status {} without error string",
                    msg.getStatus());
            throw std::runtime_error(what);
        }

        // we do have an error string to provide
        auto detailStr = msg.getErrorStr().cStr();
        auto what = fmt::format("Message status {}: {}", msg.getStatus(),
                detailStr);
        throw std::runtime_error(what);
    }

    return msg;
}


/**
 * Sends a response message to the client. This will create a wire message
 * marked with the specified type and containing payload.
 *
 * If there is an error while sending data, an exception is thrown.
 */
void IMessageHandler::send(MessageEndpoint type, uint8_t tag, 
        const std::vector<std::byte> &data) {
    struct MessageHeader hdr;
    std::vector<std::byte> send;
    int err;
    
    // validate parameter values
    if(data.size() > std::numeric_limits<uint16_t>::max()) {
        auto what = fmt::format("Message too big ({} bytes, max {})", 
                data.size(), std::numeric_limits<uint16_t>::max());
        throw std::invalid_argument(what);
    }
    
    Logging::trace("Sending {} byte message of type {:x} to {}", data.size(),
            type, (void*)this->client);


    // build a header in network byte order
    hdr.length = htons(data.size());
    hdr.type = type;
    hdr.tag = tag;

    auto hdrBytes = reinterpret_cast<std::byte *>(&hdr);
    std::copy(hdrBytes, hdrBytes+sizeof(hdr), send.begin());

    // append the payload and attempt to send
    send.insert(send.end(), data.begin(), data.end());

    err = this->client->writeBytes(send.data(), send.size());

    if(err != send.size()) {
        auto what = fmt::format("Failed to write {} byte message; only wrote {}",
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

