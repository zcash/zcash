// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_WALLET_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_WALLET_H

#include "rust/orchard/keys.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A type-safe pointer type for an Orchard wallet.
 */
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
 * Adds a checkpoint to the wallet's note commitment tree to enable
 * a future rewind.
 */
void orchard_wallet_checkpoint(
        OrchardWalletPtr* wallet
        );

/**
 * Rewinds to the most recently added checkpoint.
 */
bool orchard_wallet_rewind(
        OrchardWalletPtr* wallet
        );

/**
 * Searches the provided bundle for notes that are visible to the specified
 * wallet's incoming viewing keys, and adds those notes to the wallet.
 */
void orchard_wallet_add_notes_from_bundle(
        OrchardWalletPtr* wallet,
        const unsigned char txid[32],
        const OrchardBundlePtr* bundle
        );

/**
 * Add the note commitment values for the specified bundle to the wallet's note
 * commitment tree, and mark any Orchard notes that belong to the wallet so
 * that we can construct authentication paths to these notes in the future.
 *
 * This requires the block height and the index of the block within the
 * transaction in order to guarantee that note commitments are appended in the
 * correct order. Returns `false` if the provided bundle is not in the correct
 * position to have its note commitments appended to the note commitment tree.
 */
bool orchard_wallet_append_bundle_commitments(
        OrchardWalletPtr* wallet,
        const uint32_t block_height,
        const size_t block_tx_idx,
        const unsigned char txid[32],
        const OrchardBundlePtr* bundle
        );

/**
 * Returns whether the specified transaction contains any Orchard notes that belong
 * to this wallet.
 */
bool orchard_wallet_tx_contains_my_notes(
        const OrchardWalletPtr* wallet,
        const unsigned char txid[32]);

/**
 * Add the specified spending key to the wallet's key store.
 * This will also compute and add the associated incoming viewing key.
 */
void orchard_wallet_add_spending_key(
        OrchardWalletPtr* wallet,
        const OrchardSpendingKeyPtr* sk);

/**
 * Add the specified full viewing key to the wallet's key store.
 * This will also compute and add the associated incoming viewing key.
 */
void orchard_wallet_add_full_viewing_key(
        OrchardWalletPtr* wallet,
        const OrchardFullViewingKeyPtr* fvk);

/**
 * Add the specified raw address to the wallet's key store,
 * associated with the incoming viewing key from which that
 * address was derived.
 */
bool orchard_wallet_add_raw_address(
        OrchardWalletPtr* wallet,
        const OrchardRawAddressPtr* addr,
        const OrchardIncomingViewingKeyPtr* ivk);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_WALLET_H


