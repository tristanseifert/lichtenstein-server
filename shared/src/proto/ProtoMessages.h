/**
 * Definitions of structs that are sent over the wire as part of the Lichtenstein protocol.
 *
 * These are encoded using Cista++. To ensure data is deserialized correctly, a hash of the
 * structures is included at the cost of 16 extra bytes in each message.
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

#include <uuid.h>
#include <cista.h>

namespace Lichtenstein::Proto::MessageTypes {
namespace data = cista::offset;

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Encoding/decoding modes used for these messages
 */
constexpr auto const kCistaMode = (cista::mode::WITH_VERSION | cista::mode::WITH_INTEGRITY |
                                   cista::mode::DEEP_CHECK);

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
    data::string nodeId;
    /// supported authentication methods
    data::vector<data::string> methods;
};
// server -> client, negotiated method and aux data
struct AuthRequestAck {
    /// if non-zero, there was an error establishing auth
    AuthStatus status;
    /// selected authentication mechanism
    data::string method;

    // server-provided data for completing the authentication goes here
};
// client -> server, authentication method response (may occur more than once)
struct AuthResponse {
    /// indicate client status; non-zero aborts authentication
    AuthStatus status;
};
// server -> client, acknowledge successful authentication
struct AuthResponseAck {
    /// success/failure indication
    AuthStatus status;
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
// server -> client; acknowledges a subscription
struct PixelSubscribeAck {
    PixelStatus status;
    /// an opaque identifier for this subscription
    uint32_t subscriptionId;
};

// client -> server; remove subscription for channel
struct PixelUnsubscribe {
    /// output channel index
    uint32_t channel;
    /// previously returned subscription id, or 0 to remove all subscriptions for the channel
    uint32_t subscriptionId;
};
// server -> client; acknowledges unsubscription
struct PixelUnsubscribeAck {
    PixelStatus status;

    /// number of pixel observers that were removed as a result of this call
    uint32_t subscriptionsRemoved;
};

// server -> client; sends new pixel data
struct PixelDataMessage {
    // channel index
    uint32_t channel;
    // offset into channel
    uint32_t offset;

    // format of pixel data
    PixelFormat format;
    // pixel data
    data::vector<std::byte> pixels;
};
// client -> server; acknowledges a pixel data frame
struct PixelDataMessageAck {
    // channel for which we're acknowledging
    uint32_t channel;
};

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

// client -> server; requests info for the multicast control channel
struct McastCtrlGetInfo {
    
};
// server -> client; info on the multicast channel
struct McastCtrlGetInfoAck {
    // status
    McastCtrlStatus status;

    // address of the multicast group
    data::string address;
    // port number
    uint16_t port;

    // key id currently in use
    uint32_t keyId;
};

// server -> client; provides a key to use for multicast
struct McastCtrlRekey {
    // key id
    uint32_t keyId;
    // key type
    McastCtrlKeyType type;
    // key data
    data::vector<std::byte> key;
    // initialization vector
    data::vector<std::byte> iv;
};

// client -> server; acknowledges receipt of a new key
struct McastCtrlRekeyAck {
    // status
    McastCtrlStatus status;
    // key id that we're acknowledging
    uint32_t keyId;
};

// client -> server; requests key with the given id
struct McastCtrlGetKey {
    // desired key id
    uint32_t keyId;
};
// server -> client; provides a requested key
struct McastCtrlGetKeyAck {
    // status
    McastCtrlStatus status;

    // key id
    uint32_t keyId;
    // key type
    McastCtrlKeyType type;
    // key data
    data::vector<std::byte> key;
    // initialization vector
    data::vector<std::byte> iv;
};

};

#endif
