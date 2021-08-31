// Copyright (c) 2016 Jack Grigg
// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_CRYPTO_EQUIHASH_H
#define ZCASH_CRYPTO_EQUIHASH_H

#include <memory>
#include <vector>

inline constexpr size_t equihash_solution_size(unsigned int N, unsigned int K) {
    return (1 << K)*(N/(K+1)+1)/8;
}

typedef uint32_t eh_index;
typedef uint8_t eh_trunc;

std::vector<unsigned char> GetMinimalFromIndices(std::vector<eh_index> indices,
                                                 size_t cBitLen);
void CompressArray(const unsigned char* in, size_t in_len,
                   unsigned char* out, size_t out_len,
                   size_t bit_len, size_t byte_pad=0);
void ExpandArray(const unsigned char* in, size_t in_len,
                 unsigned char* out, size_t out_len,
                 size_t bit_len, size_t byte_pad=0);
void EhIndexToArray(const eh_index i, unsigned char* array);


#ifdef ENABLE_MINING

#include "crypto/sha256.h"
#include "utilstrencodings.h"

#include <cstring>
#include <exception>
#include <stdexcept>
#include <functional>
#include <set>

#include <rust/blake2b.h>

struct eh_HashState {
    std::unique_ptr<BLAKE2bState, decltype(&blake2b_free)> inner;

    eh_HashState() : inner(nullptr, blake2b_free) {}

    eh_HashState(size_t length, unsigned char personalization[BLAKE2bPersonalBytes]) : inner(blake2b_init(length, personalization), blake2b_free) {}

    eh_HashState(eh_HashState&& baseState) : inner(std::move(baseState.inner)) {}
    eh_HashState(const eh_HashState& baseState) : inner(blake2b_clone(baseState.inner.get()), blake2b_free) {}
    eh_HashState& operator=(eh_HashState&& baseState)
    {
        if (this != &baseState) {
            inner = std::move(baseState.inner);
        }
        return *this;
    }
    eh_HashState& operator=(const eh_HashState& baseState)
    {
        if (this != &baseState) {
            inner.reset(blake2b_clone(baseState.inner.get()));
        }
        return *this;
    }

    void Update(const unsigned char *input, size_t inputLen);
    void Finalize(unsigned char *hash, size_t hLen);
};

eh_index ArrayToEhIndex(const unsigned char* array);
eh_trunc TruncateIndex(const eh_index i, const unsigned int ilen);

std::vector<eh_index> GetIndicesFromMinimal(std::vector<unsigned char> minimal,
                                            size_t cBitLen);

template<size_t WIDTH>
class StepRow
{
    template<size_t W>
    friend class StepRow;
    friend class CompareSR;

protected:
    unsigned char hash[WIDTH];

public:
    StepRow(const unsigned char* hashIn, size_t hInLen,
            size_t hLen, size_t cBitLen);
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
    FullStepRow(const unsigned char* hashIn, size_t hInLen,
                size_t hLen, size_t cBitLen, eh_index i);
    ~FullStepRow() { }

    FullStepRow(const FullStepRow<WIDTH>& a) : StepRow<WIDTH> {a} { }
    template<size_t W>
    FullStepRow(const FullStepRow<W>& a, const FullStepRow<W>& b, size_t len, size_t lenIndices, int lenTrim);
    FullStepRow& operator=(const FullStepRow<WIDTH>& a);

    inline bool IndicesBefore(const FullStepRow<WIDTH>& a, size_t len, size_t lenIndices) const { return memcmp(hash+len, a.hash+len, lenIndices) < 0; }
    std::vector<unsigned char> GetIndices(size_t len, size_t lenIndices,
                                          size_t cBitLen) const;

    template<size_t W>
    friend bool DistinctIndices(const FullStepRow<W>& a, const FullStepRow<W>& b,
                                size_t len, size_t lenIndices);
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
    TruncatedStepRow(const unsigned char* hashIn, size_t hInLen,
                     size_t hLen, size_t cBitLen,
                     eh_index i, unsigned int ilen);
    ~TruncatedStepRow() { }

    TruncatedStepRow(const TruncatedStepRow<WIDTH>& a) : StepRow<WIDTH> {a} { }
    template<size_t W>
    TruncatedStepRow(const TruncatedStepRow<W>& a, const TruncatedStepRow<W>& b, size_t len, size_t lenIndices, int lenTrim);
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
    static_assert(K < N);
    static_assert(N % 8 == 0);
    static_assert((N/(K+1)) + 1 < 8*sizeof(eh_index));

public:
    enum : size_t { IndicesPerHashOutput=512/N };
    enum : size_t { HashOutput=IndicesPerHashOutput*N/8 };
    enum : size_t { CollisionBitLength=N/(K+1) };
    enum : size_t { CollisionByteLength=(CollisionBitLength+7)/8 };
    enum : size_t { HashLength=(K+1)*CollisionByteLength };
    enum : size_t { FullWidth=2*CollisionByteLength+sizeof(eh_index)*(1 << (K-1)) };
    enum : size_t { FinalFullWidth=2*CollisionByteLength+sizeof(eh_index)*(1 << (K)) };
    enum : size_t { TruncatedWidth=max(HashLength+sizeof(eh_trunc), 2*CollisionByteLength+sizeof(eh_trunc)*(1 << (K-1))) };
    enum : size_t { FinalTruncatedWidth=max(HashLength+sizeof(eh_trunc), 2*CollisionByteLength+sizeof(eh_trunc)*(1 << (K))) };
    enum : size_t { SolutionWidth=(1 << K)*(CollisionBitLength+1)/8 };

    Equihash() { }

    void InitialiseState(eh_HashState& base_state);
    bool BasicSolve(const eh_HashState& base_state,
                    const std::function<bool(std::vector<unsigned char>)> validBlock,
                    const std::function<bool(EhSolverCancelCheck)> cancelled);
    bool OptimisedSolve(const eh_HashState& base_state,
                        const std::function<bool(std::vector<unsigned char>)> validBlock,
                        const std::function<bool(EhSolverCancelCheck)> cancelled);
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
                    const std::function<bool(std::vector<unsigned char>)> validBlock,
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
                    const std::function<bool(std::vector<unsigned char>)> validBlock)
{
    return EhBasicSolve(n, k, base_state, validBlock,
                        [](EhSolverCancelCheck pos) { return false; });
}

inline bool EhOptimisedSolve(unsigned int n, unsigned int k, const eh_HashState& base_state,
                    const std::function<bool(std::vector<unsigned char>)> validBlock,
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
                    const std::function<bool(std::vector<unsigned char>)> validBlock)
{
    return EhOptimisedSolve(n, k, base_state, validBlock,
                            [](EhSolverCancelCheck pos) { return false; });
}
#endif // ENABLE_MINING

#endif // ZCASH_CRYPTO_EQUIHASH_H
