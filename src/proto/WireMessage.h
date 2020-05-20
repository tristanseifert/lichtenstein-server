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
 * Message endpoint types
 */
enum MessageEndpoint: uint8_t {
    /// default endpoint; this drops all messages
    Default = 0,
    /// authentication of nodes
    Authentication = 1,
};

/**
 * This is the message wrapper and contains length information, which is used
 * to read the received message into.
 */
struct MessageHeader {
    // protocol version; currently, this is 0x01
    uint8_t version;
    // message type, roughly corresponds to individual "endpoints"
    MessageEndpoint type;
    // payload length (bytes)
    uint16_t length;

    // actual payload data
    char payload[];
};

static_assert(sizeof(MessageHeader) == 4, "Incorrect message header size");
static_assert(offsetof(MessageHeader, version) == 0, "Version field offset wrong");
static_assert(offsetof(MessageHeader, type) == 1, "Type field offset wrong");
static_assert(offsetof(MessageHeader, length) == 2, "Length field offset wrong");
static_assert(offsetof(MessageHeader, payload) == 4, "Payload offset wrong");

#pragma pack(pop)
}

#endif
