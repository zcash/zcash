// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_TYPES_H
#define ZCASH_RUST_INCLUDE_RUST_TYPES_H

#include <stdint.h>

#ifdef WIN32
typedef uint16_t codeunit;
#else
typedef uint8_t codeunit;
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_TYPES_H
