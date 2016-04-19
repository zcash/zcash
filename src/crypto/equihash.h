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
typedef uint8_t eh_trunc;

struct invalid_params { };

class StepRow
{
protected:
    unsigned char* hash;
    unsigned int len;

public:
    StepRow(unsigned int n, const eh_HashState& base_state, eh_index i);
    ~StepRow();

    StepRow(const StepRow& a);

    bool IsZero();
    std::string GetHex() { return HexStr(hash, hash+len); }

    friend inline bool operator==(const StepRow& a, const StepRow& b) { return memcmp(a.hash, b.hash, a.len) == 0; }
    friend inline bool operator<(const StepRow& a, const StepRow& b) { return memcmp(a.hash, b.hash, a.len) < 0; }

    friend bool HasCollision(StepRow& a, StepRow& b, int l);
};

bool HasCollision(StepRow& a, StepRow& b, int l);

class FullStepRow : public StepRow
{
private:
    std::vector<eh_index> indices;

public:
    FullStepRow(unsigned int n, const eh_HashState& base_state, eh_index i);
    ~FullStepRow() { }

    FullStepRow(const FullStepRow& a) : StepRow {a}, indices(a.indices) { }
    FullStepRow& operator=(const FullStepRow& a);
    FullStepRow& operator^=(const FullStepRow& a);

    void TrimHash(int l);
    bool IndicesBefore(const FullStepRow& a) { return indices[0] < a.indices[0]; }
    std::vector<eh_index> GetSolution() { return std::vector<eh_index>(indices); }

    friend inline const FullStepRow operator^(const FullStepRow& a, const FullStepRow& b) {
        if (a.indices[0] < b.indices[0]) { return FullStepRow(a) ^= b; }
        else { return FullStepRow(b) ^= a; }
    }

    friend bool DistinctIndices(const FullStepRow& a, const FullStepRow& b);
    friend bool IsValidBranch(const FullStepRow& a, const unsigned int ilen, const eh_trunc t);
};

bool DistinctIndices(const FullStepRow& a, const FullStepRow& b);
bool IsValidBranch(const FullStepRow& a, const unsigned int ilen, const eh_trunc t);

class TruncatedStepRow : public StepRow
{
private:
    std::vector<eh_trunc> indices;

public:
    TruncatedStepRow(unsigned int n, const eh_HashState& base_state, eh_index i, unsigned int ilen);
    ~TruncatedStepRow() { }

    TruncatedStepRow(const TruncatedStepRow& a) : StepRow {a}, indices(a.indices) { }
    TruncatedStepRow& operator=(const TruncatedStepRow& a);
    TruncatedStepRow& operator^=(const TruncatedStepRow& a);

    bool IndicesBefore(const TruncatedStepRow& a) { return indices[0] < a.indices[0]; }
    std::vector<eh_trunc> GetPartialSolution() { return std::vector<eh_trunc>(indices); }

    friend inline const TruncatedStepRow operator^(const TruncatedStepRow& a, const TruncatedStepRow& b) {
        if (a.indices[0] < b.indices[0]) { return TruncatedStepRow(a) ^= b; }
        else { return TruncatedStepRow(b) ^= a; }
    }
};

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
    std::set<std::vector<eh_index>> OptimisedSolve(const eh_HashState& base_state);
    bool IsValidSolution(const eh_HashState& base_state, std::vector<eh_index> soln);
};

#endif // BITCOIN_EQUIHASH_H
