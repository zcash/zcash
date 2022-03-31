// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "coins.h"

#include "memusage.h"
#include "random.h"
#include "version.h"
#include "policy/fees.h"

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
bool CCoinsView::GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const { return false; }
bool CCoinsView::GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const { return false; }
bool CCoinsView::GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleFrontier &tree) const { return false; }
bool CCoinsView::GetNullifier(const uint256 &nullifier, ShieldedType type) const { return false; }
bool CCoinsView::GetCoins(const uint256 &txid, CCoins &coins) const { return false; }
bool CCoinsView::HaveCoins(const uint256 &txid) const { return false; }
uint256 CCoinsView::GetBestBlock() const { return uint256(); }
uint256 CCoinsView::GetBestAnchor(ShieldedType type) const { return uint256(); };
HistoryIndex CCoinsView::GetHistoryLength(uint32_t epochId) const { return 0; }
HistoryNode CCoinsView::GetHistoryAt(uint32_t epochId, HistoryIndex index) const { return HistoryNode(); }
uint256 CCoinsView::GetHistoryRoot(uint32_t epochId) const { return uint256(); }

bool CCoinsView::BatchWrite(CCoinsMap &mapCoins,
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
                            CHistoryCacheMap &historyCacheMap) { return false; }
bool CCoinsView::GetStats(CCoinsStats &stats) const { return false; }


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
                                  CHistoryCacheMap &historyCacheMap) {
    return base->BatchWrite(mapCoins, hashBlock,
                            hashSproutAnchor, hashSaplingAnchor, hashOrchardAnchor,
                            mapSproutAnchors, mapSaplingAnchors, mapOrchardAnchors,
                            mapSproutNullifiers, mapSaplingNullifiers, mapOrchardNullifiers,
                            historyCacheMap);
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
    for (const SpendDescription &spendDescription : tx.vShieldedSpend) {
        std::pair<CNullifiersMap::iterator, bool> ret = cacheSaplingNullifiers.insert(std::make_pair(spendDescription.nullifier, CNullifiersCacheEntry()));
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

/* ModifyNewCoins allows for faster coin modification when creating the new
 * outputs from a transaction.  It assumes that BIP 30 (no duplicate txids)
 * applies and has already been tested for (or the test is not required due to
 * BIP 34, height in coinbase).  If we can assume BIP 30 then we know that any
 * non-coinbase transaction we are adding to the UTXO must not already exist in
 * the utxo unless it is fully spent.  Thus we can check only if it exists DIRTY
 * at the current level of the cache, in which case it is not safe to mark it
 * FRESH (b/c then its spentness still needs to flushed).  If it's not dirty and
 * doesn't exist or is pruned in the current cache, we know it either doesn't
 * exist or is pruned in parent caches, which is the definition of FRESH.
 */
CCoinsModifier CCoinsViewCache::ModifyNewCoins(const uint256 &txid) {
    assert(!hasModifier);
    std::pair<CCoinsMap::iterator, bool> ret = cacheCoins.insert(std::make_pair(txid, CCoinsCacheEntry()));
    // New coins must not already exist.
    if (!ret.first->second.coins.IsPruned())
        throw std::logic_error("ModifyNewCoins should not find pre-existing coins on a non-coinbase unless they are pruned!");

    if (!(ret.first->second.flags & CCoinsCacheEntry::DIRTY)) {
        // If the coin is known to be pruned (have no unspent outputs) in
        // the current view and the cache entry is not dirty, we know the
        // coin also must be pruned in the parent view as well, so it is safe
        // to mark this fresh.
        ret.first->second.flags |= CCoinsCacheEntry::FRESH;
    }
    ret.first->second.coins.Clear();
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
                                 CHistoryCacheMap &historyCacheMapIn) {
    assert(!hasModifier);
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) { // Ignore non-dirty entries (optimization).
            CCoinsMap::iterator itUs = cacheCoins.find(it->first);
            if (itUs == cacheCoins.end()) {
                // The parent cache does not have an entry, while the child does
                // We can ignore it if it's both FRESH and pruned in the child
                if (!(it->second.flags & CCoinsCacheEntry::FRESH && it->second.coins.IsPruned())) {
                    // Otherwise we will need to create it in the parent
                    // and move the data up and mark it as dirty
                    CCoinsCacheEntry& entry = cacheCoins[it->first];
                    entry.coins.swap(it->second.coins);
                    cachedCoinsUsage += entry.coins.DynamicMemoryUsage();
                    entry.flags = CCoinsCacheEntry::DIRTY;
                    // We can mark it FRESH in the parent if it was FRESH in the child
                    // Otherwise it might have just been flushed from the parent's cache
                    // and already exist in the grandparent
                    if (it->second.flags & CCoinsCacheEntry::FRESH)
                        entry.flags |= CCoinsCacheEntry::FRESH;
                }
            } else {
                // Found the entry in the parent cache
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
                    // NOTE: It is possible the child has a FRESH flag here in
                    // the event the entry we found in the parent is pruned. But
                    // we must not copy that FRESH flag to the parent as that
                    // pruned state likely still needs to be communicated to the
                    // grandparent.
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

    hashSproutAnchor = hashSproutAnchorIn;
    hashSaplingAnchor = hashSaplingAnchorIn;
    hashOrchardAnchor = hashOrchardAnchorIn;
    hashBlock = hashBlockIn;
    return true;
}

bool CCoinsViewCache::Flush() {
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
                                historyCacheMap);
    cacheCoins.clear();
    cacheSproutAnchors.clear();
    cacheSaplingAnchors.clear();
    cacheOrchardAnchors.clear();
    cacheSproutNullifiers.clear();
    cacheSaplingNullifiers.clear();
    cacheOrchardNullifiers.clear();
    historyCacheMap.clear();
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
    if (tx.IsCoinBase())
        return 0;

    CAmount nResult = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        nResult += GetOutputFor(tx.vin[i]).nValue;

    nResult += tx.GetShieldedValueIn();

    return nResult;
}

std::optional<UnsatisfiedShieldedReq> CCoinsViewCache::HaveShieldedRequirements(const CTransaction& tx) const
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
                return UnsatisfiedShieldedReq::SproutDuplicateNullifier;
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
            return UnsatisfiedShieldedReq::SproutUnknownAnchor;
        }

        for (const uint256& commitment : joinsplit.commitments)
        {
            tree.append(commitment);
        }

        intermediates.insert(std::make_pair(tree.root(), tree));
    }

    for (const SpendDescription &spendDescription : tx.vShieldedSpend) {
        if (GetNullifier(spendDescription.nullifier, SAPLING)) { // Prevent double spends
            auto txid = tx.GetHash().ToString();
            auto nf = spendDescription.nullifier.ToString();
            TracingWarn("consensus", "Sapling double-spend detected",
                "txid", txid.c_str(),
                "nf", nf.c_str());
            return UnsatisfiedShieldedReq::SaplingDuplicateNullifier;
        }

        SaplingMerkleTree tree;
        if (!GetSaplingAnchorAt(spendDescription.anchor, tree)) {
            auto txid = tx.GetHash().ToString();
            auto anchor = spendDescription.anchor.ToString();
            TracingWarn("consensus", "Transaction uses unknown Sapling anchor",
                "txid", txid.c_str(),
                "anchor", anchor.c_str());
            return UnsatisfiedShieldedReq::SaplingUnknownAnchor;
        }
    }

    for (const uint256 &nullifier : tx.GetOrchardBundle().GetNullifiers()) {
        if (GetNullifier(nullifier, ORCHARD)) { // Prevent double spends
            auto txid = tx.GetHash().ToString();
            auto nf = nullifier.ToString();
            TracingWarn("consensus", "Orchard double-spend detected",
                "txid", txid.c_str(),
                "nf", nf.c_str());
            return UnsatisfiedShieldedReq::OrchardDuplicateNullifier;
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
            return UnsatisfiedShieldedReq::OrchardUnknownAnchor;
        }
    }

    return std::nullopt;
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

double CCoinsViewCache::GetPriority(const CTransaction &tx, int nHeight) const
{
    if (tx.IsCoinBase())
        return 0.0;

    // Shielded transfers do not reveal any information about the value or age of a note, so we
    // cannot apply the priority algorithm used for transparent utxos.  Instead, we just
    // use the maximum priority for all (partially or fully) shielded transactions.
    // (Note that coinbase transactions cannot contain JoinSplits, or Sapling shielded Spends or Outputs.)

    if (tx.vJoinSplit.size() > 0 || tx.vShieldedSpend.size() > 0 || tx.vShieldedOutput.size() > 0) {
        return MAX_PRIORITY;
    }

    // FIXME: this logic is partially duplicated between here and CreateNewBlock in miner.cpp.
    double dResult = 0.0;
    for (const CTxIn& txin : tx.vin)
    {
        const CCoins* coins = AccessCoins(txin.prevout.hash);
        assert(coins);
        if (!coins->IsAvailable(txin.prevout.n)) continue;
        if (coins->nHeight < nHeight) {
            dResult += coins->vout[txin.prevout.n].nValue * (nHeight-coins->nHeight);
        }
    }

    return tx.ComputePriority(dResult);
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
