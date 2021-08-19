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

class OrchardWalletTx
{
private:
    std::unique_ptr<OrchardWalletTxPtr, decltype(&orchard_wallet_tx_free)> inner;

    OrchardWalletTx(OrchardWalletTxPtr* ptr) : inner(ptr, orchard_wallet_tx_free) {}

    friend class OrchardWallet;
    friend class CWalletTx;
public:
    OrchardWalletTx() : inner(nullptr, orchard_wallet_tx_free) {}

    OrchardWalletTx(OrchardWalletTx&& wtx) : inner(std::move(wtx.inner)) {}

    OrchardWalletTx(const OrchardWalletTx& wtx) :
        inner(orchard_wallet_tx_clone(wtx.inner.get()), orchard_wallet_tx_free) {}

    OrchardWalletTx& operator=(OrchardWalletTx&& wtx)
    {
        if (this != &wtx) {
            inner = std::move(wtx.inner);
        }
        return *this;
    }

    OrchardWalletTx& operator=(const OrchardWalletTx& wtx)
    {
        if (this != &wtx) {
            inner.reset(orchard_wallet_tx_clone(wtx.inner.get()));
        }
        return *this;
    }

    template<typename Stream>
    void Serialize(Stream& s) const {
        RustStream rs(s);
        if (!orchard_wallet_tx_serialize(inner.get(), &rs, RustStream<Stream>::write_callback)) {
            throw std::ios_base::failure("Failed to serialize v5 Orchard bundle");
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        RustStream rs(s);
        const auto txdata = orchard_wallet_tx_parse(&rs, RustStream<Stream>::read_callback);
        if (!txdata) {
            throw std::ios_base::failure("Failed to parse v5 Orchard bundle");
        }
        inner.reset(txdata);
    }
};

class OrchardWalletNoteCommitmentTreeSerializer;

class OrchardWallet
{
private:
    std::unique_ptr<OrchardWalletPtr, decltype(&orchard_wallet_free)> inner;

    static void PushOrchardNoteMeta(void* orchardNotesRet, RawOrchardNoteMetadata rawNoteMeta);

    friend class OrchardWalletNoteCommitmentTreeSerializer;
public:
    OrchardWallet() : inner(orchard_wallet_new(), orchard_wallet_free) {}

    OrchardWallet(OrchardWallet&& wallet) : inner(std::move(wallet.inner)) {}

    OrchardWallet& operator=(OrchardWallet&& wallet)
    {
        if (this != &wallet) {
            inner = std::move(wallet.inner);
        }
        return *this;
    }

    void CheckpointNoteCommitmentTree();

    bool RewindToLastCheckpoint();

    bool AddNotes(const CTransaction& tx);

    bool RestoreDecryptedNoteCache(const CTransaction& tx);

    bool AppendNoteCommitments(const int nHeight, const size_t blockTxIdx, const CTransaction& tx);

    bool IsMine(const uint256& txid);

    bool AddSpendingKey(const libzcash::OrchardSpendingKey& sk);

    bool AddFullViewingKey(const libzcash::OrchardFullViewingKey& fvk);

    bool AddIncomingViewingKey(const libzcash::OrchardIncomingViewingKey& ivk,
                               const libzcash::OrchardRawAddress& addr);

    void GetFilteredNotes(
        std::vector<OrchardNoteMetadata>& orchardNotesRet,
        const std::vector<libzcash::OrchardRawAddress>& addrs,
        bool ignoreSpent,
        bool requireSpendingKey);

    std::optional<OrchardWalletTx> GetTxData(const uint256 txid);

    OrchardWalletNoteCommitmentTreeSerializer GetNoteCommitmentTreeSer() const;
};

class OrchardWalletNoteCommitmentTreeSerializer
{
private:
    const OrchardWallet& wallet;
public:
    OrchardWalletNoteCommitmentTreeSerializer(const OrchardWallet& wallet): wallet(wallet) {}

    template<typename Stream>
    void Serialize(Stream& s) const {
        RustStream rs(s);
        if (!orchard_wallet_note_commitment_tree_serialize(wallet.inner.get(), &rs, RustStream<Stream>::write_callback)) {
            throw std::ios_base::failure("Failed to serialize v5 Orchard bundle");
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        RustStream rs(s);
        if (!orchard_wallet_note_commitment_tree_parse_update(wallet.inner.get(), &rs, RustStream<Stream>::read_callback)) {
            throw std::ios_base::failure("Failed to parse v5 Orchard bundle");
        }
    }
};

#endif // ZCASH_ORCHARD_WALLET_H
