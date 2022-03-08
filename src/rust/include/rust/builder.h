// Copyright (c) 2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_BUILDER_H
#define ZCASH_RUST_INCLUDE_RUST_BUILDER_H

#include "rust/orchard.h"
#include "rust/orchard/keys.h"
#include "rust/transaction.h"

#ifdef __cplusplus
extern "C" {
#endif

/// A type-safe pointer to a Rust-allocated struct containing the information
/// needed to spend an Orchard note.
struct OrchardSpendInfoPtr;
typedef struct OrchardSpendInfoPtr OrchardSpendInfoPtr;

/// Pointer to Rust-allocated Orchard bundle builder.
struct OrchardBuilderPtr;
typedef struct OrchardBuilderPtr OrchardBuilderPtr;

/// Pointer to Rust-allocated Orchard bundle without proofs
/// or authorizing data.
struct OrchardUnauthorizedBundlePtr;
typedef struct OrchardUnauthorizedBundlePtr OrchardUnauthorizedBundlePtr;

/// Frees the memory associated with an Orchard spend info struct that was
/// allocated by Rust.
void orchard_spend_info_free(OrchardSpendInfoPtr* ptr);

/// Construct a new Orchard transaction builder.
///
/// If `anchor` is `null`, the root of the empty Orchard commitment tree is used.
OrchardBuilderPtr* orchard_builder_new(
    bool spends_enabled,
    bool outputs_enabled,
    const unsigned char* anchor);

/// Frees an Orchard builder returned from `orchard_builder_new`.
void orchard_builder_free(OrchardBuilderPtr* ptr);

/// Adds a note to be spent in this bundle.
///
/// Returns `false` if the Merkle path in `spend_info` does not have the
/// required anchor.
///
/// `spend_info` is always freed by this method.
bool orchard_builder_add_spend(
    OrchardBuilderPtr* ptr,
    OrchardSpendInfoPtr* spend_info);

/// Adds an address which will receive funds in this bundle.
///
/// `ovk` is a pointer to the outgoing viewing key to make this recipient recoverable by,
/// or `null` to make the recipient unrecoverable by the sender.
///
/// `memo` is a pointer to the 512-byte memo field encoding, or `null` for "no memo".
bool orchard_builder_add_recipient(
    OrchardBuilderPtr* ptr,
    const unsigned char* ovk,
    const OrchardRawAddressPtr* recipient,
    uint64_t value,
    const unsigned char* memo);

/// Builds a bundle containing the given spent notes and recipients.
///
/// Returns `null` if an error occurs.
///
/// `builder` is always freed by this method.
OrchardUnauthorizedBundlePtr* orchard_builder_build(OrchardBuilderPtr* builder);

/// Frees an Orchard bundle returned from `orchard_bundle_build`.
void orchard_unauthorized_bundle_free(OrchardUnauthorizedBundlePtr* bundle);

/// Adds proofs and signatures to the bundle.
///
/// Returns `null` if an error occurs.
///
/// `bundle` is always freed by this method.
OrchardBundlePtr* orchard_unauthorized_bundle_prove_and_sign(
    OrchardUnauthorizedBundlePtr* bundle,
    const OrchardSpendingKeyPtr** keys,
    size_t keys_len,
    const unsigned char* sighash);

/// Calculates a ZIP 244 shielded signature digest for the given under-construction
/// transaction.
///
/// Returns `false` if any of the parameters are invalid; in this case, `sighash_ret`
/// will be unaltered.
///
/// `preTx` is always freed by this method.
bool zcash_builder_zip244_shielded_signature_digest(
    PrecomputedTxParts* preTx,
    const OrchardUnauthorizedBundlePtr* bundle,
    unsigned char* sighash_ret);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_BUILDER_H
