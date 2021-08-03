// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ORCHARD_WALLET_H
#define ZCASH_ORCHARD_WALLET_H

#include <array>

#include "primitives/transaction.h"
#include "rust/orchard/keys.h"
#include "rust/orchard/wallet.h"
#include "zcash/address/orchard.hpp"

struct OrchardNoteMetadata
{
    OrchardOutPoint op;
    libzcash::OrchardRawAddress address;
    CAmount noteValue;
    std::array<unsigned char, ZC_MEMO_SIZE> memo;
    int confirmations;
};

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
                               const libzcash::OrchardRawAddress& addr) {
        orchard_wallet_add_incoming_viewing_key(inner.get(), ivk.inner.get(), addr.inner.get());
        return true;
    }

    static void PushOrchardNoteMeta(void* orchardNotesRet, RawOrchardNoteMetadata rawNoteMeta) {
        uint256 txid;
        std::move(std::begin(rawNoteMeta.txid), std::end(rawNoteMeta.txid), txid.begin());
        OrchardOutPoint op(txid, rawNoteMeta.actionIdx);
        OrchardNoteMetadata noteMeta;
        noteMeta.op = op;
        noteMeta.address = libzcash::OrchardRawAddress(rawNoteMeta.addr);
        noteMeta.noteValue = rawNoteMeta.noteValue;
        std::move(std::begin(rawNoteMeta.memo), std::end(rawNoteMeta.memo), noteMeta.memo.begin());
        // TODO: noteMeta.confirmations is only available from the C++ wallet

        reinterpret_cast<std::vector<OrchardNoteMetadata>*>(orchardNotesRet)->push_back(noteMeta);
    }

    void GetFilteredNotes(
        std::vector<OrchardNoteMetadata>& orchardNotesRet,
        const std::vector<libzcash::OrchardRawAddress>& addrs,
        bool ignoreSpent,
        bool requireSpendingKey) {

        std::vector<OrchardRawAddressPtr*> addr_ptrs;
        std::transform(
                addrs.begin(), addrs.end(), std::back_inserter(addr_ptrs),
                [](const libzcash::OrchardRawAddress& addr) {
                    return addr.inner.get();
                });

        orchard_wallet_get_filtered_notes(
            inner.get(),
            addr_ptrs.data(),
            addr_ptrs.size(),
            ignoreSpent,
            requireSpendingKey,
            &orchardNotesRet,
            PushOrchardNoteMeta
            );
    }

    // TODO: Serialization
};

#endif // ZCASH_ORCHARD_WALLET_H
