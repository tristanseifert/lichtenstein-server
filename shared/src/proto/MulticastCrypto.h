/**
 * Provides a shared interface to encrypt and decrypt data sent as multicast frames.
 *
 * This is really a small wrapper around the SSL library functions since those are disgusting to
 * work with.
 *
 * Multicast packets are encrypted using ChaCha20-Poly1305, which is an authenticated (AEAD) cipher
 * that removes the need for a separate MAC over the packet contents.
 *
 * Note that although the IV is specified as 16 bytes, we only use the first 12 bytes.
 */
#ifndef PROTO_SHARED_MULTICASTCRYPTO_H
#define PROTO_SHARED_MULTICASTCRYPTO_H

#include <cstddef>
#include <array>
#include <vector>
#include <mutex>
#include <atomic>

#include <openssl/evp.h>

namespace Lichtenstein::Proto {
    class MulticastCrypto {
        private:
            // size of the key, in bits
            constexpr static const size_t kKeySizeBits = 256;
            // size of the initialization vector, in bits
            constexpr static const size_t kIVSizeBits = 128;
            // size of the auth tag, in bits
            constexpr static const size_t kAuthTagSizeBits = 128;

        public:
            using KeyType = std::array<std::byte, (kKeySizeBits / 8)>;
            using IVType = std::array<std::byte, (kIVSizeBits / 8)>;
            using ByteBuffer = std::vector<std::byte>;

        public:
            MulticastCrypto();
            ~MulticastCrypto();

        public:
            /// Loads the given key into the cipher engine
            void loadKey(const KeyType &key);

            /// Encrypts the given plaintext producing ciphertext and an auth tag.
            void encrypt(const ByteBuffer &plaintext, const IVType &iv, ByteBuffer &ciphertext);

            /// Decrypts the given ciphertext and validates the auth tag is intact.
            bool decrypt(const ByteBuffer &ciphertext, const IVType &iv, ByteBuffer &plaintext);

        private:
            // Lock over the EVP context
            std::mutex ctxLock;

            std::atomic_bool ctxInited;
            EVP_AEAD_CTX ctx;

            // number of bytes of IV to use as nonce
            size_t nonceLength;
    };
}

#endif
