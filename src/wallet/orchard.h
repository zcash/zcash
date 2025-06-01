// Copyright (c) 2021-2023 The Zcash developers
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
#include "zcash/IncrementalMerkleTree.hpp"

class OrchardWallet;
class OrchardWalletNoteCommitmentTreeWriter;
class OrchardWalletNoteCommitmentTreeLoader;

class OrchardNoteMetadata
{
private:
    OrchardOutPoint op;
    libzcash::OrchardRawAddress address;
    CAmount noteValue;
    std::optional<libzcash::Memo> memo;
    int confirmations;
public:
    OrchardNoteMetadata(
        OrchardOutPoint op,
        const libzcash::OrchardRawAddress& address,
        CAmount noteValue,
        const std::optional<libzcash::Memo>& memo):
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

    const std::optional<libzcash::Memo>& GetMemo() const {
        return memo;
    }
};

/**
 * A container and serialization wrapper for storing information derived from
 * a transaction that is relevant to restoring Orchard wallet caches.
 */
class OrchardWalletTxMeta
{
private:
    // A map from action index to the IVK belonging to our wallet that decrypts
    // that action
    std::map<uint32_t, libzcash::OrchardIncomingViewingKey> mapOrchardActionData;
    // A vector of the action indices that spend notes belonging to our wallet
    std::vector<uint32_t> vActionsSpendingMyNotes;

    friend class OrchardWallet;
public:
    OrchardWalletTxMeta() {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(nVersion);
        }
        READWRITE(mapOrchardActionData);
        READWRITE(vActionsSpendingMyNotes);
    }

    const std::map<uint32_t, libzcash::OrchardIncomingViewingKey>& GetMyActionIVKs() const {
        return mapOrchardActionData;
    }

    const std::vector<uint32_t>& GetActionsSpendingMyNotes() const {
        return vActionsSpendingMyNotes;
    }

    bool empty() const {
        return (mapOrchardActionData.empty() && vActionsSpendingMyNotes.empty());
    }

    friend bool operator==(const OrchardWalletTxMeta& a, const OrchardWalletTxMeta& b) {
        return (a.mapOrchardActionData == b.mapOrchardActionData &&
                a.vActionsSpendingMyNotes == b.vActionsSpendingMyNotes);
    }

    friend bool operator!=(const OrchardWalletTxMeta& a, const OrchardWalletTxMeta& b) {
        return !(a == b);
    }
};

class OrchardActionSpend {
private:
    OrchardOutPoint outPoint;
    libzcash::OrchardRawAddress receivedAt;
    CAmount noteValue;
public:
    OrchardActionSpend(OrchardOutPoint outPoint, libzcash::OrchardRawAddress receivedAt, CAmount noteValue):
        outPoint(outPoint), receivedAt(receivedAt), noteValue(noteValue) { }

    OrchardOutPoint GetOutPoint() const {
        return outPoint;
    }

    const libzcash::OrchardRawAddress& GetReceivedAt() const {
        return receivedAt;
    }

    CAmount GetNoteValue() const {
        return noteValue;
    }
};

class OrchardActionOutput {
private:
    libzcash::OrchardRawAddress recipient;
    CAmount noteValue;
    std::optional<libzcash::Memo> memo;
    bool isOutgoing;
public:
    OrchardActionOutput(
            libzcash::OrchardRawAddress recipient,
            CAmount noteValue,
            std::optional<libzcash::Memo> memo,
            bool isOutgoing):
            recipient(recipient), noteValue(noteValue), memo(memo), isOutgoing(isOutgoing) { }

    const libzcash::OrchardRawAddress& GetRecipient() const {
        return recipient;
    }

    CAmount GetNoteValue() const {
        return noteValue;
    }

    const std::optional<libzcash::Memo>& GetMemo() const {
        return memo;
    }

    bool IsOutgoing() const {
        return isOutgoing;
    }
};

class OrchardActions {
private:
    std::map<uint32_t, OrchardActionSpend> spends;
    std::map<uint32_t, OrchardActionOutput> outputs;
public:
    OrchardActions() {}

    void AddSpend(uint32_t actionIdx, OrchardActionSpend spend) {
        spends.insert({actionIdx, spend});
    }

    void AddOutput(uint32_t actionIdx, OrchardActionOutput output) {
        outputs.insert({actionIdx, output});
    }

    const std::map<uint32_t, OrchardActionSpend>& GetSpends() {
        return spends;
    }

    const std::map<uint32_t, OrchardActionOutput>& GetOutputs() {
        return outputs;
    }
};

class OrchardWallet
{
private:
    std::unique_ptr<OrchardWalletPtr, decltype(&orchard_wallet_free)> inner;

    friend class ::orchard::UnauthorizedBundle;
    friend class OrchardWalletNoteCommitmentTreeWriter;
    friend class OrchardWalletNoteCommitmentTreeLoader;
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
    void Reset() {
        orchard_wallet_reset(inner.get());
    }

    /**
     * Overwrite the first bridge of the Orchard note commitment tree to have the
     * provided frontier as its latest state. This will fail with an assertion error
     * if any checkpoints exist in the tree.
     */
    void InitNoteCommitmentTree(const OrchardMerkleFrontier& frontier) {
        assert(!GetLastCheckpointHeight().has_value());
        assert(frontier.inner->init_wallet(
            reinterpret_cast<merkle_frontier::OrchardWallet*>(inner.get())));
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
    std::optional<int> GetLastCheckpointHeight() const {
        uint32_t lastHeight{0};
        if (orchard_wallet_get_last_checkpoint(inner.get(), &lastHeight)) {
            return (int) lastHeight;
        } else {
            return std::nullopt;
        }
    }

    /**
     * Rewinds to the most recent checkpoint, and marks as unspent any notes
     * previously identified as having been spent by transactions in the
     * latest block.
     */
    bool Rewind(int nBlockHeight, uint32_t& uResultHeight) {
        assert(nBlockHeight >= 0);
        return orchard_wallet_rewind(inner.get(), (uint32_t) nBlockHeight, &uResultHeight);
    }

    static void PushOrchardActionIVK(void* rec, RawOrchardActionIVK actionIVK) {
        reinterpret_cast<OrchardWalletTxMeta*>(rec)->mapOrchardActionData.insert_or_assign(
                actionIVK.actionIdx, libzcash::OrchardIncomingViewingKey(actionIVK.ivk)
                );
    }

    static void PushSpendActionIdx(void* rec, uint32_t actionIdx) {
        reinterpret_cast<OrchardWalletTxMeta*>(rec)->vActionsSpendingMyNotes.push_back(actionIdx);
    }

    /**
     * Add notes that are decryptable with IVKs for which the wallet
     * contains the full viewing key to the wallet, and return the
     * metadata describing the wallet's involvement with this action,
     * or std::nullopt if the transaction does not involve the wallet.
     */
    std::optional<OrchardWalletTxMeta> AddNotesIfInvolvingMe(
        const CTransaction& tx,
        const wallet::OrchardDecryptedOutputs* decryptedOutputs
    ) {
        OrchardWalletTxMeta txMeta;
        if (orchard_wallet_add_notes_from_bundle(
                inner.get(),
                tx.GetHash().begin(),
                tx.GetOrchardBundle().inner->as_ptr(),
                decryptedOutputs,
                &txMeta,
                PushOrchardActionIVK,
                PushSpendActionIdx
                )) {
            return txMeta;
        } else {
            return std::nullopt;
        }
    }

    /**
     * Decrypts a selection of notes from the specified transaction's
     * Orchard bundle with provided incoming viewing keys, and adds those
     * notes to the wallet.
     */
    bool LoadWalletTx(
            const CTransaction& tx,
            const OrchardWalletTxMeta& txMeta
            ) {
        std::vector<RawOrchardActionIVK> rawHints;
        for (const auto& [action_idx, ivk] : txMeta.mapOrchardActionData) {
            rawHints.push_back({ action_idx, ivk.inner.get() });
        }
        return orchard_wallet_load_bundle(
                inner.get(),
                tx.GetHash().begin(),
                tx.GetOrchardBundle().inner->as_ptr(),
                rawHints.data(),
                rawHints.size(),
                txMeta.vActionsSpendingMyNotes.data(),
                txMeta.vActionsSpendingMyNotes.size());
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
                    tx.GetOrchardBundle().inner->as_ptr()
                    )) {
                return false;
            }
        }

        return true;
    }

    uint256 GetLatestAnchor() const {
        uint256 value;
        // there is always a valid note commitment tree root at depth 0
        assert(orchard_wallet_commitment_tree_root(inner.get(), 0, value.begin()));
        return value;
    }

    /**
     * Return the root of the Orchard note commitment tree having the specified number
     * of confirmations. `confirmations` must be a value in the range `1..=100`; it is
     * not possible to spend shielded notes with 0 confirmations.
     */
    std::optional<uint256> GetAnchorWithConfirmations(unsigned int confirmations) const {
        // the checkpoint depth is equal to the number of confirmations - 1
        assert(confirmations > 0);
        uint256 value;
        if (orchard_wallet_commitment_tree_root(inner.get(), (size_t) confirmations - 1, value.begin())) {
            return value;
        } else {
            return std::nullopt;
        }
    }

    bool TxInvolvesMyNotes(const uint256& txid) {
        return orchard_wallet_tx_involves_my_notes(
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

    wallet::OrchardPreparedIncomingViewingKeys* PrepareIvks() const {
        return orchard_wallet_prepare_ivks(inner.get());
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
        OrchardNoteMetadata noteMeta(
                op,
                libzcash::OrchardRawAddress(rawNoteMeta.addr),
                rawNoteMeta.noteValue,
                libzcash::Memo::FromBytes(rawNoteMeta.memo));

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

    std::vector<uint256> GetPotentialSpendsFromNullifier(const uint256& nullifier) const {
        std::vector<uint256> result;
        orchard_wallet_get_potential_spends_from_nullifier(
            inner.get(),
            nullifier.begin(),
            &result,
            PushTxId
            );
        return result;
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

    /**
     * Return the witness and other information required to spend a given note.
     * `anchorConfirmations` must be a value in the range `1..=100`; it is not
     * possible to spend shielded notes with 0 confirmations.
     *
     * This method checks the root of the wallet's note commitment tree having
     * the specified `anchorConfirmations` to ensure that it corresponds to the
     * specified anchor and will panic if this check fails.
     */
    std::vector<std::pair<libzcash::OrchardSpendingKey, orchard::SpendInfo>> GetSpendInfo(
        const std::vector<OrchardNoteMetadata>& noteMetadata,
        unsigned int anchorConfirmations,
        const uint256& anchor) const;

    void GarbageCollect() {
        orchard_wallet_gc_note_commitment_tree(inner.get());
    }

    static void PushSpendAction(void* receiver, RawOrchardActionSpend rawSpend) {
        uint256 txid;
        std::move(std::begin(rawSpend.outpointTxId), std::end(rawSpend.outpointTxId), txid.begin());
        auto spend = OrchardActionSpend(
                OrchardOutPoint(txid, rawSpend.outpointActionIdx),
                libzcash::OrchardRawAddress(rawSpend.receivedAt),
                rawSpend.noteValue);
        reinterpret_cast<OrchardActions*>(receiver)->AddSpend(rawSpend.spendActionIdx, spend);
    }

    static void PushOutputAction(void* receiver, RawOrchardActionOutput rawOutput) {
        auto output = OrchardActionOutput(
                libzcash::OrchardRawAddress(rawOutput.recipient),
                rawOutput.noteValue,
                libzcash::Memo::FromBytes(rawOutput.memo),
                rawOutput.isOutgoing);

        reinterpret_cast<OrchardActions*>(receiver)->AddOutput(rawOutput.outputActionIdx, output);
    }

    OrchardActions GetTxActions(const CTransaction& tx, const std::vector<uint256>& ovks) const {
        OrchardActions result;
        orchard_wallet_get_txdata(
                inner.get(),
                tx.GetOrchardBundle().inner->as_ptr(),
                reinterpret_cast<const unsigned char*>(ovks.data()),
                ovks.size(),
                &result,
                PushSpendAction,
                PushOutputAction);
        return result;
    }

    bool UnspentNotesAreSpendable() const {
        return orchard_wallet_unspent_notes_are_spendable(inner.get());
    }
};

class OrchardWalletNoteCommitmentTreeWriter
{
private:
    const OrchardWallet& wallet;
public:
    OrchardWalletNoteCommitmentTreeWriter(const OrchardWallet& wallet): wallet(wallet) {}

    template<typename Stream>
    void Serialize(Stream& s) const {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            ::Serialize(s, nVersion);
        }
        RustStream rs(s);
        if (!orchard_wallet_write_note_commitment_tree(
                    wallet.inner.get(),
                    &rs, RustStream<Stream>::write_callback)) {
            throw std::ios_base::failure("Failed to serialize Orchard note commitment tree.");
        }
    }
};

class OrchardWalletNoteCommitmentTreeLoader
{
private:
    OrchardWallet& wallet;
public:
    OrchardWalletNoteCommitmentTreeLoader(OrchardWallet& wallet): wallet(wallet) {}

    template<typename Stream>
    void Unserialize(Stream& s) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            ::Unserialize(s, nVersion);
        }
        RustStream rs(s);
        if (!orchard_wallet_load_note_commitment_tree(
                    wallet.inner.get(),
                    &rs, RustStream<Stream>::read_callback)) {
            throw std::ios_base::failure("Failed to load Orchard note commitment tree.");
        }
    }
};

#endif // ZCASH_ORCHARD_WALLET_H
