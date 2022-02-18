// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H

#include "rust/streams.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A pointer to an Orchard shielded payment address, as defined in
 * https://zips.z.cash/protocol/nu5.pdf#orchardpaymentaddrencoding
 */
struct OrchardRawAddressPtr;
typedef struct OrchardRawAddressPtr OrchardRawAddressPtr;

/**
 * Clones the given Orchard payment address and returns
 * a pointer to the newly created value. Both the original
 * one's memory and the newly allocated one need to be freed
 * independently.
 */
OrchardRawAddressPtr* orchard_address_clone(
        const OrchardRawAddressPtr* ptr);

/**
 * Frees the memory allocated for the given Orchard payment address
 */
void orchard_address_free(OrchardRawAddressPtr* ptr);

/**
 * Parses Orchard raw address bytes from the given stream.
 *
 * - If the key does not parse correctly, the returned pointer will be null.
 */
OrchardRawAddressPtr* orchard_raw_address_parse(
    void* stream,
    read_callback_t read_cb);


/**
 * Serializes Orchard raw address bytes to the given stream.
 *
 * This will return `false` and leave the stream unmodified if
 * `raw_address == nullptr`;
 */
bool orchard_raw_address_serialize(
    const OrchardRawAddressPtr* raw_address,
    void* stream,
    write_callback_t write_cb);

/**
 * Implements the "equal" operation for comparing two Orchard addresses.
 */
bool orchard_address_eq(
    const OrchardRawAddressPtr* k0,
    const OrchardRawAddressPtr* k1);

/**
 * Implements the "less than" operation `k0 < k1` for comparing two Orchard
 * addresses.  This is a comparison of the raw bytes, only useful for cases
 * where a semantically irrelevant ordering is needed (such as for map keys).
 */
bool orchard_address_lt(
    const OrchardRawAddressPtr* k0,
    const OrchardRawAddressPtr* k1);

//
// Incoming Viewing Keys
//

struct OrchardIncomingViewingKeyPtr;
typedef struct OrchardIncomingViewingKeyPtr OrchardIncomingViewingKeyPtr;

/**
 * Clones the given Orchard incoming viewing key and returns
 * a pointer to the newly created value. Both the original
 * one's memory and the newly allocated one need to be freed
 * independently.
 */
OrchardIncomingViewingKeyPtr* orchard_incoming_viewing_key_clone(
        const OrchardIncomingViewingKeyPtr* ptr);

/**
 * Frees the memory allocated for the given Orchard incoming viewing key
 */
void orchard_incoming_viewing_key_free(OrchardIncomingViewingKeyPtr* ptr);

/**
 * Returns the address at the specified diversifier index.
 */
OrchardRawAddressPtr* orchard_incoming_viewing_key_to_address(
    const OrchardIncomingViewingKeyPtr* incoming_viewing_key,
    const unsigned char* j);

/**
 * Parses an Orchard incoming viewing key from the given stream.
 *
 * - If the key does not parse correctly, the returned pointer will be null.
 */
OrchardIncomingViewingKeyPtr* orchard_incoming_viewing_key_parse(
    void* stream,
    read_callback_t read_cb);


/**
 * Serializes an Orchard incoming viewing key to the given stream.
 *
 * This will return `false` and leave the stream unmodified if
 * `incoming_viewing_key == nullptr`.
 */
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

/**
 * Implements equality testing between incoming viewing keys.
 */
bool orchard_incoming_viewing_key_eq(
    const OrchardIncomingViewingKeyPtr* k0,
    const OrchardIncomingViewingKeyPtr* k1);

//
// Full Viewing Keys
//

struct OrchardFullViewingKeyPtr;
typedef struct OrchardFullViewingKeyPtr OrchardFullViewingKeyPtr;

/**
 * Clones the given Orchard full viewing key and returns
 * a pointer to the newly created value. Both the original
 * one's memory and the newly allocated one need to be freed
 * independently.
 */
OrchardFullViewingKeyPtr* orchard_full_viewing_key_clone(
        const OrchardFullViewingKeyPtr* ptr);

/**
 * Free the memory allocated for the given Orchard full viewing key
 */
void orchard_full_viewing_key_free(OrchardFullViewingKeyPtr* ptr);

/**
 * Parses an Orchard full viewing key from the given stream.
 *
 * - If the key does not parse correctly, the returned pointer will be null.
 */
OrchardFullViewingKeyPtr* orchard_full_viewing_key_parse(
    void* stream,
    read_callback_t read_cb);

/**
 * Serializes an Orchard full viewing key to the given stream.
 *
 * This will return `false` and leave the stream unmodified if
 * `full_viewing_key == nullptr`.
 */
bool orchard_full_viewing_key_serialize(
    const OrchardFullViewingKeyPtr* full_viewing_key,
    void* stream,
    write_callback_t write_cb);

/**
 * Returns the incoming viewing key for the specified full viewing key.
 */
OrchardIncomingViewingKeyPtr* orchard_full_viewing_key_to_incoming_viewing_key(
    const OrchardFullViewingKeyPtr* key);

/**
 * Implements equality testing between full viewing keys.
 */
bool orchard_full_viewing_key_eq(
    const OrchardFullViewingKeyPtr* k0,
    const OrchardFullViewingKeyPtr* k1);

//
// Spending Keys
//

struct OrchardSpendingKeyPtr;
typedef struct OrchardSpendingKeyPtr OrchardSpendingKeyPtr;

/**
 * Constructs a spending key by ZIP-32 derivation to the account
 * level from the provided seed.
 */
OrchardSpendingKeyPtr* orchard_spending_key_for_account(
        const unsigned char* seed,
        size_t seed_len,
        uint32_t bip44_coin_type,
        uint32_t account_id);

/**
 * Clones the given Orchard spending key and returns
 * a pointer to the newly created value. Both the original
 * one's memory and the newly allocated one need to be freed
 * independently.
 */
OrchardSpendingKeyPtr* orchard_spending_key_clone(
        const OrchardSpendingKeyPtr* ptr);

/**
 * Free the memory allocated for the given Orchard spending key
 */
void orchard_spending_key_free(OrchardSpendingKeyPtr* ptr);

/**
 * Parses an Orchard spending key from the given stream.
 *
 * - If the key does not parse correctly, the returned pointer will be null.
 */
OrchardSpendingKeyPtr* orchard_spending_key_parse(
    void* stream,
    read_callback_t read_cb);

/**
 * Serializes an Orchard spending key to the given stream.
 *
 * This will return `false` and leave the stream unmodified if
 * `spending_key == nullptr`.
 */
bool orchard_spending_key_serialize(
    const OrchardSpendingKeyPtr* spending_key,
    void* stream,
    write_callback_t write_cb);

/**
 * Returns the full viewing key for the specified spending key.
 *
 * The resulting pointer must be ultimately freed by the caller
 * using `orchard_full_viewing_key_free`.
 */
OrchardFullViewingKeyPtr* orchard_spending_key_to_full_viewing_key(
    const OrchardSpendingKeyPtr* key);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H
