// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H

#include "rust/streams.h"

#ifdef __cplusplus
extern "C" {
#endif

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
/// If `incoming_viewing_key == nullptr`, this serializes `nActionsOrchard = 0`.
bool orchard_incoming_viewing_key_serialize(
    const OrchardIncomingViewingKeyPtr* incoming_viewing_key,
    void* stream,
    write_callback_t write_cb);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_KEYS_H
