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

#include <boost/static_assert.hpp>

typedef crypto_generichash_blake2b_state eh_HashState;
typedef uint32_t eh_index;
typedef uint8_t eh_trunc;

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
    unsigned int lenIndices;

public:
    TruncatedStepRow(unsigned int n, const eh_HashState& base_state, eh_index i, unsigned int ilen);
    ~TruncatedStepRow() { }

    TruncatedStepRow(const TruncatedStepRow& a);
    TruncatedStepRow& operator=(const TruncatedStepRow& a);
    TruncatedStepRow& operator^=(const TruncatedStepRow& a);

    void TrimHash(int l);
    inline bool IndicesBefore(const TruncatedStepRow& a) const { return memcmp(hash+len, a.hash+a.len, lenIndices) < 0; }
    eh_trunc* GetPartialSolution(eh_index soln_size) const;

    friend inline const TruncatedStepRow operator^(const TruncatedStepRow& a, const TruncatedStepRow& b) {
        if (a.IndicesBefore(b)) { return TruncatedStepRow(a) ^= b; }
        else { return TruncatedStepRow(b) ^= a; }
    }
};

template<unsigned int N, unsigned int K>
class Equihash
{
private:
    BOOST_STATIC_ASSERT(K < N);
    BOOST_STATIC_ASSERT(N % 8 == 0);
    BOOST_STATIC_ASSERT((N/(K+1)) % 8 == 0);
    BOOST_STATIC_ASSERT((N/(K+1)) + 1 < 8*sizeof(eh_index));

public:
    enum { CollisionBitLength=N/(K+1) };
    enum { CollisionByteLength=CollisionBitLength/8 };

    Equihash() { }

    int InitialiseState(eh_HashState& base_state);
    std::set<std::vector<eh_index>> BasicSolve(const eh_HashState& base_state);
    std::set<std::vector<eh_index>> OptimisedSolve(const eh_HashState& base_state);
    bool IsValidSolution(const eh_HashState& base_state, std::vector<eh_index> soln);
};

static Equihash<96,5> Eh965;
static Equihash<48,5> Eh485;

#define EhInitialiseState(n, k, base_state) \
    if (n == 96 && k == 5) {                \
        Eh965.InitialiseState(base_state);  \
    } else if (n == 48 && k == 5) {         \
        Eh485.InitialiseState(base_state);  \
    } else {                                \
        throw std::invalid_argument("Unsupported Equihash parameters"); \
    }

#define EhBasicSolve(n, k, base_state, solns) \
    if (n == 96 && k == 5) {                  \
        solns = Eh965.BasicSolve(base_state); \
    } else if (n == 48 && k == 5) {           \
        solns = Eh485.BasicSolve(base_state); \
    } else {                                  \
        throw std::invalid_argument("Unsupported Equihash parameters"); \
    }

#define EhOptimisedSolve(n, k, base_state, solns) \
    if (n == 96 && k == 5) {                      \
        solns = Eh965.OptimisedSolve(base_state); \
    } else if (n == 48 && k == 5) {               \
        solns = Eh485.OptimisedSolve(base_state); \
    } else {                                      \
        throw std::invalid_argument("Unsupported Equihash parameters"); \
    }

#define EhIsValidSolution(n, k, base_state, soln, ret) \
    if (n == 96 && k == 5) {                           \
        ret = Eh965.IsValidSolution(base_state, soln); \
    } else if (n == 48 && k == 5) {                    \
        ret = Eh485.IsValidSolution(base_state, soln); \
    } else {                                           \
        throw std::invalid_argument("Unsupported Equihash parameters"); \
    }

#endif // BITCOIN_EQUIHASH_H
