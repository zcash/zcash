// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ORCHARD_WALLET_H
#define ZCASH_ORCHARD_WALLET_H

#include "primitives/transaction.h"
#include "rust/orchard/keys.h"
#include "rust/orchard/wallet.h"
#include "zcash/address/orchard.hpp"

struct OrchardNoteMetadata
{
    OrchardOutPoint op;
    libzcash::UnifiedAddress address;
    CAmount noteValue;
    std::array<unsigned char, ZC_MEMO_SIZE> memo;
    int confirmations;
};

//class OrchardWalletTxData
//{
//private:
//    std::unique_ptr<OrchardWalletTxDataPtr, decltype(&orchard_wallet_tx_data_free)> inner;
//
//public:
//    OrchardWalletTxData() : inner(orchard_wallet_tx_data_new(), orchard_wallet_tx_data_free) {}
//
//    OrchardWalletTxData(OrchardWalletTxData&& wallet_data) : inner(std::move(wallet_data.inner)) {}
//
//    OrchardWalletTxData(const OrchardWalletTxData& wallet_data) :
//        inner(orchard_wallet_tx_data_clone(wallet_data.inner.get()), orchard_wallet_tx_data_free) {}
//
//    OrchardWalletTxData& operator=(OrchardWalletTxData&& wallet)
//    {
//        if (this != &wallet) {
//            inner = std::move(wallet.inner);
//        }
//        return *this;
//    }
//
//    OrchardWalletTxData& operator=(const OrchardWalletTxData& wallet)
//    {
//        if (this != &wallet) {
//            inner.reset(orchard_wallet_tx_data_clone(wallet.inner.get()));
//        }
//        return *this;
//    }
//}


class OrchardWallet
{
private:
    std::unique_ptr<OrchardWalletPtr, decltype(&orchard_wallet_free)> inner;

public:
    OrchardWallet() : inner(orchard_wallet_new(), orchard_wallet_free) {}

    OrchardWallet(OrchardWallet&& wallet_data) : inner(std::move(wallet_data.inner)) {}

    OrchardWallet(const OrchardWallet& wallet_data) :
        inner(orchard_wallet_clone(wallet_data.inner.get()), orchard_wallet_free) {}

    OrchardWallet& operator=(OrchardWallet&& wallet)
    {
        if (this != &wallet) {
            inner = std::move(wallet.inner);
        }
        return *this;
    }

    OrchardWallet& operator=(const OrchardWallet& wallet)
    {
        if (this != &wallet) {
            inner.reset(orchard_wallet_clone(wallet.inner.get()));
        }
        return *this;
    }

    void CheckpointNoteCommitmentTree() {
        orchard_wallet_checkpoint(inner.get());
    }

    bool RewindToLastCheckpoint() {
        return orchard_wallet_rewind(inner.get());
    }

    bool AddNotes(const CTransaction& tx) {
        return orchard_wallet_add_notes_from_bundle(
                inner.get(),
                tx.GetHash().begin(),
                tx.GetOrchardBundle().inner.get());
    }

    bool AppendNoteCommitments(const int nHeight, const size_t blockTxIdx, const CTransaction& tx) {
        return orchard_wallet_append_bundle_commitments(
                inner.get(),
                (uint32_t) nHeight,
                blockTxIdx,
                tx.GetHash().begin(),
                tx.GetOrchardBundle().inner.get()
                );
    }

    bool IsMine(const uint256& txid) {
        return orchard_wallet_tx_is_mine(
                inner.get(),
                txid.begin());
    }

    bool AddSpendingKey(const libzcash::OrchardSpendingKey& sk) {
        orchard_wallet_add_spending_key(inner.get(), sk.inner.get());
        return true;
    }

    bool AddFullViewingKey(const libzcash::OrchardFullViewingKey& fvk) {
        orchard_wallet_add_full_viewing_key(inner.get(), fvk.inner.get());
        return true;
    }

    bool AddIncomingViewingKey(const libzcash::OrchardIncomingViewingKey& ivk,
                               const libzcash::OrchardPaymentAddress& addr) {
        orchard_wallet_add_incoming_viewing_key(inner.get(), ivk.inner.get(), addr.inner.get());
        return true;
    }

    void GetFilteredNotes(
        std::vector<OrchardNoteMetadata>& orchardNotesRet,
        const libzcash::OrchardPaymentAddress& addr,
        bool ignoreSpent,
        bool requireSpendingKey) {
        // TODO
    }

    //OrchardWalletTxData GetTxData(const uint256 txid) {
    //    // TODO
    //}

    // TODO: Serialization
};

#endif // ZCASH_ORCHARD_WALLET_H
