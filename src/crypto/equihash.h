// Copyright (c) 2016 Jack Grigg
// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EQUIHASH_H
#define BITCOIN_EQUIHASH_H

#include "crypto/sha256.h"
#include "utilstrencodings.h"

#include "sodium.h"

#include <cstring>
#include <set>
#include <vector>

typedef crypto_generichash_blake2b_state eh_HashState;
typedef uint32_t eh_index;

struct invalid_params { };

class StepRow
{
private:
    unsigned char* hash;
    unsigned int len;
    std::vector<eh_index> indices;

public:
    StepRow(unsigned int n, const eh_HashState& base_state, eh_index i);
    ~StepRow();

    StepRow(const StepRow& a);
    StepRow& operator=(const StepRow& a);
    StepRow& operator^=(const StepRow& a);

    void TrimHash(int l);
    bool IsZero();
    bool IndicesBefore(const StepRow& a) { return indices[0] < a.indices[0]; }
    std::vector<eh_index> GetSolution() { return std::vector<eh_index>(indices); }
    std::string GetHex() { return HexStr(hash, hash+len); }

    friend inline const StepRow operator^(const StepRow& a, const StepRow& b) {
        if (a.indices[0] < b.indices[0]) { return StepRow(a) ^= b; }
        else { return StepRow(b) ^= a; }
    }
    friend inline bool operator==(const StepRow& a, const StepRow& b) { return memcmp(a.hash, b.hash, a.len) == 0; }
    friend inline bool operator<(const StepRow& a, const StepRow& b) { return memcmp(a.hash, b.hash, a.len) < 0; }

    friend bool HasCollision(StepRow& a, StepRow& b, int l);
    friend bool DistinctIndices(const StepRow& a, const StepRow& b);
};

bool HasCollision(StepRow& a, StepRow& b, int l);
bool DistinctIndices(const StepRow& a, const StepRow& b);

class Equihash
{
private:
    unsigned int n;
    unsigned int k;

public:
    Equihash(unsigned int n, unsigned int k);

    inline unsigned int CollisionBitLength() { return n/(k+1); }
    inline unsigned int CollisionByteLength() { return CollisionBitLength()/8; }

    int InitialiseState(eh_HashState& base_state);
    std::set<std::vector<eh_index>> BasicSolve(const eh_HashState& base_state);
    bool IsValidSolution(const eh_HashState& base_state, std::vector<eh_index> soln);
};

#endif // BITCOIN_EQUIHASH_H
