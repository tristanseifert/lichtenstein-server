/**
 * Provides a shared interface to encrypt and decrypt data sent as multicast frames.
 *
 * This is really a small wrapper around the SSL library functions since those are disgusting to
 * work with.
 *
 * Multicast packets are encrypted using ChaCha20-Poly1305, which is an authenticated (AEAD) cipher
 * that removes the need for a separate MAC over the packet contents.
 */
#ifndef PROTO_SHARED_MULTICASTCRYPTO_H
#define PROTO_SHARED_MULTICASTCRYPTO_H

namespace Lichtenstein::Proto {
    class MulticastCrypto {

    };
}

#endif
