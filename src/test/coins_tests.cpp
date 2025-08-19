// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "coins.h"
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

#include <boost/test/unit_test.hpp>
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
    ~CCoinsViewTest() {}

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

    HistoryIndex GetHistoryLength(uint32_t epochId) const { return 0; }
    HistoryNode GetHistoryAt(uint32_t epochId, HistoryIndex index) const { return HistoryNode(); }
    uint256 GetHistoryRoot(uint32_t epochId) const { return uint256(); }

    std::optional<libzcash::LatestSubtree> GetLatestSubtree(ShieldedType type) const {
        return std::nullopt;
    };
    std::optional<libzcash::SubtreeData> GetSubtreeData(
            ShieldedType type,
            libzcash::SubtreeIndex index) const
    {
        return std::nullopt;
    };

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
    ~CCoinsViewCacheTest() {}

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
        BOOST_CHECK_EQUAL(DynamicMemoryUsage(), ret);
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

        sproutNullifier = InsecureRand256();
        JSDescription jsd;
        jsd.nullifiers[0] = sproutNullifier;
        mutableTx.vJoinSplit.emplace_back(jsd);

        mutableTx.saplingBundle = sapling::test_only_invalid_bundle(1, 0, 0);
        saplingNullifier = uint256::FromRawBytes(mutableTx.saplingBundle.GetDetails().spends()[0].nullifier());

        // The Orchard bundle builder always pads to two Actions, so we can just
        // use an empty builder to create a dummy Orchard bundle.
        // TODO: With the new BundleType::DEFAULT this is no longer true. Fix this.
        uint256 orchardAnchor;
        uint256 dataToBeSigned;
        auto builder = orchard::Builder(false, orchardAnchor);
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
template<> void AppendRandomLeaf(SproutMerkleTree &tree) { tree.append(InsecureRand256()); }
template<> void AppendRandomLeaf(SaplingMerkleTree &tree) { tree.append(InsecureRand256()); }
template<> void AppendRandomLeaf(OrchardMerkleFrontier &tree) {
    // OrchardMerkleFrontier only has APIs to append entire bundles, but
    // fortunately the tests only require that the tree root change.
    // TODO: Remove the need to create proofs by having a testing-only way to
    // append a random leaf to OrchardMerkleFrontier.
    uint256 orchardAnchor;
    uint256 dataToBeSigned;
    auto builder = orchard::Builder(false, orchardAnchor);
    auto bundle = builder.Build().value().ProveAndSign({}, dataToBeSigned).value();
    tree.AppendBundle(bundle);
}

template<typename Tree> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, Tree &tree);
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, SproutMerkleTree &tree) { return cache.GetSproutAnchorAt(rt, tree); }
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, SaplingMerkleTree &tree) { return cache.GetSaplingAnchorAt(rt, tree); }
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, OrchardMerkleFrontier &tree) { return cache.GetOrchardAnchorAt(rt, tree); }

BOOST_FIXTURE_TEST_SUITE(coins_tests, BasicTestingSetup)

void checkNullifierCache(const CCoinsViewCacheTest &cache, const TxWithNullifiers &txWithNullifiers, bool shouldBeInCache) {
    // Make sure the nullifiers have not gotten mixed up
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.sproutNullifier, SAPLING));
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.sproutNullifier, ORCHARD));
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.saplingNullifier, SPROUT));
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.saplingNullifier, ORCHARD));
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.orchardNullifier, SPROUT));
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.orchardNullifier, SAPLING));
    // Check if the nullifiers either are or are not in the cache
    bool containsSproutNullifier = cache.GetNullifier(txWithNullifiers.sproutNullifier, SPROUT);
    bool containsSaplingNullifier = cache.GetNullifier(txWithNullifiers.saplingNullifier, SAPLING);
    bool containsOrchardNullifier = cache.GetNullifier(txWithNullifiers.orchardNullifier, ORCHARD);
    BOOST_CHECK(containsSproutNullifier == shouldBeInCache);
    BOOST_CHECK(containsSaplingNullifier == shouldBeInCache);
    BOOST_CHECK(containsOrchardNullifier == shouldBeInCache);
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

        CTransaction tx(mtx);
        BOOST_CHECK(cache.CheckShieldedRequirements(tx) == tl::unexpected(UnsatisfiedShieldedReq::SproutUnknownAnchor));
    }

    {
        // js2 is trying to anchor to js1 but js1
        // appears afterwards -- not a permitted ordering
        CMutableTransaction mtx;
        mtx.vJoinSplit.push_back(js2);
        mtx.vJoinSplit.push_back(js1);

        CTransaction tx(mtx);
        BOOST_CHECK(cache.CheckShieldedRequirements(tx) == tl::unexpected(UnsatisfiedShieldedReq::SproutUnknownAnchor));
    }

    {
        CMutableTransaction mtx;
        mtx.vJoinSplit.push_back(js1);
        mtx.vJoinSplit.push_back(js2);

        CTransaction tx(mtx);
        BOOST_CHECK(cache.CheckShieldedRequirements(tx).has_value());
    }

    {
        CMutableTransaction mtx;
        mtx.vJoinSplit.push_back(js1);
        mtx.vJoinSplit.push_back(js2);
        mtx.vJoinSplit.push_back(js3);

        CTransaction tx(mtx);
        BOOST_CHECK(cache.CheckShieldedRequirements(tx).has_value());
    }

    {
        CMutableTransaction mtx;
        mtx.vJoinSplit.push_back(js1);
        mtx.vJoinSplit.push_back(js1b);
        mtx.vJoinSplit.push_back(js2);
        mtx.vJoinSplit.push_back(js3);

        CTransaction tx(mtx);
        BOOST_CHECK(cache.CheckShieldedRequirements(tx).has_value());
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
        txids[i] = InsecureRand256();
    }

    for (unsigned int i = 0; i < NUM_SIMULATION_ITERATIONS; i++) {
        // Do a random modification.
        {
            uint256 txid = txids[InsecureRandRange(txids.size())]; // txid we're going to modify in this iteration.
            CCoins& coins = result[txid];
            CCoinsModifier entry = stack.back()->ModifyCoins(txid);
            BOOST_CHECK(coins == *entry);
            if (InsecureRandRange(5) == 0 || coins.IsPruned()) {
                if (coins.IsPruned()) {
                    added_an_entry = true;
                } else {
                    updated_an_entry = true;
                }
                coins.nVersion = InsecureRand32();
                coins.vout.resize(1);
                coins.vout[0].nValue = InsecureRand32();
                *entry = coins;
            } else {
                coins.Clear();
                entry->Clear();
                removed_an_entry = true;
            }
        }

        // Once every 1000 iterations and at the end, verify the full cache.
        if (InsecureRandRange(1000) == 1 || i == NUM_SIMULATION_ITERATIONS - 1) {
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

        if (InsecureRandRange(100) == 0) {
            // Every 100 iterations, change the cache stack.
            if (stack.size() > 0 && InsecureRandBool() == 0) {
                stack.back()->Flush();
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && InsecureRandBool())) {
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
            CMutableTransaction mtx;
            mtx.vin.resize(1);
            mtx.vout.resize(1);
            mtx.vout[0].nValue = i; //Keep txs unique
            unsigned int height = InsecureRand32();

            // 1/10 times create a coinbase
            if (InsecureRandRange(10) == 0 || coinbaseids.size() < 10) {
                coinbaseids[mtx.GetHash()] = mtx.vout[0].nValue;
                assert(CTransaction(mtx).IsCoinBase());
            }
            // 9/10 times create a regular tx
            else {
                uint256 prevouthash;
                // equally likely to spend coinbase or non coinbase
                std::set<uint256>::iterator txIt = alltxids.lower_bound(InsecureRand256());
                if (txIt == alltxids.end()) {
                    txIt = alltxids.begin();
                }
                prevouthash = *txIt;

                // Construct the tx to spend the coins of prevouthash
                mtx.vin[0].prevout.hash = prevouthash;
                mtx.vin[0].prevout.n = 0;

                // Update the expected result of prevouthash to know these coins are spent
                CCoins& oldcoins = result[prevouthash];
                oldcoins.Clear();

                alltxids.erase(prevouthash);
                coinbaseids.erase(prevouthash);

                assert(!CTransaction(mtx).IsCoinBase());
            }
            // Track this tx to possibly spend later
            CTransaction tx(mtx);
            alltxids.insert(tx.GetHash());

            // Update the expected result to know about the new output coins
            CCoins &coins = result[tx.GetHash()];
            coins.FromTx(tx, height);

            UpdateCoins(tx, *(stack.back()), height);
        }

        // Once every 1000 iterations and at the end, verify the full cache.
        if (InsecureRandRange(1000) == 1 || i == NUM_SIMULATION_ITERATIONS - 1) {
            for (std::map<uint256, CCoins>::iterator it = result.begin(); it != result.end(); it++) {
                const CCoins* coins = stack.back()->AccessCoins(it->first);
                if (coins) {
                    BOOST_CHECK(*coins == it->second);
                 } else {
                    BOOST_CHECK(it->second.IsPruned());
                 }
            }
        }

        if (InsecureRandRange(100) == 0) {
            // Every 100 iterations, change the cache stack.
            if (stack.size() > 0 && InsecureRandBool() == 0) {
                stack.back()->Flush();
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && InsecureRandBool())) {
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

BOOST_AUTO_TEST_SUITE_END()
