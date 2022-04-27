// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "coins.h"
#include "gtest/utils.h"
#include "script/standard.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"
#include "consensus/validation.h"
#include "main.h"
#include "undo.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "transaction_builder.h"
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

public:
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
        if (coins.IsPruned() && insecure_rand() % 2 == 0) {
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
                    CHistoryCacheMap &historyCacheMap)
    {
        for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end(); ) {
            if (it->second.flags & CCoinsCacheEntry::DIRTY) {
                // Same optimization used in CCoinsViewDB is to only write dirty entries.
                map_[it->first] = it->second.coins;
                if (it->second.coins.IsPruned() && insecure_rand() % 3 == 0) {
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
                     memusage::DynamicUsage(historyCacheMap);
        for (CCoinsMap::iterator it = cacheCoins.begin(); it != cacheCoins.end(); it++) {
            ret += it->second.coins.DynamicMemoryUsage();
        }
        EXPECT_EQ(DynamicMemoryUsage(), ret);
    }

};

class TxWithNullifiers
{
public:
    CTransaction tx;
    uint256 sproutNullifier;
    uint256 saplingNullifier;
    uint256 orchardNullifier;

    TxWithNullifiers()
    {
        CMutableTransaction mutableTx;

        sproutNullifier = GetRandHash();
        JSDescription jsd;
        jsd.nullifiers[0] = sproutNullifier;
        mutableTx.vJoinSplit.emplace_back(jsd);
        
        saplingNullifier = GetRandHash();
        SpendDescription sd;
        sd.nullifier = saplingNullifier;
        mutableTx.vShieldedSpend.push_back(sd);

        // The Orchard bundle builder always pads to two Actions, so we can just
        // use an empty builder to create a dummy Orchard bundle.
        uint256 orchardAnchor;
        uint256 dataToBeSigned;
        auto builder = orchard::Builder(true, true, orchardAnchor);
        mutableTx.orchardBundle = builder.Build().value().ProveAndSign({}, dataToBeSigned).value();
        orchardNullifier = mutableTx.orchardBundle.GetNullifiers()[0];

        tx = CTransaction(mutableTx);
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

template<typename Tree> void AppendRandomLeaf(Tree &tree);
template<> void AppendRandomLeaf(SproutMerkleTree &tree) { tree.append(GetRandHash()); }
template<> void AppendRandomLeaf(SaplingMerkleTree &tree) { tree.append(GetRandHash()); }
template<> void AppendRandomLeaf(OrchardMerkleFrontier &tree) {
    // OrchardMerkleFrontier only has APIs to append entire bundles, but
    // fortunately the tests only require that the tree root change.
    // TODO: Remove the need to create proofs by having a testing-only way to
    // append a random leaf to OrchardMerkleFrontier.
    uint256 orchardAnchor;
    uint256 dataToBeSigned;
    auto builder = orchard::Builder(true, true, orchardAnchor);
    auto bundle = builder.Build().value().ProveAndSign({}, dataToBeSigned).value();
    tree.AppendBundle(bundle);
}

template<typename Tree> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, Tree &tree);
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, SproutMerkleTree &tree) { return cache.GetSproutAnchorAt(rt, tree); }
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, SaplingMerkleTree &tree) { return cache.GetSaplingAnchorAt(rt, tree); }
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, OrchardMerkleFrontier &tree) { return cache.GetOrchardAnchorAt(rt, tree); }



void checkNullifierCache(const CCoinsViewCacheTest &cache, const TxWithNullifiers &txWithNullifiers, bool shouldBeInCache) {
    // Make sure the nullifiers have not gotten mixed up
    EXPECT_TRUE(!cache.GetNullifier(txWithNullifiers.sproutNullifier, SAPLING));
    EXPECT_TRUE(!cache.GetNullifier(txWithNullifiers.sproutNullifier, ORCHARD));
    EXPECT_TRUE(!cache.GetNullifier(txWithNullifiers.saplingNullifier, SPROUT));
    EXPECT_TRUE(!cache.GetNullifier(txWithNullifiers.saplingNullifier, ORCHARD));
    EXPECT_TRUE(!cache.GetNullifier(txWithNullifiers.orchardNullifier, SPROUT));
    EXPECT_TRUE(!cache.GetNullifier(txWithNullifiers.orchardNullifier, SAPLING));
    // Check if the nullifiers either are or are not in the cache
    bool containsSproutNullifier = cache.GetNullifier(txWithNullifiers.sproutNullifier, SPROUT);
    bool containsSaplingNullifier = cache.GetNullifier(txWithNullifiers.saplingNullifier, SAPLING);
    bool containsOrchardNullifier = cache.GetNullifier(txWithNullifiers.orchardNullifier, ORCHARD);
    EXPECT_TRUE(containsSproutNullifier == shouldBeInCache);
    EXPECT_TRUE(containsSaplingNullifier == shouldBeInCache);
    EXPECT_TRUE(containsOrchardNullifier == shouldBeInCache);
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
        checkNullifierCache(cache1, txWithNullifiers, true);
        cache1.Flush(); // Flush to base.

        // Remove the nullifier from cache
        cache1.SetNullifiers(txWithNullifiers.tx, false);

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
        checkNullifierCache(cache1, txWithNullifiers, true);
        cache1.Flush(); // Flush to base.

        // Remove the nullifier from cache
        cache1.SetNullifiers(txWithNullifiers.tx, false);
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
        checkNullifierCache(cache1, txWithNullifiers, true);
        cache1.Flush(); // Empties cache.

        // Create cache on top.
        {
            // Remove the nullifier.
            CCoinsViewCacheTest cache2(&cache1);
            checkNullifierCache(cache2, txWithNullifiers, true);
            cache1.SetNullifiers(txWithNullifiers.tx, false);
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
        cache1.Flush(); // Empties cache.

        // Create cache on top.
        {
            // Remove the nullifier.
            CCoinsViewCacheTest cache2(&cache1);
            cache2.SetNullifiers(txWithNullifiers.tx, false);
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
    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    TxWithNullifiers txWithNullifiers;
    checkNullifierCache(cache, txWithNullifiers, false);
    cache.SetNullifiers(txWithNullifiers.tx, true);
    checkNullifierCache(cache, txWithNullifiers, true);
    cache.Flush();

    CCoinsViewCacheTest cache2(&base);

    checkNullifierCache(cache2, txWithNullifiers, true);
    cache2.SetNullifiers(txWithNullifiers.tx, false);
    checkNullifierCache(cache2, txWithNullifiers, false);
    cache2.Flush();

    CCoinsViewCacheTest cache3(&base);

    checkNullifierCache(cache3, txWithNullifiers, false);
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
