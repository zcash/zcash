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
#include <cmath>
#include <iostream>
#include <stdexcept>

void validate_params(int n, int k)
{
    if (k>=n) {
        std::cerr << "n must be larger than k\n";
        throw invalid_params();
    }
    if (n % 8 != 0) {
        std::cerr << "Parameters must satisfy n = 0 mod 8\n";
        throw invalid_params();
    }
    if ((n/(k+1)) % 8 != 0) {
        std::cerr << "Parameters must satisfy n/(k+1) = 0 mod 8\n";
        throw invalid_params();
    }
}

int Equihash::InitialiseState(eh_HashState& base_state)
{
    unsigned char personalization[crypto_generichash_blake2b_PERSONALBYTES] = {};
    memcpy(personalization, "ZcashPOW", 8);
    memcpy(personalization+8,  &n, 4);
    memcpy(personalization+12, &k, 4);
    return crypto_generichash_blake2b_init_salt_personal(&base_state,
                                                         NULL, 0, // No key.
                                                         n/8,
                                                         NULL,    // No salt.
                                                         personalization);
}

StepRow::StepRow(unsigned int n, const eh_HashState& base_state, eh_index i) :
        hash {new unsigned char[n/8]},
        len {n/8},
        indices {i}
{
    eh_HashState state;
    state = base_state;
    crypto_generichash_blake2b_update(&state, (unsigned char*) &i, sizeof(eh_index));
    crypto_generichash_blake2b_final(&state, hash, n/8);

    assert(indices.size() == 1);
}

StepRow::~StepRow()
{
    delete[] hash;
}

StepRow::StepRow(const StepRow& a) :
        hash {new unsigned char[a.len]},
        len {a.len},
        indices(a.indices)
{
    for (int i = 0; i < len; i++)
        hash[i] = a.hash[i];
}

StepRow& StepRow::operator=(const StepRow& a)
{
    unsigned char* p = new unsigned char[a.len];
    for (int i = 0; i < a.len; i++)
        p[i] = a.hash[i];
    delete[] hash;
    hash = p;
    len = a.len;
    indices = a.indices;
    return *this;
}

StepRow& StepRow::operator^=(const StepRow& a)
{
    if (a.len != len) {
        throw std::invalid_argument("Hash length differs");
    }
    if (a.indices.size() != indices.size()) {
        throw std::invalid_argument("Number of indices differs");
    }
    unsigned char* p = new unsigned char[len];
    for (int i = 0; i < len; i++)
        p[i] = hash[i] ^ a.hash[i];
    delete[] hash;
    hash = p;
    indices.reserve(indices.size() + a.indices.size());
    indices.insert(indices.end(), a.indices.begin(), a.indices.end());
    return *this;
}

void StepRow::TrimHash(int l)
{
    unsigned char* p = new unsigned char[len-l];
    for (int i = 0; i < len-l; i++)
        p[i] = hash[i+l];
    delete[] hash;
    hash = p;
    len -= l;
}

bool StepRow::IsZero()
{
    char res = 0;
    for (int i = 0; i < len; i++)
        res |= hash[i];
    return res == 0;
}

bool HasCollision(StepRow& a, StepRow& b, int l)
{
    bool res = true;
    for (int j = 0; j < l; j++)
        res &= a.hash[j] == b.hash[j];
    return res;
}

// Checks if the intersection of a.indices and b.indices is empty
bool DistinctIndices(const StepRow& a, const StepRow& b)
{
    std::vector<eh_index> aSrt(a.indices);
    std::vector<eh_index> bSrt(b.indices);

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

Equihash::Equihash(unsigned int n, unsigned int k) :
        n(n), k(k)
{
    validate_params(n, k);
}

std::set<std::vector<eh_index>> Equihash::BasicSolve(const eh_HashState& base_state)
{
    assert(CollisionBitLength() + 1 < 8*sizeof(eh_index));
    eh_index init_size { ((eh_index) 1) << (CollisionBitLength() + 1) };

    // 1) Generate first list
    LogPrint("pow", "Generating first list\n");
    std::vector<StepRow> X;
    X.reserve(init_size);
    for (eh_index i = 0; i < init_size; i++) {
        X.emplace_back(n, base_state, i);
    }

    // 3) Repeat step 2 until 2n/(k+1) bits remain
    for (int r = 1; r < k && X.size() > 0; r++) {
        LogPrint("pow", "Round %d:\n", r);
        // 2a) Sort the list
        LogPrint("pow", "- Sorting list\n");
        std::sort(X.begin(), X.end());

        LogPrint("pow", "- Finding collisions\n");
        int i = 0;
        int posFree = 0;
        std::vector<StepRow> Xc;
        while (i < X.size() - 1) {
            // 2b) Find next set of unordered pairs with collisions on the next n/(k+1) bits
            int j = 1;
            while (i+j < X.size() &&
                    HasCollision(X[i], X[i+j], CollisionByteLength())) {
                j++;
            }

            // 2c) Calculate tuples (X_i ^ X_j, (i, j))
            for (int l = 0; l < j - 1; l++) {
                for (int m = l + 1; m < j; m++) {
                    if (DistinctIndices(X[i+l], X[i+m])) {
                        Xc.push_back(X[i+l] ^ X[i+m]);
                        Xc.back().TrimHash(CollisionByteLength());
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

    // k+1) Find a collision on last 2n(k+1) bits
    LogPrint("pow", "Final round:\n");
    std::set<std::vector<eh_index>> solns;
    if (X.size() > 1) {
        LogPrint("pow", "- Sorting list\n");
        std::sort(X.begin(), X.end());
        LogPrint("pow", "- Finding collisions\n");
        for (int i = 0; i < X.size() - 1; i++) {
            StepRow res = X[i] ^ X[i+1];
            if (res.IsZero() && DistinctIndices(X[i], X[i+1])) {
                solns.insert(res.GetSolution());
            }
        }
    } else
        LogPrint("pow", "- List is empty\n");

    return solns;
}

bool Equihash::IsValidSolution(const eh_HashState& base_state, std::vector<eh_index> soln)
{
    eh_index soln_size { pow(2, k) };
    if (soln.size() != soln_size) {
        LogPrint("pow", "Invalid solution size: %d\n", soln.size());
        return false;
    }

    std::vector<StepRow> X;
    X.reserve(soln_size);
    for (eh_index i : soln) {
        X.emplace_back(n, base_state, i);
    }

    while (X.size() > 1) {
        std::vector<StepRow> Xc;
        for (int i = 0; i < X.size(); i += 2) {
            if (!HasCollision(X[i], X[i+1], CollisionByteLength())) {
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
            Xc.push_back(X[i] ^ X[i+1]);
            Xc.back().TrimHash(CollisionByteLength());
        }
        X = Xc;
    }

    assert(X.size() == 1);
    return X[0].IsZero();
}
