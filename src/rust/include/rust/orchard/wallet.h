// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_WALLET_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_WALLET_H

#include "rust/orchard/keys.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OrchardWalletPtr;
typedef struct OrchardWalletPtr OrchardWalletPtr;

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
 * Performs a deep copy of the wallet and return a pointer to the newly
 * allocated memory. This memory must be manually freed to prevent leaks.
 */
OrchardWalletPtr* orchard_wallet_clone(const OrchardWalletPtr* wallet);

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
        const unsigned char* txid,
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
        const unsigned char* txid,
        const OrchardBundlePtr* bundle
        );

/**
 * Returns whether the specified transaction contains any Orchard notes that belong
 * to this wallet.
 */
bool orchard_wallet_tx_is_mine(
        const OrchardWalletPtr* wallet,
        const unsigned char* txid);

void orchard_wallet_add_spending_key(
        const OrchardWalletPtr* wallet,
        const OrchardSpendingKeyPtr* sk);

void orchard_wallet_add_full_viewing_key(
        const OrchardWalletPtr* wallet,
        const OrchardFullViewingKeyPtr* fvk);

void orchard_wallet_add_incoming_viewing_key(
        const OrchardWalletPtr* wallet,
        const OrchardIncomingViewingKeyPtr* fvk,
        const OrchardRawAddressPtr* addr);

struct RawOrchardNoteMetadata {
    unsigned char txid[32];
    uint32_t actionIdx;
    OrchardRawAddressPtr* addr;
    CAmount noteValue;
    unsigned char memo[512];
};

typedef void (*push_callback_t)(void* context, const RawOrchardNoteMetadata noteMeta);

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
 * A type-safe pointer type for per-transaction data stored by the Orchard wallet.
 *
 * This is used exclusively as a wrapper to handle serialization operations.
 */
//struct OrchardWalletTxDataPtr;
//typedef struct OrchardWalletTxDataPtr OrchardWalletTxDataPtr;
//
//void orchard_wallet_tx_data_free(OrchardWalletTxDataPtr* txdata);
//
//OrchardWalletTxDataPtr orchard_wallet_tx_data_clone(const OrchardWalletTxDataPtr* txdata);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_WALLET_H


