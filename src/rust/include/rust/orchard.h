// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_H

#include "rust/streams.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct OrchardBundlePtr;
typedef struct OrchardBundlePtr OrchardBundlePtr;

/// Clones the given Orchard bundle.
///
/// Both bundles need to be separately freed when they go out of scope.
OrchardBundlePtr* orchard_bundle_clone(const OrchardBundlePtr* span);

/// Frees an Orchard bundle returned from `orchard_parse_bundle`.
void orchard_bundle_free(OrchardBundlePtr *);

/// Parses an authorized Orchard bundle from the given stream.
///
/// - If no error occurs, `bundle` will point to a Rust-allocated Orchard bundle.
/// - If an error occurs, `bundle` will be unaltered.
OrchardBundlePtr* orchard_bundle_parse(void* stream, read_callback_t read_cb);

/// Serializes an authorized Orchard bundle to the given stream
///
/// If `bundle == nullptr`, this serializes `nActionsOrchard = 0`.
bool orchard_bundle_serialize(
    const OrchardBundlePtr* bundle,
    void* stream,
    write_callback_t write_cb);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_H
