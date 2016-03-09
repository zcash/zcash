// Copyright (c) 2016 Jack Grigg
// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EQUIHASH_H
#define BITCOIN_EQUIHASH_H

#include "crypto/sha256.h"
#include "utilstrencodings.h"

#include <cstring>
#include <vector>

struct invalid_params { };

class StepRow
{
private:
    unsigned char* hash;
    unsigned int len;
    std::vector<uint32_t> indices;

public:
    StepRow(unsigned int n, const CSHA256& base_hasher, uint32_t i);
    ~StepRow();

    StepRow(const StepRow& a);
    StepRow& operator=(const StepRow& a);
    StepRow& operator^=(const StepRow& a);

    bool IsZero();
    std::vector<uint32_t> GetSolution() { return std::vector<uint32_t>(indices); }
    std::string GetHex() { return HexStr(hash, hash+len); }

    friend inline const StepRow operator^(const StepRow& a, const StepRow& b) { return StepRow(a) ^= b; }
    friend inline bool operator==(const StepRow& a, const StepRow& b) { return memcmp(a.hash, b.hash, a.len) == 0; }
    friend inline bool operator<(const StepRow& a, const StepRow& b) { return memcmp(a.hash, b.hash, a.len) < 0; }

    friend bool HasCollision(StepRow& a, StepRow& b, int i, int l);
    friend bool DistinctIndices(const StepRow& a, const StepRow& b);
};

bool HasCollision(StepRow& a, StepRow& b, int i, int l);
bool DistinctIndices(const StepRow& a, const StepRow& b);

class EquiHash
{
private:
    unsigned int n;
    unsigned int k;

public:
    EquiHash(unsigned int n, unsigned int k);
    std::vector<std::vector<uint32_t>> Solve(const CSHA256& base_hasher);
};

#endif // BITCOIN_EQUIHASH_H
