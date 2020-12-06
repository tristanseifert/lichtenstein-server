/**
 * Definitions of structs that are sent over the wire as part of the Lichtenstein protocol.
 *
 * Messages are encoded using bitsery. We're using it in a rather basic mode, but that's enough
 * for cross-platform communication, even across architectures and endiannesses.
 *
 * Most response messages contain a status code field. The exact values of the status fields are
 * specific to the endpoint itself, but values are assigned such that a) all endpoints use 0 to
 * indicate success, and b) each endpoint uses an unique, non-overlapping numbering space for its
 * status codes.
 */
#ifndef LICHTENSTEIN_PROTOMESSAGES_H
#define LICHTENSTEIN_PROTOMESSAGES_H

#include <cstddef>
#include <cstdint>

#include <string>
#include <vector>
#include <array>

#include <uuid.h>

#include <bitsery/bitsery.h>
#include <bitsery/adapter/buffer.h>
#include <bitsery/traits/string.h>
#include <bitsery/traits/array.h>
#include <bitsery/traits/vector.h>

namespace Lichtenstein::Proto::MessageTypes {
///////////////////////////////////////////////////////////////////////////////////////////////////
// Default endpoint messages
enum DefaultMessageType: uint8_t {
    DEF_PING_REQ                = 1,
    DEF_PING_RESP               = 2,
};

// ping request
struct PingRequest {
    // timestamp (sender defined, returned as-is)
    uint64_t timestamp;
    // opaque sequence identifier
    uint32_t sequence;
};
template <typename S>
void serialize(S& s, PingRequest& o) {
    s.value8b(o.timestamp);
    s.value4b(o.sequence);
}

// response to ping
struct PingResponse {
    // status code (should be 0)
    uint32_t status;
    // timestamp (sender defined, returned as-is)
    uint64_t timestamp;
    // opaque sequence identifier
    uint32_t sequence;
};
template <typename S>
void serialize(S& s, PingResponse& o) {
    s.value4b(o.status);
    s.value8b(o.timestamp);
    s.value4b(o.sequence);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Authentication endpoint messages
enum AuthMessageType: uint8_t {
    AUTH_REQUEST                = 1,
    AUTH_REQUEST_ACK            = 2,
    AUTH_RESPONSE               = 3,
    AUTH_RESPONSE_ACK           = 4,
};

// Authentication status codes
enum AuthStatus: uint32_t {
    AUTH_SUCCESS                = 0,
    AUTH_NO_METHODS             = 0x1000,
    AUTH_INVALID_ID             = 0x1001,
};

// client -> server, starting authentication
struct AuthRequest {
    /// UUID of the node
    std::string nodeId;
    /// supported authentication methods
    std::vector<std::string> methods;
};
template <typename S>
void serialize(S& s, AuthRequest& o) {
    s.text1b(o.nodeId, 48);
    s.container(o.methods, 48, [](S& s2, std::string& txt) {
        s2.text1b(txt, 64);
    });
};

// server -> client, negotiated method and aux data
struct AuthRequestAck {
    /// if non-zero, there was an error establishing auth
    AuthStatus status;
    /// selected authentication mechanism
    std::string method;

    // server-provided data for completing the authentication goes here
};
template <typename S>
void serialize(S& s, AuthRequestAck& o) {
    s.value4b(o.status);
    s.text1b(o.method, 64);
};

// client -> server, authentication method response (may occur more than once)
struct AuthResponse {
    /// indicate client status; non-zero aborts authentication
    AuthStatus status;
};
template <typename S>
void serialize(S& s, AuthResponse& o) {
    s.value4b(o.status);
};

// server -> client, acknowledge successful authentication
struct AuthResponseAck {
    /// success/failure indication
    AuthStatus status;
};
template <typename S>
void serialize(S& s, AuthResponseAck& o) {
    s.value4b(o.status);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Pixel data endpoint messages
enum PixelMessageType: uint8_t {
    PIX_SUBSCRIBE               = 1,
    PIX_SUBSCRIBE_ACK           = 2,
    PIX_UNSUBSCRIBE             = 3,
    PIX_UNSUBSCRIBE_ACK         = 4,
    PIX_DATA                    = 5,
    PIX_DATA_ACK                = 6,
};

enum PixelStatus: uint32_t {
    PIX_SUCCESS                 = 0,
    PIX_INVALID_CHANNEL         = 0x2000,
    PIX_INVALID_LENGTH          = 0x2001,
    PIX_INVALID_OFFSET          = 0x2002,
    PIX_INVALID_FORMAT          = 0x2003,
    PIX_ALREADY_SUBSCRIBED      = 0x2004,
    PIX_NO_SUBSCRIPTION         = 0x2005,
    PIX_NO_DATA                 = 0x2006,
};

enum PixelFormat: uint32_t {
    PIX_FORMAT_RGB              = 'RGB ',
    PIX_FORMAT_RGBW             = 'RGBW',
};

// client -> server; add subscription for data for the given channel
struct PixelSubscribe {
    /// output channel index
    uint32_t channel;

    /// pixel format the client wishes to receive data in
    PixelFormat format;
    /// start offset of subscription
    uint32_t start;
    /// length of the pixel data region we're interested in
    uint32_t length;
};
template <typename S>
void serialize(S& s, PixelSubscribe& o) {
    s.value4b(o.channel);
    s.value4b(o.format);
    s.value4b(o.start);
    s.value4b(o.length);
}
// server -> client; acknowledges a subscription
struct PixelSubscribeAck {
    PixelStatus status;
    /// an opaque identifier for this subscription
    uint32_t subscriptionId;
};
template <typename S>
void serialize(S& s, PixelSubscribeAck& o) {
    s.value4b(o.status);
    s.value4b(o.subscriptionId);
}

// client -> server; remove subscription for channel
struct PixelUnsubscribe {
    /// output channel index
    uint32_t channel;
    /// previously returned subscription id, or 0 to remove all subscriptions for the channel
    uint32_t subscriptionId;
};
template <typename S>
void serialize(S& s, PixelUnsubscribe& o) {
    s.value4b(o.channel);
    s.value4b(o.subscriptionId);
}
// server -> client; acknowledges unsubscription
struct PixelUnsubscribeAck {
    PixelStatus status;

    /// number of pixel observers that were removed as a result of this call
    uint32_t subscriptionsRemoved;
};
template <typename S>
void serialize(S& s, PixelUnsubscribeAck& o) {
    s.value4b(o.status);
    s.value4b(o.subscriptionsRemoved);
}

// server -> client; sends new pixel data
struct PixelDataMessage {
    // channel index
    uint32_t channel;
    // offset into channel
    uint32_t offset;

    // format of pixel data
    PixelFormat format;
    // pixel data
    std::vector<std::byte> pixels;
};
template <typename S>
void serialize(S& s, PixelDataMessage& o) {
    s.value4b(o.channel);
    s.value4b(o.offset);
    s.value4b(o.format);

    s.container1b(o.pixels, 1500);
}
// client -> server; acknowledges a pixel data frame
struct PixelDataMessageAck {
    // channel for which we're acknowledging
    uint32_t channel;
};
template <typename S>
void serialize(S& s, PixelDataMessageAck& o) {
    s.value4b(o.channel);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Multicast control endpoint message types
enum McastCtrlMessageType: uint8_t {
    MCC_GET_INFO                = 1,
    MCC_GET_INFO_ACK            = 2,
    MCC_REKEY                   = 3,
    MCC_REKEY_ACK               = 4,
    MCC_GET_KEY                 = 5,
    MCC_GET_KEY_ACK             = 6,
};

enum McastCtrlStatus: uint32_t {
    MCC_SUCCESS                 = 0,
    MCC_INVALID_KEY_TYPE        = 0x3000,
    MCC_INVALID_KEY             = 0x3001,
    MCC_INVALID_KEY_ID          = 0x3002,
};

enum McastCtrlKeyType: uint32_t {
    MCC_KEY_TYPE_CHACHA20_POLY1305      = 1,
};

// generic key info wrapper struct
struct McastCtrlKeyWrapper {
    // key type
    McastCtrlKeyType type;
    // key data
    std::vector<std::byte> key;
    // initialization vector
    std::vector<std::byte> iv;
};
template <typename S>
void serialize(S& s, McastCtrlKeyWrapper& o) {
    s.value4b(o.type);
    s.container1b(o.key, 128);
    s.container1b(o.iv, 128);
}
// client -> server; requests info for the multicast control channel
struct McastCtrlGetInfo {
    uint32_t reserved;
};
template <typename S>
void serialize(S& s, McastCtrlGetInfo& o) {
    s.value4b(o.reserved);
}
// server -> client; info on the multicast channel
struct McastCtrlGetInfoAck {
    // status
    McastCtrlStatus status;

    // address of the multicast group
    std::string address;
    // port number
    uint16_t port;

    // key id currently in use
    uint32_t keyId;
};
template <typename S>
void serialize(S& s, McastCtrlGetInfoAck& o) {
    s.value4b(o.status);

    s.text1b(o.address, 128);
    s.value2b(o.port);

    s.value4b(o.keyId);
}

// server -> client; provides a key to use for multicast
struct McastCtrlRekey {
    // key id
    uint32_t keyId;
    // key data
    McastCtrlKeyWrapper keyData;
};
template <typename S>
void serialize(S& s, McastCtrlRekey& o) {
    s.value4b(o.keyId);
    s.object(o.keyData);
}

// client -> server; acknowledges receipt of a new key
struct McastCtrlRekeyAck {
    // status
    McastCtrlStatus status;
    // key id that we're acknowledging
    uint32_t keyId;
};
template <typename S>
void serialize(S& s, McastCtrlRekeyAck& o) {
    s.value4b(o.status);
    s.value4b(o.keyId);
}

// client -> server; requests key with the given id
struct McastCtrlGetKey {
    // desired key id
    uint32_t keyId;
};
template <typename S>
void serialize(S& s, McastCtrlGetKey& o) {
    s.value4b(o.keyId);
}
// server -> client; provides a requested key
struct McastCtrlGetKeyAck {
    // status
    McastCtrlStatus status;

    // key id
    uint32_t keyId;
    // key data
    McastCtrlKeyWrapper keyData;
};
template <typename S>
void serialize(S& s, McastCtrlGetKeyAck& o) {
    s.value4b(o.status);
    s.value4b(o.keyId);

    s.object(o.keyData);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Multicast data endpoint message types
enum McastDataMessageType: uint8_t {
    MCD_SYNC_OUTPUT             = 1,
};

// server -> client; synchronized output
struct McastDataSyncOutput {
    // channel bitmask (currently unused. set to 0)
    uint64_t channels;
};
template <typename S>
void serialize(S& s, McastDataSyncOutput& o) {
    s.value8b(o.channels);
}


};

#endif
