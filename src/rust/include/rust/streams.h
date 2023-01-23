// Copyright (c) 2020-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_STREAMS_H
#define ZCASH_RUST_INCLUDE_RUST_STREAMS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The type that Rust expects for its `CppStreamReader` callback.
typedef long (*read_callback_t)(void* context, unsigned char* pch, size_t nSize);
/// The type that Rust expects for its `CppStreamWriter` callback.
typedef long (*write_callback_t)(void* context, const unsigned char* pch, size_t nSize);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_STREAMS_H
