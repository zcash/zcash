// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2022-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "coins.h"
#include "gtest/utils.h"
#include "script/standard.h"
#include "uint256.h"
#include "util/strencodings.h"
#include "test/test_bitcoin.h"
#include "consensus/validation.h"
#include "main.h"
#include "undo.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "transaction_builder.h"
#include "util/test.h"
#include "zcash/Note.hpp"
#include "zcash/address/mnemonic.h"

#include <vector>
#include <map>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "zcash/IncrementalMerkleTree.hpp"

namespace
{
class CCoinsViewTest : public CCoinsView
{
    uint256 hashBestBlock_;
    uint256 hashBestSproutAnchor_;
    uint256 hashBestSaplingAnchor_;
    uint256 hashBestOrchardAnchor_;
    std::map<uint256, CCoins> map_;
    std::map<uint256, SproutMerkleTree> mapSproutAnchors_;
    std::map<uint256, SaplingMerkleTree> mapSaplingAnchors_;
    std::map<uint256, OrchardMerkleFrontier> mapOrchardAnchors_;
    std::map<uint256, bool> mapSproutNullifiers_;
    std::map<uint256, bool> mapSaplingNullifiers_;
    std::map<uint256, bool> mapOrchardNullifiers_;

    std::vector<libzcash::SubtreeData> saplingSubtrees;
    std::optional<libzcash::LatestSubtree> latestSaplingSubtree;
    std::vector<libzcash::SubtreeData> orchardSubtrees;
    std::optional<libzcash::LatestSubtree> latestOrchardSubtree;

    void BatchWriteSubtrees(
        std::optional<libzcash::LatestSubtree> &latestSubtree,
        std::vector<libzcash::SubtreeData> &dbSubtrees,
        const SubtreeCache &cacheIn
    )
    {
        if (cacheIn.parentLatestSubtree.has_value()) {
            // This subtree must exist in our local vector
            auto index = cacheIn.parentLatestSubtree.value().index;
            ASSERT_TRUE(index < dbSubtrees.size());
            EXPECT_EQ(cacheIn.parentLatestSubtree.value().root, dbSubtrees[index].root);

            // Truncate
            dbSubtrees.resize(index + 1);
        } else {
            dbSubtrees.resize(0);
        }

        for (const libzcash::SubtreeData& subtreeData : cacheIn.newSubtrees) {
            // We smuggle in the expected index using nHeight for testing purposes
            EXPECT_EQ(dbSubtrees.size(), subtreeData.nHeight);
            dbSubtrees.push_back(subtreeData);
        }

        if (dbSubtrees.empty()) {
            latestSubtree = std::nullopt;
        } else {
            auto index = dbSubtrees.size() - 1;
            latestSubtree = libzcash::LatestSubtree(index, dbSubtrees[index].root, dbSubtrees[index].nHeight);
        }
    }

public:
    // This is used to check if subtree logic is consistent between
    // this mock database and the real one's logic, to test BatchWrite
    // and other functions in the real database.
    CCoinsViewDB *memorydb = nullptr;

    CCoinsViewTest() {
        hashBestSproutAnchor_ = SproutMerkleTree::empty_root();
        hashBestSaplingAnchor_ = SaplingMerkleTree::empty_root();
        hashBestOrchardAnchor_ = OrchardMerkleFrontier::empty_root();
    }

    bool GetSproutAnchorAt(const uint256& rt, SproutMerkleTree &tree) const {
        if (rt == SproutMerkleTree::empty_root()) {
            SproutMerkleTree new_tree;
            tree = new_tree;
            return true;
        }

        std::map<uint256, SproutMerkleTree>::const_iterator it = mapSproutAnchors_.find(rt);
        if (it == mapSproutAnchors_.end()) {
            return false;
        } else {
            tree = it->second;
            return true;
        }
    }

    bool GetSaplingAnchorAt(const uint256& rt, SaplingMerkleTree &tree) const {
        if (rt == SaplingMerkleTree::empty_root()) {
            SaplingMerkleTree new_tree;
            tree = new_tree;
            return true;
        }

        std::map<uint256, SaplingMerkleTree>::const_iterator it = mapSaplingAnchors_.find(rt);
        if (it == mapSaplingAnchors_.end()) {
            return false;
        } else {
            tree = it->second;
            return true;
        }
    }

    bool GetOrchardAnchorAt(const uint256& rt, OrchardMerkleFrontier &tree) const {
        if (rt == OrchardMerkleFrontier::empty_root()) {
            OrchardMerkleFrontier new_tree;
            tree = new_tree;
            return true;
        }

        std::map<uint256, OrchardMerkleFrontier>::const_iterator it = mapOrchardAnchors_.find(rt);
        if (it == mapOrchardAnchors_.end()) {
            return false;
        } else {
            tree = it->second;
            return true;
        }
    }

    bool GetNullifier(const uint256 &nf, ShieldedType type) const
    {
        const std::map<uint256, bool>* mapToUse;
        switch (type) {
            case SPROUT:
                mapToUse = &mapSproutNullifiers_;
                break;
            case SAPLING:
                mapToUse = &mapSaplingNullifiers_;
                break;
            case ORCHARD:
                mapToUse = &mapOrchardNullifiers_;
                break;
            default:
                throw std::runtime_error("Unknown shielded type");
        }
        std::map<uint256, bool>::const_iterator it = mapToUse->find(nf);
        if (it == mapToUse->end()) {
            return false;
        } else {
            // The map shouldn't contain any false entries.
            assert(it->second);
            return true;
        }
    }

    uint256 GetBestAnchor(ShieldedType type) const {
        switch (type) {
            case SPROUT:
                return hashBestSproutAnchor_;
                break;
            case SAPLING:
                return hashBestSaplingAnchor_;
                break;
            case ORCHARD:
                return hashBestOrchardAnchor_;
                break;
            default:
                throw std::runtime_error("Unknown shielded type");
        }
    }

    bool GetCoins(const uint256& txid, CCoins& coins) const
    {
        std::map<uint256, CCoins>::const_iterator it = map_.find(txid);
        if (it == map_.end()) {
            return false;
        }
        coins = it->second;
        if (coins.IsPruned() && InsecureRandBool() == 0) {
            // Randomly return false in case of an empty entry.
            return false;
        }
        return true;
    }

    bool HaveCoins(const uint256& txid) const
    {
        CCoins coins;
        return GetCoins(txid, coins);
    }

    uint256 GetBestBlock() const { return hashBestBlock_; }

    HistoryIndex GetHistoryLength(uint32_t epochId) const { return 0; }
    HistoryNode GetHistoryAt(uint32_t epochId, HistoryIndex index) const {
        throw std::runtime_error("`GetHistoryAt` unimplemented for mock CCoinsViewTest");
    }
    uint256 GetHistoryRoot(uint32_t epochId) const {
        throw std::runtime_error("`GetHistoryRoot` unimplemented for mock CCoinsViewTest");
    }

    std::optional<libzcash::LatestSubtree> GetLatestSubtree(ShieldedType type) const {
        std::optional<libzcash::LatestSubtree> latestSubtreeDB;
        if (memorydb) {
            latestSubtreeDB = memorydb->GetLatestSubtree(type);
        }

        switch (type) {
            case SAPLING:
                {
                    if (memorydb) {
                        assert(latestSubtreeDB.has_value() == latestSaplingSubtree.has_value());
                        if (latestSubtreeDB.has_value()) {
                            assert(latestSubtreeDB->index == latestSaplingSubtree->index);
                            assert(latestSubtreeDB->nHeight == latestSaplingSubtree->nHeight);
                        }
                    }
                    return latestSaplingSubtree;
                }
            case ORCHARD:
                {
                    if (memorydb) {
                        assert(latestSubtreeDB.has_value() == latestOrchardSubtree.has_value());
                        if (latestSubtreeDB.has_value()) {
                            assert(latestSubtreeDB->index == latestOrchardSubtree->index);
                            assert(latestSubtreeDB->nHeight == latestOrchardSubtree->nHeight);
                        }
                    }
                    return latestOrchardSubtree;
                }
            default:
                throw std::runtime_error("Unknown shielded type");
        }
    };
    std::optional<libzcash::SubtreeData> GetSubtreeData(
            ShieldedType type,
            libzcash::SubtreeIndex index) const
    {
        std::optional<libzcash::SubtreeData> subtreeDataDB;
        if (memorydb) {
            subtreeDataDB = memorydb->GetSubtreeData(type, index);
        }

        const std::vector<libzcash::SubtreeData>* vecToUse;
        switch (type) {
            case SAPLING:
                vecToUse = &saplingSubtrees;
                break;
            case ORCHARD:
                vecToUse = &orchardSubtrees;
                break;
            default:
                throw std::runtime_error("Unknown shielded type");
        }
        if (index >= vecToUse->size()) {
            if (memorydb) {
                EXPECT_FALSE(subtreeDataDB.has_value());
            }
            return std::nullopt;
        } else {
            if (memorydb) {
                assert(subtreeDataDB.has_value());
                EXPECT_EQ(vecToUse->at(index).nHeight, subtreeDataDB->nHeight);
            }
            return vecToUse->at(index);
        }
    };

    void BatchWriteNullifiers(CNullifiersMap& mapNullifiers, std::map<uint256, bool>& cacheNullifiers)
    {
        for (CNullifiersMap::iterator it = mapNullifiers.begin(); it != mapNullifiers.end(); ) {
            if (it->second.flags & CNullifiersCacheEntry::DIRTY) {
                // Same optimization used in CCoinsViewDB is to only write dirty entries.
                if (it->second.entered) {
                    cacheNullifiers[it->first] = true;
                } else {
                    cacheNullifiers.erase(it->first);
                }
            }
            it = mapNullifiers.erase(it);
        }
    }

    template<typename Tree, typename Map, typename MapEntry>
    void BatchWriteAnchors(Map& mapAnchors, std::map<uint256, Tree>& cacheAnchors)
    {
        for (auto it = mapAnchors.begin(); it != mapAnchors.end(); ) {
            if (it->second.flags & MapEntry::DIRTY) {
                // Same optimization used in CCoinsViewDB is to only write dirty entries.
                if (it->second.entered) {
                    if (it->first != Tree::empty_root()) {
                        auto ret = cacheAnchors.insert(std::make_pair(it->first, Tree())).first;
                        ret->second = it->second.tree;
                    }
                } else {
                    cacheAnchors.erase(it->first);
                }
            }
            it = mapAnchors.erase(it);
        }
    }

    bool BatchWrite(CCoinsMap& mapCoins,
                    const uint256& hashBlock,
                    const uint256& hashSproutAnchor,
                    const uint256& hashSaplingAnchor,
                    const uint256& hashOrchardAnchor,
                    CAnchorsSproutMap& mapSproutAnchors,
                    CAnchorsSaplingMap& mapSaplingAnchors,
                    CAnchorsOrchardMap& mapOrchardAnchors,
                    CNullifiersMap& mapSproutNullifiers,
                    CNullifiersMap& mapSaplingNullifiers,
                    CNullifiersMap& mapOrchardNullifiers,
                    CHistoryCacheMap &historyCacheMap,
                    SubtreeCache &cacheSaplingSubtrees,
                    SubtreeCache &cacheOrchardSubtrees)
    {
        for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end(); ) {
            if (it->second.flags & CCoinsCacheEntry::DIRTY) {
                // Same optimization used in CCoinsViewDB is to only write dirty entries.
                map_[it->first] = it->second.coins;
                if (it->second.coins.IsPruned() && InsecureRandRange(3) == 0) {
                    // Randomly delete empty entries on write.
                    map_.erase(it->first);
                }
            }
            it = mapCoins.erase(it);
        }

        BatchWriteAnchors<SproutMerkleTree, CAnchorsSproutMap, CAnchorsSproutCacheEntry>(mapSproutAnchors, mapSproutAnchors_);
        BatchWriteAnchors<SaplingMerkleTree, CAnchorsSaplingMap, CAnchorsSaplingCacheEntry>(mapSaplingAnchors, mapSaplingAnchors_);
        BatchWriteAnchors<OrchardMerkleFrontier, CAnchorsOrchardMap, CAnchorsOrchardCacheEntry>(mapOrchardAnchors, mapOrchardAnchors_);

        BatchWriteNullifiers(mapSproutNullifiers, mapSproutNullifiers_);
        BatchWriteNullifiers(mapSaplingNullifiers, mapSaplingNullifiers_);
        BatchWriteNullifiers(mapOrchardNullifiers, mapOrchardNullifiers_);

        BatchWriteSubtrees(latestSaplingSubtree, saplingSubtrees, cacheSaplingSubtrees);
        BatchWriteSubtrees(latestOrchardSubtree, orchardSubtrees, cacheOrchardSubtrees);

        if (memorydb) {
            memorydb->BatchWrite(mapCoins,
                                 hashBlock,
                                 hashSproutAnchor,
                                 hashSaplingAnchor,
                                 hashOrchardAnchor,
                                 mapSproutAnchors,
                                 mapSaplingAnchors,
                                 mapOrchardAnchors,
                                 mapSproutNullifiers,
                                 mapSaplingNullifiers,
                                 mapOrchardNullifiers,
                                 historyCacheMap,
                                 cacheSaplingSubtrees,
                                 cacheOrchardSubtrees);
        }

        if (!hashBlock.IsNull())
            hashBestBlock_ = hashBlock;
        if (!hashSproutAnchor.IsNull())
            hashBestSproutAnchor_ = hashSproutAnchor;
        if (!hashSaplingAnchor.IsNull())
            hashBestSaplingAnchor_ = hashSaplingAnchor;
        if (!hashOrchardAnchor.IsNull())
            hashBestOrchardAnchor_ = hashOrchardAnchor;
        return true;
    }

    bool GetStats(CCoinsStats& stats) const { return false; }
};

class CCoinsViewCacheTest : public CCoinsViewCache
{
public:
    CCoinsViewCacheTest(CCoinsView* base) : CCoinsViewCache(base) {}

    void SelfTest() const
    {
        // Manually recompute the dynamic usage of the whole data, and compare it.
        size_t ret = memusage::DynamicUsage(cacheCoins) +
                     memusage::DynamicUsage(cacheSproutAnchors) +
                     memusage::DynamicUsage(cacheSaplingAnchors) +
                     memusage::DynamicUsage(cacheOrchardAnchors) +
                     memusage::DynamicUsage(cacheSproutNullifiers) +
                     memusage::DynamicUsage(cacheSaplingNullifiers) +
                     memusage::DynamicUsage(cacheOrchardNullifiers) +
                     memusage::DynamicUsage(historyCacheMap) +
                     memusage::DynamicUsage(cacheSaplingSubtrees) +
                     memusage::DynamicUsage(cacheOrchardSubtrees);
        for (CCoinsMap::iterator it = cacheCoins.begin(); it != cacheCoins.end(); it++) {
            ret += it->second.coins.DynamicMemoryUsage();
        }
        EXPECT_EQ(DynamicMemoryUsage(), ret);
    }

};

libzcash::SubtreeData RandomSubtree(libzcash::SubtreeIndex index) {
    std::array<uint8_t, 32> root;
    for (size_t i = 0; i < 32; i++) {
        root[i] = InsecureRandRange(256);
    }

    // We store the intended index in the nHeight field
    // so that the mock database can exercise tests. (Indices
    // are not stored in the SubtreeData struct because they
    // are already known by the caller of GetSubtreeData.)
    return libzcash::SubtreeData(root, index);
}

class TxWithNullifiers
{
public:
    CTransaction tx;
    CTransaction txV5;
    uint256 sproutNullifier;
    uint256 saplingNullifier;
    uint256 orchardNullifier;

    TxWithNullifiers()
    {
        CMutableTransaction mutableTx;
        CMutableTransaction mutableTxV5;

        mutableTxV5.fOverwintered = true;
        mutableTxV5.nVersion = ZIP225_TX_VERSION;
        mutableTxV5.nVersionGroupId = ZIP225_VERSION_GROUP_ID;
        mutableTxV5.nConsensusBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_NU5].nBranchId;

        sproutNullifier = GetRandHash();
        JSDescription jsd;
        jsd.nullifiers[0] = sproutNullifier;
        mutableTx.vJoinSplit.emplace_back(jsd);

        mutableTxV5.saplingBundle = sapling::test_only_invalid_bundle(1, 0, 0);
        saplingNullifier = uint256::FromRawBytes(mutableTxV5.saplingBundle.GetDetails().spends()[0].nullifier());

        RawHDSeed seed(32, 0);
        auto to = libzcash::OrchardSpendingKey::ForAccount(seed, 133, 0)
            .ToFullViewingKey()
            .GetChangeAddress();
        uint256 orchardAnchor;
        uint256 dataToBeSigned;
        auto builder = orchard::Builder(false, orchardAnchor);
        builder.AddOutput(std::nullopt, to, 0, std::nullopt);
        mutableTxV5.orchardBundle = builder.Build().value().ProveAndSign({}, dataToBeSigned).value();
        orchardNullifier = mutableTxV5.orchardBundle.GetNullifiers().at(0);

        tx = CTransaction(mutableTx);
        txV5 = CTransaction(mutableTxV5);
    }
};

}

uint256 appendRandomSproutCommitment(SproutMerkleTree &tree)
{
    libzcash::SproutSpendingKey k = libzcash::SproutSpendingKey::random();
    libzcash::SproutPaymentAddress addr = k.address();

    libzcash::SproutNote note(addr.a_pk, 0, uint256(), uint256());

    auto cm = note.cm();
    tree.append(cm);
    return cm;
}

template<typename Tree> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, Tree &tree);
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, SproutMerkleTree &tree) { return cache.GetSproutAnchorAt(rt, tree); }
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, SaplingMerkleTree &tree) { return cache.GetSaplingAnchorAt(rt, tree); }
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, OrchardMerkleFrontier &tree) { return cache.GetOrchardAnchorAt(rt, tree); }



void checkNullifierCache(const CCoinsViewCacheTest &cache, const TxWithNullifiers &txWithNullifiers, bool shouldBeInCache) {
    // Make sure the nullifiers have not gotten mixed up
    EXPECT_FALSE(cache.GetNullifier(txWithNullifiers.sproutNullifier, SAPLING));
    EXPECT_FALSE(cache.GetNullifier(txWithNullifiers.sproutNullifier, ORCHARD));
    EXPECT_FALSE(cache.GetNullifier(txWithNullifiers.saplingNullifier, SPROUT));
    EXPECT_FALSE(cache.GetNullifier(txWithNullifiers.saplingNullifier, ORCHARD));
    EXPECT_FALSE(cache.GetNullifier(txWithNullifiers.orchardNullifier, SPROUT));
    EXPECT_FALSE(cache.GetNullifier(txWithNullifiers.orchardNullifier, SAPLING));
    // Check if the nullifiers either are or are not in the cache
    bool containsSproutNullifier = cache.GetNullifier(txWithNullifiers.sproutNullifier, SPROUT);
    bool containsSaplingNullifier = cache.GetNullifier(txWithNullifiers.saplingNullifier, SAPLING);
    bool containsOrchardNullifier = cache.GetNullifier(txWithNullifiers.orchardNullifier, ORCHARD);
    EXPECT_EQ(containsSproutNullifier, shouldBeInCache);
    EXPECT_EQ(containsSaplingNullifier, shouldBeInCache);
    EXPECT_EQ(containsOrchardNullifier, shouldBeInCache);
}


TEST(CoinsTests, NullifierRegression)
{
    LoadProofParameters();
    // Correct behavior:
    {
    SCOPED_TRACE("add-remove without flush");
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        TxWithNullifiers txWithNullifiers;

        // Insert a nullifier into the base.
        cache1.SetNullifiers(txWithNullifiers.tx, true);
        cache1.SetNullifiers(txWithNullifiers.txV5, true);
        checkNullifierCache(cache1, txWithNullifiers, true);
        cache1.Flush(); // Flush to base.

        // Remove the nullifier from cache
        cache1.SetNullifiers(txWithNullifiers.tx, false);
        cache1.SetNullifiers(txWithNullifiers.txV5, false);

        // The nullifier now should be `false`.
        checkNullifierCache(cache1, txWithNullifiers, false);
    }

    // Also correct behavior:
    {
    SCOPED_TRACE("add-remove with flush");
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        TxWithNullifiers txWithNullifiers;

        // Insert a nullifier into the base.
        cache1.SetNullifiers(txWithNullifiers.tx, true);
        cache1.SetNullifiers(txWithNullifiers.txV5, true);
        checkNullifierCache(cache1, txWithNullifiers, true);
        cache1.Flush(); // Flush to base.

        // Remove the nullifier from cache
        cache1.SetNullifiers(txWithNullifiers.tx, false);
        cache1.SetNullifiers(txWithNullifiers.txV5, false);
        cache1.Flush(); // Flush to base.

        // The nullifier now should be `false`.
        checkNullifierCache(cache1, txWithNullifiers, false);
    }

    // Works because we bring it from the parent cache:
    {
    SCOPED_TRACE("remove from parent");
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert a nullifier into the base.
        TxWithNullifiers txWithNullifiers;
        cache1.SetNullifiers(txWithNullifiers.tx, true);
        cache1.SetNullifiers(txWithNullifiers.txV5, true);
        checkNullifierCache(cache1, txWithNullifiers, true);
        cache1.Flush(); // Empties cache.

        // Create cache on top.
        {
            // Remove the nullifier.
            CCoinsViewCacheTest cache2(&cache1);
            checkNullifierCache(cache2, txWithNullifiers, true);
            cache1.SetNullifiers(txWithNullifiers.tx, false);
            cache1.SetNullifiers(txWithNullifiers.txV5, false);
            cache2.Flush(); // Empties cache, flushes to cache1.
        }

        // The nullifier now should be `false`.
        checkNullifierCache(cache1, txWithNullifiers, false);
    }

    // Was broken:
    {
    SCOPED_TRACE("remove from child");
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert a nullifier into the base.
        TxWithNullifiers txWithNullifiers;
        cache1.SetNullifiers(txWithNullifiers.tx, true);
        cache1.SetNullifiers(txWithNullifiers.txV5, true);
        cache1.Flush(); // Empties cache.

        // Create cache on top.
        {
            // Remove the nullifier.
            CCoinsViewCacheTest cache2(&cache1);
            cache2.SetNullifiers(txWithNullifiers.tx, false);
            cache2.SetNullifiers(txWithNullifiers.txV5, false);
            cache2.Flush(); // Empties cache, flushes to cache1.
        }

        // The nullifier now should be `false`.
        checkNullifierCache(cache1, txWithNullifiers, false);
    }
}

template<typename Tree> void anchorPopRegressionTestImpl(ShieldedType type)
{
    // Correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Create dummy anchor/commitment
        Tree tree;
        AppendRandomLeaf(tree);

        // Add the anchor
        cache1.PushAnchor(tree);
        cache1.Flush();

        // Remove the anchor
        cache1.PopAnchor(Tree::empty_root(), type);
        cache1.Flush();

        // Add the anchor back
        cache1.PushAnchor(tree);
        cache1.Flush();

        // The base contains the anchor, of course!
        {
            Tree checkTree;
            EXPECT_TRUE(GetAnchorAt(cache1, tree.root(), checkTree));
            EXPECT_TRUE(checkTree.root() == tree.root());
        }
    }

    // Previously incorrect behavior
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Create dummy anchor/commitment
        Tree tree;
        AppendRandomLeaf(tree);

        // Add the anchor and flush to disk
        cache1.PushAnchor(tree);
        cache1.Flush();

        // Remove the anchor, but don't flush yet!
        cache1.PopAnchor(Tree::empty_root(), type);

        {
            CCoinsViewCacheTest cache2(&cache1); // Build cache on top
            cache2.PushAnchor(tree); // Put the same anchor back!
            cache2.Flush(); // Flush to cache1
        }

        // cache2's flush kinda worked, i.e. cache1 thinks the
        // tree is there, but it didn't bring down the correct
        // treestate...
        {
            Tree checktree;
            EXPECT_TRUE(GetAnchorAt(cache1, tree.root(), checktree));
            EXPECT_TRUE(checktree.root() == tree.root()); // Oh, shucks.
        }

        // Flushing cache won't help either, just makes the inconsistency
        // permanent.
        cache1.Flush();
        {
            Tree checktree;
            EXPECT_TRUE(GetAnchorAt(cache1, tree.root(), checktree));
            EXPECT_TRUE(checktree.root() == tree.root()); // Oh, shucks.
        }
    }
}


TEST(CoinsTests, AnchorPopRegression)
{
    LoadProofParameters();

    {
    SCOPED_TRACE("Sprout");
        anchorPopRegressionTestImpl<SproutMerkleTree>(SPROUT);
    }

    {
    SCOPED_TRACE("Sapling");
        anchorPopRegressionTestImpl<SaplingMerkleTree>(SAPLING);
    }

    {
    SCOPED_TRACE("Orchard");
        anchorPopRegressionTestImpl<OrchardMerkleFrontier>(ORCHARD);
    }
}


template<typename Tree> void anchorRegressionTestImpl(ShieldedType type)
{
    // Correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        AppendRandomLeaf(tree);

        cache1.PushAnchor(tree);
        cache1.Flush();

        cache1.PopAnchor(Tree::empty_root(), type);
        EXPECT_TRUE(cache1.GetBestAnchor(type) == Tree::empty_root());
        EXPECT_TRUE(!GetAnchorAt(cache1, tree.root(), tree));
    }

    // Also correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        AppendRandomLeaf(tree);
        cache1.PushAnchor(tree);
        cache1.Flush();

        cache1.PopAnchor(Tree::empty_root(), type);
        cache1.Flush();
        EXPECT_TRUE(cache1.GetBestAnchor(type) == Tree::empty_root());
        EXPECT_TRUE(!GetAnchorAt(cache1, tree.root(), tree));
    }

    // Works because we bring the anchor in from parent cache.
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        AppendRandomLeaf(tree);
        cache1.PushAnchor(tree);
        cache1.Flush();

        {
            // Pop anchor.
            CCoinsViewCacheTest cache2(&cache1);
            EXPECT_TRUE(GetAnchorAt(cache2, tree.root(), tree));
            cache2.PopAnchor(Tree::empty_root(), type);
            cache2.Flush();
        }

        EXPECT_TRUE(cache1.GetBestAnchor(type) == Tree::empty_root());
        EXPECT_TRUE(!GetAnchorAt(cache1, tree.root(), tree));
    }

    // Was broken:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        AppendRandomLeaf(tree);
        cache1.PushAnchor(tree);
        cache1.Flush();

        {
            // Pop anchor.
            CCoinsViewCacheTest cache2(&cache1);
            cache2.PopAnchor(Tree::empty_root(), type);
            cache2.Flush();
        }

        EXPECT_TRUE(cache1.GetBestAnchor(type) == Tree::empty_root());
        EXPECT_TRUE(!GetAnchorAt(cache1, tree.root(), tree));
    }
}


TEST(CoinsTests, AnchorRegression)
{
    LoadProofParameters();

    {
    SCOPED_TRACE("Sprout");
        anchorRegressionTestImpl<SproutMerkleTree>(SPROUT);
    }

    {
    SCOPED_TRACE("Sapling");
        anchorRegressionTestImpl<SaplingMerkleTree>(SAPLING);
    }

    {
    SCOPED_TRACE("Orchard");
        anchorRegressionTestImpl<OrchardMerkleFrontier>(ORCHARD);
    }
}


TEST(CoinsTests, NullifiersTest)
{
    LoadProofParameters();

    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    TxWithNullifiers txWithNullifiers;
    {
        SCOPED_TRACE("cache with unspent nullifiers");
        checkNullifierCache(cache, txWithNullifiers, false);
    }
    cache.SetNullifiers(txWithNullifiers.tx, true);
    cache.SetNullifiers(txWithNullifiers.txV5, true);
    {
        SCOPED_TRACE("cache with spent nullifiers");
        checkNullifierCache(cache, txWithNullifiers, true);
    }
    cache.Flush();

    CCoinsViewCacheTest cache2(&base);

    {
        SCOPED_TRACE("cache2 with spent nullifiers");
        checkNullifierCache(cache2, txWithNullifiers, true);
    }
    cache2.SetNullifiers(txWithNullifiers.tx, false);
    cache2.SetNullifiers(txWithNullifiers.txV5, false);
    {
        SCOPED_TRACE("cache2 with unspent nullifiers");
        checkNullifierCache(cache2, txWithNullifiers, false);
    }
    cache2.Flush();

    CCoinsViewCacheTest cache3(&base);
    {
        SCOPED_TRACE("cache3 with unspent nullifiers");
        checkNullifierCache(cache3, txWithNullifiers, false);
    }
}


template<typename Tree> void anchorsFlushImpl(ShieldedType type)
{
    CCoinsViewTest base;
    uint256 newrt;
    {
        CCoinsViewCacheTest cache(&base);
        Tree tree;
        EXPECT_TRUE(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));
        AppendRandomLeaf(tree);

        newrt = tree.root();

        cache.PushAnchor(tree);
        cache.Flush();
    }

    {
        CCoinsViewCacheTest cache(&base);
        Tree tree;
        EXPECT_TRUE(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));

        // Get the cached entry.
        EXPECT_TRUE(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));

        uint256 check_rt = tree.root();

        EXPECT_TRUE(check_rt == newrt);
    }
}


TEST(CoinsTests,AnchorsFlushTest)
{
    LoadProofParameters();
    {
    SCOPED_TRACE("Sprout");
        anchorsFlushImpl<SproutMerkleTree>(SPROUT);
    }

    {
    SCOPED_TRACE("Sapling");
        anchorsFlushImpl<SaplingMerkleTree>(SAPLING);
    }

    {
    SCOPED_TRACE("Orchard");
        anchorsFlushImpl<OrchardMerkleFrontier>(ORCHARD);
    }
}


template<typename Tree> void anchorsTestImpl(ShieldedType type)
{
    // TODO: These tests should be more methodical.

    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    EXPECT_TRUE(cache.GetBestAnchor(type) == Tree::empty_root());

    {
        Tree tree;

        EXPECT_TRUE(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));
        EXPECT_TRUE(cache.GetBestAnchor(type) == tree.root());
        AppendRandomLeaf(tree);
        AppendRandomLeaf(tree);
        AppendRandomLeaf(tree);
        AppendRandomLeaf(tree);
        AppendRandomLeaf(tree);
        AppendRandomLeaf(tree);
        AppendRandomLeaf(tree);

        Tree save_tree_for_later;
        save_tree_for_later = tree;

        uint256 newrt = tree.root();
        uint256 newrt2;

        cache.PushAnchor(tree);
        EXPECT_TRUE(cache.GetBestAnchor(type) == newrt);

        {
            Tree confirm_same;
            EXPECT_TRUE(GetAnchorAt(cache, cache.GetBestAnchor(type), confirm_same));

            EXPECT_TRUE(confirm_same.root() == newrt);
        }

        AppendRandomLeaf(tree);
        AppendRandomLeaf(tree);

        newrt2 = tree.root();

        cache.PushAnchor(tree);
        EXPECT_TRUE(cache.GetBestAnchor(type) == newrt2);

        Tree test_tree;
        EXPECT_TRUE(GetAnchorAt(cache, cache.GetBestAnchor(type), test_tree));

        EXPECT_TRUE(tree.root() == test_tree.root());

        {
            Tree test_tree2;
            GetAnchorAt(cache, newrt, test_tree2);

            EXPECT_TRUE(test_tree2.root() == newrt);
        }

        {
            cache.PopAnchor(newrt, type);
            Tree obtain_tree;
            assert(!GetAnchorAt(cache, newrt2, obtain_tree)); // should have been popped off
            assert(GetAnchorAt(cache, newrt, obtain_tree));

            assert(obtain_tree.root() == newrt);
        }
    }
}

TEST(CoinsTests, AnchorsTest)
{
    {
    SCOPED_TRACE("Sprout");
    anchorsTestImpl<SproutMerkleTree>(SPROUT);
    }

    {
    SCOPED_TRACE("Sapling");
    anchorsTestImpl<SaplingMerkleTree>(SAPLING);
    }

    {
    SCOPED_TRACE("Orchard");
    anchorsTestImpl<OrchardMerkleFrontier>(ORCHARD);
    }
}

enum SubtreeAction {
    FlushCache1,
    FlushCache2,
    Pop,
    PopIsError,

    // Push the nth index subtree to cache2
    Push0,
    Push1,
    Push2,
    Push3,

    // The latest subtree in cache N equals the index K subtree
    LatestC1EqualsX, // no value
    LatestC1Equals0,
    LatestC1Equals1,
    LatestC1Equals2,
    LatestC1Equals3,

    LatestC2EqualsX, // no value
    LatestC2Equals0,
    LatestC2Equals1,
    LatestC2Equals2,
    LatestC2Equals3,

    LatestBaseEqualsX,
    LatestBaseEquals0,
    LatestBaseEquals1,
    LatestBaseEquals2,
    LatestBaseEquals3,

    // Get K index from cache N
    Get0C1,
    Get1C1,
    Get2C1,
    Get3C1,

    Get0C2,
    Get1C2,
    Get2C2,
    Get3C2,

    Get0B,
    Get1B,
    Get2B,
    Get3B,

    // Assert that K index is not known for cache N
    None0C1,
    None1C1,
    None2C1,
    None3C1,

    None0C2,
    None1C2,
    None2C2,
    None3C2,

    None0B,
    None1B,
    None2B,
    None3B,
};

void testSubtreesForShieldedType(ShieldedType type) {
    auto attempt = [&type](std::vector<SubtreeAction> actions) {
        auto tree0 = RandomSubtree(0);
        auto tree1 = RandomSubtree(1);
        auto tree2 = RandomSubtree(2);
        auto tree3 = RandomSubtree(3);

        CCoinsViewTest base;
        SelectParams(CBaseChainParams::REGTEST);
        base.memorydb = new CCoinsViewDB(1 << 23, true);
        CCoinsViewCache cache1(&base);
        CCoinsViewCache cache2(&cache1);

        auto LatestEqualsX = [&type](CCoinsView& view) {
            auto latest = view.GetLatestSubtree(type);
            ASSERT_FALSE(latest.has_value());
        };

        auto LatestEqualsN = [&type](CCoinsView& view, int n) {
            auto latest = view.GetLatestSubtree(type);
            ASSERT_TRUE(latest.has_value());
            EXPECT_EQ(latest->index, n);
            EXPECT_EQ(latest->nHeight, n);
            auto subtree = view.GetSubtreeData(type, n);
            ASSERT_TRUE(subtree.has_value());
            EXPECT_EQ(subtree->nHeight, n);
        };

        auto GetN = [&type](CCoinsView& view, int n) {
            auto subtree = view.GetSubtreeData(type, n);
            ASSERT_TRUE(subtree.has_value());
            EXPECT_EQ(subtree->nHeight, n);
        };

        auto NoneN = [&type](CCoinsView& view, int n) {
            auto subtree = view.GetSubtreeData(type, n);
            ASSERT_FALSE(subtree.has_value());
        };

        for (auto action : actions) {
            switch (action) {
                case FlushCache1: cache1.Flush(); break;
                case FlushCache2: cache2.Flush(); break;
                case Pop: cache2.PopSubtree(type); break;
                case PopIsError:
                {
                    ASSERT_THROW({ cache2.PopSubtree(type); }, std::runtime_error);
                }
                break;
                case Push0: cache2.PushSubtree(type, tree0); break;
                case Push1: cache2.PushSubtree(type, tree1); break;
                case Push2: cache2.PushSubtree(type, tree2); break;
                case Push3: cache2.PushSubtree(type, tree3); break;

                case LatestC1EqualsX: LatestEqualsX(cache1); break;
                case LatestC1Equals0: LatestEqualsN(cache1, 0); break;
                case LatestC1Equals1: LatestEqualsN(cache1, 1); break;
                case LatestC1Equals2: LatestEqualsN(cache1, 2); break;
                case LatestC1Equals3: LatestEqualsN(cache1, 3); break;

                case LatestC2EqualsX: LatestEqualsX(cache2); break;
                case LatestC2Equals0: LatestEqualsN(cache2, 0); break;
                case LatestC2Equals1: LatestEqualsN(cache2, 1); break;
                case LatestC2Equals2: LatestEqualsN(cache2, 2); break;
                case LatestC2Equals3: LatestEqualsN(cache2, 3); break;

                case LatestBaseEqualsX: LatestEqualsX(base); break;
                case LatestBaseEquals0: LatestEqualsN(base, 0); break;
                case LatestBaseEquals1: LatestEqualsN(base, 1); break;
                case LatestBaseEquals2: LatestEqualsN(base, 2); break;
                case LatestBaseEquals3: LatestEqualsN(base, 3); break;

                case Get0C1: GetN(cache1, 0); break;
                case Get1C1: GetN(cache1, 1); break;
                case Get2C1: GetN(cache1, 2); break;
                case Get3C1: GetN(cache1, 3); break;

                case Get0C2: GetN(cache2, 0); break;
                case Get1C2: GetN(cache2, 1); break;
                case Get2C2: GetN(cache2, 2); break;
                case Get3C2: GetN(cache2, 3); break;

                case Get0B: GetN(base, 0); break;
                case Get1B: GetN(base, 1); break;
                case Get2B: GetN(base, 2); break;
                case Get3B: GetN(base, 3); break;

                case None0C1: NoneN(cache1, 0); break;
                case None1C1: NoneN(cache1, 1); break;
                case None2C1: NoneN(cache1, 2); break;
                case None3C1: NoneN(cache1, 3); break;

                case None0C2: NoneN(cache2, 0); break;
                case None1C2: NoneN(cache2, 1); break;
                case None2C2: NoneN(cache2, 2); break;
                case None3C2: NoneN(cache2, 3); break;

                case None0B: NoneN(base, 0); break;
                case None1B: NoneN(base, 1); break;
                case None2B: NoneN(base, 2); break;
                case None3B: NoneN(base, 3); break;
            }
        }

        // Sanity checks for all cases
        cache2.Flush();
        cache1.Flush();
        auto latest = base.GetLatestSubtree(type);
        if (latest.has_value()) {
            auto latest2 = cache1.GetLatestSubtree(type);
            ASSERT_TRUE(latest2.has_value());
            EXPECT_EQ(latest->index, latest2->index);
            EXPECT_EQ(latest->root, latest2->root);
            EXPECT_EQ(latest->nHeight, latest2->nHeight);
        }

        delete base.memorydb;
    };

    attempt({Push0, Push1, FlushCache2, Get0C2});
    attempt({Push0, FlushCache2, FlushCache1, LatestBaseEquals0, FlushCache1, LatestBaseEquals0});
    attempt({Push0, FlushCache2, Push1, FlushCache2, FlushCache1, Push2, Push3, Pop, FlushCache2, LatestC2Equals2, LatestC1Equals2, LatestBaseEquals1, FlushCache1, LatestBaseEquals2});
    attempt({None0B, None0C1, None0C2, LatestBaseEqualsX, LatestC1EqualsX, LatestC2EqualsX, PopIsError});
    attempt({Push0, None0B, None0C1, Get0C2, None1C2});
    attempt({Push0, None0C1, FlushCache2, Get0C2, Get0C1, LatestC1Equals0, LatestC2Equals0, LatestBaseEqualsX});
    attempt({Push0, None0C1, FlushCache2, Get0C2, Get0C1, None0B, FlushCache1, Get0B, LatestC1Equals0, LatestC2Equals0, LatestBaseEquals0});
    attempt({Push0, Push1, Get0C2, None0C1, FlushCache2, Get0C2, Get0C1, Get1C2, Get1C1, None0B, FlushCache1, Get0B, Get1B});
    attempt({Push0, FlushCache2, Pop, PopIsError, None0C2, Get0C1});
    attempt({Push0, Push1, FlushCache2, Push2, Pop, Get0C2, Get1C2, LatestC2Equals1, Pop, Get0C2, LatestC2Equals0, Pop, FlushCache2, None0C1, None0C2, LatestC1EqualsX, PopIsError});
    attempt({Push0, FlushCache2, Push1, FlushCache2, FlushCache1, LatestBaseEquals1, Push2, FlushCache2, LatestC1Equals2, FlushCache1, LatestBaseEquals2});
    attempt({Push0, FlushCache2, Push1, FlushCache2, FlushCache1, LatestBaseEquals1, FlushCache1, LatestBaseEquals1});
    attempt({Push0, Push1, FlushCache2, Get0C1, Pop, Pop, PopIsError, FlushCache2, PopIsError, None0C1, None0C2, LatestC1EqualsX, LatestC2EqualsX, LatestBaseEqualsX});
    attempt({Push0, Push1, FlushCache2, LatestC1Equals1, Pop, FlushCache2, LatestC1Equals0});
    attempt({Push0, FlushCache2, Push1, None2C2, None2C1});
    attempt({Push0, FlushCache2, Push1, Get1C2});
    attempt({Push0, Push1, Get0C2, Get1C2, FlushCache2, Get0C2, Get0C2, Get1C2, Get1C2});
    attempt({Push0, Push1, LatestC2Equals1, FlushCache2, Push2, LatestC2Equals2});
    attempt({Push0, FlushCache2, LatestC2Equals0, LatestC1Equals0});
    attempt({Push0, FlushCache2, FlushCache1, Get0B, Pop, FlushCache2, FlushCache1, None0B, LatestBaseEqualsX});
    attempt({Push0, FlushCache2, FlushCache1, LatestC1Equals0, LatestC2Equals0, LatestBaseEquals0});
    attempt({Push0, Push1, FlushCache2, LatestBaseEqualsX, FlushCache1, LatestBaseEquals1});
    attempt({Push0, Push1, FlushCache2, FlushCache1, Pop, LatestC1Equals1, FlushCache2, LatestC1Equals0, LatestBaseEquals1, FlushCache1, LatestBaseEquals0});
}

TEST(CoinsTests, SubtreesTest)
{
    {
    SCOPED_TRACE("Sapling");
    testSubtreesForShieldedType(SAPLING);
    }

    {
    SCOPED_TRACE("Orchard");
    testSubtreesForShieldedType(ORCHARD);
    }
}
