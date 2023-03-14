// Copyright (c) 2022-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZCASH_ZCASH_CACHE_H
#define ZCASH_ZCASH_CACHE_H

#include "cuckoocache.h"

#include <rust/cxx.h>

#include <array>

namespace libzcash
{
typedef std::array<uint8_t, 32> BundleCacheEntry;

/**
 * We're hashing a nonce into the entries themselves, so we don't need extra
 * blinding in the set hash computation.
 *
 * This may exhibit platform endian dependent behavior but because these are
 * nonced hashes (random) and this state is only ever used locally it is safe.
 * All that matters is local consistency.
 */
class BundleCacheHasher
{
public:
    template <uint8_t hash_select>
    uint32_t operator()(const BundleCacheEntry& key) const
    {
        static_assert(hash_select < 8, "BundleCacheHasher only has 8 hashes available.");
        uint32_t u;
        std::memcpy(&u, key.begin() + 4 * hash_select, 4);
        return u;
    }
};

typedef CuckooCache::cache<BundleCacheEntry, BundleCacheHasher> BundleValidityCache;

std::unique_ptr<BundleValidityCache> NewBundleValidityCache(rust::Str kind, size_t nMaxCacheSize);
} // namespace libzcash

#endif // ZCASH_ZCASH_CACHE_H
