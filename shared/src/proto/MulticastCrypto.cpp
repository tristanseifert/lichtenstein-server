#include "MulticastCrypto.h"

#include <Format.h>
#include <Logging.h>

#include <stdexcept>
#include <algorithm>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>

using namespace Lichtenstein::Proto;

/**
 * Constructs the cryptor and sets up the EVP cipher context.
 */
MulticastCrypto::MulticastCrypto() {
    int err;

    // initialize the context
    const auto algo = EVP_aead_chacha20_poly1305();
    this->ctxInited = false;

    this->nonceLength = EVP_AEAD_nonce_length(algo);
    Logging::trace("Using {} bytes of IV as nonce", this->nonceLength);

    XASSERT(EVP_AEAD_max_tag_len(algo) == (kAuthTagSizeBits / 8), "Tag length mismatch: {}, expected {}",
            EVP_AEAD_max_tag_len(algo), (kAuthTagSizeBits / 8));
}
/**
 * Shuts down the cipher context
 */
MulticastCrypto::~MulticastCrypto() {
    // clean up context
    if(this->ctxInited) {
        EVP_AEAD_CTX_cleanup(&this->ctx);
    }
}

/**
 * Re-initializes the context with the given key. It will be used for all subsequent cipher operations.
 */
void MulticastCrypto::loadKey(const KeyType &key) {
    int err;

    // release previous resources
    if(this->ctxInited) {
        EVP_AEAD_CTX_cleanup(&this->ctx);
    }

    // re-initialize context
    const auto algo = EVP_aead_chacha20_poly1305();
    err = EVP_AEAD_CTX_init(&this->ctx, algo, reinterpret_cast<const unsigned char *>(key.data()),
            key.size(), (kAuthTagSizeBits / 8), nullptr);

    if(err != 1) {
        throw std::runtime_error("failed to set up AEAD ctx");
    }

    this->ctxInited = true;
}


/**
 * Encrypts the given plaintext using ChaCha20-Poly1305.
 *
 * If successful, the ciphertext and auth tag output vectors are filled with the corresponding
 * data.
 *
 * @note If any errors occur, an exception is thrown.
 */
void MulticastCrypto::encrypt(const ByteBuffer &plaintext, const IVType &iv, ByteBuffer &ciphertext) {
    int err;
    size_t outLen = 0;

    std::lock_guard<std::mutex> lg(this->ctxLock);
    const auto algo = EVP_aead_chacha20_poly1305();

    // plaintext may not be nil
    XASSERT(!plaintext.empty(), "Plaintext must not be empty");

    // reserve output space and get the nonce
    ciphertext.resize(plaintext.size() + EVP_AEAD_max_overhead(algo));

    XASSERT(iv.size() >= this->nonceLength, "Invalid IV size (minimum {})", this->nonceLength);
    std::vector<std::byte> nonce(iv.begin(), iv.begin() + this->nonceLength);

    // encrypt into a temporary buffer
    err = EVP_AEAD_CTX_seal(&this->ctx, reinterpret_cast<unsigned char *>(ciphertext.data()),
            &outLen, ciphertext.size(), reinterpret_cast<const unsigned char *>(nonce.data()),
            nonce.size(), reinterpret_cast<const unsigned char *>(plaintext.data()), plaintext.size(),
            nullptr, 0);

    if(err != 1) {
        throw std::runtime_error("AEAD seal failed");
    }

    // resize the output buffer
    ciphertext.resize(outLen);
}

/**
 * Decrypts and authenticates the given ciphertext.
 *
 * @return true if decryption + authentication was successful, false otherwise.
 *
 * @note If this call fails for ANY REASON, you should NOT touch the plaintext, even if it appears
 * to have decrypted successfully as this indicates authentication failures.
 */
bool MulticastCrypto::decrypt(const ByteBuffer &ciphertext, const IVType &iv, ByteBuffer &plaintext) {
    int err;
    size_t outLen = 0;

    std::lock_guard<std::mutex> lg(this->ctxLock);
    const auto algo = EVP_aead_chacha20_poly1305();

    // reserve output space and get nonce
    plaintext.resize(ciphertext.size() + EVP_AEAD_max_overhead(algo));

    XASSERT(iv.size() >= this->nonceLength, "Invalid IV size (minimum {})", this->nonceLength);
    std::vector<std::byte> nonce(iv.begin(), iv.begin() + this->nonceLength);

    // decrypt and authenticate
    err = EVP_AEAD_CTX_open(&this->ctx, reinterpret_cast<unsigned char *>(plaintext.data()),
            &outLen, plaintext.size(), reinterpret_cast<const unsigned char *>(nonce.data()),
            nonce.size(), reinterpret_cast<const unsigned char *>(ciphertext.data()),
            ciphertext.size(), nullptr, 0);

    if(err != 1) {
        throw std::runtime_error("AEAD open failed");
        return false;
    }

    // hanle succes
    if(err == 1) {
        return true;
    }

    return false;
}

