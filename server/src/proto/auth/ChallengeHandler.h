/*
 * Implements challenge/response authentication. The server will send some
 * random bytes, as well as a nonce, to the client; the client will then
 * concatenate the nonce and random data, and compute a HMAC over it with
 * its secret key. If that computed HMAC value matches what we expect, we can
 * assume that the node is good.
 */
#include "IAuthHandler.h"
#include "db/DataStorePrimitives.h"

#include <cstddef>
#include <vector>

namespace Lichtenstein::Server::Proto::Auth {
    class ChallengeHandler: public IAuthHandler {
        using Node = DB::Types::Node;

        public:
            ChallengeHandler() = delete;
            ChallengeHandler(const Node &);

        private:
            int doHmac(std::vector<std::byte> &);

        private:
            std::vector<std::byte> rand;
            uint64_t nonce;
    };
}
