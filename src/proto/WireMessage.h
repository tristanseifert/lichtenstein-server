/**
 * Defines the thin wrapping struct that contains a message as sent over the
 * wire. A small header identifies the type and size of the message.
 */
#ifndef PROTO_WIREMESSAGE_H
#define PROTO_WIREMESSAGE_H

#include <cstdint>

namespace Lichtenstein::Server::Proto {
#pragma pack(push, 1)

/**
 * Current protocol version
 */
static const uint8_t kLichtensteinProtoVersion = 0x01;

/**
 * This is the message wrapper and contains length information, which is used
 * to read the received message into.
 */
struct MessageHeader {
    // protocol version; currently, this is 0x01
    uint8_t version;
    // message type, roughly corresponds to individual "endpoints"
    uint8_t type;
    // payload length (bytes)
    uint16_t length;

    // actual payload data
    char payload[];
};

#pragma pack(pop)
}

#endif
