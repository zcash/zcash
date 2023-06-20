// Copyright (c) 2012-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "coins.h"

#include "memusage.h"
#include "random.h"
#include "version.h"

#include <assert.h>

#include <tracing.h>

/**
 * calculate number of bytes for the bitmask, and its number of non-zero bytes
 * each bit in the bitmask represents the availability of one output, but the
 * availabilities of the first two outputs are encoded separately
 */
void CCoins::CalcMaskSize(unsigned int &nBytes, unsigned int &nNonzeroBytes) const {
    unsigned int nLastUsedByte = 0;
    for (unsigned int b = 0; 2+b*8 < vout.size(); b++) {
        bool fZero = true;
        for (unsigned int i = 0; i < 8 && 2+b*8+i < vout.size(); i++) {
            if (!vout[2+b*8+i].IsNull()) {
                fZero = false;
                continue;
            }
        }
        if (!fZero) {
            nLastUsedByte = b + 1;
            nNonzeroBytes++;
        }
    }
    nBytes += nLastUsedByte;
}

bool CCoins::Spend(uint32_t nPos)
{
    if (nPos >= vout.size() || vout[nPos].IsNull())
        return false;
    vout[nPos].SetNull();
    Cleanup();
    return true;
}

CCoinsViewBacked::CCoinsViewBacked(CCoinsView *viewIn) : base(viewIn) { }

bool CCoinsViewBacked::GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const { return base->GetSproutAnchorAt(rt, tree); }
bool CCoinsViewBacked::GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const { return base->GetSaplingAnchorAt(rt, tree); }
bool CCoinsViewBacked::GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleFrontier &tree) const { return base->GetOrchardAnchorAt(rt, tree); }
bool CCoinsViewBacked::GetNullifier(const uint256 &nullifier, ShieldedType type) const { return base->GetNullifier(nullifier, type); }
bool CCoinsViewBacked::GetCoins(const uint256 &txid, CCoins &coins) const { return base->GetCoins(txid, coins); }
bool CCoinsViewBacked::HaveCoins(const uint256 &txid) const { return base->HaveCoins(txid); }
uint256 CCoinsViewBacked::GetBestBlock() const { return base->GetBestBlock(); }
uint256 CCoinsViewBacked::GetBestAnchor(ShieldedType type) const { return base->GetBestAnchor(type); }
HistoryIndex CCoinsViewBacked::GetHistoryLength(uint32_t epochId) const { return base->GetHistoryLength(epochId); }
HistoryNode CCoinsViewBacked::GetHistoryAt(uint32_t epochId, HistoryIndex index) const { return base->GetHistoryAt(epochId, index); }
uint256 CCoinsViewBacked::GetHistoryRoot(uint32_t epochId) const { return base->GetHistoryRoot(epochId); }
std::optional<libzcash::LatestSubtree> CCoinsViewBacked::GetLatestSubtree(ShieldedType type) const { return base->GetLatestSubtree(type); }
std::optional<libzcash::SubtreeData> CCoinsViewBacked::GetSubtreeData(ShieldedType type, libzcash::SubtreeIndex index) const { return base->GetSubtreeData(type, index); }
void CCoinsViewBacked::SetBackend(CCoinsView &viewIn) { base = &viewIn; }
bool CCoinsViewBacked::BatchWrite(CCoinsMap &mapCoins,
                                  const uint256 &hashBlock,
                                  const uint256 &hashSproutAnchor,
                                  const uint256 &hashSaplingAnchor,
                                  const uint256 &hashOrchardAnchor,
                                  CAnchorsSproutMap &mapSproutAnchors,
                                  CAnchorsSaplingMap &mapSaplingAnchors,
                                  CAnchorsOrchardMap &mapOrchardAnchors,
                                  CNullifiersMap &mapSproutNullifiers,
                                  CNullifiersMap &mapSaplingNullifiers,
                                  CNullifiersMap &mapOrchardNullifiers,
                                  CHistoryCacheMap &historyCacheMap,
                                  SubtreeCache &cacheSaplingSubtrees,
                                  SubtreeCache &cacheOrchardSubtrees) {
    return base->BatchWrite(mapCoins, hashBlock,
                            hashSproutAnchor, hashSaplingAnchor, hashOrchardAnchor,
                            mapSproutAnchors, mapSaplingAnchors, mapOrchardAnchors,
                            mapSproutNullifiers, mapSaplingNullifiers, mapOrchardNullifiers,
                            historyCacheMap, cacheSaplingSubtrees, cacheOrchardSubtrees);
}
bool CCoinsViewBacked::GetStats(CCoinsStats &stats) const { return base->GetStats(stats); }

SaltedTxidHasher::SaltedTxidHasher() : k0(GetRand(std::numeric_limits<uint64_t>::max())), k1(GetRand(std::numeric_limits<uint64_t>::max())) {}

CCoinsViewCache::CCoinsViewCache(CCoinsView *baseIn) : CCoinsViewBacked(baseIn), hasModifier(false), cachedCoinsUsage(0) { }

CCoinsViewCache::~CCoinsViewCache()
{
    assert(!hasModifier);
}

size_t CCoinsViewCache::DynamicMemoryUsage() const {
    return memusage::DynamicUsage(cacheCoins) +
           memusage::DynamicUsage(cacheSproutAnchors) +
           memusage::DynamicUsage(cacheSaplingAnchors) +
           memusage::DynamicUsage(cacheOrchardAnchors) +
           memusage::DynamicUsage(cacheSproutNullifiers) +
           memusage::DynamicUsage(cacheSaplingNullifiers) +
           memusage::DynamicUsage(cacheOrchardNullifiers) +
           memusage::DynamicUsage(historyCacheMap) +
           memusage::DynamicUsage(cacheSaplingSubtrees) +
           memusage::DynamicUsage(cacheOrchardSubtrees) +
           cachedCoinsUsage;
}

CCoinsMap::const_iterator CCoinsViewCache::FetchCoins(const uint256 &txid) const {
    CCoinsMap::iterator it = cacheCoins.find(txid);
    if (it != cacheCoins.end())
        return it;
    CCoins tmp;
    if (!base->GetCoins(txid, tmp))
        return cacheCoins.end();
    CCoinsMap::iterator ret = cacheCoins.insert(std::make_pair(txid, CCoinsCacheEntry())).first;
    tmp.swap(ret->second.coins);
    if (ret->second.coins.IsPruned()) {
        // The parent only has an empty entry for this txid; we can consider our
        // version as fresh.
        ret->second.flags = CCoinsCacheEntry::FRESH;
    }
    cachedCoinsUsage += ret->second.coins.DynamicMemoryUsage();
    return ret;
}


bool CCoinsViewCache::GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const {
    CAnchorsSproutMap::const_iterator it = cacheSproutAnchors.find(rt);
    if (it != cacheSproutAnchors.end()) {
        if (it->second.entered) {
            tree = it->second.tree;
            return true;
        } else {
            return false;
        }
    }

    if (!base->GetSproutAnchorAt(rt, tree)) {
        return false;
    }

    CAnchorsSproutMap::iterator ret = cacheSproutAnchors.insert(std::make_pair(rt, CAnchorsSproutCacheEntry())).first;
    ret->second.entered = true;
    ret->second.tree = tree;
    cachedCoinsUsage += ret->second.tree.DynamicMemoryUsage();

    return true;
}

bool CCoinsViewCache::GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
    CAnchorsSaplingMap::const_iterator it = cacheSaplingAnchors.find(rt);
    if (it != cacheSaplingAnchors.end()) {
        if (it->second.entered) {
            tree = it->second.tree;
            return true;
        } else {
            return false;
        }
    }

    if (!base->GetSaplingAnchorAt(rt, tree)) {
        return false;
    }

    CAnchorsSaplingMap::iterator ret = cacheSaplingAnchors.insert(std::make_pair(rt, CAnchorsSaplingCacheEntry())).first;
    ret->second.entered = true;
    ret->second.tree = tree;
    cachedCoinsUsage += ret->second.tree.DynamicMemoryUsage();

    return true;
}

bool CCoinsViewCache::GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleFrontier &tree) const {
    CAnchorsOrchardMap::const_iterator it = cacheOrchardAnchors.find(rt);
    if (it != cacheOrchardAnchors.end()) {
        if (it->second.entered) {
            tree = it->second.tree;
            return true;
        } else {
            return false;
        }
    }

    if (!base->GetOrchardAnchorAt(rt, tree)) {
        return false;
    }

    CAnchorsOrchardMap::iterator ret = cacheOrchardAnchors.insert(std::make_pair(rt, CAnchorsOrchardCacheEntry())).first;
    ret->second.entered = true;
    ret->second.tree = tree;
    cachedCoinsUsage += ret->second.tree.DynamicMemoryUsage();

    return true;
}

bool CCoinsViewCache::GetNullifier(const uint256 &nullifier, ShieldedType type) const {
    CNullifiersMap* cacheToUse;
    switch (type) {
        case SPROUT:
            cacheToUse = &cacheSproutNullifiers;
            break;
        case SAPLING:
            cacheToUse = &cacheSaplingNullifiers;
            break;
        case ORCHARD:
            cacheToUse = &cacheOrchardNullifiers;
            break;
        default:
            throw std::runtime_error("Unknown shielded type");
    }
    CNullifiersMap::iterator it = cacheToUse->find(nullifier);
    if (it != cacheToUse->end())
        return it->second.entered;

    CNullifiersCacheEntry entry;
    bool tmp = base->GetNullifier(nullifier, type);
    entry.entered = tmp;

    cacheToUse->insert(std::make_pair(nullifier, entry));

    return tmp;
}

HistoryIndex CCoinsViewCache::GetHistoryLength(uint32_t epochId) const {
    HistoryCache& historyCache = SelectHistoryCache(epochId);
    return historyCache.length;
}

HistoryNode CCoinsViewCache::GetHistoryAt(uint32_t epochId, HistoryIndex index) const {
    HistoryCache& historyCache = SelectHistoryCache(epochId);

    if (index >= historyCache.length) {
        // Caller should ensure that it is limiting history
        // request to 0..GetHistoryLength(epochId)-1 range
        throw std::runtime_error("Invalid history request");
    }

    if (index >= historyCache.updateDepth) {
        return historyCache.appends[index];
    }

    return base->GetHistoryAt(epochId, index);
}

uint256 CCoinsViewCache::GetHistoryRoot(uint32_t epochId) const {
    return SelectHistoryCache(epochId).root;
}

std::optional<libzcash::LatestSubtree> CCoinsViewCache::GetLatestSubtree(ShieldedType type) const {
    switch (type) {
        case SAPLING:
            return cacheSaplingSubtrees.GetLatestSubtree(base);
        case ORCHARD:
            return cacheOrchardSubtrees.GetLatestSubtree(base);
        default:
            throw std::runtime_error("GetLatestSubtree: unsupported shielded type");
    }
}

std::optional<libzcash::SubtreeData> CCoinsViewCache::GetSubtreeData(
    ShieldedType type,
    libzcash::SubtreeIndex index) const
{
    switch (type) {
        case SAPLING:
            return cacheSaplingSubtrees.GetSubtreeData(base, index);
        case ORCHARD:
            return cacheOrchardSubtrees.GetSubtreeData(base, index);
        default:
            throw std::runtime_error("GetSubtreeData: unsupported shielded type");
    }
}

template<typename Tree, typename Cache, typename CacheIterator, typename CacheEntry>
void CCoinsViewCache::AbstractPushAnchor(
    const Tree &tree,
    ShieldedType type,
    Cache &cacheAnchors,
    uint256 &hash
)
{
    uint256 newrt = tree.root();

    auto currentRoot = GetBestAnchor(type);

    // We don't want to overwrite an anchor we already have.
    // This occurs when a block doesn't modify mapAnchors at all,
    // because there are no joinsplits. We could get around this a
    // different way (make all blocks modify mapAnchors somehow)
    // but this is simpler to reason about.
    if (currentRoot != newrt) {
        auto insertRet = cacheAnchors.insert(std::make_pair(newrt, CacheEntry()));
        CacheIterator ret = insertRet.first;

        ret->second.entered = true;
        ret->second.tree = tree;
        ret->second.flags = CacheEntry::DIRTY;

        if (insertRet.second) {
            // An insert took place
            cachedCoinsUsage += ret->second.tree.DynamicMemoryUsage();
        }

        hash = newrt;
    }
}

template<> void CCoinsViewCache::PushAnchor(const SproutMerkleTree &tree)
{
    AbstractPushAnchor<SproutMerkleTree, CAnchorsSproutMap, CAnchorsSproutMap::iterator, CAnchorsSproutCacheEntry>(
        tree,
        SPROUT,
        cacheSproutAnchors,
        hashSproutAnchor
    );
}

template<> void CCoinsViewCache::PushAnchor(const SaplingMerkleTree &tree)
{
    AbstractPushAnchor<SaplingMerkleTree, CAnchorsSaplingMap, CAnchorsSaplingMap::iterator, CAnchorsSaplingCacheEntry>(
        tree,
        SAPLING,
        cacheSaplingAnchors,
        hashSaplingAnchor
    );
}

template<> void CCoinsViewCache::PushAnchor(const OrchardMerkleFrontier &tree)
{
    AbstractPushAnchor<OrchardMerkleFrontier, CAnchorsOrchardMap, CAnchorsOrchardMap::iterator, CAnchorsOrchardCacheEntry>(
        tree,
        ORCHARD,
        cacheOrchardAnchors,
        hashOrchardAnchor
    );
}

template<>
void CCoinsViewCache::BringBestAnchorIntoCache(
    const uint256 &currentRoot,
    SproutMerkleTree &tree
)
{
    assert(GetSproutAnchorAt(currentRoot, tree));
}

template<>
void CCoinsViewCache::BringBestAnchorIntoCache(
    const uint256 &currentRoot,
    SaplingMerkleTree &tree
)
{
    assert(GetSaplingAnchorAt(currentRoot, tree));
}

template<>
void CCoinsViewCache::BringBestAnchorIntoCache(
    const uint256 &currentRoot,
    OrchardMerkleFrontier &tree
)
{
    assert(GetOrchardAnchorAt(currentRoot, tree));
}

void draftMMRNode(std::vector<uint32_t> &indices,
                  std::vector<HistoryEntry> &entries,
                  HistoryNode nodeData,
                  uint32_t alt,
                  uint32_t peak_pos)
{
    HistoryEntry newEntry = alt == 0
        ? libzcash::LeafToEntry(nodeData)
        // peak_pos - (1 << alt) is the array position of left child.
        // peak_pos - 1 is the array position of right child.
        : libzcash::NodeToEntry(nodeData, peak_pos - (1 << alt), peak_pos - 1);

    indices.push_back(peak_pos);
    entries.push_back(newEntry);
}

// Computes floor(log2(x)).
static inline uint32_t floor_log2(uint32_t x) {
    assert(x > 0);
    int log = 0;
    while (x >>= 1) { ++log; }
    return log;
}

// Computes the altitude of the largest subtree for an MMR with n nodes,
// which is floor(log2(n + 1)) - 1.
static inline uint32_t altitude(uint32_t n) {
    return floor_log2(n + 1) - 1;
}

uint32_t CCoinsViewCache::PreloadHistoryTree(uint32_t epochId, bool extra, std::vector<HistoryEntry> &entries, std::vector<uint32_t> &entry_indices) {
    auto treeLength = GetHistoryLength(epochId);

    if (treeLength <= 0) {
        throw std::runtime_error("Invalid PreloadHistoryTree state called - tree should exist");
    } else if (treeLength == 1) {
        entries.push_back(libzcash::LeafToEntry(GetHistoryAt(epochId, 0)));
        entry_indices.push_back(0);
        return 1;
    }

    uint32_t last_peak_pos = 0;
    uint32_t last_peak_alt = 0;
    uint32_t alt = 0;
    uint32_t peak_pos = 0;
    uint32_t total_peaks = 0;

    // Assume the following example peak layout with 14 leaves, and 25 stored nodes in
    // total (the "tree length"):
    //
    //             P
    //            /\
    //           /  \
    //          / \  \
    //        /    \  \  Altitude
    //     _A_      \  \    3
    //   _/   \_     B  \   2
    //  / \   / \   / \  C  1
    // /\ /\ /\ /\ /\ /\ /\ 0
    //
    // We start by determining the altitude of the highest peak (A).
    alt = altitude(treeLength);

    // We determine the position of the highest peak (A) by pretending it is the right
    // sibling in a tree, and its left-most leaf has position 0. Then the left sibling
    // of (A) has position -1, and so we can "jump" to the peak's position by computing
    // -1 + 2^(alt + 1) - 1.
    peak_pos = (1 << (alt + 1)) - 2;

    // Now that we have the position and altitude of the highest peak (A), we collect
    // the remaining peaks (B, C). We navigate the peaks as if they were nodes in this
    // Merkle tree (with additional imaginary nodes 1 and 2, that have positions beyond
    // the MMR's length):
    //
    //             / \
    //            /   \
    //           /     \
    //         /         \
    //       A ==========> 1
    //      / \          //  \
    //    _/   \_       B ==> 2
    //   /\     /\     /\    //
    //  /  \   /  \   /  \   C
    // /\  /\ /\  /\ /\  /\ /\
    //
    while (alt != 0) {
        // If peak_pos is out of bounds of the tree, we compute the position of its left
        // child, and drop down one level in the tree.
        if (peak_pos >= treeLength) {
            // left child, -2^alt
            peak_pos = peak_pos - (1 << alt);
            alt = alt - 1;
        }

        // If the peak exists, we take it and then continue with its right sibling.
        if (peak_pos < treeLength) {
            draftMMRNode(entry_indices, entries, GetHistoryAt(epochId, peak_pos), alt, peak_pos);

            last_peak_pos = peak_pos;
            last_peak_alt = alt;

            // right sibling
            peak_pos = peak_pos + (1 << (alt + 1)) - 1;
        }
    }

    total_peaks = entries.size();

    // Return early if we don't require extra nodes.
    if (!extra) return total_peaks;

    alt = last_peak_alt;
    peak_pos = last_peak_pos;


    //             P
    //            /\
    //           /  \
    //          / \  \
    //        /    \  \
    //     _A_      \  \
    //   _/   \_     B  \
    //  / \   / \   / \  C
    // /\ /\ /\ /\ /\ /\ /\
    //                   D E
    //
    // For extra peaks needed for deletion, we do extra pass on right slope of the last peak
    // and add those nodes + their siblings. Extra would be (D, E) for the picture above.
    while (alt > 0) {
        uint32_t left_pos = peak_pos - (1 << alt);
        uint32_t right_pos = peak_pos - 1;
        alt = alt - 1;

        // drafting left child
        draftMMRNode(entry_indices, entries, GetHistoryAt(epochId, left_pos), alt, left_pos);

        // drafting right child
        draftMMRNode(entry_indices, entries, GetHistoryAt(epochId, right_pos), alt, right_pos);

        // continuing on right slope
        peak_pos = right_pos;
    }

    return total_peaks;
}

HistoryCache& CCoinsViewCache::SelectHistoryCache(uint32_t epochId) const {
    auto entry = historyCacheMap.find(epochId);

    if (entry != historyCacheMap.end()) {
        return entry->second;
    } else {
        auto cache = HistoryCache(
            base->GetHistoryLength(epochId),
            base->GetHistoryRoot(epochId),
            epochId
        );
        return historyCacheMap.insert({epochId, cache}).first->second;
    }
}

void CCoinsViewCache::PushHistoryNode(uint32_t epochId, const HistoryNode node) {
    HistoryCache& historyCache = SelectHistoryCache(epochId);

    if (historyCache.length == 0) {
        // special case, it just goes into the cache right away
        historyCache.Extend(node);

        if (librustzcash_mmr_hash_node(epochId, &node, historyCache.root.begin()) != 0) {
            throw std::runtime_error("hashing node failed");
        };

        return;
    }

    std::vector<HistoryEntry> entries;
    std::vector<uint32_t> entry_indices;

    PreloadHistoryTree(epochId, false, entries, entry_indices);

    uint256 newRoot;
    std::array<HistoryNode, 32> appendBuf = {};

    uint32_t appends = librustzcash_mmr_append(
        epochId,
        historyCache.length,
        entry_indices.data(),
        entries.data(),
        entry_indices.size(),
        &node,
        newRoot.begin(),
        appendBuf.data()
    );

    for (size_t i = 0; i < appends; i++) {
        historyCache.Extend(appendBuf[i]);
    }

    historyCache.root = newRoot;
}

void CCoinsViewCache::PopHistoryNode(uint32_t epochId) {
    HistoryCache& historyCache = SelectHistoryCache(epochId);
    uint256 newRoot;

    switch (historyCache.length) {
        case 0:
        {
            // Caller is generally not expected to pop from empty tree! Caller
            // should switch to previous epoch and pop history from there.

            // If we are doing an expected rollback that changes the consensus
            // branch ID for some upgrade (or introduces one that wasn't present
            // at the equivalent height) this will occur because
            // `SelectHistoryCache` selects the tree for the new consensus
            // branch ID, not the one that existed on the chain being rolled
            // back.

            // Sensible action is to truncate the history cache:
        }
        case 1:
        {
            // Just resetting tree to empty
            historyCache.Truncate(0);
            historyCache.root = uint256();
            return;
        }
        case 2:
        {
            // - A tree with one leaf has length 1.
            // - A tree with two leaves has length 3.
            throw std::runtime_error("a history tree cannot have two nodes");
        }
        case 3:
        {
            const HistoryNode tmpHistoryRoot = GetHistoryAt(epochId, 0);
            // After removing a leaf from a tree with two leaves, we are left
            // with a single-node tree, whose root is just the hash of that
            // node.
            if (librustzcash_mmr_hash_node(
                epochId,
                &tmpHistoryRoot,
                newRoot.begin()
            ) != 0) {
                throw std::runtime_error("hashing node failed");
            }
            historyCache.Truncate(1);
            historyCache.root = newRoot;
            return;
        }
        default:
        {
            // This is a non-elementary pop, so use the full tree logic.
            std::vector<HistoryEntry> entries;
            std::vector<uint32_t> entry_indices;

            uint32_t peak_count = PreloadHistoryTree(epochId, true, entries, entry_indices);

            uint32_t numberOfDeletes = librustzcash_mmr_delete(
                epochId,
                historyCache.length,
                entry_indices.data(),
                entries.data(),
                peak_count,
                entries.size() - peak_count,
                newRoot.begin()
            );

            historyCache.Truncate(historyCache.length - numberOfDeletes);
            historyCache.root = newRoot;
            return;
        }
    }
}

void CCoinsViewCache::PushSubtree(ShieldedType type, libzcash::SubtreeData subtree)
{
    switch(type) {
        case SAPLING:
            return cacheSaplingSubtrees.PushSubtree(base, subtree);
        case ORCHARD:
            return cacheOrchardSubtrees.PushSubtree(base, subtree);
        default:
            throw std::runtime_error("PushSubtree: unsupported shielded type");
    }
}

void CCoinsViewCache::PopSubtree(ShieldedType type)
{
    switch(type) {
        case SAPLING:
            return cacheSaplingSubtrees.PopSubtree(base);
        case ORCHARD:
            return cacheOrchardSubtrees.PopSubtree(base);
        default:
            throw std::runtime_error("PopSubtree: unsupported shielded type");
    }
}

void CCoinsViewCache::ResetSubtrees(ShieldedType type)
{
    switch(type) {
        case SAPLING:
            return cacheSaplingSubtrees.ResetSubtrees();
        case ORCHARD:
            return cacheOrchardSubtrees.ResetSubtrees();
        default:
            throw std::runtime_error("ResetSubtrees: unsupported shielded type");
    }
}

template<typename Tree, typename Cache, typename CacheEntry>
void CCoinsViewCache::AbstractPopAnchor(
    const uint256 &newrt,
    ShieldedType type,
    Cache &cacheAnchors,
    uint256 &hash
)
{
    auto currentRoot = GetBestAnchor(type);

    // Blocks might not change the commitment tree, in which
    // case restoring the "old" anchor during a reorg must
    // have no effect.
    if (currentRoot != newrt) {
        // Bring the current best anchor into our local cache
        // so that its tree exists in memory.
        {
            Tree tree;
            BringBestAnchorIntoCache(currentRoot, tree);
        }

        // Mark the anchor as unentered, removing it from view
        cacheAnchors[currentRoot].entered = false;

        // Mark the cache entry as dirty so it's propagated
        cacheAnchors[currentRoot].flags = CacheEntry::DIRTY;

        // Mark the new root as the best anchor
        hash = newrt;
    }
}

void CCoinsViewCache::PopAnchor(const uint256 &newrt, ShieldedType type) {
    switch (type) {
        case SPROUT:
            AbstractPopAnchor<SproutMerkleTree, CAnchorsSproutMap, CAnchorsSproutCacheEntry>(
                newrt,
                SPROUT,
                cacheSproutAnchors,
                hashSproutAnchor
            );
            break;
        case SAPLING:
            AbstractPopAnchor<SaplingMerkleTree, CAnchorsSaplingMap, CAnchorsSaplingCacheEntry>(
                newrt,
                SAPLING,
                cacheSaplingAnchors,
                hashSaplingAnchor
            );
            break;
        case ORCHARD:
            AbstractPopAnchor<OrchardMerkleFrontier, CAnchorsOrchardMap, CAnchorsOrchardCacheEntry>(
                newrt,
                ORCHARD,
                cacheOrchardAnchors,
                hashOrchardAnchor
            );
            break;
        default:
            throw std::runtime_error("Unknown shielded type");
    }
}

void CCoinsViewCache::SetNullifiers(const CTransaction& tx, bool spent) {
    for (const JSDescription &joinsplit : tx.vJoinSplit) {
        for (const uint256 &nullifier : joinsplit.nullifiers) {
            std::pair<CNullifiersMap::iterator, bool> ret = cacheSproutNullifiers.insert(std::make_pair(nullifier, CNullifiersCacheEntry()));
            ret.first->second.entered = spent;
            ret.first->second.flags |= CNullifiersCacheEntry::DIRTY;
        }
    }
    for (const auto& spendDescription : tx.GetSaplingSpends()) {
        std::pair<CNullifiersMap::iterator, bool> ret = cacheSaplingNullifiers.insert(std::make_pair(uint256::FromRawBytes(spendDescription.nullifier()), CNullifiersCacheEntry()));
        ret.first->second.entered = spent;
        ret.first->second.flags |= CNullifiersCacheEntry::DIRTY;
    }
    for (const uint256& nf : tx.GetOrchardBundle().GetNullifiers()) {
        std::pair<CNullifiersMap::iterator, bool> ret = cacheOrchardNullifiers.insert(std::make_pair(nf, CNullifiersCacheEntry()));
        ret.first->second.entered = spent;
        ret.first->second.flags |= CNullifiersCacheEntry::DIRTY;
    }
}

bool CCoinsViewCache::GetCoins(const uint256 &txid, CCoins &coins) const {
    CCoinsMap::const_iterator it = FetchCoins(txid);
    if (it != cacheCoins.end()) {
        coins = it->second.coins;
        return true;
    }
    return false;
}

CCoinsModifier CCoinsViewCache::ModifyCoins(const uint256 &txid) {
    assert(!hasModifier);
    std::pair<CCoinsMap::iterator, bool> ret = cacheCoins.insert(std::make_pair(txid, CCoinsCacheEntry()));
    size_t cachedCoinUsage = 0;
    if (ret.second) {
        if (!base->GetCoins(txid, ret.first->second.coins)) {
            // The parent view does not have this entry; mark it as fresh.
            ret.first->second.coins.Clear();
            ret.first->second.flags = CCoinsCacheEntry::FRESH;
        } else if (ret.first->second.coins.IsPruned()) {
            // The parent view only has a pruned entry for this; mark it as fresh.
            ret.first->second.flags = CCoinsCacheEntry::FRESH;
        }
    } else {
        cachedCoinUsage = ret.first->second.coins.DynamicMemoryUsage();
    }
    // Assume that whenever ModifyCoins is called, the entry will be modified.
    ret.first->second.flags |= CCoinsCacheEntry::DIRTY;
    return CCoinsModifier(*this, ret.first, cachedCoinUsage);
}

CCoinsModifier CCoinsViewCache::ModifyNewCoins(const uint256 &txid) {
    assert(!hasModifier);
    std::pair<CCoinsMap::iterator, bool> ret = cacheCoins.insert(std::make_pair(txid, CCoinsCacheEntry()));
    ret.first->second.coins.Clear();
    ret.first->second.flags = CCoinsCacheEntry::FRESH;
    ret.first->second.flags |= CCoinsCacheEntry::DIRTY;
    return CCoinsModifier(*this, ret.first, 0);
}

const CCoins* CCoinsViewCache::AccessCoins(const uint256 &txid) const {
    CCoinsMap::const_iterator it = FetchCoins(txid);
    if (it == cacheCoins.end()) {
        return NULL;
    } else {
        return &it->second.coins;
    }
}

bool CCoinsViewCache::HaveCoins(const uint256 &txid) const {
    CCoinsMap::const_iterator it = FetchCoins(txid);
    // We're using vtx.empty() instead of IsPruned here for performance reasons,
    // as we only care about the case where a transaction was replaced entirely
    // in a reorganization (which wipes vout entirely, as opposed to spending
    // which just cleans individual outputs).
    return (it != cacheCoins.end() && !it->second.coins.vout.empty());
}

uint256 CCoinsViewCache::GetBestBlock() const {
    if (hashBlock.IsNull())
        hashBlock = base->GetBestBlock();
    return hashBlock;
}


uint256 CCoinsViewCache::GetBestAnchor(ShieldedType type) const {
    switch (type) {
        case SPROUT:
            if (hashSproutAnchor.IsNull())
                hashSproutAnchor = base->GetBestAnchor(type);
            return hashSproutAnchor;
            break;
        case SAPLING:
            if (hashSaplingAnchor.IsNull())
                hashSaplingAnchor = base->GetBestAnchor(type);
            return hashSaplingAnchor;
            break;
        case ORCHARD:
            if (hashOrchardAnchor.IsNull())
                hashOrchardAnchor = base->GetBestAnchor(type);
            return hashOrchardAnchor;
            break;
        default:
            throw std::runtime_error("Unknown shielded type");
    }
}

void CCoinsViewCache::SetBestBlock(const uint256 &hashBlockIn) {
    hashBlock = hashBlockIn;
}

void BatchWriteNullifiers(CNullifiersMap &mapNullifiers, CNullifiersMap &cacheNullifiers)
{
    for (CNullifiersMap::iterator child_it = mapNullifiers.begin(); child_it != mapNullifiers.end();) {
        if (child_it->second.flags & CNullifiersCacheEntry::DIRTY) { // Ignore non-dirty entries (optimization).
            CNullifiersMap::iterator parent_it = cacheNullifiers.find(child_it->first);

            if (parent_it == cacheNullifiers.end()) {
                CNullifiersCacheEntry& entry = cacheNullifiers[child_it->first];
                entry.entered = child_it->second.entered;
                entry.flags = CNullifiersCacheEntry::DIRTY;
            } else {
                if (parent_it->second.entered != child_it->second.entered) {
                    parent_it->second.entered = child_it->second.entered;
                    parent_it->second.flags |= CNullifiersCacheEntry::DIRTY;
                }
            }
        }
        child_it = mapNullifiers.erase(child_it);
    }
}

template<typename Map, typename MapIterator, typename MapEntry>
void BatchWriteAnchors(
    Map &mapAnchors,
    Map &cacheAnchors,
    size_t &cachedCoinsUsage
)
{
    for (MapIterator child_it = mapAnchors.begin(); child_it != mapAnchors.end();)
    {
        if (child_it->second.flags & MapEntry::DIRTY) {
            MapIterator parent_it = cacheAnchors.find(child_it->first);

            if (parent_it == cacheAnchors.end()) {
                MapEntry& entry = cacheAnchors[child_it->first];
                entry.entered = child_it->second.entered;
                entry.tree = child_it->second.tree;
                entry.flags = MapEntry::DIRTY;

                cachedCoinsUsage += entry.tree.DynamicMemoryUsage();
            } else {
                if (parent_it->second.entered != child_it->second.entered) {
                    // The parent may have removed the entry.
                    parent_it->second.entered = child_it->second.entered;
                    parent_it->second.flags |= MapEntry::DIRTY;
                }
            }
        }

        child_it = mapAnchors.erase(child_it);
    }
}

void BatchWriteHistory(CHistoryCacheMap& historyCacheMap, CHistoryCacheMap& historyCacheMapIn) {
    for (auto nextHistoryCache = historyCacheMapIn.begin(); nextHistoryCache != historyCacheMapIn.end(); nextHistoryCache++) {
        auto historyCacheIn = nextHistoryCache->second;
        auto epochId = nextHistoryCache->first;

        auto historyCache = historyCacheMap.find(epochId);
        if (historyCache != historyCacheMap.end()) {
            // delete old entries since updateDepth
            historyCache->second.Truncate(historyCacheIn.updateDepth);

            // Replace/append new/updated entries. HistoryCache.Extend
            // auto-indexes the nodes, so we need to extend in the same order as
            // this cache is indexed.
            for (size_t i = historyCacheIn.updateDepth; i < historyCacheIn.length; i++) {
                historyCache->second.Extend(historyCacheIn.appends[i]);
            }

            // the lengths should now match
            assert(historyCache->second.length == historyCacheIn.length);

            // write current root
            historyCache->second.root = historyCacheIn.root;
        } else {
            // Just insert the history cache into its parent
            historyCacheMap.insert({epochId, historyCacheIn});
        }
    }
}

bool CCoinsViewCache::BatchWrite(CCoinsMap &mapCoins,
                                 const uint256 &hashBlockIn,
                                 const uint256 &hashSproutAnchorIn,
                                 const uint256 &hashSaplingAnchorIn,
                                 const uint256 &hashOrchardAnchorIn,
                                 CAnchorsSproutMap &mapSproutAnchors,
                                 CAnchorsSaplingMap &mapSaplingAnchors,
                                 CAnchorsOrchardMap &mapOrchardAnchors,
                                 CNullifiersMap &mapSproutNullifiers,
                                 CNullifiersMap &mapSaplingNullifiers,
                                 CNullifiersMap &mapOrchardNullifiers,
                                 CHistoryCacheMap &historyCacheMapIn,
                                 SubtreeCache &cacheSaplingSubtreesIn,
                                 SubtreeCache &cacheOrchardSubtreesIn) {
    assert(!hasModifier);
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) { // Ignore non-dirty entries (optimization).
            CCoinsMap::iterator itUs = cacheCoins.find(it->first);
            if (itUs == cacheCoins.end()) {
                if (!it->second.coins.IsPruned()) {
                    // The parent cache does not have an entry, while the child
                    // cache does have (a non-pruned) one. Move the data up, and
                    // mark it as fresh (if the grandparent did have it, we
                    // would have pulled it in at first GetCoins).
                    assert(it->second.flags & CCoinsCacheEntry::FRESH);
                    CCoinsCacheEntry& entry = cacheCoins[it->first];
                    entry.coins.swap(it->second.coins);
                    cachedCoinsUsage += entry.coins.DynamicMemoryUsage();
                    entry.flags = CCoinsCacheEntry::DIRTY | CCoinsCacheEntry::FRESH;
                }
            } else {
                if ((itUs->second.flags & CCoinsCacheEntry::FRESH) && it->second.coins.IsPruned()) {
                    // The grandparent does not have an entry, and the child is
                    // modified and being pruned. This means we can just delete
                    // it from the parent.
                    cachedCoinsUsage -= itUs->second.coins.DynamicMemoryUsage();
                    cacheCoins.erase(itUs);
                } else {
                    // A normal modification.
                    cachedCoinsUsage -= itUs->second.coins.DynamicMemoryUsage();
                    itUs->second.coins.swap(it->second.coins);
                    cachedCoinsUsage += itUs->second.coins.DynamicMemoryUsage();
                    itUs->second.flags |= CCoinsCacheEntry::DIRTY;
                }
            }
        }
        it = mapCoins.erase(it);
    }

    ::BatchWriteAnchors<CAnchorsSproutMap, CAnchorsSproutMap::iterator, CAnchorsSproutCacheEntry>(mapSproutAnchors, cacheSproutAnchors, cachedCoinsUsage);
    ::BatchWriteAnchors<CAnchorsSaplingMap, CAnchorsSaplingMap::iterator, CAnchorsSaplingCacheEntry>(mapSaplingAnchors, cacheSaplingAnchors, cachedCoinsUsage);
    ::BatchWriteAnchors<CAnchorsOrchardMap, CAnchorsOrchardMap::iterator, CAnchorsOrchardCacheEntry>(mapOrchardAnchors, cacheOrchardAnchors, cachedCoinsUsage);

    ::BatchWriteNullifiers(mapSproutNullifiers, cacheSproutNullifiers);
    ::BatchWriteNullifiers(mapSaplingNullifiers, cacheSaplingNullifiers);
    ::BatchWriteNullifiers(mapOrchardNullifiers, cacheOrchardNullifiers);

    ::BatchWriteHistory(historyCacheMap, historyCacheMapIn);

    cacheSaplingSubtrees.BatchWrite(base, cacheSaplingSubtreesIn);
    cacheOrchardSubtrees.BatchWrite(base, cacheOrchardSubtreesIn);

    hashSproutAnchor = hashSproutAnchorIn;
    hashSaplingAnchor = hashSaplingAnchorIn;
    hashOrchardAnchor = hashOrchardAnchorIn;
    hashBlock = hashBlockIn;
    return true;
}

bool CCoinsViewCache::Flush() {
    // This ensures that before we pass the subtree caches
    // they have been initialized correctly
    cacheSaplingSubtrees.Initialize(base);
    cacheOrchardSubtrees.Initialize(base);

    bool fOk = base->BatchWrite(cacheCoins,
                                hashBlock,
                                hashSproutAnchor,
                                hashSaplingAnchor,
                                hashOrchardAnchor,
                                cacheSproutAnchors,
                                cacheSaplingAnchors,
                                cacheOrchardAnchors,
                                cacheSproutNullifiers,
                                cacheSaplingNullifiers,
                                cacheOrchardNullifiers,
                                historyCacheMap,
                                cacheSaplingSubtrees,
                                cacheOrchardSubtrees);
    cacheCoins.clear();
    cacheSproutAnchors.clear();
    cacheSaplingAnchors.clear();
    cacheOrchardAnchors.clear();
    cacheSproutNullifiers.clear();
    cacheSaplingNullifiers.clear();
    cacheOrchardNullifiers.clear();
    historyCacheMap.clear();
    cacheSaplingSubtrees.clear();
    cacheOrchardSubtrees.clear();
    cachedCoinsUsage = 0;
    return fOk;
}

unsigned int CCoinsViewCache::GetCacheSize() const {
    return cacheCoins.size();
}

const CTxOut &CCoinsViewCache::GetOutputFor(const CTxIn& input) const
{
    const CCoins* coins = AccessCoins(input.prevout.hash);
    assert(coins && coins->IsAvailable(input.prevout.n));
    return coins->vout[input.prevout.n];
}

CAmount CCoinsViewCache::GetValueIn(const CTransaction& tx) const
{
    return GetTransparentValueIn(tx) + tx.GetShieldedValueIn();
}

CAmount CCoinsViewCache::GetTransparentValueIn(const CTransaction& tx) const
{
    if (tx.IsCoinBase())
        return 0;

    CAmount nResult = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        nResult += GetOutputFor(tx.vin[i]).nValue;

    return nResult;
}

tl::expected<void, UnsatisfiedShieldedReq> CCoinsViewCache::CheckShieldedRequirements(const CTransaction& tx) const
{
    boost::unordered_map<uint256, SproutMerkleTree, SaltedTxidHasher> intermediates;

    for (const JSDescription &joinsplit : tx.vJoinSplit)
    {
        for (const uint256& nullifier : joinsplit.nullifiers)
        {
            if (GetNullifier(nullifier, SPROUT)) {
                // If the nullifier is set, this transaction
                // double-spends!
                auto txid = tx.GetHash().ToString();
                auto nf = nullifier.ToString();
                TracingWarn("consensus", "Sprout double-spend detected",
                    "txid", txid.c_str(),
                    "nf", nf.c_str());
                return tl::unexpected(UnsatisfiedShieldedReq::SproutDuplicateNullifier);
            }
        }

        SproutMerkleTree tree;
        auto it = intermediates.find(joinsplit.anchor);
        if (it != intermediates.end()) {
            tree = it->second;
        } else if (!GetSproutAnchorAt(joinsplit.anchor, tree)) {
            auto txid = tx.GetHash().ToString();
            auto anchor = joinsplit.anchor.ToString();
            TracingWarn("consensus", "Transaction uses unknown Sprout anchor",
                "txid", txid.c_str(),
                "anchor", anchor.c_str());
            return tl::unexpected(UnsatisfiedShieldedReq::SproutUnknownAnchor);
        }

        for (const uint256& commitment : joinsplit.commitments)
        {
            tree.append(commitment);
        }

        intermediates.insert(std::make_pair(tree.root(), tree));
    }

    for (const auto& spendDescription : tx.GetSaplingSpends()) {
        uint256 nullifier = uint256::FromRawBytes(spendDescription.nullifier());
        if (GetNullifier(nullifier, SAPLING)) { // Prevent double spends
            auto txid = tx.GetHash().ToString();
            auto nf = nullifier.ToString();
            TracingWarn("consensus", "Sapling double-spend detected",
                "txid", txid.c_str(),
                "nf", nf.c_str());
            return tl::unexpected(UnsatisfiedShieldedReq::SaplingDuplicateNullifier);
        }

        SaplingMerkleTree tree;
        uint256 rt = uint256::FromRawBytes(spendDescription.anchor());
        if (!GetSaplingAnchorAt(rt, tree)) {
            auto txid = tx.GetHash().ToString();
            auto anchor = rt.ToString();
            TracingWarn("consensus", "Transaction uses unknown Sapling anchor",
                "txid", txid.c_str(),
                "anchor", anchor.c_str());
            return tl::unexpected(UnsatisfiedShieldedReq::SaplingUnknownAnchor);
        }
    }

    for (const uint256 &nullifier : tx.GetOrchardBundle().GetNullifiers()) {
        if (GetNullifier(nullifier, ORCHARD)) { // Prevent double spends
            auto txid = tx.GetHash().ToString();
            auto nf = nullifier.ToString();
            TracingWarn("consensus", "Orchard double-spend detected",
                "txid", txid.c_str(),
                "nf", nf.c_str());
            return tl::unexpected(UnsatisfiedShieldedReq::OrchardDuplicateNullifier);
        }
    }

    std::optional<uint256> root = tx.GetOrchardBundle().GetAnchor();
    if (root) {
        OrchardMerkleFrontier tree;
        if (!GetOrchardAnchorAt(root.value(), tree)) {
            auto txid = tx.GetHash().ToString();
            auto anchor = root.value().ToString();
            TracingWarn("consensus", "Transaction uses unknown Orchard anchor",
                "txid", txid.c_str(),
                "anchor", anchor.c_str());
            return tl::unexpected(UnsatisfiedShieldedReq::OrchardUnknownAnchor);
        }
    }

    return {};
}

bool CCoinsViewCache::HaveInputs(const CTransaction& tx) const
{
    if (!tx.IsCoinBase()) {
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            const COutPoint &prevout = tx.vin[i].prevout;
            const CCoins* coins = AccessCoins(prevout.hash);
            if (!coins || !coins->IsAvailable(prevout.n)) {
                return false;
            }
        }
    }
    return true;
}

CCoinsModifier::CCoinsModifier(CCoinsViewCache& cache_, CCoinsMap::iterator it_, size_t usage) : cache(cache_), it(it_), cachedCoinUsage(usage) {
    assert(!cache.hasModifier);
    cache.hasModifier = true;
}

CCoinsModifier::~CCoinsModifier()
{
    assert(cache.hasModifier);
    cache.hasModifier = false;
    it->second.coins.Cleanup();
    cache.cachedCoinsUsage -= cachedCoinUsage; // Subtract the old usage
    if ((it->second.flags & CCoinsCacheEntry::FRESH) && it->second.coins.IsPruned()) {
        cache.cacheCoins.erase(it);
    } else {
        // If the coin still exists after the modification, add the new usage
        cache.cachedCoinsUsage += it->second.coins.DynamicMemoryUsage();
    }
}

void SubtreeCache::clear() {
    initialized = false;
    parentLatestSubtree = std::nullopt;
    newSubtrees.clear();
}

void SubtreeCache::Initialize(CCoinsView *parentView)
{
    if (!initialized) {
        parentLatestSubtree = parentView->GetLatestSubtree(type);
        initialized = true;
    }
}

std::optional<libzcash::LatestSubtree> SubtreeCache::GetLatestSubtree(CCoinsView *parentView) {
    Initialize(parentView);

    if (newSubtrees.size() > 0) {
        // The latest subtree is in our cache.

        libzcash::SubtreeIndex index;
        if (parentLatestSubtree.has_value()) {
            // The best subtree index is newSubtrees.size() larger than
            // our parent view's subtree index.
            index = parentLatestSubtree.value().index + newSubtrees.size();
        } else {
            // The parent view has no subtrees
            index = newSubtrees.size() - 1;
        }

        auto lastSubtree = newSubtrees.back();
        return libzcash::LatestSubtree(index, lastSubtree.root, lastSubtree.nHeight);
    } else {
        return parentLatestSubtree;
    }
}

std::optional<libzcash::SubtreeData> SubtreeCache::GetSubtreeData(CCoinsView *parentView, libzcash::SubtreeIndex index) {
    Initialize(parentView);

    auto latestSubtree = GetLatestSubtree(parentView);

    if (!latestSubtree.has_value() || latestSubtree.value().index < index) {
        // This subtree isn't complete in our local view
        return std::nullopt;
    }

    if (parentLatestSubtree.has_value()) {
        if (index <= parentLatestSubtree.value().index) {
            // This subtree in question must have previously been flushed to the parent cache layer,
            // so we ask for it there.
            return parentView->GetSubtreeData(type, index);
        } else {
            // Get the index into our local `newSubtrees` where the subtree should
            // be located.
            auto localIndex = index - (parentLatestSubtree.value().index + 1);
            assert(newSubtrees.size() > localIndex);
            return newSubtrees[localIndex];
        }
    } else {
        // The index we've been given is the index into our local `newSubtrees`
        // since the parent view has no subtrees.
        assert(newSubtrees.size() > index);
        return newSubtrees[index];
    }
}

void SubtreeCache::PushSubtree(CCoinsView *parentView, libzcash::SubtreeData subtree) {
    Initialize(parentView);

    newSubtrees.push_back(subtree);
}

void SubtreeCache::PopSubtree(CCoinsView *parentView) {
    Initialize(parentView);

    if (newSubtrees.empty()) {
        // Try to pop from the parent view
        if (parentLatestSubtree.has_value()) {
            libzcash::SubtreeIndex parentIndex = parentLatestSubtree.value().index;

            if (parentIndex == 0) {
                // This pops the only subtree left in the parent view.
                parentLatestSubtree = std::nullopt;
            } else {
                parentIndex -= 1;
                auto newParent = parentView->GetSubtreeData(type, parentIndex);
                if (!newParent.has_value()) {
                    throw std::runtime_error("cache inconsistency; parent view does not have subtree");
                }

                parentLatestSubtree = libzcash::LatestSubtree(
                    parentIndex,
                    newParent.value().root,
                    newParent.value().nHeight
                );
            }
        } else {
            throw std::runtime_error("tried to pop a subtree from an empty subtree list");
        }
    } else {
        newSubtrees.pop_back();
    }
}

void SubtreeCache::ResetSubtrees() {
    // This ensures that all subtrees will be popped from the parent view
    initialized = true;

    parentLatestSubtree = std::nullopt;
    newSubtrees.clear();
}

void SubtreeCache::BatchWrite(CCoinsView *parentView, SubtreeCache &childMap) {
    Initialize(parentView);
    auto bestSubtree = GetLatestSubtree(parentView);
    if (!bestSubtree.has_value()) {
        // We do not have any local subtrees, so it cannot be possible
        // for the childMap to think we have any latest subtree, which
        // suggests the wrong childMap was passed or the wrong backing
        // view has been set in the cache.
        if (childMap.parentLatestSubtree.has_value()) {
            throw std::runtime_error("cache inconsistency; child view of parent's latest subtree cannot be correct");
        }
    } else {
        uint64_t pops;
        // Compute the number of times we must PopSubtree until our best subtree
        // is the same as the child's parentLatestSubtree index.
        if (childMap.parentLatestSubtree.has_value()) {
            if (childMap.parentLatestSubtree.value().index > bestSubtree.value().index) {
                throw std::runtime_error("cache inconsistency; child view of parent's latest subtree cannot be correct");
            }
            pops = bestSubtree.value().index - childMap.parentLatestSubtree.value().index;
        } else {
            // We have to pop everything.
            pops = bestSubtree.value().index + 1;
        }

        for (uint64_t i = 0; i < pops; i++) {
            PopSubtree(parentView);
        }
    }

    // Now we can inherit the child's new subtrees
    newSubtrees.insert(
        newSubtrees.end(),
        std::make_move_iterator(childMap.newSubtrees.begin()),
        std::make_move_iterator(childMap.newSubtrees.end())
    );

    childMap.clear();
}
