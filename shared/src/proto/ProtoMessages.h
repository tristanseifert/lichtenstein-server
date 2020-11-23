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

struct AuthRequest {
    /// UUID of the node
    data::string nodeId;
    /// supported authentication methods
    data::vector<data::string> methods;
};

struct AuthRequestAck {
    /// if non-zero, there was an error establishing auth
    uint32_t status;
    /// selected authentication mechanism
    data::string method;

    // server-provided data for completing the authentication goes here
};

};

#endif
