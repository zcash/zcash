// Copyright (c) 2016 Jack Grigg
// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/equihash.h"

#include <algorithm>
#include <cmath>
#include <iostream>

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

StepRow::StepRow(unsigned int n, const CSHA256& hasher, uint32_t i) :
        hash {new unsigned char[n/8]},
        len {n/8},
        indices {i}
{
    unsigned char tmp[hasher.OUTPUT_SIZE];
    CSHA256(hasher)
            .Write((unsigned char*) &i, sizeof(uint32_t))
            .Finalize(tmp);
    // TODO generate correct size instead?
    std::copy(tmp, tmp+(n/8), hash);

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
    if (a.len != len)
        throw 1; // TODO throw better error
    unsigned char* p = new unsigned char[len];
    for (int i = 0; i < len; i++)
        p[i] = hash[i] ^ a.hash[i];
    delete[] hash;
    hash = p;
    indices.reserve(indices.size() + a.indices.size());
    indices.insert(indices.end(), a.indices.begin(), a.indices.end());
    std::sort(indices.begin(), indices.end());
    return *this;
}

bool StepRow::IsZero()
{
    char res = 0;
    for (int i = 0; i < len; i++)
        res |= hash[i];
    return res == 0;
}

bool HasCollision(StepRow& a, StepRow& b, int i, int l)
{
    bool res = true;
    for (int j = (i - 1) * l / 8; j < i * l / 8; j++)
        res &= a.hash[j] == b.hash[j];
    return res;
}

bool DistinctIndices(const StepRow& a, const StepRow& b)
{
    unsigned int i = 0, j = 0;
    while (i < a.indices.size() && j < b.indices.size()) {
        while (i < a.indices.size() && a.indices[i] < b.indices[j])
            i++;
        while (j < b.indices.size() && a.indices[i] > b.indices[j])
            j++;
        if (a.indices[i] == b.indices[j])
            return false;
    }
    return true;
}

EquiHash::EquiHash(unsigned int n, unsigned int k) :
        n(n), k(k)
{
    validate_params(n, k);
}

std::vector<std::vector<uint32_t>> EquiHash::Solve(const CSHA256& base_hasher)
{
    unsigned int collision_length { n / (k + 1) };
    unsigned int init_size { pow(2, (collision_length + 1)) };

    // 1) Generate first list
    std::vector<StepRow> X;
    X.reserve(init_size);
    for (uint32_t i = 0; i < init_size; i++) {
        X.push_back(StepRow(n, base_hasher, i));
    }

    // 3) Repeat step 2 until 2n/(k+1) bits remain
    for (int i = 1; i < k; i++) {
        // 2a) Sort the list
        std::sort(X.begin(), X.end());

        std::vector<StepRow> Xc;
        Xc.reserve(X.size());
        while (X.size() > 0) {
            int end = X.size()-1;
            // 2b) Find next set of unordered pairs with collisions on first n/(k+1) bits
            int j = 1;
            while (j < X.size() &&
                    HasCollision(X[end], X[end - j], i, collision_length)) {
                j++;
            }

            // 2c) Store tuples (X_i ^ X_j, (i, j)) on the table
            for (int l = 0; l < j - 1; l++) {
                for (int m = l + 1; m < j; m++) {
                    if (DistinctIndices(X[end-l], X[end-m]))
                        Xc.push_back(X[end-l] ^ X[end-m]);
                }
            }

            // 2d) Drop this set
            X.erase(X.end() - j, X.end());
        }

        // 2e) Replace previous list with new list
        X = Xc;
    }

    // k+1) Find a collision on last 2n(k+1) bits
    std::vector<std::vector<uint32_t>> solns;
    std::sort(X.begin(), X.end());
    for (int i = 0; i < X.size() - 1; i++) {
        StepRow res = X[i] ^ X[i+1];
        if (res.IsZero())
            solns.push_back(res.GetSolution());
    }

    return solns;
}
