#include "ChallengeHandler.h"

#include "../Server.h"
#include "../controllers/Authentication.h"

#include <Logging.h>

#include <cstring>

#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

using namespace Lichtenstein::Server::Proto::Auth;
using SSLError = Lichtenstein::Server::Proto::Server::SSLError;

/**
 * Initializes the challenge/response controller. It will process the auth
 * handshake for requests sent to the specified controller.
 */
ChallengeHandler::ChallengeHandler(const Node &in) : IAuthHandler(in) {
    int err;

    // ensure inputs are sane
    XASSERT(node.id > 0, "Node must be valid (id = {})", in.id);

    // generate the random data
    this->rand.resize(16, std::byte(0));
    auto dataPtr = reinterpret_cast<unsigned char *>(this->rand.data());

    err = RAND_bytes(dataPtr, 16);
    if(err != 1) {
        throw SSLError("RAND_bytes() failed");
    }
}



/**
 * Calculates the expected response to the challenge. The private key for the
 * HMAC is taken from the passed in node.
 */
int ChallengeHandler::doHmac(std::vector<std::byte> &out) {
    unsigned int outLen;

    // reserve adequate space in the output buffer
    out.resize(EVP_MAX_MD_SIZE, std::byte(0));
    auto outPtr = reinterpret_cast<unsigned char *>(out.data());

    // generate the concatenated input
    size_t inLen = sizeof(this->nonce) + this->rand.size();
    uint8_t in[inLen];
    memset(&in, 0, inLen);

    const size_t nonceSz = sizeof(this->nonce);
    memcpy(&in, &this->nonce, nonceSz);
    
    XASSERT((this->rand.size() + nonceSz) <= inLen, "Rand too big ({} bytes)",
            this->rand.size());
    memcpy(&in[nonceSz], this->rand.data(), this->rand.size());

    // prepare the algorithm
    auto algo = EVP_sha1();

    // calculate that shit
    auto hmacPtr = HMAC(algo, this->node.sharedSecret.data(),
            this->node.sharedSecret.size(), in, inLen, outPtr, &outLen);

    if(!hmacPtr) {
        throw SSLError("HMAC() failed");
    }

    // return the total number of bytes written to the out vector
    out.resize(outLen);
    return outLen;
}
