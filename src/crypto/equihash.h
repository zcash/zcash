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

eh_index ArrayToEhIndex(const unsigned char* array);
eh_trunc TruncateIndex(const eh_index i, const unsigned int ilen);

template<size_t WIDTH>
class StepRow
{
    template<size_t W>
    friend class StepRow;
    friend class CompareSR;

protected:
    unsigned char hash[WIDTH];

public:
    StepRow(unsigned int n, const eh_HashState& base_state, eh_index i);
    ~StepRow() { }

    template<size_t W>
    StepRow(const StepRow<W>& a);

    bool IsZero(size_t len);
    std::string GetHex(size_t len) { return HexStr(hash, hash+len); }

    template<size_t W>
    friend bool HasCollision(StepRow<W>& a, StepRow<W>& b, int l);
};

class CompareSR
{
private:
    size_t len;

public:
    CompareSR(size_t l) : len {l} { }

    template<size_t W>
    inline bool operator()(const StepRow<W>& a, const StepRow<W>& b) { return memcmp(a.hash, b.hash, len) < 0; }
};

template<size_t WIDTH>
bool HasCollision(StepRow<WIDTH>& a, StepRow<WIDTH>& b, int l);

template<size_t WIDTH>
class FullStepRow : public StepRow<WIDTH>
{
    template<size_t W>
    friend class FullStepRow;

    using StepRow<WIDTH>::hash;

public:
    FullStepRow(unsigned int n, const eh_HashState& base_state, eh_index i);
    ~FullStepRow() { }

    FullStepRow(const FullStepRow<WIDTH>& a) : StepRow<WIDTH> {a} { }
    template<size_t W>
    FullStepRow(const FullStepRow<W>& a, const FullStepRow<W>& b, size_t len, size_t lenIndices, int trim);
    FullStepRow& operator=(const FullStepRow<WIDTH>& a);

    inline bool IndicesBefore(const FullStepRow<WIDTH>& a, size_t len, size_t lenIndices) const { return memcmp(hash+len, a.hash+len, lenIndices) < 0; }
    std::vector<eh_index> GetIndices(size_t len, size_t lenIndices) const;

    template<size_t W>
    friend bool IsValidBranch(const FullStepRow<W>& a, const size_t len, const unsigned int ilen, const eh_trunc t);
};

template<size_t WIDTH>
class TruncatedStepRow : public StepRow<WIDTH>
{
    template<size_t W>
    friend class TruncatedStepRow;

    using StepRow<WIDTH>::hash;

public:
    TruncatedStepRow(unsigned int n, const eh_HashState& base_state, eh_index i, unsigned int ilen);
    ~TruncatedStepRow() { }

    TruncatedStepRow(const TruncatedStepRow<WIDTH>& a) : StepRow<WIDTH> {a} { }
    template<size_t W>
    TruncatedStepRow(const TruncatedStepRow<W>& a, const TruncatedStepRow<W>& b, size_t len, size_t lenIndices, int trim);
    TruncatedStepRow& operator=(const TruncatedStepRow<WIDTH>& a);

    inline bool IndicesBefore(const TruncatedStepRow<WIDTH>& a, size_t len, size_t lenIndices) const { return memcmp(hash+len, a.hash+len, lenIndices) < 0; }
    eh_trunc* GetTruncatedIndices(size_t len, size_t lenIndices) const;
};

inline constexpr const size_t max(const size_t A, const size_t B) { return A > B ? A : B; }

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
    enum : size_t { FullWidth=2*CollisionByteLength+sizeof(eh_index)*(1 << (K-1)) };
    enum : size_t { FinalFullWidth=2*CollisionByteLength+sizeof(eh_index)*(1 << (K)) };
    enum : size_t { TruncatedWidth=max((N/8)+sizeof(eh_trunc), 2*CollisionByteLength+sizeof(eh_trunc)*(1 << (K-1))) };
    enum : size_t { FinalTruncatedWidth=max((N/8)+sizeof(eh_trunc), 2*CollisionByteLength+sizeof(eh_trunc)*(1 << (K))) };

    Equihash() { }

    int InitialiseState(eh_HashState& base_state);
    std::set<std::vector<eh_index>> BasicSolve(const eh_HashState& base_state);
    std::set<std::vector<eh_index>> OptimisedSolve(const eh_HashState& base_state);
    bool IsValidSolution(const eh_HashState& base_state, std::vector<eh_index> soln);
};

#include "equihash.tcc"

static Equihash<96,3> Eh96_3;
static Equihash<96,5> Eh96_5;
static Equihash<48,5> Eh48_5;

#define EhInitialiseState(n, k, base_state)  \
    if (n == 96 && k == 3) {                 \
        Eh96_3.InitialiseState(base_state);  \
    } else if (n == 96 && k == 5) {          \
        Eh96_5.InitialiseState(base_state);  \
    } else if (n == 48 && k == 5) {          \
        Eh48_5.InitialiseState(base_state);  \
    } else {                                 \
        throw std::invalid_argument("Unsupported Equihash parameters"); \
    }

#define EhBasicSolve(n, k, base_state, solns)   \
    if (n == 96 && k == 3) {                    \
        solns = Eh96_3.BasicSolve(base_state);  \
    } else if (n == 96 && k == 5) {             \
        solns = Eh96_5.BasicSolve(base_state);  \
    } else if (n == 48 && k == 5) {             \
        solns = Eh48_5.BasicSolve(base_state);  \
    } else {                                    \
        throw std::invalid_argument("Unsupported Equihash parameters"); \
    }

#define EhOptimisedSolve(n, k, base_state, solns)   \
    if (n == 96 && k == 3) {                        \
        solns = Eh96_3.OptimisedSolve(base_state);  \
    } else if (n == 96 && k == 5) {                 \
        solns = Eh96_5.OptimisedSolve(base_state);  \
    } else if (n == 48 && k == 5) {                 \
        solns = Eh48_5.OptimisedSolve(base_state);  \
    } else {                                        \
        throw std::invalid_argument("Unsupported Equihash parameters"); \
    }

#define EhIsValidSolution(n, k, base_state, soln, ret)   \
    if (n == 96 && k == 3) {                             \
        ret = Eh96_3.IsValidSolution(base_state, soln);  \
    } else if (n == 96 && k == 5) {                      \
        ret = Eh96_5.IsValidSolution(base_state, soln);  \
    } else if (n == 48 && k == 5) {                      \
        ret = Eh48_5.IsValidSolution(base_state, soln);  \
    } else {                                             \
        throw std::invalid_argument("Unsupported Equihash parameters"); \
    }

#endif // BITCOIN_EQUIHASH_H
