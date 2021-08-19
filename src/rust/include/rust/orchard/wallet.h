// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_WALLET_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_WALLET_H

#include "rust/orchard/keys.h"
#include "rust/streams.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A type-safe pointer type for an Orchard wallet.
 */
struct OrchardWalletPtr;
typedef struct OrchardWalletPtr OrchardWalletPtr;

/**
 * A type-safe pointer type for per-transaction data stored by the Orchard wallet.
 *
 * This is used exclusively as a wrapper to handle serialization operations.
 */
struct OrchardWalletTxPtr;
typedef struct OrchardWalletTxPtr OrchardWalletTxPtr;

/**
 * Constructs a new empty Orchard wallet and return a pointer to it.
 * Memory is allocated by Rust and must be manually freed.
 */
OrchardWalletPtr* orchard_wallet_new();

/**
 * Frees the memory associated with an Orchard wallet that was allocated
 * by Rust.
 */
void orchard_wallet_free(OrchardWalletPtr* wallet);

/**
 * Adds a checkpoint to the wallet's note commitment tree to enable
 * a future rewind.
 */
void orchard_wallet_checkpoint(
        const OrchardWalletPtr* wallet
        );

/**
 * Rewinds to the most recently added checkpoint.
 */
bool orchard_wallet_rewind(
        const OrchardWalletPtr* wallet
        );

/**
 * Searches the provided bundle for notes that are visible to the specified
 * wallet's incoming viewing keys, and adds those notes to the wallet.
 */
bool orchard_wallet_add_notes_from_bundle(
        const OrchardWalletPtr* wallet,
        const unsigned char txid[32],
        const OrchardBundlePtr* bundle
        );

/**
 * Searches the provided bundle for notes that are visible to the specified
 * wallet's incoming viewing keys, and adds those notes to the wallet.
 */
bool orchard_wallet_restore_decrypted_note_cache(
        const OrchardWalletPtr* wallet,
        const unsigned char txid[32],
        const OrchardBundlePtr* bundle
        );

/**
 * Add the note commitment values for the specified bundle to the wallet's
 * note commitment tree, and mark any Orchard notes that belong to the wallet so that
 * we can construct authentication paths to these notes in the future.
 */
bool orchard_wallet_append_bundle_commitments(
        const OrchardWalletPtr* wallet,
        const unsigned int block_height,
        const size_t block_tx_idx,
        const unsigned char txid[32],
        const OrchardBundlePtr* bundle
        );

/**
 * Returns whether the specified transaction contains any Orchard notes that belong
 * to this wallet.
 */
bool orchard_wallet_tx_is_mine(
        const OrchardWalletPtr* wallet,
        const unsigned char txid[32]);

/**
 * Add the specified spending key to the wallet's key store.
 * This will also compute and add the associated incoming viewing key.
 */
void orchard_wallet_add_spending_key(
        const OrchardWalletPtr* wallet,
        const OrchardSpendingKeyPtr* sk);

/**
 * Add the specified full viewing key to the wallet's key store.
 * This will also compute and add the associated incoming viewing key.
 */
void orchard_wallet_add_full_viewing_key(
        const OrchardWalletPtr* wallet,
        const OrchardFullViewingKeyPtr* fvk);

/**
 * Add the specified incoming viewing key to the wallet's key store.
 */
void orchard_wallet_add_incoming_viewing_key(
        const OrchardWalletPtr* wallet,
        const OrchardIncomingViewingKeyPtr* fvk,
        const OrchardRawAddressPtr* addr);

/**
 * A C struct used to transfer note metadata information across the Rust FFI
 * boundary. This must have the same in-memory representation as the
 * `NoteMetadata` type in orchard_ffi/wallet.rs.
 */
struct RawOrchardNoteMetadata {
    unsigned char txid[32];
    uint32_t actionIdx;
    OrchardRawAddressPtr* addr;
    CAmount noteValue;
    unsigned char memo[512];
};

typedef void (*push_callback_t)(void* resultVector, const RawOrchardNoteMetadata noteMeta);

/**
 * Finds notes that belong to the wallet that match any of the provided
 * addresses, subject to the specified flags, and uses the provided callback to
 * push RawOrchardNoteMetadata values corresponding to those notes on to the
 * provided result vector. Note that the push_cb callback can perform any
 * necessary conversion from a RawOrchardNoteMetadata value prior in addition
 * to modifying the provided result vector.
 */
void orchard_wallet_get_filtered_notes(
        const OrchardWalletPtr* wallet,
        OrchardRawAddressPtr** addrs,
        size_t addrs_len,
        bool ignoreSpent,
        bool requireSpendingKey,
        void* resultVector,
        push_callback_t push_cb
        );

/**
 * Adds a OrchardWalletTx value to the wallet. This is used when restoring the wallet from
 * its serialized representation.
 */
void orchard_wallet_set_txdata(
        const OrchardWalletPtr* wallet,
        const OrchardWalletTxPtr* wtx
        );

/**
 * Retrieves an OrchardWalletTx from the wallet. Used for serialization.
 */
OrchardWalletTxPtr* orchard_wallet_get_txdata(
        const OrchardWalletPtr* wallet,
        const unsigned char txid[32]
        );

/**
 * Write the wallet's note commitment tree to the specified stream.
 */
bool orchard_wallet_note_commitment_tree_serialize(
        const OrchardWalletPtr* wallet,
        void* stream,
        write_callback_t write_cb);

/**
 * Read a note commitment tree from the specified stream,
 * and update the wallet's internal note commitment tree state
 * to equal the value read.
 */
bool orchard_wallet_note_commitment_tree_parse_update(
        const OrchardWalletPtr* wallet,
        void* stream,
        read_callback_t read_cb);

/**
 * Frees the memory associated with an Orchard txdata value that was allocated
 * by Rust.
 */
void orchard_wallet_tx_free(OrchardWalletTxPtr* txdata);

/**
 * Performs a deep copy of the txdata value and returns a pointer to the newly
 * allocated memory. This memory must be manually freed to prevent leaks.
 */
OrchardWalletTxPtr* orchard_wallet_tx_clone(const OrchardWalletTxPtr* txdata);

/**
 * Serializes a txdata value to the specified stream.
 */
bool orchard_wallet_tx_serialize(
    const OrchardWalletTxPtr* ptr,
    void* stream,
    write_callback_t write_cb);

/**
 * Parses an authorized Orchard txdata from the given stream.
 *
 * - If no error occurs, the resulting pointer will point to a Rust-allocated Orchard txdata
 *   which must be manually freed by the caller.
 * - If an error occurs, the result will be the null pointer.
 */
OrchardWalletTxPtr* orchard_wallet_tx_parse(
    void* stream,
    read_callback_t read_cb);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_WALLET_H


