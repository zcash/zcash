// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_CRYPTO_COMMON_H
#define BITCOIN_CRYPTO_COMMON_H

#if defined(HAVE_CONFIG_H)
#include "bitcoin-config.h"
#endif

#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "sodium.h"
#include "compat/endian.h"

#if defined(NDEBUG)
# error "Zcash cannot be compiled without assertions."
#endif

uint16_t static inline ReadLE16(const unsigned char* ptr)
{
    uint16_t x;
    memcpy((char*)&x, ptr, 2);
    return le16toh(x);
}

uint32_t static inline ReadLE32(const unsigned char* ptr)
{
    uint32_t x;
    memcpy((char*)&x, ptr, 4);
    return le32toh(x);
}

uint64_t static inline ReadLE64(const unsigned char* ptr)
{
    uint64_t x;
    memcpy((char*)&x, ptr, 8);
    return le64toh(x);
}

void static inline WriteLE16(unsigned char* ptr, uint16_t x)
{
    uint16_t v = htole16(x);
    memcpy(ptr, (char*)&v, 2);
}

void static inline WriteLE32(unsigned char* ptr, uint32_t x)
{
    uint32_t v = htole32(x);
    memcpy(ptr, (char*)&v, 4);
}

void static inline WriteLE64(unsigned char* ptr, uint64_t x)
{
    uint64_t v = htole64(x);
    memcpy(ptr, (char*)&v, 8);
}

uint32_t static inline ReadBE32(const unsigned char* ptr)
{
    uint32_t x;
    memcpy((char*)&x, ptr, 4);
    return be32toh(x);
}

uint64_t static inline ReadBE64(const unsigned char* ptr)
{
    uint64_t x;
    memcpy((char*)&x, ptr, 8);
    return be64toh(x);
}

void static inline WriteBE32(unsigned char* ptr, uint32_t x)
{
    uint32_t v = htobe32(x);
    memcpy(ptr, (char*)&v, 4);
}

void static inline WriteBE64(unsigned char* ptr, uint64_t x)
{
    uint64_t v = htobe64(x);
    memcpy(ptr, (char*)&v, 8);
}

int inline init_and_check_sodium()
{
    if (sodium_init() == -1) {
        return -1;
    }

    // What follows is a runtime test that ensures the version of libsodium
    // we're linked against checks that signatures are canonical (s < L).
    const unsigned char message[1] = { 0 };

    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    unsigned char sig[crypto_sign_BYTES];

    crypto_sign_keypair(pk, sk);
    crypto_sign_detached(sig, NULL, message, sizeof(message), sk);

    assert(crypto_sign_verify_detached(sig, message, sizeof(message), pk) == 0);

    // Copied from libsodium/crypto_sign/ed25519/ref10/open.c
    static const unsigned char L[32] =
      { 0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
        0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 };

    // Add L to S, which starts at sig[32].
    unsigned int s = 0;
    for (size_t i = 0; i < 32; i++) {
        s = sig[32 + i] + L[i] + (s >> 8);
        sig[32 + i] = s & 0xff;
    }

    assert(crypto_sign_verify_detached(sig, message, sizeof(message), pk) != 0);

    return 0;
}

#endif // BITCOIN_CRYPTO_COMMON_H
