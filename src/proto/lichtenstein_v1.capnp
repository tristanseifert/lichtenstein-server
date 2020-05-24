@0xe50996628ee0a764;

# Place our types into the right namespace
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("Lichtenstein::Server::Proto::WireTypes");

###############################################################################
# Message wrapper; all messages will be contained in one of these.
struct Message {
    # status code; 0 is success
    status          @0: Int32;

    # message payload or error info
    union {
        payload     @1: AnyPointer;
        errorStr    @2: Text;
    }
}

###############################################################################
# Endpoint 0x01 - Node authentication
#
# Supported authentication methods
enum AuthMethod {
    # node ID is all that is required to authenticate
    nullMethod      @0;
    # server provides a challenge involving secret key; node replies
    challengeMethod @1;
}

# Sent by a node to initiate authentication. It provides its unique ID, as well
# as a list of supported authentication mechanisms.
struct AuthRequest {
    # unique node identifier
    nodeUuid        @0: Data;
    # list of supported auth mechanisms
    supported       @1: List(AuthMethod);
}
# Sent by the server to start the authentication process. It indicates the
# method selected by the server, as well as any server-provided information to
# begin authentication.
struct AuthRequestAck {
    # selected auth method
    method          @0: AuthMethod;

    # method payload
    payload: union {
        # no payload
        none        @1: Void;
        # server-provided auth challenge
        challenge     : group {
            # binary data client computes challenge over
            bytes   @2: Data;
            # nonce to include in computation
            nonce   @3: UInt64;
        }
    }
}

# Sent by the client for the second stage of authentication. Some methods may
# not require this second response.
struct AuthResponse {
    # selected auth method
    method          @0: AuthMethod;

    # response payload
    payload: union {
        # no payload
        none        @1: Void;
        # client response to auth challenge
        challenge: group {
            # raw response bytes
            bytes   @2: Data;
        }
    }
}
# Sent by the server after authentication completes, indicating whether the
# node was successfully authenticated.
struct AuthResponseAck {
    # selected auth method
    method          @0: AuthMethod;
    # did authentication succeed?
    success         @1: Bool;
    # optional descriptive message
    message         @2: Text;
}
