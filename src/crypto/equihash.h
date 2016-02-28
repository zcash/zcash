// Copyright (c) 2016 Jack Grigg
// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EQUIHASH_H
#define BITCOIN_EQUIHASH_H

#include "crypto/sha256.h"

#include <vector>

struct StepRow
{
    unsigned char* hash;
    std::vector<uint32_t> indices;
};

struct invalid_params { };

class EquiHash
{
private:
    unsigned int n;
    unsigned int k;

public:
    EquiHash(unsigned int n, unsigned int k);
    std::vector<std::vector<uint32_t>> Solve(const CSHA256 base_hasher);
};

#endif // BITCOIN_EQUIHASH_H
