// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WALLET_ORCHARD_H
#define ZCASH_WALLET_ORCHARD_H

#include <array>

#include "primitives/transaction.h"
#include "transaction_builder.h"

#include "rust/orchard/keys.h"
#include "rust/orchard/wallet.h"
#include "zcash/address/orchard.hpp"

class OrchardNoteMetadata
{
private:
    OrchardOutPoint op;
    libzcash::OrchardRawAddress address;
    CAmount noteValue;
    std::array<uint8_t, ZC_MEMO_SIZE> memo;
    int confirmations;
public:
    OrchardNoteMetadata(
        OrchardOutPoint op,
        const libzcash::OrchardRawAddress& address,
        CAmount noteValue,
        const std::array<unsigned char, ZC_MEMO_SIZE>& memo):
        op(op), address(address), noteValue(noteValue), memo(memo), confirmations(0) {}

    const OrchardOutPoint& GetOutPoint() const {
        return op;
    }

    const libzcash::OrchardRawAddress& GetAddress() const {
        return address;
    }

    void SetConfirmations(int c) {
        confirmations = c;
    }

    int GetConfirmations() const {
        return confirmations;
    }

    CAmount GetNoteValue() const {
        return noteValue;
    }
};

class OrchardWallet
{
private:
    std::unique_ptr<OrchardWalletPtr, decltype(&orchard_wallet_free)> inner;

    friend class ::orchard::UnauthorizedBundle;

public:
    OrchardWallet() : inner(orchard_wallet_new(), orchard_wallet_free) {}
    OrchardWallet(OrchardWallet&& wallet_data) : inner(std::move(wallet_data.inner)) {}
    OrchardWallet& operator=(OrchardWallet&& wallet)
    {
        if (this != &wallet) {
            inner = std::move(wallet.inner);
        }
        return *this;
    }

    // OrchardWallet should never be copied
    OrchardWallet(const OrchardWallet&) = delete;
    OrchardWallet& operator=(const OrchardWallet&) = delete;

    /**
     * Reset the state of the wallet to be suitable for rescan from the NU5 activation
     * height.  This removes all witness and spentness information from the wallet. The
     * keystore is unmodified and decrypted note, nullifier, and conflict data are left
     * in place with the expectation that they will be overwritten and/or updated in the
     * rescan process.
     */
    bool Reset() {
        return orchard_wallet_reset(inner.get());
    }

    /**
     * Checkpoint the note commitment tree. This returns `false` and leaves the note
     * commitment tree unmodified if the block height specified is not the successor
     * to the last block height checkpointed.
     */
    bool CheckpointNoteCommitmentTree(int nBlockHeight) {
        assert(nBlockHeight >= 0);
        return orchard_wallet_checkpoint(inner.get(), (uint32_t) nBlockHeight);
    }

    /**
     * Return whether the orchard note commitment tree contains any checkpoints.
     */
    bool IsCheckpointed() const {
        return orchard_wallet_is_checkpointed(inner.get());
    }

    /**
     * Rewinds to the most recent checkpoint, and marks as unspent any notes
     * previously identified as having been spent by transactions in the
     * latest block.
     */
    bool Rewind(int nBlockHeight, uint32_t& blocksRewoundRet) {
        assert(nBlockHeight >= 0);
        return orchard_wallet_rewind(inner.get(), (uint32_t) nBlockHeight, &blocksRewoundRet);
    }

    static void PushOrchardActionIVK(void* orchardActionsIVKRet, RawOrchardActionIVK actionIVK) {
        reinterpret_cast<std::map<uint32_t, libzcash::OrchardIncomingViewingKey>*>(orchardActionsIVKRet)->insert_or_assign(
                actionIVK.actionIdx, libzcash::OrchardIncomingViewingKey(actionIVK.ivk)
                );
    }

    /**
     * Add notes that are decryptable with IVKs for which the wallet
     * contains the full viewing key to the wallet, and return the
     * mapping from each decrypted Orchard action index to the IVK
     * that was used to decrypt that action's note.
     */
    std::map<uint32_t, libzcash::OrchardIncomingViewingKey> AddNotesIfInvolvingMe(const CTransaction& tx) {
        std::map<uint32_t, libzcash::OrchardIncomingViewingKey> result;
        orchard_wallet_add_notes_from_bundle(
                inner.get(),
                tx.GetHash().begin(),
                tx.GetOrchardBundle().inner.get(),
                &result,
                PushOrchardActionIVK
                );
        return result;
    }

    /**
     * Decrypts a selection of notes from the specified transaction's
     * Orchard bundle with provided incoming viewing keys, and adds those
     * notes to the wallet.
     */
    bool RestoreDecryptedNotes(
            const std::optional<int> nBlockHeight,
            const CTransaction& tx,
            std::map<uint32_t, libzcash::OrchardIncomingViewingKey> hints
            ) {
        std::vector<RawOrchardActionIVK> rawHints;
        for (const auto& [action_idx, ivk] : hints) {
            rawHints.push_back({ action_idx, ivk.inner.get() });
        }
        uint32_t blockHeight = nBlockHeight.has_value() ? (uint32_t) nBlockHeight.value() : 0;
        return orchard_wallet_restore_notes(
                inner.get(),
                nBlockHeight.has_value() ? &blockHeight : nullptr,
                tx.GetHash().begin(),
                tx.GetOrchardBundle().inner.get(),
                rawHints.data(),
                rawHints.size()
                );
    }

    /**
     * Append each Orchard note commitment from the specified block to the
     * wallet's note commitment tree.
     *
     * Returns `false` if the caller attempts to insert a block out-of-order.
     */
    bool AppendNoteCommitments(const int nBlockHeight, const CBlock& block) {
        assert(nBlockHeight >= 0);
        for (int txidx = 0; txidx < block.vtx.size(); txidx++) {
            const CTransaction& tx = block.vtx[txidx];
            if (!orchard_wallet_append_bundle_commitments(
                    inner.get(),
                    (uint32_t) nBlockHeight,
                    txidx,
                    tx.GetHash().begin(),
                    tx.GetOrchardBundle().inner.get()
                    )) {
                return false;
            }
        }

        return true;
    }

    uint256 GetLatestAnchor() const {
        uint256 value;
        orchard_wallet_commitment_tree_root(inner.get(), value.begin());
        return value;
    }

    bool TxContainsMyNotes(const uint256& txid) {
        return orchard_wallet_tx_contains_my_notes(
                inner.get(),
                txid.begin());
    }

    void AddSpendingKey(const libzcash::OrchardSpendingKey& sk) {
        orchard_wallet_add_spending_key(inner.get(), sk.inner.get());
    }

    void AddFullViewingKey(const libzcash::OrchardFullViewingKey& fvk) {
        orchard_wallet_add_full_viewing_key(inner.get(), fvk.inner.get());
    }

    std::optional<libzcash::OrchardSpendingKey> GetSpendingKeyForAddress(
            const libzcash::OrchardRawAddress& addr) const;

    std::optional<libzcash::OrchardIncomingViewingKey> GetIncomingViewingKeyForAddress(
            const libzcash::OrchardRawAddress& addr) const {
        auto ivkPtr = orchard_wallet_get_ivk_for_address(inner.get(), addr.inner.get());
        if (ivkPtr == nullptr) return std::nullopt;
        return libzcash::OrchardIncomingViewingKey(ivkPtr);
    }

    /**
     * Adds an address/IVK pair to the wallet, and returns `true` if the
     * IVK corresponds to a full viewing key known to the wallet, `false`
     * otherwise.
     */
    bool AddRawAddress(
            const libzcash::OrchardRawAddress& addr,
            const libzcash::OrchardIncomingViewingKey& ivk) {
        return orchard_wallet_add_raw_address(inner.get(), addr.inner.get(), ivk.inner.get());
    }

    static void PushOrchardNoteMeta(void* orchardNotesRet, RawOrchardNoteMetadata rawNoteMeta) {
        uint256 txid;
        std::move(std::begin(rawNoteMeta.txid), std::end(rawNoteMeta.txid), txid.begin());
        OrchardOutPoint op(txid, rawNoteMeta.actionIdx);
        std::array<uint8_t, ZC_MEMO_SIZE> memo;
        std::move(std::begin(rawNoteMeta.memo), std::end(rawNoteMeta.memo), memo.begin());
        OrchardNoteMetadata noteMeta(
                op,
                libzcash::OrchardRawAddress(rawNoteMeta.addr),
                rawNoteMeta.noteValue,
                memo);

        reinterpret_cast<std::vector<OrchardNoteMetadata>*>(orchardNotesRet)->push_back(noteMeta);
    }

    void GetFilteredNotes(
        std::vector<OrchardNoteMetadata>& orchardNotesRet,
        const std::optional<libzcash::OrchardIncomingViewingKey>& ivk,
        bool ignoreMined,
        bool requireSpendingKey) const {

        orchard_wallet_get_filtered_notes(
            inner.get(),
            ivk.has_value() ? ivk.value().inner.get() : nullptr,
            ignoreMined,
            requireSpendingKey,
            &orchardNotesRet,
            PushOrchardNoteMeta
            );
    }

    static void PushTxId(void* txidsRet, unsigned char txid[32]) {
        uint256 txid_out;
        std::copy(txid, txid + 32, txid_out.begin());
        reinterpret_cast<std::vector<uint256>*>(txidsRet)->push_back(txid_out);
    }

    std::vector<uint256> GetPotentialSpends(const OrchardOutPoint& outPoint) const {
        std::vector<uint256> result;
        orchard_wallet_get_potential_spends(
            inner.get(),
            outPoint.hash.begin(),
            outPoint.n,
            &result,
            PushTxId
            );
        return result;
    }

    std::vector<std::pair<libzcash::OrchardSpendingKey, orchard::SpendInfo>> GetSpendInfo(
        const std::vector<OrchardNoteMetadata>& noteMetadata) const;
};

#endif // ZCASH_ORCHARD_WALLET_H
