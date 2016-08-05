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
#include <exception>
#include <functional>
#include <memory>
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
    StepRow(const eh_HashState& base_state, eh_index i,
            size_t hLen, size_t cBitLen, size_t cByteLen);
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
    FullStepRow(const eh_HashState& base_state, eh_index i,
                size_t hLen, size_t cBitLen, size_t cByteLen);
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
    TruncatedStepRow(const eh_HashState& base_state, eh_index i,
                     size_t hLen, size_t cBitLen, size_t cByteLen,
                     unsigned int ilen);
    ~TruncatedStepRow() { }

    TruncatedStepRow(const TruncatedStepRow<WIDTH>& a) : StepRow<WIDTH> {a} { }
    template<size_t W>
    TruncatedStepRow(const TruncatedStepRow<W>& a, const TruncatedStepRow<W>& b, size_t len, size_t lenIndices, int trim);
    TruncatedStepRow& operator=(const TruncatedStepRow<WIDTH>& a);

    inline bool IndicesBefore(const TruncatedStepRow<WIDTH>& a, size_t len, size_t lenIndices) const { return memcmp(hash+len, a.hash+len, lenIndices) < 0; }
    std::shared_ptr<eh_trunc> GetTruncatedIndices(size_t len, size_t lenIndices) const;
};

enum EhSolverCancelCheck
{
    ListGeneration,
    ListSorting,
    ListColliding,
    RoundEnd,
    FinalSorting,
    FinalColliding,
    PartialGeneration,
    PartialSorting,
    PartialSubtreeEnd,
    PartialIndexEnd,
    PartialEnd
};

class EhSolverCancelledException : public std::exception
{
    virtual const char* what() const throw() {
        return "Equihash solver was cancelled";
    }
};

inline constexpr const size_t max(const size_t A, const size_t B) { return A > B ? A : B; }

template<unsigned int N, unsigned int K>
class Equihash
{
private:
    BOOST_STATIC_ASSERT(K < N);
    BOOST_STATIC_ASSERT(N % 8 == 0);
    BOOST_STATIC_ASSERT((N/(K+1)) + 1 < 8*sizeof(eh_index));

public:
    enum { CollisionBitLength=N/(K+1) };
    enum : size_t { CollisionByteLength=(CollisionBitLength+7)/8 };
    enum : size_t { HashLength=(K+1)*CollisionByteLength };
    enum : size_t { FullWidth=2*CollisionByteLength+sizeof(eh_index)*(1 << (K-1)) };
    enum : size_t { FinalFullWidth=2*CollisionByteLength+sizeof(eh_index)*(1 << (K)) };
    enum : size_t { TruncatedWidth=max(HashLength+sizeof(eh_trunc), 2*CollisionByteLength+sizeof(eh_trunc)*(1 << (K-1))) };
    enum : size_t { FinalTruncatedWidth=max(HashLength+sizeof(eh_trunc), 2*CollisionByteLength+sizeof(eh_trunc)*(1 << (K))) };

    Equihash() { }

    int InitialiseState(eh_HashState& base_state);
    bool BasicSolve(const eh_HashState& base_state,
                    const std::function<bool(std::vector<eh_index>)> validBlock,
                    const std::function<bool(EhSolverCancelCheck)> cancelled);
    bool OptimisedSolve(const eh_HashState& base_state,
                        const std::function<bool(std::vector<eh_index>)> validBlock,
                        const std::function<bool(EhSolverCancelCheck)> cancelled);
    bool IsValidSolution(const eh_HashState& base_state, std::vector<eh_index> soln);
};

#include "equihash.tcc"

static Equihash<96,3> Eh96_3;
static Equihash<200,9> Eh200_9;
static Equihash<96,5> Eh96_5;
static Equihash<48,5> Eh48_5;

#define EhInitialiseState(n, k, base_state)  \
    if (n == 96 && k == 3) {                 \
        Eh96_3.InitialiseState(base_state);  \
    } else if (n == 200 && k == 9) {         \
        Eh200_9.InitialiseState(base_state); \
    } else if (n == 96 && k == 5) {          \
        Eh96_5.InitialiseState(base_state);  \
    } else if (n == 48 && k == 5) {          \
        Eh48_5.InitialiseState(base_state);  \
    } else {                                 \
        throw std::invalid_argument("Unsupported Equihash parameters"); \
    }

inline bool EhBasicSolve(unsigned int n, unsigned int k, const eh_HashState& base_state,
                    const std::function<bool(std::vector<eh_index>)> validBlock,
                    const std::function<bool(EhSolverCancelCheck)> cancelled)
{
    if (n == 96 && k == 3) {
        return Eh96_3.BasicSolve(base_state, validBlock, cancelled);
    } else if (n == 200 && k == 9) {
        return Eh200_9.BasicSolve(base_state, validBlock, cancelled);
    } else if (n == 96 && k == 5) {
        return Eh96_5.BasicSolve(base_state, validBlock, cancelled);
    } else if (n == 48 && k == 5) {
        return Eh48_5.BasicSolve(base_state, validBlock, cancelled);
    } else {
        throw std::invalid_argument("Unsupported Equihash parameters");
    }
}

inline bool EhBasicSolveUncancellable(unsigned int n, unsigned int k, const eh_HashState& base_state,
                    const std::function<bool(std::vector<eh_index>)> validBlock)
{
    return EhBasicSolve(n, k, base_state, validBlock,
                        [](EhSolverCancelCheck pos) { return false; });
}

inline bool EhOptimisedSolve(unsigned int n, unsigned int k, const eh_HashState& base_state,
                    const std::function<bool(std::vector<eh_index>)> validBlock,
                    const std::function<bool(EhSolverCancelCheck)> cancelled)
{
    if (n == 96 && k == 3) {
        return Eh96_3.OptimisedSolve(base_state, validBlock, cancelled);
    } else if (n == 200 && k == 9) {
        return Eh200_9.OptimisedSolve(base_state, validBlock, cancelled);
    } else if (n == 96 && k == 5) {
        return Eh96_5.OptimisedSolve(base_state, validBlock, cancelled);
    } else if (n == 48 && k == 5) {
        return Eh48_5.OptimisedSolve(base_state, validBlock, cancelled);
    } else {
        throw std::invalid_argument("Unsupported Equihash parameters");
    }
}

inline bool EhOptimisedSolveUncancellable(unsigned int n, unsigned int k, const eh_HashState& base_state,
                    const std::function<bool(std::vector<eh_index>)> validBlock)
{
    return EhOptimisedSolve(n, k, base_state, validBlock,
                            [](EhSolverCancelCheck pos) { return false; });
}

#define EhIsValidSolution(n, k, base_state, soln, ret)   \
    if (n == 96 && k == 3) {                             \
        ret = Eh96_3.IsValidSolution(base_state, soln);  \
    } else if (n == 200 && k == 9) {                     \
        ret = Eh200_9.IsValidSolution(base_state, soln); \
    } else if (n == 96 && k == 5) {                      \
        ret = Eh96_5.IsValidSolution(base_state, soln);  \
    } else if (n == 48 && k == 5) {                      \
        ret = Eh48_5.IsValidSolution(base_state, soln);  \
    } else {                                             \
        throw std::invalid_argument("Unsupported Equihash parameters"); \
    }

#endif // BITCOIN_EQUIHASH_H
