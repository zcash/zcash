// Copyright (c) 2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_INT128_H
#define ZCASH_INT128_H

#include <cstdint>

// <cstdint> will define INT128_MAX iff (in some future world) it provides int128_t.
// Otherwise use the __int128 extension which is supported in clang and gcc.
#ifndef INT128_MAX
typedef __int128 int128_t;
#define INT128_MAX (std::numeric_limits<int128_t>::max())
#define INT128_MIN (std::numeric_limits<int128_t>::min())
#endif

// <cstdint> will define UINT128_MAX iff (in some future world) it provides uint128_t.
// Otherwise use the __uint128_t extension which is supported in clang and gcc.
#ifndef UINT128_MAX
typedef __uint128_t uint128_t;
#define UINT128_MAX (std::numeric_limits<uint128_t>::max())
#endif

#endif // ZCASH_INT128_H
