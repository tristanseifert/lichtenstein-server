/**
 * Defines the thin wrapping struct that contains a message as sent over the
 * wire. A small header identifies the type and size of the message.
 */
#ifndef PROTO_WIREMESSAGE_H
#define PROTO_WIREMESSAGE_H

#include <cstdint>

namespace Lichtenstein::Proto {
#pragma pack(push, 1)

/**
 * Current protocol version
 */
static const uint8_t kLichtensteinProtoVersion = 0x01;

/**
 * Message endpoint types
 *
 * Note that this defines the format of the messages.
 */
enum MessageEndpoint: uint8_t {
    /// default endpoint; this drops all messages
    Default = 0,
    /// authentication of nodes
    Authentication = 1,
    /// pixel data (subscriptions and data transmission)
    PixelData = 2,
    /// Multicast sync messages (control via DTLS)
    MulticastControl = 3,
    /// Multicast data messages (encrypted multicast packets)
    MulticastData = 4,
};

/**
 * This is the message wrapper and contains length information, which is used
 * to read the received message into.
 */
struct MessageHeader {
    // protocol version; currently, this is 0x01
    uint8_t version;
    // message type, roughly corresponds to individual "endpoints"
    MessageEndpoint endpoint;
    // message type. this is specific to the endpoint
    uint8_t messageType;
    // tag (responses carry the tag of the originating request)
    uint8_t tag;
    // payload length (bytes)
    uint16_t length;

    // actual payload data
    char payload[];
};
static_assert(sizeof(MessageHeader) == 6, "Incorrect message header size");


/**
 * Message wrapper used for encrypted multicast messages.
 */
struct MulticastMessageHeader {
    // protocol version; currently, this is 0x01
    uint8_t version;
    // message type, roughly corresponds to individual "endpoints"
    MessageEndpoint endpoint;
    // message type. this is specific to the endpoint
    uint8_t messageType;
    // tag (responses carry the tag of the originating request)
    uint8_t tag;
    // payload length (bytes)
    uint16_t length;
    // key ID used to encrypt this packet (TODO: is this leaking too much info?)
    uint32_t keyId;

    // actual payload data
    char payload[];
};
static_assert(sizeof(MulticastMessageHeader) == 10, "Incorrect multicast message header size");


#pragma pack(pop)
}

#endif
