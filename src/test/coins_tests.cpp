// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"
#include "random.h"
#include "script/standard.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"
#include "consensus/validation.h"
#include "main.h"
#include "undo.h"
#include "pubkey.h"

#include <vector>
#include <map>

#include <boost/test/unit_test.hpp>
#include "zcash/IncrementalMerkleTree.hpp"

namespace
{
class CCoinsViewTest : public CCoinsView
{
    uint256 hashBestBlock_;
    uint256 hashBestAnchor_;
    std::map<uint256, CCoins> map_;
    std::map<uint256, ZCIncrementalMerkleTree> mapAnchors_;
    std::map<uint256, bool> mapNullifiers_;

public:
    CCoinsViewTest() {
        hashBestAnchor_ = ZCIncrementalMerkleTree::empty_root();
    }

    bool GetAnchorAt(const uint256& rt, ZCIncrementalMerkleTree &tree) const {
        if (rt == ZCIncrementalMerkleTree::empty_root()) {
            ZCIncrementalMerkleTree new_tree;
            tree = new_tree;
            return true;
        }

        std::map<uint256, ZCIncrementalMerkleTree>::const_iterator it = mapAnchors_.find(rt);
        if (it == mapAnchors_.end()) {
            return false;
        } else {
            tree = it->second;
            return true;
        }
    }

    bool GetNullifier(const uint256 &nf) const
    {
        std::map<uint256, bool>::const_iterator it = mapNullifiers_.find(nf);

        if (it == mapNullifiers_.end()) {
            return false;
        } else {
            // The map shouldn't contain any false entries.
            assert(it->second);
            return true;
        }
    }

    uint256 GetBestAnchor() const { return hashBestAnchor_; }

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

    bool BatchWrite(CCoinsMap& mapCoins,
                    const uint256& hashBlock,
                    const uint256& hashAnchor,
                    CAnchorsMap& mapAnchors,
                    CNullifiersMap& mapNullifiers)
    {
        for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end(); ) {
            map_[it->first] = it->second.coins;
            if (it->second.coins.IsPruned() && insecure_rand() % 3 == 0) {
                // Randomly delete empty entries on write.
                map_.erase(it->first);
            }
            mapCoins.erase(it++);
        }
        for (CAnchorsMap::iterator it = mapAnchors.begin(); it != mapAnchors.end(); ) {
            if (it->second.entered) {
                std::map<uint256, ZCIncrementalMerkleTree>::iterator ret =
                    mapAnchors_.insert(std::make_pair(it->first, ZCIncrementalMerkleTree())).first;

                ret->second = it->second.tree;
            } else {
                mapAnchors_.erase(it->first);
            }
            mapAnchors.erase(it++);
        }
        for (CNullifiersMap::iterator it = mapNullifiers.begin(); it != mapNullifiers.end(); ) {
            if (it->second.entered) {
                mapNullifiers_[it->first] = true;
            } else {
                mapNullifiers_.erase(it->first);
            }
            mapNullifiers.erase(it++);
        }
        mapCoins.clear();
        mapAnchors.clear();
        mapNullifiers.clear();
        hashBestBlock_ = hashBlock;
        hashBestAnchor_ = hashAnchor;
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
                     memusage::DynamicUsage(cacheAnchors) +
                     memusage::DynamicUsage(cacheNullifiers);
        for (CCoinsMap::iterator it = cacheCoins.begin(); it != cacheCoins.end(); it++) {
            ret += memusage::DynamicUsage(it->second.coins);
        }
        BOOST_CHECK_EQUAL(memusage::DynamicUsage(*this), ret);
    }

};

}

uint256 appendRandomCommitment(ZCIncrementalMerkleTree &tree)
{
    libzcash::SpendingKey k = libzcash::SpendingKey::random();
    libzcash::PaymentAddress addr = k.address();

    libzcash::Note note(addr.a_pk, 0, uint256(), uint256());

    auto cm = note.cm();
    tree.append(cm);
    return cm;
}

BOOST_FIXTURE_TEST_SUITE(coins_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(nullifier_regression_test)
{
    // Correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert a nullifier into the base.
        uint256 nf = GetRandHash();
        cache1.SetNullifier(nf, true);
        cache1.Flush(); // Flush to base.

        // Remove the nullifier from cache
        cache1.SetNullifier(nf, false);

        // The nullifier now should be `false`.
        BOOST_CHECK(!cache1.GetNullifier(nf));
    }

    // Also correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert a nullifier into the base.
        uint256 nf = GetRandHash();
        cache1.SetNullifier(nf, true);
        cache1.Flush(); // Flush to base.

        // Remove the nullifier from cache
        cache1.SetNullifier(nf, false);
        cache1.Flush(); // Flush to base.

        // The nullifier now should be `false`.
        BOOST_CHECK(!cache1.GetNullifier(nf));
    }

    // Works because we bring it from the parent cache:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert a nullifier into the base.
        uint256 nf = GetRandHash();
        cache1.SetNullifier(nf, true);
        cache1.Flush(); // Empties cache.

        // Create cache on top.
        {
            // Remove the nullifier.
            CCoinsViewCacheTest cache2(&cache1);
            BOOST_CHECK(cache2.GetNullifier(nf));
            cache2.SetNullifier(nf, false);
            cache2.Flush(); // Empties cache, flushes to cache1.
        }

        // The nullifier now should be `false`.
        BOOST_CHECK(!cache1.GetNullifier(nf));
    }

    // Was broken:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert a nullifier into the base.
        uint256 nf = GetRandHash();
        cache1.SetNullifier(nf, true);
        cache1.Flush(); // Empties cache.

        // Create cache on top.
        {
            // Remove the nullifier.
            CCoinsViewCacheTest cache2(&cache1);
            cache2.SetNullifier(nf, false);
            cache2.Flush(); // Empties cache, flushes to cache1.
        }

        // The nullifier now should be `false`.
        BOOST_CHECK(!cache1.GetNullifier(nf));
    }
}

BOOST_AUTO_TEST_CASE(anchor_pop_regression_test)
{
    // Correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Create dummy anchor/commitment
        ZCIncrementalMerkleTree tree;
        uint256 cm = GetRandHash();
        tree.append(cm);

        // Add the anchor
        cache1.PushAnchor(tree);
        cache1.Flush();

        // Remove the anchor
        cache1.PopAnchor(ZCIncrementalMerkleTree::empty_root());
        cache1.Flush();

        // Add the anchor back
        cache1.PushAnchor(tree);
        cache1.Flush();

        // The base contains the anchor, of course!
        {
            ZCIncrementalMerkleTree checktree;
            BOOST_CHECK(cache1.GetAnchorAt(tree.root(), checktree));
            BOOST_CHECK(checktree.root() == tree.root());
        }
    }

    // Previously incorrect behavior
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Create dummy anchor/commitment
        ZCIncrementalMerkleTree tree;
        uint256 cm = GetRandHash();
        tree.append(cm);

        // Add the anchor and flush to disk
        cache1.PushAnchor(tree);
        cache1.Flush();

        // Remove the anchor, but don't flush yet!
        cache1.PopAnchor(ZCIncrementalMerkleTree::empty_root());

        {
            CCoinsViewCacheTest cache2(&cache1); // Build cache on top
            cache2.PushAnchor(tree); // Put the same anchor back!
            cache2.Flush(); // Flush to cache1
        }

        // cache2's flush kinda worked, i.e. cache1 thinks the
        // tree is there, but it didn't bring down the correct
        // treestate...
        {
            ZCIncrementalMerkleTree checktree;
            BOOST_CHECK(cache1.GetAnchorAt(tree.root(), checktree));
            BOOST_CHECK(checktree.root() == tree.root()); // Oh, shucks.
        }

        // Flushing cache won't help either, just makes the inconsistency
        // permanent.
        cache1.Flush();
        {
            ZCIncrementalMerkleTree checktree;
            BOOST_CHECK(cache1.GetAnchorAt(tree.root(), checktree));
            BOOST_CHECK(checktree.root() == tree.root()); // Oh, shucks.
        }
    }
}

BOOST_AUTO_TEST_CASE(anchor_regression_test)
{
    // Correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        ZCIncrementalMerkleTree tree;
        uint256 cm = GetRandHash();
        tree.append(cm);
        cache1.PushAnchor(tree);
        cache1.Flush();

        cache1.PopAnchor(ZCIncrementalMerkleTree::empty_root());
        BOOST_CHECK(cache1.GetBestAnchor() == ZCIncrementalMerkleTree::empty_root());
        BOOST_CHECK(!cache1.GetAnchorAt(tree.root(), tree));
    }

    // Also correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        ZCIncrementalMerkleTree tree;
        uint256 cm = GetRandHash();
        tree.append(cm);
        cache1.PushAnchor(tree);
        cache1.Flush();

        cache1.PopAnchor(ZCIncrementalMerkleTree::empty_root());
        cache1.Flush();
        BOOST_CHECK(cache1.GetBestAnchor() == ZCIncrementalMerkleTree::empty_root());
        BOOST_CHECK(!cache1.GetAnchorAt(tree.root(), tree));
    }

    // Works because we bring the anchor in from parent cache.
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        ZCIncrementalMerkleTree tree;
        uint256 cm = GetRandHash();
        tree.append(cm);
        cache1.PushAnchor(tree);
        cache1.Flush();

        {
            // Pop anchor.
            CCoinsViewCacheTest cache2(&cache1);
            BOOST_CHECK(cache2.GetAnchorAt(tree.root(), tree));
            cache2.PopAnchor(ZCIncrementalMerkleTree::empty_root());
            cache2.Flush();
        }

        BOOST_CHECK(cache1.GetBestAnchor() == ZCIncrementalMerkleTree::empty_root());
        BOOST_CHECK(!cache1.GetAnchorAt(tree.root(), tree));
    }

    // Was broken:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        ZCIncrementalMerkleTree tree;
        uint256 cm = GetRandHash();
        tree.append(cm);
        cache1.PushAnchor(tree);
        cache1.Flush();

        {
            // Pop anchor.
            CCoinsViewCacheTest cache2(&cache1);
            cache2.PopAnchor(ZCIncrementalMerkleTree::empty_root());
            cache2.Flush();
        }

        BOOST_CHECK(cache1.GetBestAnchor() == ZCIncrementalMerkleTree::empty_root());
        BOOST_CHECK(!cache1.GetAnchorAt(tree.root(), tree));
    }
}

BOOST_AUTO_TEST_CASE(nullifiers_test)
{
    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    uint256 nf = GetRandHash();

    BOOST_CHECK(!cache.GetNullifier(nf));
    cache.SetNullifier(nf, true);
    BOOST_CHECK(cache.GetNullifier(nf));
    cache.Flush();

    CCoinsViewCacheTest cache2(&base);

    BOOST_CHECK(cache2.GetNullifier(nf));
    cache2.SetNullifier(nf, false);
    BOOST_CHECK(!cache2.GetNullifier(nf));
    cache2.Flush();

    CCoinsViewCacheTest cache3(&base);

    BOOST_CHECK(!cache3.GetNullifier(nf));
}

BOOST_AUTO_TEST_CASE(anchors_flush_test)
{
    CCoinsViewTest base;
    uint256 newrt;
    {
        CCoinsViewCacheTest cache(&base);
        ZCIncrementalMerkleTree tree;
        BOOST_CHECK(cache.GetAnchorAt(cache.GetBestAnchor(), tree));
        appendRandomCommitment(tree);

        newrt = tree.root();

        cache.PushAnchor(tree);
        cache.Flush();
    }
    
    {
        CCoinsViewCacheTest cache(&base);
        ZCIncrementalMerkleTree tree;
        BOOST_CHECK(cache.GetAnchorAt(cache.GetBestAnchor(), tree));

        // Get the cached entry.
        BOOST_CHECK(cache.GetAnchorAt(cache.GetBestAnchor(), tree));

        uint256 check_rt = tree.root();

        BOOST_CHECK(check_rt == newrt);
    }
}

BOOST_AUTO_TEST_CASE(chained_joinsplits)
{
    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    ZCIncrementalMerkleTree tree;

    JSDescription js1;
    js1.anchor = tree.root();
    js1.commitments[0] = appendRandomCommitment(tree);
    js1.commitments[1] = appendRandomCommitment(tree);

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

    js2.commitments[0] = appendRandomCommitment(tree);
    js2.commitments[1] = appendRandomCommitment(tree);

    js3.commitments[0] = appendRandomCommitment(tree);
    js3.commitments[1] = appendRandomCommitment(tree);

    {
        CMutableTransaction mtx;
        mtx.vjoinsplit.push_back(js2);

        BOOST_CHECK(!cache.HaveJoinSplitRequirements(mtx));
    }

    {
        // js2 is trying to anchor to js1 but js1
        // appears afterwards -- not a permitted ordering
        CMutableTransaction mtx;
        mtx.vjoinsplit.push_back(js2);
        mtx.vjoinsplit.push_back(js1);

        BOOST_CHECK(!cache.HaveJoinSplitRequirements(mtx));
    }

    {
        CMutableTransaction mtx;
        mtx.vjoinsplit.push_back(js1);
        mtx.vjoinsplit.push_back(js2);

        BOOST_CHECK(cache.HaveJoinSplitRequirements(mtx));
    }

    {
        CMutableTransaction mtx;
        mtx.vjoinsplit.push_back(js1);
        mtx.vjoinsplit.push_back(js2);
        mtx.vjoinsplit.push_back(js3);

        BOOST_CHECK(cache.HaveJoinSplitRequirements(mtx));
    }

    {
        CMutableTransaction mtx;
        mtx.vjoinsplit.push_back(js1);
        mtx.vjoinsplit.push_back(js1b);
        mtx.vjoinsplit.push_back(js2);
        mtx.vjoinsplit.push_back(js3);

        BOOST_CHECK(cache.HaveJoinSplitRequirements(mtx));
    }
}

BOOST_AUTO_TEST_CASE(anchors_test)
{
    // TODO: These tests should be more methodical.
    //       Or, integrate with Bitcoin's tests later.

    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    BOOST_CHECK(cache.GetBestAnchor() == ZCIncrementalMerkleTree::empty_root());

    {
        ZCIncrementalMerkleTree tree;

        BOOST_CHECK(cache.GetAnchorAt(cache.GetBestAnchor(), tree));
        BOOST_CHECK(cache.GetBestAnchor() == tree.root());
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);

        ZCIncrementalMerkleTree save_tree_for_later;
        save_tree_for_later = tree;

        uint256 newrt = tree.root();
        uint256 newrt2;

        cache.PushAnchor(tree);
        BOOST_CHECK(cache.GetBestAnchor() == newrt);

        {
            ZCIncrementalMerkleTree confirm_same;
            BOOST_CHECK(cache.GetAnchorAt(cache.GetBestAnchor(), confirm_same));

            BOOST_CHECK(confirm_same.root() == newrt);
        }

        appendRandomCommitment(tree);
        appendRandomCommitment(tree);

        newrt2 = tree.root();

        cache.PushAnchor(tree);
        BOOST_CHECK(cache.GetBestAnchor() == newrt2);

        ZCIncrementalMerkleTree test_tree;
        BOOST_CHECK(cache.GetAnchorAt(cache.GetBestAnchor(), test_tree));

        BOOST_CHECK(tree.root() == test_tree.root());

        {
            ZCIncrementalMerkleTree test_tree2;
            cache.GetAnchorAt(newrt, test_tree2);
            
            BOOST_CHECK(test_tree2.root() == newrt);
        }

        {
            cache.PopAnchor(newrt);
            ZCIncrementalMerkleTree obtain_tree;
            assert(!cache.GetAnchorAt(newrt2, obtain_tree)); // should have been popped off
            assert(cache.GetAnchorAt(newrt, obtain_tree));

            assert(obtain_tree.root() == newrt);
        }
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
            BOOST_FOREACH(const CCoinsViewCacheTest *test, stack) {
                test->SelfTest();
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
    UpdateCoins(tx, state, cache, 100);

    // Create coinbase spend
    CMutableTransaction mtx2;
    mtx2.vin.resize(1);
    mtx2.vin[0].prevout = COutPoint(tx.GetHash(), 0);
    mtx2.vin[0].scriptSig = CScript() << OP_1;
    mtx2.vin[0].nSequence = 0;

    {
        CTransaction tx2(mtx2);
        BOOST_CHECK(NonContextualCheckInputs(tx2, state, cache, false, SCRIPT_VERIFY_NONE, false, Params().GetConsensus()));
    }

    mtx2.vout.resize(1);
    mtx2.vout[0].nValue = 500;
    mtx2.vout[0].scriptPubKey = CScript() << OP_1;

    {
        CTransaction tx2(mtx2);
        BOOST_CHECK(!NonContextualCheckInputs(tx2, state, cache, false, SCRIPT_VERIFY_NONE, false, Params().GetConsensus()));
        BOOST_CHECK(state.GetRejectReason() == "bad-txns-coinbase-spend-has-transparent-outputs");
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

BOOST_AUTO_TEST_SUITE_END()
