// Copyright (c) 2016 Jack Grigg
// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

// Implementation of the Equihash Proof-of-Work algorithm.
//
// Reference
// =========
// Alex Biryukov and Dmitry Khovratovich
// Equihash: Asymmetric Proof-of-Work Based on the Generalized Birthday Problem
// NDSS ’16, 21-24 February 2016, San Diego, CA, USA
// https://www.internetsociety.org/sites/default/files/blogs-media/equihash-asymmetric-proof-of-work-based-generalized-birthday-problem.pdf

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "compat/endian.h"
#include "crypto/equihash.h"
#include "util.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include <boost/optional.hpp>

static EhSolverCancelledException solver_cancelled;

template<unsigned int N, unsigned int K>
int Equihash<N,K>::InitialiseState(eh_HashState& base_state)
{
    uint32_t le_N = htole32(N);
    uint32_t le_K = htole32(K);
    unsigned char personalization[crypto_generichash_blake2b_PERSONALBYTES] = {};
    memcpy(personalization, "ZcashPoW", 8);
    memcpy(personalization+8,  &le_N, 4);
    memcpy(personalization+12, &le_K, 4);
    return crypto_generichash_blake2b_init_salt_personal(&base_state,
                                                         NULL, 0, // No key.
                                                         (512/N)*N/8,
                                                         NULL,    // No salt.
                                                         personalization);
}

void GenerateHash(const eh_HashState& base_state, eh_index g,
                  unsigned char* hash, size_t hLen)
{
    eh_HashState state;
    state = base_state;
    eh_index lei = htole32(g);
    crypto_generichash_blake2b_update(&state, (const unsigned char*) &lei,
                                      sizeof(eh_index));
    crypto_generichash_blake2b_final(&state, hash, hLen);
}

void ExpandArray(const unsigned char* in, size_t in_len,
                 unsigned char* out, size_t out_len,
                 size_t bit_len, size_t byte_pad)
{
    assert(bit_len >= 8);
    assert(8*sizeof(uint32_t) >= 7+bit_len);

    size_t out_width { (bit_len+7)/8 + byte_pad };
    assert(out_len == 8*out_width*in_len/bit_len);

    uint32_t bit_len_mask { ((uint32_t)1 << bit_len) - 1 };

    // The acc_bits least-significant bits of acc_value represent a bit sequence
    // in big-endian order.
    size_t acc_bits = 0;
    uint32_t acc_value = 0;

    size_t j = 0;
    for (size_t i = 0; i < in_len; i++) {
        acc_value = (acc_value << 8) | in[i];
        acc_bits += 8;

        // When we have bit_len or more bits in the accumulator, write the next
        // output element.
        if (acc_bits >= bit_len) {
            acc_bits -= bit_len;
            for (size_t x = 0; x < byte_pad; x++) {
                out[j+x] = 0;
            }
            for (size_t x = byte_pad; x < out_width; x++) {
                out[j+x] = (
                    // Big-endian
                    acc_value >> (acc_bits+(8*(out_width-x-1)))
                ) & (
                    // Apply bit_len_mask across byte boundaries
                    (bit_len_mask >> (8*(out_width-x-1))) & 0xFF
                );
            }
            j += out_width;
        }
    }
}

void CompressArray(const unsigned char* in, size_t in_len,
                   unsigned char* out, size_t out_len,
                   size_t bit_len, size_t byte_pad)
{
    assert(bit_len >= 8);
    assert(8*sizeof(uint32_t) >= 7+bit_len);

    size_t in_width { (bit_len+7)/8 + byte_pad };
    assert(out_len == bit_len*in_len/(8*in_width));

    uint32_t bit_len_mask { ((uint32_t)1 << bit_len) - 1 };

    // The acc_bits least-significant bits of acc_value represent a bit sequence
    // in big-endian order.
    size_t acc_bits = 0;
    uint32_t acc_value = 0;

    size_t j = 0;
    for (size_t i = 0; i < out_len; i++) {
        // When we have fewer than 8 bits left in the accumulator, read the next
        // input element.
        if (acc_bits < 8) {
            acc_value = acc_value << bit_len;
            for (size_t x = byte_pad; x < in_width; x++) {
                acc_value = acc_value | (
                    (
                        // Apply bit_len_mask across byte boundaries
                        in[j+x] & ((bit_len_mask >> (8*(in_width-x-1))) & 0xFF)
                    ) << (8*(in_width-x-1))); // Big-endian
            }
            j += in_width;
            acc_bits += bit_len;
        }

        acc_bits -= 8;
        out[i] = (acc_value >> acc_bits) & 0xFF;
    }
}

// Big-endian so that lexicographic array comparison is equivalent to integer
// comparison
void EhIndexToArray(const eh_index i, unsigned char* array)
{
    BOOST_STATIC_ASSERT(sizeof(eh_index) == 4);
    eh_index bei = htobe32(i);
    memcpy(array, &bei, sizeof(eh_index));
}

// Big-endian so that lexicographic array comparison is equivalent to integer
// comparison
eh_index ArrayToEhIndex(const unsigned char* array)
{
    BOOST_STATIC_ASSERT(sizeof(eh_index) == 4);
    eh_index bei;
    memcpy(&bei, array, sizeof(eh_index));
    return be32toh(bei);
}

eh_trunc TruncateIndex(const eh_index i, const unsigned int ilen)
{
    // Truncate to 8 bits
    BOOST_STATIC_ASSERT(sizeof(eh_trunc) == 1);
    return (i >> (ilen - 8)) & 0xff;
}

eh_index UntruncateIndex(const eh_trunc t, const eh_index r, const unsigned int ilen)
{
    eh_index i{t};
    return (i << (ilen - 8)) | r;
}

std::vector<eh_index> GetIndicesFromMinimal(std::vector<unsigned char> minimal,
                                            size_t cBitLen)
{
    assert(((cBitLen+1)+7)/8 <= sizeof(eh_index));
    size_t lenIndices { 8*sizeof(eh_index)*minimal.size()/(cBitLen+1) };
    size_t bytePad { sizeof(eh_index) - ((cBitLen+1)+7)/8 };
    std::vector<unsigned char> array(lenIndices);
    ExpandArray(minimal.data(), minimal.size(),
                array.data(), lenIndices, cBitLen+1, bytePad);
    std::vector<eh_index> ret;
    for (int i = 0; i < lenIndices; i += sizeof(eh_index)) {
        ret.push_back(ArrayToEhIndex(array.data()+i));
    }
    return ret;
}

std::vector<unsigned char> GetMinimalFromIndices(std::vector<eh_index> indices,
                                                 size_t cBitLen)
{
    assert(((cBitLen+1)+7)/8 <= sizeof(eh_index));
    size_t lenIndices { indices.size()*sizeof(eh_index) };
    size_t minLen { (cBitLen+1)*lenIndices/(8*sizeof(eh_index)) };
    size_t bytePad { sizeof(eh_index) - ((cBitLen+1)+7)/8 };
    std::vector<unsigned char> array(lenIndices);
    for (int i = 0; i < indices.size(); i++) {
        EhIndexToArray(indices[i], array.data()+(i*sizeof(eh_index)));
    }
    std::vector<unsigned char> ret(minLen);
    CompressArray(array.data(), lenIndices,
                  ret.data(), minLen, cBitLen+1, bytePad);
    return ret;
}

template<size_t WIDTH>
StepRow<WIDTH>::StepRow(const unsigned char* hashIn, size_t hInLen,
                        size_t hLen, size_t cBitLen)
{
    assert(hLen <= WIDTH);
    ExpandArray(hashIn, hInLen, hash, hLen, cBitLen);
}

template<size_t WIDTH> template<size_t W>
StepRow<WIDTH>::StepRow(const StepRow<W>& a)
{
    BOOST_STATIC_ASSERT(W <= WIDTH);
    std::copy(a.hash, a.hash+W, hash);
}

template<size_t WIDTH>
FullStepRow<WIDTH>::FullStepRow(const unsigned char* hashIn, size_t hInLen,
                                size_t hLen, size_t cBitLen, eh_index i) :
        StepRow<WIDTH> {hashIn, hInLen, hLen, cBitLen}
{
    EhIndexToArray(i, hash+hLen);
}

template<size_t WIDTH> template<size_t W>
FullStepRow<WIDTH>::FullStepRow(const FullStepRow<W>& a, const FullStepRow<W>& b, size_t len, size_t lenIndices, int trim) :
        StepRow<WIDTH> {a}
{
    assert(len+lenIndices <= W);
    assert(len-trim+(2*lenIndices) <= WIDTH);
    for (int i = trim; i < len; i++)
        hash[i-trim] = a.hash[i] ^ b.hash[i];
    if (a.IndicesBefore(b, len, lenIndices)) {
        std::copy(a.hash+len, a.hash+len+lenIndices, hash+len-trim);
        std::copy(b.hash+len, b.hash+len+lenIndices, hash+len-trim+lenIndices);
    } else {
        std::copy(b.hash+len, b.hash+len+lenIndices, hash+len-trim);
        std::copy(a.hash+len, a.hash+len+lenIndices, hash+len-trim+lenIndices);
    }
}

template<size_t WIDTH>
FullStepRow<WIDTH>& FullStepRow<WIDTH>::operator=(const FullStepRow<WIDTH>& a)
{
    std::copy(a.hash, a.hash+WIDTH, hash);
    return *this;
}

template<size_t WIDTH>
bool StepRow<WIDTH>::IsZero(size_t len)
{
    // This doesn't need to be constant time.
    for (int i = 0; i < len; i++) {
        if (hash[i] != 0)
            return false;
    }
    return true;
}

template<size_t WIDTH>
std::vector<unsigned char> FullStepRow<WIDTH>::GetIndices(size_t len, size_t lenIndices,
                                                          size_t cBitLen) const
{
    assert(((cBitLen+1)+7)/8 <= sizeof(eh_index));
    size_t minLen { (cBitLen+1)*lenIndices/(8*sizeof(eh_index)) };
    size_t bytePad { sizeof(eh_index) - ((cBitLen+1)+7)/8 };
    std::vector<unsigned char> ret(minLen);
    CompressArray(hash+len, lenIndices, ret.data(), minLen, cBitLen+1, bytePad);
    return ret;
}

template<size_t WIDTH>
bool HasCollision(StepRow<WIDTH>& a, StepRow<WIDTH>& b, int l)
{
    // This doesn't need to be constant time.
    for (int j = 0; j < l; j++) {
        if (a.hash[j] != b.hash[j])
            return false;
    }
    return true;
}

template<size_t WIDTH>
TruncatedStepRow<WIDTH>::TruncatedStepRow(const unsigned char* hashIn, size_t hInLen,
                                          size_t hLen, size_t cBitLen,
                                          eh_index i, unsigned int ilen) :
        StepRow<WIDTH> {hashIn, hInLen, hLen, cBitLen}
{
    hash[hLen] = TruncateIndex(i, ilen);
}

template<size_t WIDTH> template<size_t W>
TruncatedStepRow<WIDTH>::TruncatedStepRow(const TruncatedStepRow<W>& a, const TruncatedStepRow<W>& b, size_t len, size_t lenIndices, int trim) :
        StepRow<WIDTH> {a}
{
    assert(len+lenIndices <= W);
    assert(len-trim+(2*lenIndices) <= WIDTH);
    for (int i = trim; i < len; i++)
        hash[i-trim] = a.hash[i] ^ b.hash[i];
    if (a.IndicesBefore(b, len, lenIndices)) {
        std::copy(a.hash+len, a.hash+len+lenIndices, hash+len-trim);
        std::copy(b.hash+len, b.hash+len+lenIndices, hash+len-trim+lenIndices);
    } else {
        std::copy(b.hash+len, b.hash+len+lenIndices, hash+len-trim);
        std::copy(a.hash+len, a.hash+len+lenIndices, hash+len-trim+lenIndices);
    }
}

template<size_t WIDTH>
TruncatedStepRow<WIDTH>& TruncatedStepRow<WIDTH>::operator=(const TruncatedStepRow<WIDTH>& a)
{
    std::copy(a.hash, a.hash+WIDTH, hash);
    return *this;
}

template<size_t WIDTH>
std::shared_ptr<eh_trunc> TruncatedStepRow<WIDTH>::GetTruncatedIndices(size_t len, size_t lenIndices) const
{
    std::shared_ptr<eh_trunc> p (new eh_trunc[lenIndices], std::default_delete<eh_trunc[]>());
    std::copy(hash+len, hash+len+lenIndices, p.get());
    return p;
}

#ifdef ENABLE_MINING
template<unsigned int N, unsigned int K>
bool Equihash<N,K>::BasicSolve(const eh_HashState& base_state,
                               const std::function<bool(std::vector<unsigned char>)> validBlock,
                               const std::function<bool(EhSolverCancelCheck)> cancelled)
{
    eh_index init_size { 1 << (CollisionBitLength + 1) };

    // 1) Generate first list
    LogPrint("pow", "Generating first list\n");
    size_t hashLen = HashLength;
    size_t lenIndices = sizeof(eh_index);
    std::vector<FullStepRow<FullWidth>> X;
    X.reserve(init_size);
    unsigned char tmpHash[HashOutput];
    for (eh_index g = 0; X.size() < init_size; g++) {
        GenerateHash(base_state, g, tmpHash, HashOutput);
        for (eh_index i = 0; i < IndicesPerHashOutput && X.size() < init_size; i++) {
            X.emplace_back(tmpHash+(i*N/8), N/8, HashLength,
                           CollisionBitLength, (g*IndicesPerHashOutput)+i);
        }
        if (cancelled(ListGeneration)) throw solver_cancelled;
    }

    // 3) Repeat step 2 until 2n/(k+1) bits remain
    for (int r = 1; r < K && X.size() > 0; r++) {
        LogPrint("pow", "Round %d:\n", r);
        // 2a) Sort the list
        LogPrint("pow", "- Sorting list\n");
        std::sort(X.begin(), X.end(), CompareSR(CollisionByteLength));
        if (cancelled(ListSorting)) throw solver_cancelled;

        LogPrint("pow", "- Finding collisions\n");
        int i = 0;
        int posFree = 0;
        std::vector<FullStepRow<FullWidth>> Xc;
        while (i < X.size() - 1) {
            // 2b) Find next set of unordered pairs with collisions on the next n/(k+1) bits
            int j = 1;
            while (i+j < X.size() &&
                    HasCollision(X[i], X[i+j], CollisionByteLength)) {
                j++;
            }

            // 2c) Calculate tuples (X_i ^ X_j, (i, j))
            for (int l = 0; l < j - 1; l++) {
                for (int m = l + 1; m < j; m++) {
                    if (DistinctIndices(X[i+l], X[i+m], hashLen, lenIndices)) {
                        Xc.emplace_back(X[i+l], X[i+m], hashLen, lenIndices, CollisionByteLength);
                    }
                }
            }

            // 2d) Store tuples on the table in-place if possible
            while (posFree < i+j && Xc.size() > 0) {
                X[posFree++] = Xc.back();
                Xc.pop_back();
            }

            i += j;
            if (cancelled(ListColliding)) throw solver_cancelled;
        }

        // 2e) Handle edge case where final table entry has no collision
        while (posFree < X.size() && Xc.size() > 0) {
            X[posFree++] = Xc.back();
            Xc.pop_back();
        }

        if (Xc.size() > 0) {
            // 2f) Add overflow to end of table
            X.insert(X.end(), Xc.begin(), Xc.end());
        } else if (posFree < X.size()) {
            // 2g) Remove empty space at the end
            X.erase(X.begin()+posFree, X.end());
            X.shrink_to_fit();
        }

        hashLen -= CollisionByteLength;
        lenIndices *= 2;
        if (cancelled(RoundEnd)) throw solver_cancelled;
    }

    // k+1) Find a collision on last 2n(k+1) bits
    LogPrint("pow", "Final round:\n");
    if (X.size() > 1) {
        LogPrint("pow", "- Sorting list\n");
        std::sort(X.begin(), X.end(), CompareSR(hashLen));
        if (cancelled(FinalSorting)) throw solver_cancelled;
        LogPrint("pow", "- Finding collisions\n");
        int i = 0;
        while (i < X.size() - 1) {
            int j = 1;
            while (i+j < X.size() &&
                    HasCollision(X[i], X[i+j], hashLen)) {
                j++;
            }

            for (int l = 0; l < j - 1; l++) {
                for (int m = l + 1; m < j; m++) {
                    FullStepRow<FinalFullWidth> res(X[i+l], X[i+m], hashLen, lenIndices, 0);
                    if (DistinctIndices(X[i+l], X[i+m], hashLen, lenIndices)) {
                        auto soln = res.GetIndices(hashLen, 2*lenIndices, CollisionBitLength);
                        assert(soln.size() == equihash_solution_size(N, K));
                        if (validBlock(soln)) {
                            return true;
                        }
                    }
                }
            }

            i += j;
            if (cancelled(FinalColliding)) throw solver_cancelled;
        }
    } else
        LogPrint("pow", "- List is empty\n");

    return false;
}

template<size_t WIDTH>
void CollideBranches(std::vector<FullStepRow<WIDTH>>& X, const size_t hlen, const size_t lenIndices, const unsigned int clen, const unsigned int ilen, const eh_trunc lt, const eh_trunc rt)
{
    int i = 0;
    int posFree = 0;
    std::vector<FullStepRow<WIDTH>> Xc;
    while (i < X.size() - 1) {
        // 2b) Find next set of unordered pairs with collisions on the next n/(k+1) bits
        int j = 1;
        while (i+j < X.size() &&
                HasCollision(X[i], X[i+j], clen)) {
            j++;
        }

        // 2c) Calculate tuples (X_i ^ X_j, (i, j))
        for (int l = 0; l < j - 1; l++) {
            for (int m = l + 1; m < j; m++) {
                if (DistinctIndices(X[i+l], X[i+m], hlen, lenIndices)) {
                    if (IsValidBranch(X[i+l], hlen, ilen, lt) && IsValidBranch(X[i+m], hlen, ilen, rt)) {
                        Xc.emplace_back(X[i+l], X[i+m], hlen, lenIndices, clen);
                    } else if (IsValidBranch(X[i+m], hlen, ilen, lt) && IsValidBranch(X[i+l], hlen, ilen, rt)) {
                        Xc.emplace_back(X[i+m], X[i+l], hlen, lenIndices, clen);
                    }
                }
            }
        }

        // 2d) Store tuples on the table in-place if possible
        while (posFree < i+j && Xc.size() > 0) {
            X[posFree++] = Xc.back();
            Xc.pop_back();
        }

        i += j;
    }

    // 2e) Handle edge case where final table entry has no collision
    while (posFree < X.size() && Xc.size() > 0) {
        X[posFree++] = Xc.back();
        Xc.pop_back();
    }

    if (Xc.size() > 0) {
        // 2f) Add overflow to end of table
        X.insert(X.end(), Xc.begin(), Xc.end());
    } else if (posFree < X.size()) {
        // 2g) Remove empty space at the end
        X.erase(X.begin()+posFree, X.end());
        X.shrink_to_fit();
    }
}

template<unsigned int N, unsigned int K>
bool Equihash<N,K>::OptimisedSolve(const eh_HashState& base_state,
                                   const std::function<bool(std::vector<unsigned char>)> validBlock,
                                   const std::function<bool(EhSolverCancelCheck)> cancelled)
{
    eh_index init_size { 1 << (CollisionBitLength + 1) };
    eh_index recreate_size { UntruncateIndex(1, 0, CollisionBitLength + 1) };

    // First run the algorithm with truncated indices

    const eh_index soln_size { 1 << K };
    std::vector<std::shared_ptr<eh_trunc>> partialSolns;
    int invalidCount = 0;
    {

        // 1) Generate first list
        LogPrint("pow", "Generating first list\n");
        size_t hashLen = HashLength;
        size_t lenIndices = sizeof(eh_trunc);
        std::vector<TruncatedStepRow<TruncatedWidth>> Xt;
        Xt.reserve(init_size);
        unsigned char tmpHash[HashOutput];
        for (eh_index g = 0; Xt.size() < init_size; g++) {
            GenerateHash(base_state, g, tmpHash, HashOutput);
            for (eh_index i = 0; i < IndicesPerHashOutput && Xt.size() < init_size; i++) {
                Xt.emplace_back(tmpHash+(i*N/8), N/8, HashLength, CollisionBitLength,
                                (g*IndicesPerHashOutput)+i, CollisionBitLength + 1);
            }
            if (cancelled(ListGeneration)) throw solver_cancelled;
        }

        // 3) Repeat step 2 until 2n/(k+1) bits remain
        for (int r = 1; r < K && Xt.size() > 0; r++) {
            LogPrint("pow", "Round %d:\n", r);
            // 2a) Sort the list
            LogPrint("pow", "- Sorting list\n");
            std::sort(Xt.begin(), Xt.end(), CompareSR(CollisionByteLength));
            if (cancelled(ListSorting)) throw solver_cancelled;

            LogPrint("pow", "- Finding collisions\n");
            int i = 0;
            int posFree = 0;
            std::vector<TruncatedStepRow<TruncatedWidth>> Xc;
            while (i < Xt.size() - 1) {
                // 2b) Find next set of unordered pairs with collisions on the next n/(k+1) bits
                int j = 1;
                while (i+j < Xt.size() &&
                        HasCollision(Xt[i], Xt[i+j], CollisionByteLength)) {
                    j++;
                }

                // 2c) Calculate tuples (X_i ^ X_j, (i, j))
                bool checking_for_zero = (i == 0 && Xt[0].IsZero(hashLen));
                for (int l = 0; l < j - 1; l++) {
                    for (int m = l + 1; m < j; m++) {
                        // We truncated, so don't check for distinct indices here
                        TruncatedStepRow<TruncatedWidth> Xi {Xt[i+l], Xt[i+m],
                                                             hashLen, lenIndices,
                                                             CollisionByteLength};
                        if (!(Xi.IsZero(hashLen-CollisionByteLength) &&
                              IsProbablyDuplicate<soln_size>(Xi.GetTruncatedIndices(hashLen-CollisionByteLength, 2*lenIndices),
                                                             2*lenIndices))) {
                            Xc.emplace_back(Xi);
                        }
                    }
                }

                // 2d) Store tuples on the table in-place if possible
                while (posFree < i+j && Xc.size() > 0) {
                    Xt[posFree++] = Xc.back();
                    Xc.pop_back();
                }

                i += j;
                if (cancelled(ListColliding)) throw solver_cancelled;
            }

            // 2e) Handle edge case where final table entry has no collision
            while (posFree < Xt.size() && Xc.size() > 0) {
                Xt[posFree++] = Xc.back();
                Xc.pop_back();
            }

            if (Xc.size() > 0) {
                // 2f) Add overflow to end of table
                Xt.insert(Xt.end(), Xc.begin(), Xc.end());
            } else if (posFree < Xt.size()) {
                // 2g) Remove empty space at the end
                Xt.erase(Xt.begin()+posFree, Xt.end());
                Xt.shrink_to_fit();
            }

            hashLen -= CollisionByteLength;
            lenIndices *= 2;
            if (cancelled(RoundEnd)) throw solver_cancelled;
        }

        // k+1) Find a collision on last 2n(k+1) bits
        LogPrint("pow", "Final round:\n");
        if (Xt.size() > 1) {
            LogPrint("pow", "- Sorting list\n");
            std::sort(Xt.begin(), Xt.end(), CompareSR(hashLen));
            if (cancelled(FinalSorting)) throw solver_cancelled;
            LogPrint("pow", "- Finding collisions\n");
            int i = 0;
            while (i < Xt.size() - 1) {
                int j = 1;
                while (i+j < Xt.size() &&
                        HasCollision(Xt[i], Xt[i+j], hashLen)) {
                    j++;
                }

                for (int l = 0; l < j - 1; l++) {
                    for (int m = l + 1; m < j; m++) {
                        TruncatedStepRow<FinalTruncatedWidth> res(Xt[i+l], Xt[i+m],
                                                                  hashLen, lenIndices, 0);
                        auto soln = res.GetTruncatedIndices(hashLen, 2*lenIndices);
                        if (!IsProbablyDuplicate<soln_size>(soln, 2*lenIndices)) {
                            partialSolns.push_back(soln);
                        }
                    }
                }

                i += j;
                if (cancelled(FinalColliding)) throw solver_cancelled;
            }
        } else
            LogPrint("pow", "- List is empty\n");

    } // Ensure Xt goes out of scope and is destroyed

    LogPrint("pow", "Found %d partial solutions\n", partialSolns.size());

    // Now for each solution run the algorithm again to recreate the indices
    LogPrint("pow", "Culling solutions\n");
    for (std::shared_ptr<eh_trunc> partialSoln : partialSolns) {
        std::set<std::vector<unsigned char>> solns;
        size_t hashLen;
        size_t lenIndices;
        unsigned char tmpHash[HashOutput];
        std::vector<boost::optional<std::vector<FullStepRow<FinalFullWidth>>>> X;
        X.reserve(K+1);

        // 3) Repeat steps 1 and 2 for each partial index
        for (eh_index i = 0; i < soln_size; i++) {
            // 1) Generate first list of possibilities
            std::vector<FullStepRow<FinalFullWidth>> icv;
            icv.reserve(recreate_size);
            for (eh_index j = 0; j < recreate_size; j++) {
                eh_index newIndex { UntruncateIndex(partialSoln.get()[i], j, CollisionBitLength + 1) };
                if (j == 0 || newIndex % IndicesPerHashOutput == 0) {
                    GenerateHash(base_state, newIndex/IndicesPerHashOutput,
                                 tmpHash, HashOutput);
                }
                icv.emplace_back(tmpHash+((newIndex % IndicesPerHashOutput) * N/8),
                                 N/8, HashLength, CollisionBitLength, newIndex);
                if (cancelled(PartialGeneration)) throw solver_cancelled;
            }
            boost::optional<std::vector<FullStepRow<FinalFullWidth>>> ic = icv;

            // 2a) For each pair of lists:
            hashLen = HashLength;
            lenIndices = sizeof(eh_index);
            size_t rti = i;
            for (size_t r = 0; r <= K; r++) {
                // 2b) Until we are at the top of a subtree:
                if (r < X.size()) {
                    if (X[r]) {
                        // 2c) Merge the lists
                        ic->reserve(ic->size() + X[r]->size());
                        ic->insert(ic->end(), X[r]->begin(), X[r]->end());
                        std::sort(ic->begin(), ic->end(), CompareSR(hashLen));
                        if (cancelled(PartialSorting)) throw solver_cancelled;
                        size_t lti = rti-(1<<r);
                        CollideBranches(*ic, hashLen, lenIndices,
                                        CollisionByteLength,
                                        CollisionBitLength + 1,
                                        partialSoln.get()[lti], partialSoln.get()[rti]);

                        // 2d) Check if this has become an invalid solution
                        if (ic->size() == 0)
                            goto invalidsolution;

                        X[r] = boost::none;
                        hashLen -= CollisionByteLength;
                        lenIndices *= 2;
                        rti = lti;
                    } else {
                        X[r] = *ic;
                        break;
                    }
                } else {
                    X.push_back(ic);
                    break;
                }
                if (cancelled(PartialSubtreeEnd)) throw solver_cancelled;
            }
            if (cancelled(PartialIndexEnd)) throw solver_cancelled;
        }

        // We are at the top of the tree
        assert(X.size() == K+1);
        for (FullStepRow<FinalFullWidth> row : *X[K]) {
            auto soln = row.GetIndices(hashLen, lenIndices, CollisionBitLength);
            assert(soln.size() == equihash_solution_size(N, K));
            solns.insert(soln);
        }
        for (auto soln : solns) {
            if (validBlock(soln))
                return true;
        }
        if (cancelled(PartialEnd)) throw solver_cancelled;
        continue;

invalidsolution:
        invalidCount++;
    }
    LogPrint("pow", "- Number of invalid solutions found: %d\n", invalidCount);

    return false;
}
#endif // ENABLE_MINING

template<unsigned int N, unsigned int K>
bool Equihash<N,K>::IsValidSolution(const eh_HashState& base_state, std::vector<unsigned char> soln)
{
    if (soln.size() != SolutionWidth) {
        LogPrint("pow", "Invalid solution length: %d (expected %d)\n",
                 soln.size(), SolutionWidth);
        return false;
    }

    std::vector<FullStepRow<FinalFullWidth>> X;
    X.reserve(1 << K);
    unsigned char tmpHash[HashOutput];
    for (eh_index i : GetIndicesFromMinimal(soln, CollisionBitLength)) {
        GenerateHash(base_state, i/IndicesPerHashOutput, tmpHash, HashOutput);
        X.emplace_back(tmpHash+((i % IndicesPerHashOutput) * N/8),
                       N/8, HashLength, CollisionBitLength, i);
    }

    size_t hashLen = HashLength;
    size_t lenIndices = sizeof(eh_index);
    while (X.size() > 1) {
        std::vector<FullStepRow<FinalFullWidth>> Xc;
        for (int i = 0; i < X.size(); i += 2) {
            if (!HasCollision(X[i], X[i+1], CollisionByteLength)) {
                LogPrint("pow", "Invalid solution: invalid collision length between StepRows\n");
                LogPrint("pow", "X[i]   = %s\n", X[i].GetHex(hashLen));
                LogPrint("pow", "X[i+1] = %s\n", X[i+1].GetHex(hashLen));
                return false;
            }
            if (X[i+1].IndicesBefore(X[i], hashLen, lenIndices)) {
                LogPrint("pow", "Invalid solution: Index tree incorrectly ordered\n");
                return false;
            }
            if (!DistinctIndices(X[i], X[i+1], hashLen, lenIndices)) {
                LogPrint("pow", "Invalid solution: duplicate indices\n");
                return false;
            }
            Xc.emplace_back(X[i], X[i+1], hashLen, lenIndices, CollisionByteLength);
        }
        X = Xc;
        hashLen -= CollisionByteLength;
        lenIndices *= 2;
    }

    assert(X.size() == 1);
    return X[0].IsZero(hashLen);
}

// Explicit instantiations for Equihash<96,3>
template int Equihash<96,3>::InitialiseState(eh_HashState& base_state);
#ifdef ENABLE_MINING
template bool Equihash<96,3>::BasicSolve(const eh_HashState& base_state,
                                         const std::function<bool(std::vector<unsigned char>)> validBlock,
                                         const std::function<bool(EhSolverCancelCheck)> cancelled);
template bool Equihash<96,3>::OptimisedSolve(const eh_HashState& base_state,
                                             const std::function<bool(std::vector<unsigned char>)> validBlock,
                                             const std::function<bool(EhSolverCancelCheck)> cancelled);
#endif
template bool Equihash<96,3>::IsValidSolution(const eh_HashState& base_state, std::vector<unsigned char> soln);

// Explicit instantiations for Equihash<200,9>
template int Equihash<200,9>::InitialiseState(eh_HashState& base_state);
#ifdef ENABLE_MINING
template bool Equihash<200,9>::BasicSolve(const eh_HashState& base_state,
                                          const std::function<bool(std::vector<unsigned char>)> validBlock,
                                          const std::function<bool(EhSolverCancelCheck)> cancelled);
template bool Equihash<200,9>::OptimisedSolve(const eh_HashState& base_state,
                                              const std::function<bool(std::vector<unsigned char>)> validBlock,
                                              const std::function<bool(EhSolverCancelCheck)> cancelled);
#endif
template bool Equihash<200,9>::IsValidSolution(const eh_HashState& base_state, std::vector<unsigned char> soln);

// Explicit instantiations for Equihash<96,5>
template int Equihash<96,5>::InitialiseState(eh_HashState& base_state);
#ifdef ENABLE_MINING
template bool Equihash<96,5>::BasicSolve(const eh_HashState& base_state,
                                         const std::function<bool(std::vector<unsigned char>)> validBlock,
                                         const std::function<bool(EhSolverCancelCheck)> cancelled);
template bool Equihash<96,5>::OptimisedSolve(const eh_HashState& base_state,
                                             const std::function<bool(std::vector<unsigned char>)> validBlock,
                                             const std::function<bool(EhSolverCancelCheck)> cancelled);
#endif
template bool Equihash<96,5>::IsValidSolution(const eh_HashState& base_state, std::vector<unsigned char> soln);

// Explicit instantiations for Equihash<48,5>
template int Equihash<48,5>::InitialiseState(eh_HashState& base_state);
#ifdef ENABLE_MINING
template bool Equihash<48,5>::BasicSolve(const eh_HashState& base_state,
                                         const std::function<bool(std::vector<unsigned char>)> validBlock,
                                         const std::function<bool(EhSolverCancelCheck)> cancelled);
template bool Equihash<48,5>::OptimisedSolve(const eh_HashState& base_state,
                                             const std::function<bool(std::vector<unsigned char>)> validBlock,
                                             const std::function<bool(EhSolverCancelCheck)> cancelled);
#endif
template bool Equihash<48,5>::IsValidSolution(const eh_HashState& base_state, std::vector<unsigned char> soln);
