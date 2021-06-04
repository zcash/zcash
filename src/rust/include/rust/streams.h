// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_STREAMS_H
#define ZCASH_RUST_INCLUDE_RUST_STREAMS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long (*read_callback_t)(void* context, unsigned char* pch, size_t nSize);
typedef long (*write_callback_t)(void* context, const unsigned char* pch, size_t nSize);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_STREAMS_H
