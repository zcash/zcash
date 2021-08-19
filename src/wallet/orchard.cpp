#include "orchard.h"

void OrchardWallet::CheckpointNoteCommitmentTree() {
    orchard_wallet_checkpoint(inner.get());
}

bool OrchardWallet::RewindToLastCheckpoint() {
    return orchard_wallet_rewind(inner.get());
}

bool OrchardWallet::AddNotes(const CTransaction& tx) {
    return orchard_wallet_add_notes_from_bundle(
            inner.get(),
            tx.GetHash().begin(),
            tx.GetOrchardBundle().inner.get());
}

bool OrchardWallet::RestoreDecryptedNoteCache(const CTransaction& tx) {
    return orchard_wallet_restore_decrypted_note_cache(
            inner.get(),
            tx.GetHash().begin(),
            tx.GetOrchardBundle().inner.get());
}

bool OrchardWallet::AppendNoteCommitments(const int nHeight, const size_t blockTxIdx, const CTransaction& tx) {
    return orchard_wallet_append_bundle_commitments(
            inner.get(),
            (uint32_t) nHeight,
            blockTxIdx,
            tx.GetHash().begin(),
            tx.GetOrchardBundle().inner.get()
            );
}

bool OrchardWallet::IsMine(const uint256& txid) {
    return orchard_wallet_tx_is_mine(
            inner.get(),
            txid.begin());
}

bool OrchardWallet::AddSpendingKey(const libzcash::OrchardSpendingKey& sk) {
    orchard_wallet_add_spending_key(inner.get(), sk.inner.get());
    return true;
}

bool OrchardWallet::AddFullViewingKey(const libzcash::OrchardFullViewingKey& fvk) {
    orchard_wallet_add_full_viewing_key(inner.get(), fvk.inner.get());
    return true;
}

bool OrchardWallet::AddIncomingViewingKey(const libzcash::OrchardIncomingViewingKey& ivk,
                           const libzcash::OrchardRawAddress& addr) {
    orchard_wallet_add_incoming_viewing_key(inner.get(), ivk.inner.get(), addr.inner.get());
    return true;
}

void OrchardWallet::PushOrchardNoteMeta(void* orchardNotesRet, RawOrchardNoteMetadata rawNoteMeta) {
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

void OrchardWallet::GetFilteredNotes(
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

std::optional<OrchardWalletTx> OrchardWallet::GetTxData(const uint256 txid) {
    OrchardWalletTxPtr* txdata_ptr = orchard_wallet_get_txdata(inner.get(), txid.begin());
    if (txdata_ptr) {
        return OrchardWalletTx(txdata_ptr);
    } else {
        return std::nullopt;
    }
}

OrchardWalletNoteCommitmentTreeSerializer OrchardWallet::GetNoteCommitmentTreeSer() const {
    return OrchardWalletNoteCommitmentTreeSerializer(*this);
}
