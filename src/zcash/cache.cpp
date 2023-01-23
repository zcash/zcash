// Copyright (c) 2022-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zcash/cache.h"
#include "util/system.h"

namespace libzcash
{
std::unique_ptr<BundleValidityCache> NewBundleValidityCache(rust::Str kind, size_t nMaxCacheSize)
{
    auto cache = std::unique_ptr<BundleValidityCache>(new BundleValidityCache());
    size_t nElems = cache->setup_bytes(nMaxCacheSize);
    LogPrintf("Using %zu MiB out of %zu requested for %s bundle cache, able to store %zu elements\n",
              (nElems * sizeof(BundleCacheEntry)) >> 20, nMaxCacheSize >> 20, kind, nElems);
    return cache;
}
} // namespace libzcash

// Explicit instantiations for libzcash::BundleValidityCache
template void libzcash::BundleValidityCache::insert(libzcash::BundleCacheEntry e);
template bool libzcash::BundleValidityCache::contains(const libzcash::BundleCacheEntry& e, const bool erase) const;
