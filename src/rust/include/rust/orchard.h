// Copyright (c) 2020-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_H

#include "rust/streams.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Typesafe pointer to a Rust-allocated orchard::bundle::Bundle value
struct OrchardBundlePtr;
typedef struct OrchardBundlePtr OrchardBundlePtr;

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_H
