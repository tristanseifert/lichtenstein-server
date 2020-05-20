@0xe50996628ee0a764;

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
    # status code (negative indicates error, 0 is success)
    status          @1: Int16;

    # method payload
    payload: union {
        # no payload
        none        @2: Void;
        # server-provided auth challenge
        challenge     : group {
            # binary data client computes challenge over
            bytes   @3: Data;
            # nonce to include in computation
            nonce   @4: UInt64;
        }
    }
}

# Sent by the client for the second stage of authentication. Some methods may
# not require this second response.
struct AuthResponse {
    # selected auth method
    method          @0: AuthMethod;
    # status code (negative indicates error, 0 is success)
    status          @1: Int16;

    # response payload
    payload: union {
        # no payload
        none        @2: Void;
        # client response to auth challenge
        challenge: group {
            # raw response bytes
            bytes   @3: Data;
        }
    }
}
# Sent by the server after authentication completes, indicating whether the
# node was successfully authenticated.
struct AuthResponseAck {
    # selected auth method
    method          @0: AuthMethod;
    # status code (negative indicates error, 0 is success)
    status          @1: Int16;

    # optional descriptive message
    message         @2: Text;
}
