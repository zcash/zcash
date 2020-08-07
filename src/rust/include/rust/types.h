// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef RUST_TYPES_H_
#define RUST_TYPES_H_

#include <stdint.h>

#ifdef WIN32
typedef uint16_t codeunit;
#else
typedef uint8_t codeunit;
#endif

#endif // RUST_TYPES_H_
