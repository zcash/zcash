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
    if ((n/(k+1)) % 8 != 0) {
        std::cerr << "Parameters must satisfy n/(k+1) = 0 mod 8\n";
        throw invalid_params();
    }
}

bool stepRowSorter(StepRow &lhs, StepRow &rhs)
{
    return lhs.hash < rhs.hash;
}

bool hasCollision(const unsigned char* ha, const unsigned char* hb, int i, int l)
{
    bool res = true;
    for (int j = (i - 1) * l / 8; j < i * l / 8; j++)
        res &= ha[j] == hb[j];
    return res;
}

bool distinctIndices(const std::vector<uint32_t>& ia,
        const std::vector<uint32_t>& ib)
{
    unsigned int i = 0, j = 0;
    while (i < ia.size() && j < ib.size()) {
        while (ia[i] < ia[j])
            i++;
        while (ia[i] > ia[j])
            j++;
        if (ia[i] == ib[j])
            return false;
    }
    return true;
}

unsigned char* xorHashes(const unsigned char* ha, const unsigned char* hb, int len)
{
    unsigned char res[len];
    for (int i = 0; i < len; i++)
        res[i] = ha[i] ^ hb[i];
    return res;
}

void combineIndices(const std::vector<uint32_t>& ia,
        const std::vector<uint32_t>& ib, std::vector<uint32_t> &ic)
{
    ic.reserve(ia.size() + ib.size());
    ic.insert(ic.end(), ia.begin(), ia.end());
    ic.insert(ic.end(), ib.begin(), ib.end());
    std::sort(ic.begin(), ic.end());
}

bool isZero(unsigned char* hash, int len)
{
    char res = 0;
    for (int i = 0; i < len; i++)
        res |= hash[i];
    return res == 0;
}

EquiHash::EquiHash(unsigned int n, unsigned int k) :
        n(n), k(k)
{
    validate_params(n, k);
}

std::vector<std::vector<uint32_t>> EquiHash::Solve(const CSHA256 base_hasher)
{
    unsigned int collision_length { n / (k + 1) };
    unsigned int init_size { pow(2, (collision_length + 1)) };

    // 1) Generate first list
    std::vector<StepRow> X;
    X.reserve(init_size);
    for (uint32_t i = 0; i < init_size; i++) {
        StepRow Xi;
        // TODO truncate first list or generate correct size
        CSHA256(base_hasher).Write((unsigned char*) &i, sizeof(uint32_t)).Finalize(
                Xi.hash);
        Xi.indices.push_back(i);
        X[i] = Xi;
    }

    // 3) Repeat step 2 until 2n/(k+1) bits remain
    for (int i = 1; i < k; i++) {
        // 2a) Sort the list
        std::sort(X.begin(), X.end(), stepRowSorter);

        std::vector<StepRow> Xc;
        Xc.reserve(X.size());
        while (X.size() > 0) {
            // 2b) Find next set of unordered pairs with collisions on first n/(k+1) bits
            int j = 1;
            while (j < X.size()) {
                if (!hasCollision(X[X.size() - 1].hash,
                        X[X.size() - 1 - j].hash, i, collision_length))
                    break;
                j++;
            }

            // 2c) Store tuples (X_i ^ X_j, (i, j)) on the table
            for (int l = 0; l < j - 1; l++) {
                for (int m = l + 1; m < j; m++) {
                    StepRow *Xl = &X[X.size() - 1 - l];
                    StepRow *Xm = &X[X.size() - 1 - m];
                    if (distinctIndices(Xl->indices, Xm->indices)) {
                        StepRow Xi;
                        Xi.hash = xorHashes(Xl->hash, Xm->hash, n);
                        combineIndices(Xl->indices, Xm->indices, Xi.indices);
                        Xc.push_back(Xi);
                    }
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
    std::sort(X.begin(), X.end(), stepRowSorter);
    for (int i = 0; i < sizeof(X) - 1; i++) {
        unsigned char* res = xorHashes(X[i].hash, X[i + 1].hash, n);
        if (isZero(res, n) && X[i].indices != X[i + 1].indices) {
            std::vector<uint32_t> soln;
            combineIndices(X[i].indices, X[i + 1].indices, soln);
            solns.push_back(soln);
        }
    }

    return solns;
}
