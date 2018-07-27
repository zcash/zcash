// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRANSACTION_BUILDER_H
#define TRANSACTION_BUILDER_H

#include "consensus/params.h"
#include "primitives/transaction.h"
#include "uint256.h"
#include "zcash/Address.hpp"
#include "zcash/IncrementalMerkleTree.hpp"
#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"

#include <boost/optional.hpp>

struct SpendDescriptionInfo {
    libzcash::SaplingExpandedSpendingKey xsk;
    libzcash::SaplingNote note;
    uint256 alpha;
    uint256 anchor;
    ZCSaplingIncrementalWitness witness;

    SpendDescriptionInfo(
        libzcash::SaplingExpandedSpendingKey xsk,
        libzcash::SaplingNote note,
        uint256 anchor,
        ZCSaplingIncrementalWitness witness);
};

struct OutputDescriptionInfo {
    uint256 ovk;
    libzcash::SaplingNote note;
    std::array<unsigned char, ZC_MEMO_SIZE> memo;

    OutputDescriptionInfo(
        uint256 ovk,
        libzcash::SaplingNote note,
        std::array<unsigned char, ZC_MEMO_SIZE> memo) : ovk(ovk), note(note), memo(memo) {}
};

class TransactionBuilder
{
private:
    Consensus::Params consensusParams;
    int nHeight;
    CMutableTransaction mtx;

    std::vector<SpendDescriptionInfo> spends;
    std::vector<OutputDescriptionInfo> outputs;

public:
    TransactionBuilder(const Consensus::Params& consensusParams, int nHeight);

    // Returns false if the anchor does not match the anchor used by
    // previously-added Sapling spends.
    bool AddSaplingSpend(
        libzcash::SaplingExpandedSpendingKey xsk,
        libzcash::SaplingNote note,
        uint256 anchor,
        ZCSaplingIncrementalWitness witness);

    void AddSaplingOutput(
        libzcash::SaplingFullViewingKey from,
        libzcash::SaplingPaymentAddress to,
        CAmount value,
        std::array<unsigned char, ZC_MEMO_SIZE> memo);

    boost::optional<CTransaction> Build();
};

#endif /* TRANSACTION_BUILDER_H */
