// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H

#include "rust/streams.h"

#ifdef __cplusplus
extern "C" {
#endif

// A pointer to an Orchard shielded payment address, as defined in
// https://zips.z.cash/protocol/nu5.pdf#orchardpaymentaddrencoding
struct OrchardPaymentAddressPtr;
typedef struct OrchardPaymentAddressPtr OrchardPaymentAddressPtr;

// Clones the given Orchard payment address and returns
// a pointer to the newly created value. Both the original
// one's memory and the newly allocated one need to be freed
// independently.
OrchardPaymentAddressPtr* orchard_payment_address_clone(
        const OrchardPaymentAddressPtr* ptr);

// Free the memory allocated for the given Orchard payment address
void orchard_payment_address_free(OrchardPaymentAddressPtr* ptr);

/// Parses an Orchard payment address from the given stream.
///
/// - If no error occurs, `payment_address_ret` will point to a Rust-allocated Orchard payment address.
/// - If an error occurs, `payment_address_ret` will be unaltered.
bool orchard_payment_address_parse(
    void* stream,
    read_callback_t read_cb,
    OrchardPaymentAddressPtr** payment_address_ret);

/// Serializes an Orchard payment address to the given stream
///
/// If `payment_address == nullptr`, this will return `false`
bool orchard_payment_address_serialize(
    const OrchardPaymentAddressPtr* payment_address,
    void* stream,
    write_callback_t write_cb);

struct OrchardIncomingViewingKeyPtr;
typedef struct OrchardIncomingViewingKeyPtr OrchardIncomingViewingKeyPtr;

// Clones the given Orchard incoming viewing key and returns
// a pointer to the newly created value. Both the original
// one's memory and the newly allocated one need to be freed
// independently.
OrchardIncomingViewingKeyPtr* orchard_incoming_viewing_key_clone(
        const OrchardIncomingViewingKeyPtr* ptr);

// Free the memory allocated for the given Orchard incoming viewing key
void orchard_incoming_viewing_key_free(OrchardIncomingViewingKeyPtr* ptr);

/// Parses an authorized Orchard incoming_viewing_key from the given stream.
///
/// - If no error occurs, `incoming_viewing_key_ret` will point to a Rust-allocated Orchard incoming_viewing_key.
/// - If an error occurs, `incoming_viewing_key_ret` will be unaltered.
bool orchard_incoming_viewing_key_parse(
    void* stream,
    read_callback_t read_cb,
    OrchardIncomingViewingKeyPtr** incoming_viewing_key_ret);

/// Serializes an authorized Orchard incoming_viewing_key to the given stream
///
/// If `incoming_viewing_key == nullptr`, this will return `false`
bool orchard_incoming_viewing_key_serialize(
    const OrchardIncomingViewingKeyPtr* incoming_viewing_key,
    void* stream,
    write_callback_t write_cb);

/**
 * Implements the "less than" operation for comparing two keys.
 */
bool orchard_incoming_viewing_key_lt(
    const OrchardIncomingViewingKeyPtr* k0,
    const OrchardIncomingViewingKeyPtr* k1);

//
// Full Viewing Keys
//
struct OrchardFullViewingKeyPtr;
typedef struct OrchardFullViewingKeyPtr OrchardFullViewingKeyPtr;

// Clones the given Orchard full viewing key and returns
// a pointer to the newly created value. Both the original
// one's memory and the newly allocated one need to be freed
// independently.
OrchardFullViewingKeyPtr* orchard_full_viewing_key_clone(
        const OrchardFullViewingKeyPtr* ptr);

// Free the memory allocated for the given Orchard full viewing key
void orchard_full_viewing_key_free(OrchardFullViewingKeyPtr* ptr);

/// Parses an authorized Orchard full_viewing_key from the given stream.
///
/// - If no error occurs, `full_viewing_key_ret` will point to a Rust-allocated Orchard full_viewing_key.
/// - If an error occurs, `full_viewing_key_ret` will be unaltered.
bool orchard_full_viewing_key_parse(
    void* stream,
    read_callback_t read_cb,
    OrchardFullViewingKeyPtr** full_viewing_key_ret);

/// Serializes an authorized Orchard full_viewing_key to the given stream
///
/// If `full_viewing_key == nullptr`, this will return `false`
bool orchard_full_viewing_key_serialize(
    const OrchardFullViewingKeyPtr* full_viewing_key,
    void* stream,
    write_callback_t write_cb);

//
// Spending Keys
//
struct OrchardSpendingKeyPtr;
typedef struct OrchardSpendingKeyPtr OrchardSpendingKeyPtr;

// Clones the given Orchard spending key and returns
// a pointer to the newly created value. Both the original
// one's memory and the newly allocated one need to be freed
// independently.
OrchardSpendingKeyPtr* orchard_spending_key_clone(
        const OrchardSpendingKeyPtr* ptr);

// Free the memory allocated for the given Orchard spending key
void orchard_spending_key_free(OrchardSpendingKeyPtr* ptr);

/// Parses an authorized Orchard spending_key from the given stream.
///
/// - If no error occurs, `spending_key_ret` will point to a Rust-allocated Orchard spending_key.
/// - If an error occurs, `spending_key_ret` will be unaltered.
bool orchard_spending_key_parse(
    void* stream,
    read_callback_t read_cb,
    OrchardSpendingKeyPtr** spending_key_ret);

/// Serializes an authorized Orchard spending_key to the given stream
///
/// If `spending_key == nullptr`, this will return `false`
bool orchard_spending_key_serialize(
    const OrchardSpendingKeyPtr* spending_key,
    void* stream,
    write_callback_t write_cb);

OrchardIncomingViewingKeyPtr* orchard_spending_key_to_incoming_viewing_key(
    const OrchardSpendingKeyPtr* spending_key);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H
