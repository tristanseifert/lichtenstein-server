/**
 * Definitions of structs that are sent over the wire as part of the Lichtenstein protocol.
 *
 * These are encoded using Cista++.
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
    AUTH_REQUEST        = 1,
    AUTH_REQUEST_ACK    = 2,
    AUTH_RESPONSE       = 3,
    AUTH_RESPONSE_ACK   = 4,
};

// Authentication status codes
enum AuthStatus: uint32_t {
    AUTH_SUCCESS        = 0,
    AUTH_NO_METHODS     = 0x1000,
    AUTH_INVALID_ID     = 0x1001,
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

};

#endif
