// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2016-2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_CRYPTO_SHA256_H
#define BITCOIN_CRYPTO_SHA256_H

#include <stdint.h>
#include <stdlib.h>
#include <string>

/** A hasher class for SHA-256. */
class CSHA256
{
public:
    static const size_t OUTPUT_SIZE = 32;

    CSHA256();
    CSHA256& Write(const unsigned char* data, size_t len);
    void Finalize(unsigned char hash[OUTPUT_SIZE]);
    void FinalizeNoPadding(unsigned char hash[OUTPUT_SIZE]) {
    	FinalizeNoPadding(hash, true);
    };
    CSHA256& Reset();

private:
    uint32_t s[8];
    unsigned char buf[64];
    size_t bytes;
    void FinalizeNoPadding(unsigned char hash[OUTPUT_SIZE], bool enforce_compression);
};

/** Autodetect the best available SHA256 implementation.
 *  Returns the name of the implementation.
 */
std::string SHA256AutoDetect();

/** Compute multiple double-SHA256's of 64-byte blobs.
 *  output:  pointer to a blocks*32 byte output buffer
 *  input:   pointer to a blocks*64 byte input buffer
 *  blocks:  the number of hashes to compute.
 */
void SHA256D64(unsigned char* output, const unsigned char* input, size_t blocks);

#endif // BITCOIN_CRYPTO_SHA256_H
