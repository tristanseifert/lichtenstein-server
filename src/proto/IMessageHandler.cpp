#include "IMessageHandler.h"
#include "ServerWorker.h"
#include "WireMessage.h"

#include "../Logging.h"

#include <limits>
#include <stdexcept>
#include <algorithm>

#include <fmt/format.h>
#include <openssl/ssl.h>

using namespace Lichtenstein::Server::Proto;

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
void IMessageHandler::send(uint8_t type, const std::vector<std::byte> &data) {
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
