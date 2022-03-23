// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ADDRESS_H
#define ZCASH_RUST_INCLUDE_RUST_ADDRESS_H

#include "rust/orchard/keys.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*orchard_receiver_t)(void* ua, OrchardRawAddressPtr* addr);
typedef bool (*raw_to_receiver_t)(void* ua, const unsigned char* raw);
typedef bool (*unknown_receiver_t)(
    void* ua,
    uint32_t typecode,
    const unsigned char* data,
    size_t len);
typedef uint32_t (*get_typecode_t)(const void* ua, size_t index);
typedef size_t (*get_receiver_len_t)(const void* ua, size_t index);
typedef void (*get_receiver_t)(const void* ua, size_t index, unsigned char* data, size_t length);

/// Parses a unified address from the given string.
bool zcash_address_parse_unified(
    const char* str,
    const char* network,
    void* ua,
    orchard_receiver_t orchard_cb,
    raw_to_receiver_t sapling_cb,
    raw_to_receiver_t p2sh_cb,
    raw_to_receiver_t p2pkh_cb,
    unknown_receiver_t unknown_cb);

/// Serializes the given unified address to a string.
///
/// Returns nullptr if the unified address is invalid.
char* zcash_address_serialize_unified(
    const char* network,
    const void* ua,
    size_t receivers_len,
    get_typecode_t typecode_cb,
    get_receiver_len_t receiver_len_cb,
    get_receiver_t receiver_cb);

/// Frees a string returned by `zcash_address_serialize_unified`.
void zcash_address_string_free(char* str);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ADDRESS_H
