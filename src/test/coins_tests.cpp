// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "coins.h"
#include "test_random.h"
#include "script/standard.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"
#include "consensus/validation.h"
#include "main.h"
#include "undo.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "zcash/Note.hpp"

#include <vector>
#include <map>

#include <boost/test/unit_test.hpp>
#include "zcash/IncrementalMerkleTree.hpp"

namespace
{
class CCoinsViewTest : public CCoinsView
{
    uint256 hashBestBlock_;
    uint256 hashBestSproutAnchor_;
    uint256 hashBestSaplingAnchor_;
    std::map<uint256, CCoins> map_;
    std::map<uint256, SproutMerkleTree> mapSproutAnchors_;
    std::map<uint256, SaplingMerkleTree> mapSaplingAnchors_;
    std::map<uint256, bool> mapSproutNullifiers_;
    std::map<uint256, bool> mapSaplingNullifiers_;

public:
    CCoinsViewTest() {
        hashBestSproutAnchor_ = SproutMerkleTree::empty_root();
        hashBestSaplingAnchor_ = SaplingMerkleTree::empty_root();
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
                    CAnchorsSproutMap& mapSproutAnchors,
                    CAnchorsSaplingMap& mapSaplingAnchors,
                    CNullifiersMap& mapSproutNullifiers,
                    CNullifiersMap& mapSaplingNullifiers,
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

        BatchWriteNullifiers(mapSproutNullifiers, mapSproutNullifiers_);
        BatchWriteNullifiers(mapSaplingNullifiers, mapSaplingNullifiers_);

        if (!hashBlock.IsNull())
            hashBestBlock_ = hashBlock;
        if (!hashSproutAnchor.IsNull())
            hashBestSproutAnchor_ = hashSproutAnchor;
        if (!hashSaplingAnchor.IsNull())
            hashBestSaplingAnchor_ = hashSaplingAnchor;
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
                     memusage::DynamicUsage(cacheSproutNullifiers) +
                     memusage::DynamicUsage(cacheSaplingNullifiers) +
                     memusage::DynamicUsage(historyCacheMap);
        for (CCoinsMap::iterator it = cacheCoins.begin(); it != cacheCoins.end(); it++) {
            ret += it->second.coins.DynamicMemoryUsage();
        }
        BOOST_CHECK_EQUAL(DynamicMemoryUsage(), ret);
    }

    CCoinsMap& map() { return cacheCoins; }
    size_t& usage() { return cachedCoinsUsage; }
};

class TxWithNullifiers
{
public:
    CTransaction tx;
    uint256 sproutNullifier;
    uint256 saplingNullifier;

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

template<typename Tree> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, Tree &tree);
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, SproutMerkleTree &tree) { return cache.GetSproutAnchorAt(rt, tree); }
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, SaplingMerkleTree &tree) { return cache.GetSaplingAnchorAt(rt, tree); }

BOOST_FIXTURE_TEST_SUITE(coins_tests, BasicTestingSetup)

void checkNullifierCache(const CCoinsViewCacheTest &cache, const TxWithNullifiers &txWithNullifiers, bool shouldBeInCache) {
    // Make sure the nullifiers have not gotten mixed up
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.sproutNullifier, SAPLING));
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.saplingNullifier, SPROUT));
    // Check if the nullifiers either are or are not in the cache
    bool containsSproutNullifier = cache.GetNullifier(txWithNullifiers.sproutNullifier, SPROUT);
    bool containsSaplingNullifier = cache.GetNullifier(txWithNullifiers.saplingNullifier, SAPLING);
    BOOST_CHECK(containsSproutNullifier == shouldBeInCache);
    BOOST_CHECK(containsSaplingNullifier == shouldBeInCache);
}

BOOST_AUTO_TEST_CASE(nullifier_regression_test)
{
    // Correct behavior:
    {
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
        tree.append(GetRandHash());

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
            BOOST_CHECK(GetAnchorAt(cache1, tree.root(), checkTree));
            BOOST_CHECK(checkTree.root() == tree.root());
        }
    }

    // Previously incorrect behavior
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Create dummy anchor/commitment
        Tree tree;
        tree.append(GetRandHash());

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
            BOOST_CHECK(GetAnchorAt(cache1, tree.root(), checktree));
            BOOST_CHECK(checktree.root() == tree.root()); // Oh, shucks.
        }

        // Flushing cache won't help either, just makes the inconsistency
        // permanent.
        cache1.Flush();
        {
            Tree checktree;
            BOOST_CHECK(GetAnchorAt(cache1, tree.root(), checktree));
            BOOST_CHECK(checktree.root() == tree.root()); // Oh, shucks.
        }
    }
}

BOOST_AUTO_TEST_CASE(anchor_pop_regression_test)
{
    BOOST_TEST_CONTEXT("Sprout") {
        anchorPopRegressionTestImpl<SproutMerkleTree>(SPROUT);
    }
    BOOST_TEST_CONTEXT("Sapling") {
        anchorPopRegressionTestImpl<SaplingMerkleTree>(SAPLING);
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
        tree.append(GetRandHash());

        cache1.PushAnchor(tree);
        cache1.Flush();

        cache1.PopAnchor(Tree::empty_root(), type);
        BOOST_CHECK(cache1.GetBestAnchor(type) == Tree::empty_root());
        BOOST_CHECK(!GetAnchorAt(cache1, tree.root(), tree));
    }

    // Also correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        tree.append(GetRandHash());
        cache1.PushAnchor(tree);
        cache1.Flush();

        cache1.PopAnchor(Tree::empty_root(), type);
        cache1.Flush();
        BOOST_CHECK(cache1.GetBestAnchor(type) == Tree::empty_root());
        BOOST_CHECK(!GetAnchorAt(cache1, tree.root(), tree));
    }

    // Works because we bring the anchor in from parent cache.
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        tree.append(GetRandHash());
        cache1.PushAnchor(tree);
        cache1.Flush();

        {
            // Pop anchor.
            CCoinsViewCacheTest cache2(&cache1);
            BOOST_CHECK(GetAnchorAt(cache2, tree.root(), tree));
            cache2.PopAnchor(Tree::empty_root(), type);
            cache2.Flush();
        }

        BOOST_CHECK(cache1.GetBestAnchor(type) == Tree::empty_root());
        BOOST_CHECK(!GetAnchorAt(cache1, tree.root(), tree));
    }

    // Was broken:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        tree.append(GetRandHash());
        cache1.PushAnchor(tree);
        cache1.Flush();

        {
            // Pop anchor.
            CCoinsViewCacheTest cache2(&cache1);
            cache2.PopAnchor(Tree::empty_root(), type);
            cache2.Flush();
        }

        BOOST_CHECK(cache1.GetBestAnchor(type) == Tree::empty_root());
        BOOST_CHECK(!GetAnchorAt(cache1, tree.root(), tree));
    }
}

BOOST_AUTO_TEST_CASE(anchor_regression_test)
{
    BOOST_TEST_CONTEXT("Sprout") {
        anchorRegressionTestImpl<SproutMerkleTree>(SPROUT);
    }
    BOOST_TEST_CONTEXT("Sapling") {
        anchorRegressionTestImpl<SaplingMerkleTree>(SAPLING);
    }
}

BOOST_AUTO_TEST_CASE(nullifiers_test)
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
        BOOST_CHECK(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));
        tree.append(GetRandHash());

        newrt = tree.root();

        cache.PushAnchor(tree);
        cache.Flush();
    }

    {
        CCoinsViewCacheTest cache(&base);
        Tree tree;
        BOOST_CHECK(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));

        // Get the cached entry.
        BOOST_CHECK(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));

        uint256 check_rt = tree.root();

        BOOST_CHECK(check_rt == newrt);
    }
}

BOOST_AUTO_TEST_CASE(anchors_flush_test)
{
    BOOST_TEST_CONTEXT("Sprout") {
        anchorsFlushImpl<SproutMerkleTree>(SPROUT);
    }
    BOOST_TEST_CONTEXT("Sapling") {
        anchorsFlushImpl<SaplingMerkleTree>(SAPLING);
    }
}

BOOST_AUTO_TEST_CASE(chained_joinsplits)
{
    // TODO update this or add a similar test when the SaplingNote class exist
    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    SproutMerkleTree tree;

    JSDescription js1;
    js1.anchor = tree.root();
    js1.commitments[0] = appendRandomSproutCommitment(tree);
    js1.commitments[1] = appendRandomSproutCommitment(tree);

    // Although it's not possible given our assumptions, if
    // two joinsplits create the same treestate twice, we should
    // still be able to anchor to it.
    JSDescription js1b;
    js1b.anchor = tree.root();
    js1b.commitments[0] = js1.commitments[0];
    js1b.commitments[1] = js1.commitments[1];

    JSDescription js2;
    JSDescription js3;

    js2.anchor = tree.root();
    js3.anchor = tree.root();

    js2.commitments[0] = appendRandomSproutCommitment(tree);
    js2.commitments[1] = appendRandomSproutCommitment(tree);

    js3.commitments[0] = appendRandomSproutCommitment(tree);
    js3.commitments[1] = appendRandomSproutCommitment(tree);

    {
        CMutableTransaction mtx;
        mtx.vJoinSplit.push_back(js2);

        BOOST_CHECK(cache.HaveShieldedRequirements(mtx) == std::optional<UnsatisfiedShieldedReq>(UnsatisfiedShieldedReq::SproutUnknownAnchor));
    }

    {
        // js2 is trying to anchor to js1 but js1
        // appears afterwards -- not a permitted ordering
        CMutableTransaction mtx;
        mtx.vJoinSplit.push_back(js2);
        mtx.vJoinSplit.push_back(js1);

        BOOST_CHECK(cache.HaveShieldedRequirements(mtx) == std::optional<UnsatisfiedShieldedReq>(UnsatisfiedShieldedReq::SproutUnknownAnchor));
    }

    {
        CMutableTransaction mtx;
        mtx.vJoinSplit.push_back(js1);
        mtx.vJoinSplit.push_back(js2);

        BOOST_CHECK(cache.HaveShieldedRequirements(mtx) == std::nullopt);
    }

    {
        CMutableTransaction mtx;
        mtx.vJoinSplit.push_back(js1);
        mtx.vJoinSplit.push_back(js2);
        mtx.vJoinSplit.push_back(js3);

        BOOST_CHECK(cache.HaveShieldedRequirements(mtx) == std::nullopt);
    }

    {
        CMutableTransaction mtx;
        mtx.vJoinSplit.push_back(js1);
        mtx.vJoinSplit.push_back(js1b);
        mtx.vJoinSplit.push_back(js2);
        mtx.vJoinSplit.push_back(js3);

        BOOST_CHECK(cache.HaveShieldedRequirements(mtx) == std::nullopt);
    }
}

template<typename Tree> void anchorsTestImpl(ShieldedType type)
{
    // TODO: These tests should be more methodical.
    //       Or, integrate with Bitcoin's tests later.

    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    BOOST_CHECK(cache.GetBestAnchor(type) == Tree::empty_root());

    {
        Tree tree;

        BOOST_CHECK(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));
        BOOST_CHECK(cache.GetBestAnchor(type) == tree.root());
        tree.append(GetRandHash());
        tree.append(GetRandHash());
        tree.append(GetRandHash());
        tree.append(GetRandHash());
        tree.append(GetRandHash());
        tree.append(GetRandHash());
        tree.append(GetRandHash());

        Tree save_tree_for_later;
        save_tree_for_later = tree;

        uint256 newrt = tree.root();
        uint256 newrt2;

        cache.PushAnchor(tree);
        BOOST_CHECK(cache.GetBestAnchor(type) == newrt);

        {
            Tree confirm_same;
            BOOST_CHECK(GetAnchorAt(cache, cache.GetBestAnchor(type), confirm_same));

            BOOST_CHECK(confirm_same.root() == newrt);
        }

        tree.append(GetRandHash());
        tree.append(GetRandHash());

        newrt2 = tree.root();

        cache.PushAnchor(tree);
        BOOST_CHECK(cache.GetBestAnchor(type) == newrt2);

        Tree test_tree;
        BOOST_CHECK(GetAnchorAt(cache, cache.GetBestAnchor(type), test_tree));

        BOOST_CHECK(tree.root() == test_tree.root());

        {
            Tree test_tree2;
            GetAnchorAt(cache, newrt, test_tree2);

            BOOST_CHECK(test_tree2.root() == newrt);
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

BOOST_AUTO_TEST_CASE(anchors_test)
{
    BOOST_TEST_CONTEXT("Sprout") {
        anchorsTestImpl<SproutMerkleTree>(SPROUT);
    }
    BOOST_TEST_CONTEXT("Sapling") {
        anchorsTestImpl<SaplingMerkleTree>(SAPLING);
    }
}

static const unsigned int NUM_SIMULATION_ITERATIONS = 40000;

// This is a large randomized insert/remove simulation test on a variable-size
// stack of caches on top of CCoinsViewTest.
//
// It will randomly create/update/delete CCoins entries to a tip of caches, with
// txids picked from a limited list of random 256-bit hashes. Occasionally, a
// new tip is added to the stack of caches, or the tip is flushed and removed.
//
// During the process, booleans are kept to make sure that the randomized
// operation hits all branches.
BOOST_AUTO_TEST_CASE(coins_cache_simulation_test)
{
    // Various coverage trackers.
    bool removed_all_caches = false;
    bool reached_4_caches = false;
    bool added_an_entry = false;
    bool removed_an_entry = false;
    bool updated_an_entry = false;
    bool found_an_entry = false;
    bool missed_an_entry = false;

    // A simple map to track what we expect the cache stack to represent.
    std::map<uint256, CCoins> result;

    // The cache stack.
    CCoinsViewTest base; // A CCoinsViewTest at the bottom.
    std::vector<CCoinsViewCacheTest*> stack; // A stack of CCoinsViewCaches on top.
    stack.push_back(new CCoinsViewCacheTest(&base)); // Start with one cache.

    // Use a limited set of random transaction ids, so we do test overwriting entries.
    std::vector<uint256> txids;
    txids.resize(NUM_SIMULATION_ITERATIONS / 8);
    for (unsigned int i = 0; i < txids.size(); i++) {
        txids[i] = GetRandHash();
    }

    for (unsigned int i = 0; i < NUM_SIMULATION_ITERATIONS; i++) {
        // Do a random modification.
        {
            uint256 txid = txids[insecure_rand() % txids.size()]; // txid we're going to modify in this iteration.
            CCoins& coins = result[txid];
            CCoinsModifier entry = stack.back()->ModifyCoins(txid);
            BOOST_CHECK(coins == *entry);
            if (insecure_rand() % 5 == 0 || coins.IsPruned()) {
                if (coins.IsPruned()) {
                    added_an_entry = true;
                } else {
                    updated_an_entry = true;
                }
                coins.nVersion = insecure_rand();
                coins.vout.resize(1);
                coins.vout[0].nValue = insecure_rand();
                *entry = coins;
            } else {
                coins.Clear();
                entry->Clear();
                removed_an_entry = true;
            }
        }

        // Once every 1000 iterations and at the end, verify the full cache.
        if (insecure_rand() % 1000 == 1 || i == NUM_SIMULATION_ITERATIONS - 1) {
            for (std::map<uint256, CCoins>::iterator it = result.begin(); it != result.end(); it++) {
                const CCoins* coins = stack.back()->AccessCoins(it->first);
                if (coins) {
                    BOOST_CHECK(*coins == it->second);
                    found_an_entry = true;
                } else {
                    BOOST_CHECK(it->second.IsPruned());
                    missed_an_entry = true;
                }
            }
            for (const CCoinsViewCacheTest *test : stack) {
                test->SelfTest();
            }
        }

        if (insecure_rand() % 100 == 0) {
            // Every 100 iterations, flush an intermediate cache
            if (stack.size() > 1 && insecure_rand() % 2 == 0) {
                unsigned int flushIndex = insecure_rand() % (stack.size() - 1);
                stack[flushIndex]->Flush();
            }
        }
        if (insecure_rand() % 100 == 0) {
            // Every 100 iterations, change the cache stack.
            if (stack.size() > 0 && insecure_rand() % 2 == 0) {
                //Remove the top cache
                stack.back()->Flush();
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && insecure_rand() % 2)) {
                //Add a new cache
                CCoinsView* tip = &base;
                if (stack.size() > 0) {
                    tip = stack.back();
                } else {
                    removed_all_caches = true;
                }
                stack.push_back(new CCoinsViewCacheTest(tip));
                if (stack.size() == 4) {
                    reached_4_caches = true;
                }
            }
        }
    }

    // Clean up the stack.
    while (stack.size() > 0) {
        delete stack.back();
        stack.pop_back();
    }

    // Verify coverage.
    BOOST_CHECK(removed_all_caches);
    BOOST_CHECK(reached_4_caches);
    BOOST_CHECK(added_an_entry);
    BOOST_CHECK(removed_an_entry);
    BOOST_CHECK(updated_an_entry);
    BOOST_CHECK(found_an_entry);
    BOOST_CHECK(missed_an_entry);
}

BOOST_AUTO_TEST_CASE(coins_coinbase_spends)
{
    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    // Create coinbase transaction
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].scriptSig = CScript() << OP_1;
    mtx.vin[0].nSequence = 0;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 500;
    mtx.vout[0].scriptPubKey = CScript() << OP_1;

    CTransaction tx(mtx);

    BOOST_CHECK(tx.IsCoinBase());

    CValidationState state;
    UpdateCoins(tx, cache, 100);

    // Create coinbase spend
    CMutableTransaction mtx2;
    mtx2.vin.resize(1);
    mtx2.vin[0].prevout = COutPoint(tx.GetHash(), 0);
    mtx2.vin[0].scriptSig = CScript() << OP_1;
    mtx2.vin[0].nSequence = 0;

    {
        CTransaction tx2(mtx2);
        BOOST_CHECK(Consensus::CheckTxInputs(tx2, state, cache, 100+COINBASE_MATURITY, Params().GetConsensus()));
    }

    mtx2.vout.resize(1);
    mtx2.vout[0].nValue = 500;
    mtx2.vout[0].scriptPubKey = CScript() << OP_1;

    {
        CTransaction tx2(mtx2);
        BOOST_CHECK(!Consensus::CheckTxInputs(tx2, state, cache, 100+COINBASE_MATURITY, Params().GetConsensus()));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-coinbase-spend-has-transparent-outputs");
    }
}

// This test is similar to the previous test
// except the emphasis is on testing the functionality of UpdateCoins
// random txs are created and UpdateCoins is used to update the cache stack
BOOST_AUTO_TEST_CASE(updatecoins_simulation_test)
{
    // A simple map to track what we expect the cache stack to represent.
    std::map<uint256, CCoins> result;

    // The cache stack.
    CCoinsViewTest base; // A CCoinsViewTest at the bottom.
    std::vector<CCoinsViewCacheTest*> stack; // A stack of CCoinsViewCaches on top.
    stack.push_back(new CCoinsViewCacheTest(&base)); // Start with one cache.

    // Track the txids we've used and whether they have been spent or not
    std::map<uint256, CAmount> coinbaseids;
    std::set<uint256> alltxids;

    for (unsigned int i = 0; i < NUM_SIMULATION_ITERATIONS; i++) {
        {
            CMutableTransaction tx;
            tx.vin.resize(1);
            tx.vout.resize(1);
            tx.vout[0].nValue = i; //Keep txs unique
            unsigned int height = insecure_rand();

            // 1/10 times create a coinbase
            if (insecure_rand() % 10 == 0 || coinbaseids.size() < 10) {
                coinbaseids[tx.GetHash()] = tx.vout[0].nValue;
                assert(CTransaction(tx).IsCoinBase());
            }
            // 9/10 times create a regular tx
            else {
                uint256 prevouthash;
                // equally likely to spend coinbase or non coinbase
                std::set<uint256>::iterator txIt = alltxids.lower_bound(GetRandHash());
                if (txIt == alltxids.end()) {
                    txIt = alltxids.begin();
                }
                prevouthash = *txIt;

                // Construct the tx to spend the coins of prevouthash
                tx.vin[0].prevout.hash = prevouthash;
                tx.vin[0].prevout.n = 0;

                // Update the expected result of prevouthash to know these coins are spent
                CCoins& oldcoins = result[prevouthash];
                oldcoins.Clear();

                alltxids.erase(prevouthash);
                coinbaseids.erase(prevouthash);

                assert(!CTransaction(tx).IsCoinBase());
            }
            // Track this tx to possibly spend later
            alltxids.insert(tx.GetHash());

            // Update the expected result to know about the new output coins
            CCoins &coins = result[tx.GetHash()];
            coins.FromTx(tx, height);

            UpdateCoins(tx, *(stack.back()), height);
        }

        // Once every 1000 iterations and at the end, verify the full cache.
        if (insecure_rand() % 1000 == 1 || i == NUM_SIMULATION_ITERATIONS - 1) {
            for (std::map<uint256, CCoins>::iterator it = result.begin(); it != result.end(); it++) {
                const CCoins* coins = stack.back()->AccessCoins(it->first);
                if (coins) {
                    BOOST_CHECK(*coins == it->second);
                 } else {
                    BOOST_CHECK(it->second.IsPruned());
                 }
            }
        }

        if (insecure_rand() % 100 == 0) {
            // Every 100 iterations, flush an intermediate cache
            if (stack.size() > 1 && insecure_rand() % 2 == 0) {
                unsigned int flushIndex = insecure_rand() % (stack.size() - 1);
                stack[flushIndex]->Flush();
            }
        }
        if (insecure_rand() % 100 == 0) {
            // Every 100 iterations, change the cache stack.
            if (stack.size() > 0 && insecure_rand() % 2 == 0) {
                stack.back()->Flush();
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && insecure_rand() % 2)) {
                CCoinsView* tip = &base;
                if (stack.size() > 0) {
                    tip = stack.back();
                }
                stack.push_back(new CCoinsViewCacheTest(tip));
           }
        }
    }

    // Clean up the stack.
    while (stack.size() > 0) {
        delete stack.back();
        stack.pop_back();
    }
}

BOOST_AUTO_TEST_CASE(ccoins_serialization)
{
    // Good example
    CDataStream ss1(ParseHex("0104835800816115944e077fe7c803cfa57f29b36bf87c1d358bb85e"), SER_DISK, CLIENT_VERSION);
    CCoins cc1;
    ss1 >> cc1;
    BOOST_CHECK_EQUAL(cc1.nVersion, 1);
    BOOST_CHECK_EQUAL(cc1.fCoinBase, false);
    BOOST_CHECK_EQUAL(cc1.nHeight, 203998);
    BOOST_CHECK_EQUAL(cc1.vout.size(), 2);
    BOOST_CHECK_EQUAL(cc1.IsAvailable(0), false);
    BOOST_CHECK_EQUAL(cc1.IsAvailable(1), true);
    BOOST_CHECK_EQUAL(cc1.vout[1].nValue, 60000000000ULL);
    BOOST_CHECK_EQUAL(HexStr(cc1.vout[1].scriptPubKey), HexStr(GetScriptForDestination(CKeyID(uint160(ParseHex("816115944e077fe7c803cfa57f29b36bf87c1d35"))))));

    // Good example
    CDataStream ss2(ParseHex("0109044086ef97d5790061b01caab50f1b8e9c50a5057eb43c2d9563a4eebbd123008c988f1a4a4de2161e0f50aac7f17e7f9555caa486af3b"), SER_DISK, CLIENT_VERSION);
    CCoins cc2;
    ss2 >> cc2;
    BOOST_CHECK_EQUAL(cc2.nVersion, 1);
    BOOST_CHECK_EQUAL(cc2.fCoinBase, true);
    BOOST_CHECK_EQUAL(cc2.nHeight, 120891);
    BOOST_CHECK_EQUAL(cc2.vout.size(), 17);
    for (int i = 0; i < 17; i++) {
        BOOST_CHECK_EQUAL(cc2.IsAvailable(i), i == 4 || i == 16);
    }
    BOOST_CHECK_EQUAL(cc2.vout[4].nValue, 234925952);
    BOOST_CHECK_EQUAL(HexStr(cc2.vout[4].scriptPubKey), HexStr(GetScriptForDestination(CKeyID(uint160(ParseHex("61b01caab50f1b8e9c50a5057eb43c2d9563a4ee"))))));
    BOOST_CHECK_EQUAL(cc2.vout[16].nValue, 110397);
    BOOST_CHECK_EQUAL(HexStr(cc2.vout[16].scriptPubKey), HexStr(GetScriptForDestination(CKeyID(uint160(ParseHex("8c988f1a4a4de2161e0f50aac7f17e7f9555caa4"))))));

    // Smallest possible example
    CDataStream ssx(SER_DISK, CLIENT_VERSION);
    BOOST_CHECK_EQUAL(HexStr(ssx.begin(), ssx.end()), "");

    CDataStream ss3(ParseHex("0002000600"), SER_DISK, CLIENT_VERSION);
    CCoins cc3;
    ss3 >> cc3;
    BOOST_CHECK_EQUAL(cc3.nVersion, 0);
    BOOST_CHECK_EQUAL(cc3.fCoinBase, false);
    BOOST_CHECK_EQUAL(cc3.nHeight, 0);
    BOOST_CHECK_EQUAL(cc3.vout.size(), 1);
    BOOST_CHECK_EQUAL(cc3.IsAvailable(0), true);
    BOOST_CHECK_EQUAL(cc3.vout[0].nValue, 0);
    BOOST_CHECK_EQUAL(cc3.vout[0].scriptPubKey.size(), 0);

    // scriptPubKey that ends beyond the end of the stream
    CDataStream ss4(ParseHex("0002000800"), SER_DISK, CLIENT_VERSION);
    try {
        CCoins cc4;
        ss4 >> cc4;
        BOOST_CHECK_MESSAGE(false, "We should have thrown");
    } catch (const std::ios_base::failure& e) {
    }

    // Very large scriptPubKey (3*10^9 bytes) past the end of the stream
    CDataStream tmp(SER_DISK, CLIENT_VERSION);
    uint64_t x = 3000000000ULL;
    tmp << VARINT(x);
    BOOST_CHECK_EQUAL(HexStr(tmp.begin(), tmp.end()), "8a95c0bb00");
    CDataStream ss5(ParseHex("0002008a95c0bb0000"), SER_DISK, CLIENT_VERSION);
    try {
        CCoins cc5;
        ss5 >> cc5;
        BOOST_CHECK_MESSAGE(false, "We should have thrown");
    } catch (const std::ios_base::failure& e) {
    }
}

const static uint256 TXID;
const static CAmount PRUNED = -1;
const static CAmount ABSENT = -2;
const static CAmount VALUE1 = 100;
const static CAmount VALUE2 = 200;
const static CAmount VALUE3 = 300;
const static char DIRTY = CCoinsCacheEntry::DIRTY;
const static char FRESH = CCoinsCacheEntry::FRESH;
const static char NO_ENTRY = -1;

const static auto FLAGS = {char(0), FRESH, DIRTY, char(DIRTY | FRESH)};
const static auto CLEAN_FLAGS = {char(0), FRESH};
const static auto DIRTY_FLAGS = {DIRTY, char(DIRTY | FRESH)};
const static auto ABSENT_FLAGS = {NO_ENTRY};

void SetCoinsValue(CAmount value, CCoins& coins)
{
    assert(value != ABSENT);
    coins.Clear();
    assert(coins.IsPruned());
    if (value != PRUNED) {
        coins.vout.emplace_back();
        coins.vout.back().nValue = value;
        assert(!coins.IsPruned());
    }
}

size_t InsertCoinsMapEntry(CCoinsMap& map, CAmount value, char flags)
{
    if (value == ABSENT) {
        assert(flags == NO_ENTRY);
        return 0;
    }
    assert(flags != NO_ENTRY);
    CCoinsCacheEntry entry;
    entry.flags = flags;
    SetCoinsValue(value, entry.coins);
    auto inserted = map.emplace(TXID, std::move(entry));
    assert(inserted.second);
    return inserted.first->second.coins.DynamicMemoryUsage();
}

void GetCoinsMapEntry(const CCoinsMap& map, CAmount& value, char& flags)
{
    auto it = map.find(TXID);
    if (it == map.end()) {
        value = ABSENT;
        flags = NO_ENTRY;
    } else {
        if (it->second.coins.IsPruned()) {
            assert(it->second.coins.vout.size() == 0);
            value = PRUNED;
        } else {
            assert(it->second.coins.vout.size() == 1);
            value = it->second.coins.vout[0].nValue;
        }
        flags = it->second.flags;
        assert(flags != NO_ENTRY);
    }
}

void WriteCoinsViewEntry(CCoinsView& view, CAmount value, char flags)
{

    CCoinsMap map;
    CAnchorsSproutMap mapSproutAnchors;
    CAnchorsSaplingMap mapSaplingAnchors;
    CNullifiersMap mapSproutNullifiers;
    CNullifiersMap mapSaplingNullifiers;
    CHistoryCacheMap historyCacheMap;

    InsertCoinsMapEntry(map, value, flags);
    view.BatchWrite(map, {}, {}, {}, mapSproutAnchors, mapSaplingAnchors, mapSproutNullifiers, mapSaplingNullifiers, historyCacheMap);
}

class SingleEntryCacheTest
{
public:
    SingleEntryCacheTest(CAmount base_value, CAmount cache_value, char cache_flags)
    {
        WriteCoinsViewEntry(base, base_value, base_value == ABSENT ? NO_ENTRY : DIRTY);
        cache.usage() += InsertCoinsMapEntry(cache.map(), cache_value, cache_flags);
    }

    CCoinsView root;
    CCoinsViewCacheTest base{&root};
    CCoinsViewCacheTest cache{&base};
};

void CheckAccessCoins(CAmount base_value, CAmount cache_value, CAmount expected_value, char cache_flags, char expected_flags)
{
    SingleEntryCacheTest test(base_value, cache_value, cache_flags);
    test.cache.AccessCoins(TXID);
    test.cache.SelfTest();

    CAmount result_value;
    char result_flags;
    GetCoinsMapEntry(test.cache.map(), result_value, result_flags);
    BOOST_CHECK_EQUAL(result_value, expected_value);
    BOOST_CHECK_EQUAL(result_flags, expected_flags);
}

BOOST_AUTO_TEST_CASE(ccoins_access)
{
    /* Check AccessCoin behavior, requesting a coin from a cache view layered on
     * top of a base view, and checking the resulting entry in the cache after
     * the access.
     *
     *               Base    Cache   Result  Cache        Result
     *               Value   Value   Value   Flags        Flags
     */
    CheckAccessCoins(ABSENT, ABSENT, ABSENT, NO_ENTRY   , NO_ENTRY   );
    CheckAccessCoins(ABSENT, PRUNED, PRUNED, 0          , 0          );
    CheckAccessCoins(ABSENT, PRUNED, PRUNED, FRESH      , FRESH      );
    CheckAccessCoins(ABSENT, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckAccessCoins(ABSENT, PRUNED, PRUNED, DIRTY|FRESH, DIRTY|FRESH);
    CheckAccessCoins(ABSENT, VALUE2, VALUE2, 0          , 0          );
    CheckAccessCoins(ABSENT, VALUE2, VALUE2, FRESH      , FRESH      );
    CheckAccessCoins(ABSENT, VALUE2, VALUE2, DIRTY      , DIRTY      );
    CheckAccessCoins(ABSENT, VALUE2, VALUE2, DIRTY|FRESH, DIRTY|FRESH);
    CheckAccessCoins(PRUNED, ABSENT, PRUNED, NO_ENTRY   , FRESH      );
    CheckAccessCoins(PRUNED, PRUNED, PRUNED, 0          , 0          );
    CheckAccessCoins(PRUNED, PRUNED, PRUNED, FRESH      , FRESH      );
    CheckAccessCoins(PRUNED, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckAccessCoins(PRUNED, PRUNED, PRUNED, DIRTY|FRESH, DIRTY|FRESH);
    CheckAccessCoins(PRUNED, VALUE2, VALUE2, 0          , 0          );
    CheckAccessCoins(PRUNED, VALUE2, VALUE2, FRESH      , FRESH      );
    CheckAccessCoins(PRUNED, VALUE2, VALUE2, DIRTY      , DIRTY      );
    CheckAccessCoins(PRUNED, VALUE2, VALUE2, DIRTY|FRESH, DIRTY|FRESH);
    CheckAccessCoins(VALUE1, ABSENT, VALUE1, NO_ENTRY   , 0          );
    CheckAccessCoins(VALUE1, PRUNED, PRUNED, 0          , 0          );
    CheckAccessCoins(VALUE1, PRUNED, PRUNED, FRESH      , FRESH      );
    CheckAccessCoins(VALUE1, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckAccessCoins(VALUE1, PRUNED, PRUNED, DIRTY|FRESH, DIRTY|FRESH);
    CheckAccessCoins(VALUE1, VALUE2, VALUE2, 0          , 0          );
    CheckAccessCoins(VALUE1, VALUE2, VALUE2, FRESH      , FRESH      );
    CheckAccessCoins(VALUE1, VALUE2, VALUE2, DIRTY      , DIRTY      );
    CheckAccessCoins(VALUE1, VALUE2, VALUE2, DIRTY|FRESH, DIRTY|FRESH);
}

void CheckModifyCoins(CAmount base_value, CAmount cache_value, CAmount modify_value, CAmount expected_value, char cache_flags, char expected_flags)
{
    SingleEntryCacheTest test(base_value, cache_value, cache_flags);
    SetCoinsValue(modify_value, *test.cache.ModifyCoins(TXID));
    test.cache.SelfTest();

    CAmount result_value;
    char result_flags;
    GetCoinsMapEntry(test.cache.map(), result_value, result_flags);
    BOOST_CHECK_EQUAL(result_value, expected_value);
    BOOST_CHECK_EQUAL(result_flags, expected_flags);
};

BOOST_AUTO_TEST_CASE(ccoins_modify)
{
    /* Check ModifyCoin behavior, requesting a coin from a cache view layered on
     * top of a base view, writing a modification to the coin, and then checking
     * the resulting entry in the cache after the modification.
     *
     *               Base    Cache   Write   Result  Cache        Result
     *               Value   Value   Value   Value   Flags        Flags
     */
    CheckModifyCoins(ABSENT, ABSENT, PRUNED, ABSENT, NO_ENTRY   , NO_ENTRY   );
    CheckModifyCoins(ABSENT, ABSENT, VALUE3, VALUE3, NO_ENTRY   , DIRTY|FRESH);
    CheckModifyCoins(ABSENT, PRUNED, PRUNED, PRUNED, 0          , DIRTY      );
    CheckModifyCoins(ABSENT, PRUNED, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckModifyCoins(ABSENT, PRUNED, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckModifyCoins(ABSENT, PRUNED, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckModifyCoins(ABSENT, PRUNED, VALUE3, VALUE3, 0          , DIRTY      );
    CheckModifyCoins(ABSENT, PRUNED, VALUE3, VALUE3, FRESH      , DIRTY|FRESH);
    CheckModifyCoins(ABSENT, PRUNED, VALUE3, VALUE3, DIRTY      , DIRTY      );
    CheckModifyCoins(ABSENT, PRUNED, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH);
    CheckModifyCoins(ABSENT, VALUE2, PRUNED, PRUNED, 0          , DIRTY      );
    CheckModifyCoins(ABSENT, VALUE2, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckModifyCoins(ABSENT, VALUE2, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckModifyCoins(ABSENT, VALUE2, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckModifyCoins(ABSENT, VALUE2, VALUE3, VALUE3, 0          , DIRTY      );
    CheckModifyCoins(ABSENT, VALUE2, VALUE3, VALUE3, FRESH      , DIRTY|FRESH);
    CheckModifyCoins(ABSENT, VALUE2, VALUE3, VALUE3, DIRTY      , DIRTY      );
    CheckModifyCoins(ABSENT, VALUE2, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH);
    CheckModifyCoins(PRUNED, ABSENT, PRUNED, ABSENT, NO_ENTRY   , NO_ENTRY   );
    CheckModifyCoins(PRUNED, ABSENT, VALUE3, VALUE3, NO_ENTRY   , DIRTY|FRESH);
    CheckModifyCoins(PRUNED, PRUNED, PRUNED, PRUNED, 0          , DIRTY      );
    CheckModifyCoins(PRUNED, PRUNED, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckModifyCoins(PRUNED, PRUNED, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckModifyCoins(PRUNED, PRUNED, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckModifyCoins(PRUNED, PRUNED, VALUE3, VALUE3, 0          , DIRTY      );
    CheckModifyCoins(PRUNED, PRUNED, VALUE3, VALUE3, FRESH      , DIRTY|FRESH);
    CheckModifyCoins(PRUNED, PRUNED, VALUE3, VALUE3, DIRTY      , DIRTY      );
    CheckModifyCoins(PRUNED, PRUNED, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH);
    CheckModifyCoins(PRUNED, VALUE2, PRUNED, PRUNED, 0          , DIRTY      );
    CheckModifyCoins(PRUNED, VALUE2, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckModifyCoins(PRUNED, VALUE2, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckModifyCoins(PRUNED, VALUE2, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckModifyCoins(PRUNED, VALUE2, VALUE3, VALUE3, 0          , DIRTY      );
    CheckModifyCoins(PRUNED, VALUE2, VALUE3, VALUE3, FRESH      , DIRTY|FRESH);
    CheckModifyCoins(PRUNED, VALUE2, VALUE3, VALUE3, DIRTY      , DIRTY      );
    CheckModifyCoins(PRUNED, VALUE2, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH);
    CheckModifyCoins(VALUE1, ABSENT, PRUNED, PRUNED, NO_ENTRY   , DIRTY      );
    CheckModifyCoins(VALUE1, ABSENT, VALUE3, VALUE3, NO_ENTRY   , DIRTY      );
    CheckModifyCoins(VALUE1, PRUNED, PRUNED, PRUNED, 0          , DIRTY      );
    CheckModifyCoins(VALUE1, PRUNED, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckModifyCoins(VALUE1, PRUNED, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckModifyCoins(VALUE1, PRUNED, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckModifyCoins(VALUE1, PRUNED, VALUE3, VALUE3, 0          , DIRTY      );
    CheckModifyCoins(VALUE1, PRUNED, VALUE3, VALUE3, FRESH      , DIRTY|FRESH);
    CheckModifyCoins(VALUE1, PRUNED, VALUE3, VALUE3, DIRTY      , DIRTY      );
    CheckModifyCoins(VALUE1, PRUNED, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH);
    CheckModifyCoins(VALUE1, VALUE2, PRUNED, PRUNED, 0          , DIRTY      );
    CheckModifyCoins(VALUE1, VALUE2, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckModifyCoins(VALUE1, VALUE2, PRUNED, PRUNED, DIRTY      , DIRTY      );
    CheckModifyCoins(VALUE1, VALUE2, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckModifyCoins(VALUE1, VALUE2, VALUE3, VALUE3, 0          , DIRTY      );
    CheckModifyCoins(VALUE1, VALUE2, VALUE3, VALUE3, FRESH      , DIRTY|FRESH);
    CheckModifyCoins(VALUE1, VALUE2, VALUE3, VALUE3, DIRTY      , DIRTY      );
    CheckModifyCoins(VALUE1, VALUE2, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH);
}

void CheckModifyNewCoinsBase(CAmount base_value, CAmount cache_value, CAmount modify_value, CAmount expected_value, char cache_flags, char expected_flags)
{
    SingleEntryCacheTest test(base_value, cache_value, cache_flags);
    SetCoinsValue(modify_value, *test.cache.ModifyNewCoins(TXID));

    CAmount result_value;
    char result_flags;
    GetCoinsMapEntry(test.cache.map(), result_value, result_flags);
    BOOST_CHECK_EQUAL(result_value, expected_value);
    BOOST_CHECK_EQUAL(result_flags, expected_flags);
}

// Simple wrapper for CheckModifyNewCoinsBase function above that loops through
// different possible base_values, making sure each one gives the same results.
// This wrapper lets the modify_new test below be shorter and less repetitive,
// while still verifying that the CoinsViewCache::ModifyNewCoins implementation
// ignores base values.
template <typename... Args>
void CheckModifyNewCoins(Args&&... args)
{
    for (CAmount base_value : {ABSENT, PRUNED, VALUE1})
        CheckModifyNewCoinsBase(base_value, std::forward<Args>(args)...);
}

BOOST_AUTO_TEST_CASE(ccoins_modify_new)
{
    /* Check ModifyNewCoin behavior, requesting a new coin from a cache view,
     * writing a modification to the coin, and then checking the resulting
     * entry in the cache after the modification. Verify behavior with the
     * with the ModifyNewCoin coinbase argument set to false, and to true.
     *
     *                  Cache   Write   Result  Cache        Result
     *                  Value   Value   Value   Flags        Flags
     */
    CheckModifyNewCoins(ABSENT, PRUNED, ABSENT, NO_ENTRY   , NO_ENTRY   );
    CheckModifyNewCoins(ABSENT, VALUE3, VALUE3, NO_ENTRY   , DIRTY|FRESH);
    CheckModifyNewCoins(PRUNED, PRUNED, ABSENT, 0          , NO_ENTRY   );
    CheckModifyNewCoins(PRUNED, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckModifyNewCoins(PRUNED, PRUNED, ABSENT, DIRTY      , NO_ENTRY   );
    CheckModifyNewCoins(PRUNED, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckModifyNewCoins(PRUNED, VALUE3, VALUE3, 0          , DIRTY|FRESH);
    CheckModifyNewCoins(PRUNED, VALUE3, VALUE3, FRESH      , DIRTY|FRESH);
    CheckModifyNewCoins(PRUNED, VALUE3, VALUE3, DIRTY      , DIRTY|FRESH);
    CheckModifyNewCoins(PRUNED, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH);
    CheckModifyNewCoins(VALUE2, PRUNED, ABSENT, 0          , NO_ENTRY   );
    CheckModifyNewCoins(VALUE2, PRUNED, ABSENT, FRESH      , NO_ENTRY   );
    CheckModifyNewCoins(VALUE2, PRUNED, ABSENT, DIRTY      , NO_ENTRY   );
    CheckModifyNewCoins(VALUE2, PRUNED, ABSENT, DIRTY|FRESH, NO_ENTRY   );
    CheckModifyNewCoins(VALUE2, VALUE3, VALUE3, 0          , DIRTY|FRESH);
    CheckModifyNewCoins(VALUE2, VALUE3, VALUE3, FRESH      , DIRTY|FRESH);
    CheckModifyNewCoins(VALUE2, VALUE3, VALUE3, DIRTY      , DIRTY|FRESH);
    CheckModifyNewCoins(VALUE2, VALUE3, VALUE3, DIRTY|FRESH, DIRTY|FRESH);
}

void CheckWriteCoins(CAmount parent_value, CAmount child_value, CAmount expected_value, char parent_flags, char child_flags, char expected_flags)
{
    SingleEntryCacheTest test(ABSENT, parent_value, parent_flags);
    WriteCoinsViewEntry(test.cache, child_value, child_flags);
    test.cache.SelfTest();

    CAmount result_value;
    char result_flags;
    GetCoinsMapEntry(test.cache.map(), result_value, result_flags);
    BOOST_CHECK_EQUAL(result_value, expected_value);
    BOOST_CHECK_EQUAL(result_flags, expected_flags);
}

BOOST_AUTO_TEST_CASE(ccoins_write)
{
    /* Check BatchWrite behavior, flushing one entry from a child cache to a
     * parent cache, and checking the resulting entry in the parent cache
     * after the write.
     *
     *              Parent  Child   Result  Parent       Child        Result
     *              Value   Value   Value   Flags        Flags        Flags
     */
    CheckWriteCoins(ABSENT, ABSENT, ABSENT, NO_ENTRY   , NO_ENTRY   , NO_ENTRY   );
    CheckWriteCoins(ABSENT, PRUNED, PRUNED, NO_ENTRY   , DIRTY      , DIRTY      );
    CheckWriteCoins(ABSENT, PRUNED, ABSENT, NO_ENTRY   , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(ABSENT, VALUE2, VALUE2, NO_ENTRY   , DIRTY      , DIRTY      );
    CheckWriteCoins(ABSENT, VALUE2, VALUE2, NO_ENTRY   , DIRTY|FRESH, DIRTY|FRESH);
    CheckWriteCoins(PRUNED, ABSENT, PRUNED, 0          , NO_ENTRY   , 0          );
    CheckWriteCoins(PRUNED, ABSENT, PRUNED, FRESH      , NO_ENTRY   , FRESH      );
    CheckWriteCoins(PRUNED, ABSENT, PRUNED, DIRTY      , NO_ENTRY   , DIRTY      );
    CheckWriteCoins(PRUNED, ABSENT, PRUNED, DIRTY|FRESH, NO_ENTRY   , DIRTY|FRESH);
    CheckWriteCoins(PRUNED, PRUNED, PRUNED, 0          , DIRTY      , DIRTY      );
    CheckWriteCoins(PRUNED, PRUNED, PRUNED, 0          , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(PRUNED, PRUNED, ABSENT, FRESH      , DIRTY      , NO_ENTRY   );
    CheckWriteCoins(PRUNED, PRUNED, ABSENT, FRESH      , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(PRUNED, PRUNED, PRUNED, DIRTY      , DIRTY      , DIRTY      );
    CheckWriteCoins(PRUNED, PRUNED, PRUNED, DIRTY      , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(PRUNED, PRUNED, ABSENT, DIRTY|FRESH, DIRTY      , NO_ENTRY   );
    CheckWriteCoins(PRUNED, PRUNED, ABSENT, DIRTY|FRESH, DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, 0          , DIRTY      , DIRTY      );
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, 0          , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, FRESH      , DIRTY      , DIRTY|FRESH);
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, FRESH      , DIRTY|FRESH, DIRTY|FRESH);
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, DIRTY      , DIRTY      , DIRTY      );
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, DIRTY      , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, DIRTY|FRESH, DIRTY      , DIRTY|FRESH);
    CheckWriteCoins(PRUNED, VALUE2, VALUE2, DIRTY|FRESH, DIRTY|FRESH, DIRTY|FRESH);
    CheckWriteCoins(VALUE1, ABSENT, VALUE1, 0          , NO_ENTRY   , 0          );
    CheckWriteCoins(VALUE1, ABSENT, VALUE1, FRESH      , NO_ENTRY   , FRESH      );
    CheckWriteCoins(VALUE1, ABSENT, VALUE1, DIRTY      , NO_ENTRY   , DIRTY      );
    CheckWriteCoins(VALUE1, ABSENT, VALUE1, DIRTY|FRESH, NO_ENTRY   , DIRTY|FRESH);
    CheckWriteCoins(VALUE1, PRUNED, PRUNED, 0          , DIRTY      , DIRTY      );
    CheckWriteCoins(VALUE1, PRUNED, PRUNED, 0          , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(VALUE1, PRUNED, ABSENT, FRESH      , DIRTY      , NO_ENTRY   );
    CheckWriteCoins(VALUE1, PRUNED, ABSENT, FRESH      , DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(VALUE1, PRUNED, PRUNED, DIRTY      , DIRTY      , DIRTY      );
    CheckWriteCoins(VALUE1, PRUNED, PRUNED, DIRTY      , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(VALUE1, PRUNED, ABSENT, DIRTY|FRESH, DIRTY      , NO_ENTRY   );
    CheckWriteCoins(VALUE1, PRUNED, ABSENT, DIRTY|FRESH, DIRTY|FRESH, NO_ENTRY   );
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, 0          , DIRTY      , DIRTY      );
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, 0          , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, FRESH      , DIRTY      , DIRTY|FRESH);
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, FRESH      , DIRTY|FRESH, DIRTY|FRESH);
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, DIRTY      , DIRTY      , DIRTY      );
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, DIRTY      , DIRTY|FRESH, DIRTY      );
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, DIRTY|FRESH, DIRTY      , DIRTY|FRESH);
    CheckWriteCoins(VALUE1, VALUE2, VALUE2, DIRTY|FRESH, DIRTY|FRESH, DIRTY|FRESH);

    // The checks above omit cases where the child flags are not DIRTY, since
    // they would be too repetitive (the parent cache is never updated in these
    // cases). The loop below covers these cases and makes sure the parent cache
    // is always left unchanged.
    for (CAmount parent_value : {ABSENT, PRUNED, VALUE1})
        for (CAmount child_value : {ABSENT, PRUNED, VALUE2})
            for (char parent_flags : parent_value == ABSENT ? ABSENT_FLAGS : FLAGS)
                for (char child_flags : child_value == ABSENT ? ABSENT_FLAGS : CLEAN_FLAGS)
                    CheckWriteCoins(parent_value, child_value, parent_value, parent_flags, child_flags, parent_flags);
}

BOOST_AUTO_TEST_SUITE_END()
