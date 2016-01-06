// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"
#include "random.h"
#include "uint256.h"
#include "test/test_bitcoin.h"

#include <vector>
#include <map>

#include <boost/test/unit_test.hpp>
#include "libzerocash/IncrementalMerkleTree.h"

namespace
{
class CCoinsViewTest : public CCoinsView
{
    uint256 hashBestBlock_;
    uint256 hashBestAnchor_;
    std::map<uint256, CCoins> map_;
    std::map<uint256, libzerocash::IncrementalMerkleTree> mapAnchors_;
    std::map<uint256, bool> mapSerials_;

public:
    bool GetAnchorAt(const uint256& rt, libzerocash::IncrementalMerkleTree &tree) const {
        if (rt.IsNull()) {
            IncrementalMerkleTree new_tree(INCREMENTAL_MERKLE_TREE_DEPTH);
            tree.setTo(new_tree);
            return true;
        }

        std::map<uint256, libzerocash::IncrementalMerkleTree>::const_iterator it = mapAnchors_.find(rt);
        if (it == mapAnchors_.end()) {
            return false;
        } else {
            tree.setTo(it->second);
            return true;
        }
    }

    bool GetSerial(const uint256 &serial) const
    {
        std::map<uint256, bool>::const_iterator it = mapSerials_.find(serial);

        if (it == mapSerials_.end()) {
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
                    CSerialsMap& mapSerials)
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
                mapAnchors_[it->first] = it->second.tree;
            } else {
                mapAnchors_.erase(it->first);
            }
            mapAnchors.erase(it++);
        }
        for (CSerialsMap::iterator it = mapSerials.begin(); it != mapSerials.end(); ) {
            if (it->second.entered) {
                mapSerials_[it->first] = true;
            } else {
                mapSerials_.erase(it->first);
            }
            mapSerials.erase(it++);
        }
        mapCoins.clear();
        mapAnchors.clear();
        mapSerials.clear();
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
        size_t ret = memusage::DynamicUsage(cacheCoins);
        for (CCoinsMap::iterator it = cacheCoins.begin(); it != cacheCoins.end(); it++) {
            ret += memusage::DynamicUsage(it->second.coins);
        }
        BOOST_CHECK_EQUAL(memusage::DynamicUsage(*this), ret);
    }

};

}

BOOST_AUTO_TEST_CASE(serials_test)
{
    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    uint256 myserial = GetRandHash();

    BOOST_CHECK(!cache.GetSerial(myserial));
    cache.SetSerial(myserial, true);
    BOOST_CHECK(cache.GetSerial(myserial));
    cache.Flush();

    CCoinsViewCacheTest cache2(&base);

    BOOST_CHECK(cache2.GetSerial(myserial));
    cache2.SetSerial(myserial, false);
    BOOST_CHECK(!cache2.GetSerial(myserial));
    cache2.Flush();

    CCoinsViewCacheTest cache3(&base);

    BOOST_CHECK(!cache3.GetSerial(myserial));
}

void appendRandomCommitment(IncrementalMerkleTree &tree)
{
    Address addr = Address::CreateNewRandomAddress();
    Coin coin(addr.getPublicAddress(), 100);

    std::vector<bool> commitment(ZC_CM_SIZE * 8);
    convertBytesVectorToVector(coin.getCoinCommitment().getCommitmentValue(), commitment);

    std::vector<bool> index;
    tree.insertElement(commitment, index);
}

BOOST_AUTO_TEST_CASE(anchors_test)
{
    // TODO: These tests should be more methodical.
    //       Or, integrate with Bitcoin's tests later.

    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    BOOST_CHECK(cache.GetBestAnchor() == uint256());

    {
        IncrementalMerkleTree tree(INCREMENTAL_MERKLE_TREE_DEPTH);

        BOOST_CHECK(cache.GetAnchorAt(cache.GetBestAnchor(), tree));
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        tree.prune();

        IncrementalMerkleTree save_tree_for_later(INCREMENTAL_MERKLE_TREE_DEPTH);
        save_tree_for_later.setTo(tree);

        uint256 newrt;
        uint256 newrt2;
        {
            std::vector<unsigned char> newrt_v(32);
            tree.getRootValue(newrt_v);

            newrt = uint256(newrt_v);
        }

        cache.PushAnchor(tree);
        BOOST_CHECK(cache.GetBestAnchor() == newrt);

        {
            IncrementalMerkleTree confirm_same(INCREMENTAL_MERKLE_TREE_DEPTH);
            BOOST_CHECK(cache.GetAnchorAt(cache.GetBestAnchor(), confirm_same));

            uint256 confirm_rt;
            {
                std::vector<unsigned char> newrt_v(32);
                confirm_same.getRootValue(newrt_v);

                confirm_rt = uint256(newrt_v);
            }

            BOOST_CHECK(confirm_rt == newrt);
        }

        appendRandomCommitment(tree);
        appendRandomCommitment(tree);
        tree.prune();

        {
            std::vector<unsigned char> newrt_v(32);
            tree.getRootValue(newrt_v);

            newrt2 = uint256(newrt_v);
        }

        cache.PushAnchor(tree);
        BOOST_CHECK(cache.GetBestAnchor() == newrt2);

        IncrementalMerkleTree test_tree(INCREMENTAL_MERKLE_TREE_DEPTH);
        BOOST_CHECK(cache.GetAnchorAt(cache.GetBestAnchor(), test_tree));

        {
            std::vector<unsigned char> a(32);
            std::vector<unsigned char> b(32);
            tree.getRootValue(a);
            test_tree.getRootValue(b);

            BOOST_CHECK(a == b);
        }

        {
            std::vector<unsigned char> a(32);
            std::vector<unsigned char> b(32);
            IncrementalMerkleTree test_tree2(INCREMENTAL_MERKLE_TREE_DEPTH);
            cache.GetAnchorAt(newrt, test_tree2);
            
            uint256 recovered_rt;
            {
                std::vector<unsigned char> newrt_v(32);
                test_tree2.getRootValue(newrt_v);

                recovered_rt = uint256(newrt_v);
            }

            BOOST_CHECK(recovered_rt == newrt);
        }

        {
            cache.PopAnchor(newrt);
            IncrementalMerkleTree obtain_tree(INCREMENTAL_MERKLE_TREE_DEPTH);
            assert(!cache.GetAnchorAt(newrt2, obtain_tree)); // should have been popped off
            assert(cache.GetAnchorAt(newrt, obtain_tree));

            uint256 recovered_rt;
            {
                std::vector<unsigned char> newrt_v(32);
                obtain_tree.getRootValue(newrt_v);

                recovered_rt = uint256(newrt_v);
            }

            assert(recovered_rt == newrt);
        }
    }
}

BOOST_FIXTURE_TEST_SUITE(coins_tests, BasicTestingSetup)

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

BOOST_AUTO_TEST_SUITE_END()
