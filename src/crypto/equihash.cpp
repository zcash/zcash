// Copyright (c) 2016 Jack Grigg
// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Implementation of the Equihash Proof-of-Work algorithm.
//
// Reference
// =========
// Alex Biryukov and Dmitry Khovratovich
// Equihash: Asymmetric Proof-of-Work Based on the Generalized Birthday Problem
// NDSS ’16, 21-24 February 2016, San Diego, CA, USA
// https://www.internetsociety.org/sites/default/files/blogs-media/equihash-asymmetric-proof-of-work-based-generalized-birthday-problem.pdf

#include "crypto/equihash.h"
#include "util.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

template<unsigned int N, unsigned int K>
int Equihash<N,K>::InitialiseState(eh_HashState& base_state)
{
    unsigned int n = N;
    unsigned int k = K;
    unsigned char personalization[crypto_generichash_blake2b_PERSONALBYTES] = {};
    memcpy(personalization, "ZcashPOW", 8);
    memcpy(personalization+8,  &n, 4);
    memcpy(personalization+12, &k, 4);
    return crypto_generichash_blake2b_init_salt_personal(&base_state,
                                                         NULL, 0, // No key.
                                                         N/8,
                                                         NULL,    // No salt.
                                                         personalization);
}

void EhIndexToArray(eh_index i, unsigned char* array)
{
    assert(sizeof(eh_index) == 4);
    array[0] = (i >> 24) & 0xFF;
    array[1] = (i >> 16) & 0xFF;
    array[2] = (i >>  8) & 0xFF;
    array[3] =  i        & 0xFF;
}

eh_index ArrayToEhIndex(unsigned char* array)
{
    assert(sizeof(eh_index) == 4);
    eh_index ret {array[0]};
    ret <<= 8;
    ret |= array[1];
    ret <<= 8;
    ret |= array[2];
    ret <<= 8;
    ret |= array[3];
    return ret;
}

eh_trunc TruncateIndex(eh_index i, unsigned int ilen)
{
    // Truncate to 8 bits
    assert(sizeof(eh_trunc) == 1);
    return (i >> (ilen - 8)) & 0xff;
}

eh_index UntruncateIndex(eh_trunc t, eh_index r, unsigned int ilen)
{
    eh_index i{t};
    return (i << (ilen - 8)) | r;
}

StepRow::StepRow(unsigned int n, const eh_HashState& base_state, eh_index i) :
        hash {new unsigned char[n/8]},
        len {n/8}
{
    eh_HashState state;
    state = base_state;
    crypto_generichash_blake2b_update(&state, (unsigned char*) &i, sizeof(eh_index));
    crypto_generichash_blake2b_final(&state, hash, n/8);
}

StepRow::~StepRow()
{
    delete[] hash;
}

StepRow::StepRow(const StepRow& a) :
        hash {new unsigned char[a.len]},
        len {a.len}
{
    std::copy(a.hash, a.hash+a.len, hash);
}

FullStepRow::FullStepRow(unsigned int n, const eh_HashState& base_state, eh_index i) :
        StepRow {n, base_state, i},
        lenIndices {sizeof(eh_index)}
{
    unsigned char* p = new unsigned char[len+lenIndices];
    std::copy(hash, hash+len, p);
    EhIndexToArray(i, p+len);
    delete[] hash;
    hash = p;
}

FullStepRow::FullStepRow(const FullStepRow& a) :
        StepRow {a},
        lenIndices {a.lenIndices}
{
    unsigned char* p = new unsigned char[a.len+a.lenIndices];
    std::copy(a.hash, a.hash+a.len+a.lenIndices, p);
    delete[] hash;
    hash = p;
}

FullStepRow::FullStepRow(const FullStepRow& a, const FullStepRow& b, int trim) :
        StepRow {a},
        lenIndices {a.lenIndices+b.lenIndices}
{
    if (a.len != b.len) {
        throw std::invalid_argument("Hash length differs");
    }
    if (a.lenIndices != b.lenIndices) {
        throw std::invalid_argument("Number of indices differs");
    }
    unsigned char* p = new unsigned char[a.len-trim+a.lenIndices+b.lenIndices];
    for (int i = trim; i < a.len; i++)
        p[i-trim] = a.hash[i] ^ b.hash[i];
    len = a.len-trim;
    if (a.IndicesBefore(b)) {
        std::copy(a.hash+a.len, a.hash+a.len+a.lenIndices, p+len);
        std::copy(b.hash+b.len, b.hash+b.len+b.lenIndices, p+len+a.lenIndices);
    } else {
        std::copy(b.hash+b.len, b.hash+b.len+b.lenIndices, p+len);
        std::copy(a.hash+a.len, a.hash+a.len+a.lenIndices, p+len+b.lenIndices);
    }
    delete[] hash;
    hash = p;
}

FullStepRow& FullStepRow::operator=(const FullStepRow& a)
{
    unsigned char* p = new unsigned char[a.len+a.lenIndices];
    std::copy(a.hash, a.hash+a.len+a.lenIndices, p);
    delete[] hash;
    hash = p;
    len = a.len;
    lenIndices = a.lenIndices;
    return *this;
}

bool StepRow::IsZero()
{
    char res = 0;
    for (int i = 0; i < len; i++)
        res |= hash[i];
    return res == 0;
}

std::vector<eh_index> FullStepRow::GetIndices() const
{
    std::vector<eh_index> ret;
    for (int i = 0; i < lenIndices; i += sizeof(eh_index)) {
        ret.push_back(ArrayToEhIndex(hash+len+i));
    }
    return ret;
}

bool HasCollision(StepRow& a, StepRow& b, int l)
{
    bool res = true;
    for (int j = 0; j < l; j++)
        res &= a.hash[j] == b.hash[j];
    return res;
}

// Checks if the intersection of a.indices and b.indices is empty
bool DistinctIndices(const FullStepRow& a, const FullStepRow& b)
{
    std::vector<eh_index> aSrt = a.GetIndices();
    std::vector<eh_index> bSrt = b.GetIndices();

    std::sort(aSrt.begin(), aSrt.end());
    std::sort(bSrt.begin(), bSrt.end());

    unsigned int i = 0;
    for (unsigned int j = 0; j < bSrt.size(); j++) {
        while (aSrt[i] < bSrt[j]) {
            i++;
            if (i == aSrt.size()) { return true; }
        }
        assert(aSrt[i] >= bSrt[j]);
        if (aSrt[i] == bSrt[j]) { return false; }
    }
    return true;
}

bool IsValidBranch(const FullStepRow& a, const unsigned int ilen, const eh_trunc t)
{
    return TruncateIndex(ArrayToEhIndex(a.hash+a.len), ilen) == t;
}

TruncatedStepRow::TruncatedStepRow(unsigned int n, const eh_HashState& base_state, eh_index i, unsigned int ilen) :
        StepRow {n, base_state, i},
        lenIndices {1}
{
    unsigned char* p = new unsigned char[len+lenIndices];
    std::copy(hash, hash+len, p);
    p[len] = TruncateIndex(i, ilen);
    delete[] hash;
    hash = p;
}

TruncatedStepRow::TruncatedStepRow(const TruncatedStepRow& a) :
        StepRow {a},
        lenIndices {a.lenIndices}
{
    unsigned char* p = new unsigned char[a.len+a.lenIndices];
    std::copy(a.hash, a.hash+a.len+a.lenIndices, p);
    delete[] hash;
    hash = p;
}

TruncatedStepRow::TruncatedStepRow(const TruncatedStepRow& a, const TruncatedStepRow& b, int trim) :
        StepRow {a},
        lenIndices {a.lenIndices+b.lenIndices}
{
    if (a.len != b.len) {
        throw std::invalid_argument("Hash length differs");
    }
    if (a.lenIndices != b.lenIndices) {
        throw std::invalid_argument("Number of indices differs");
    }
    unsigned char* p = new unsigned char[a.len-trim+a.lenIndices+b.lenIndices];
    for (int i = trim; i < a.len; i++)
        p[i-trim] = a.hash[i] ^ b.hash[i];
    len = a.len-trim;
    if (a.IndicesBefore(b)) {
        std::copy(a.hash+a.len, a.hash+a.len+a.lenIndices, p+len);
        std::copy(b.hash+b.len, b.hash+b.len+b.lenIndices, p+len+a.lenIndices);
    } else {
        std::copy(b.hash+b.len, b.hash+b.len+b.lenIndices, p+len);
        std::copy(a.hash+a.len, a.hash+a.len+a.lenIndices, p+len+b.lenIndices);
    }
    delete[] hash;
    hash = p;
}

TruncatedStepRow& TruncatedStepRow::operator=(const TruncatedStepRow& a)
{
    unsigned char* p = new unsigned char[a.len+a.lenIndices];
    std::copy(a.hash, a.hash+a.len+a.lenIndices, p);
    delete[] hash;
    hash = p;
    len = a.len;
    lenIndices = a.lenIndices;
    return *this;
}

eh_trunc* TruncatedStepRow::GetPartialSolution(eh_index soln_size) const
{
    assert(lenIndices == soln_size);
    eh_trunc* p = new eh_trunc[lenIndices];
    std::copy(hash+len, hash+len+lenIndices, p);
    return p;
}

template<unsigned int N, unsigned int K>
std::set<std::vector<eh_index>> Equihash<N,K>::BasicSolve(const eh_HashState& base_state)
{
    eh_index init_size { 1 << (CollisionBitLength + 1) };

    // 1) Generate first list
    LogPrint("pow", "Generating first list\n");
    size_t hashLen = N/8;
    std::vector<FullStepRow> X;
    X.reserve(init_size);
    for (eh_index i = 0; i < init_size; i++) {
        X.emplace_back(N, base_state, i);
    }

    // 3) Repeat step 2 until 2n/(k+1) bits remain
    for (int r = 1; r < K && X.size() > 0; r++) {
        LogPrint("pow", "Round %d:\n", r);
        // 2a) Sort the list
        LogPrint("pow", "- Sorting list\n");
        std::sort(X.begin(), X.end(), CompareSR(hashLen));

        LogPrint("pow", "- Finding collisions\n");
        int i = 0;
        int posFree = 0;
        std::vector<FullStepRow> Xc;
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
                    if (DistinctIndices(X[i+l], X[i+m])) {
                        Xc.emplace_back(X[i+l], X[i+m], CollisionByteLength);
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

        hashLen -= CollisionByteLength;
    }

    // k+1) Find a collision on last 2n(k+1) bits
    LogPrint("pow", "Final round:\n");
    std::set<std::vector<eh_index>> solns;
    if (X.size() > 1) {
        LogPrint("pow", "- Sorting list\n");
        std::sort(X.begin(), X.end(), CompareSR(hashLen));
        LogPrint("pow", "- Finding collisions\n");
        for (int i = 0; i < X.size() - 1; i++) {
            FullStepRow res(X[i], X[i+1], 0);
            if (res.IsZero() && DistinctIndices(X[i], X[i+1])) {
                solns.insert(res.GetIndices());
            }
        }
    } else
        LogPrint("pow", "- List is empty\n");

    return solns;
}

void CollideBranches(std::vector<FullStepRow>& X, const unsigned int clen, const unsigned int ilen, const eh_trunc lt, const eh_trunc rt)
{
    int i = 0;
    int posFree = 0;
    std::vector<FullStepRow> Xc;
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
                if (DistinctIndices(X[i+l], X[i+m])) {
                    if (IsValidBranch(X[i+l], ilen, lt) && IsValidBranch(X[i+m], ilen, rt)) {
                        Xc.emplace_back(X[i+l], X[i+m], clen);
                    } else if (IsValidBranch(X[i+m], ilen, lt) && IsValidBranch(X[i+l], ilen, rt)) {
                        Xc.emplace_back(X[i+m], X[i+l], clen);
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
std::set<std::vector<eh_index>> Equihash<N,K>::OptimisedSolve(const eh_HashState& base_state)
{
    eh_index init_size { 1 << (CollisionBitLength + 1) };

    // First run the algorithm with truncated indices

    eh_index soln_size { 1 << K };
    std::vector<eh_trunc*> partialSolns;
    {

        // 1) Generate first list
        LogPrint("pow", "Generating first list\n");
        size_t hashLen = N/8;
        std::vector<TruncatedStepRow> Xt;
        Xt.reserve(init_size);
        for (eh_index i = 0; i < init_size; i++) {
            Xt.emplace_back(N, base_state, i, CollisionBitLength + 1);
        }

        // 3) Repeat step 2 until 2n/(k+1) bits remain
        for (int r = 1; r < K && Xt.size() > 0; r++) {
            LogPrint("pow", "Round %d:\n", r);
            // 2a) Sort the list
            LogPrint("pow", "- Sorting list\n");
            std::sort(Xt.begin(), Xt.end(), CompareSR(hashLen));

            LogPrint("pow", "- Finding collisions\n");
            int i = 0;
            int posFree = 0;
            std::vector<TruncatedStepRow> Xc;
            while (i < Xt.size() - 1) {
                // 2b) Find next set of unordered pairs with collisions on the next n/(k+1) bits
                int j = 1;
                while (i+j < Xt.size() &&
                        HasCollision(Xt[i], Xt[i+j], CollisionByteLength)) {
                    j++;
                }

                // 2c) Calculate tuples (X_i ^ X_j, (i, j))
                for (int l = 0; l < j - 1; l++) {
                    for (int m = l + 1; m < j; m++) {
                        // We truncated, so don't check for distinct indices here
                        Xc.emplace_back(Xt[i+l], Xt[i+m], CollisionByteLength);
                    }
                }

                // 2d) Store tuples on the table in-place if possible
                while (posFree < i+j && Xc.size() > 0) {
                    Xt[posFree++] = Xc.back();
                    Xc.pop_back();
                }

                i += j;
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
        }

        // k+1) Find a collision on last 2n(k+1) bits
        LogPrint("pow", "Final round:\n");
        if (Xt.size() > 1) {
            LogPrint("pow", "- Sorting list\n");
            std::sort(Xt.begin(), Xt.end(), CompareSR(hashLen));
            LogPrint("pow", "- Finding collisions\n");
            for (int i = 0; i < Xt.size() - 1; i++) {
                TruncatedStepRow res(Xt[i], Xt[i+1], 0);
                if (res.IsZero()) {
                    partialSolns.push_back(res.GetPartialSolution(soln_size));
                }
            }
        } else
            LogPrint("pow", "- List is empty\n");

    } // Ensure Xt goes out of scope and is destroyed

    LogPrint("pow", "Found %d partial solutions\n", partialSolns.size());

    // Now for each solution run the algorithm again to recreate the indices
    LogPrint("pow", "Culling solutions\n");
    std::set<std::vector<eh_index>> solns;
    eh_index recreate_size { UntruncateIndex(1, 0, CollisionBitLength + 1) };
    int invalidCount = 0;
    for (eh_trunc* partialSoln : partialSolns) {
        // 1) Generate first list of possibilities
        size_t hashLen = N/8;
        std::vector<std::vector<FullStepRow>> X;
        X.reserve(soln_size);
        for (eh_index i = 0; i < soln_size; i++) {
            std::vector<FullStepRow> ic;
            ic.reserve(recreate_size);
            for (eh_index j = 0; j < recreate_size; j++) {
                eh_index newIndex { UntruncateIndex(partialSoln[i], j, CollisionBitLength + 1) };
                ic.emplace_back(N, base_state, newIndex);
            }
            X.push_back(ic);
        }

        // 3) Repeat step 2 for each level of the tree
        for (int r = 0; X.size() > 1; r++) {
            std::vector<std::vector<FullStepRow>> Xc;
            Xc.reserve(X.size()/2);

            // 2a) For each pair of lists:
            for (int v = 0; v < X.size(); v += 2) {
                // 2b) Merge the lists
                std::vector<FullStepRow> ic(X[v]);
                ic.reserve(X[v].size() + X[v+1].size());
                ic.insert(ic.end(), X[v+1].begin(), X[v+1].end());
                std::sort(ic.begin(), ic.end(), CompareSR(hashLen));
                CollideBranches(ic, CollisionByteLength, CollisionBitLength + 1, partialSoln[(1<<r)*v], partialSoln[(1<<r)*(v+1)]);

                // 2v) Check if this has become an invalid solution
                if (ic.size() == 0)
                    goto invalidsolution;

                Xc.push_back(ic);
            }

            X = Xc;
            hashLen -= CollisionByteLength;
        }

        // We are at the top of the tree
        assert(X.size() == 1);
        for (FullStepRow row : X[0]) {
            solns.insert(row.GetIndices());
        }
        goto deletesolution;

invalidsolution:
        invalidCount++;

deletesolution:
        delete[] partialSoln;
    }
    LogPrint("pow", "- Number of invalid solutions found: %d\n", invalidCount);

    return solns;
}

template<unsigned int N, unsigned int K>
bool Equihash<N,K>::IsValidSolution(const eh_HashState& base_state, std::vector<eh_index> soln)
{
    eh_index soln_size { 1u << K };
    if (soln.size() != soln_size) {
        LogPrint("pow", "Invalid solution size: %d\n", soln.size());
        return false;
    }

    std::vector<FullStepRow> X;
    X.reserve(soln_size);
    for (eh_index i : soln) {
        X.emplace_back(N, base_state, i);
    }

    while (X.size() > 1) {
        std::vector<FullStepRow> Xc;
        for (int i = 0; i < X.size(); i += 2) {
            if (!HasCollision(X[i], X[i+1], CollisionByteLength)) {
                LogPrint("pow", "Invalid solution: invalid collision length between StepRows\n");
                LogPrint("pow", "X[i]   = %s\n", X[i].GetHex());
                LogPrint("pow", "X[i+1] = %s\n", X[i+1].GetHex());
                return false;
            }
            if (X[i+1].IndicesBefore(X[i])) {
                return false;
                LogPrint("pow", "Invalid solution: Index tree incorrectly ordered\n");
            }
            if (!DistinctIndices(X[i], X[i+1])) {
                LogPrint("pow", "Invalid solution: duplicate indices\n");
                return false;
            }
            Xc.emplace_back(X[i], X[i+1], CollisionByteLength);
        }
        X = Xc;
    }

    assert(X.size() == 1);
    return X[0].IsZero();
}

// Explicit instantiations for Equihash<96,5>
template int Equihash<96,5>::InitialiseState(eh_HashState& base_state);
template std::set<std::vector<eh_index>> Equihash<96,5>::BasicSolve(const eh_HashState& base_state);
template std::set<std::vector<eh_index>> Equihash<96,5>::OptimisedSolve(const eh_HashState& base_state);
template bool Equihash<96,5>::IsValidSolution(const eh_HashState& base_state, std::vector<eh_index> soln);

// Explicit instantiations for Equihash<48,5>
template int Equihash<48,5>::InitialiseState(eh_HashState& base_state);
template std::set<std::vector<eh_index>> Equihash<48,5>::BasicSolve(const eh_HashState& base_state);
template std::set<std::vector<eh_index>> Equihash<48,5>::OptimisedSolve(const eh_HashState& base_state);
template bool Equihash<48,5>::IsValidSolution(const eh_HashState& base_state, std::vector<eh_index> soln);
