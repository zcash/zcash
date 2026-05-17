// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2018-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "consensus/upgrades.h"
#include "main.h"
#include "txmempool.h"
#include "util/system.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>
#include <list>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(mempool_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(MempoolRemoveTest)
{
    // Test CTxMemPool::remove functionality

    TestMemPoolEntryHelper entry;
    // Parent transaction with three children,
    // and three grand-children:
    CMutableTransaction mtxParent;
    mtxParent.vin.resize(1);
    mtxParent.vin[0].scriptSig = CScript() << OP_11;
    mtxParent.vout.resize(3);
    for (int i = 0; i < 3; i++)
    {
        mtxParent.vout[i].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        mtxParent.vout[i].nValue = 33000LL;
    }
    CTransaction txParent(mtxParent);

    CTransaction txChild[3];
    for (int i = 0; i < 3; i++)
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].scriptSig = CScript() << OP_11;
        mtx.vin[0].prevout.hash = mtxParent.GetHash();
        mtx.vin[0].prevout.n = i;
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        mtx.vout[0].nValue = 11000LL;
        txChild[i] = CTransaction(mtx);
    }

    CTransaction txGrandChild[3];
    for (int i = 0; i < 3; i++)
    {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].scriptSig = CScript() << OP_11;
        mtx.vin[0].prevout.hash = txChild[i].GetHash();
        mtx.vin[0].prevout.n = 0;
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        mtx.vout[0].nValue = 11000LL;
        txGrandChild[i] = CTransaction(mtx);
    }


    CTxMemPool testPool(CFeeRate(0));
    std::list<CTransaction> removed;

    // Nothing in pool, remove should do nothing:
    testPool.remove(txParent, removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 0);

    // Just the parent:
    testPool.addUnchecked(txParent.GetHash(), entry.FromTx(txParent));
    testPool.remove(txParent, removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 1);
    removed.clear();

    // Parent, children, grandchildren:
    testPool.addUnchecked(txParent.GetHash(), entry.FromTx(txParent));
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(txChild[i].GetHash(), entry.FromTx(txChild[i]));
        testPool.addUnchecked(txGrandChild[i].GetHash(), entry.FromTx(txGrandChild[i]));
    }
    // Remove Child[0], GrandChild[0] should be removed:
    testPool.remove(txChild[0], removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 2);
    removed.clear();
    // ... make sure grandchild and child are gone:
    testPool.remove(txGrandChild[0], removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 0);
    testPool.remove(txChild[0], removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 0);
    // Remove parent, all children/grandchildren should go:
    testPool.remove(txParent, removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 5);
    BOOST_CHECK_EQUAL(testPool.size(), 0);
    removed.clear();

    // Add children and grandchildren, but NOT the parent (simulate the parent being in a block)
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(txChild[i].GetHash(), entry.FromTx(txChild[i]));
        testPool.addUnchecked(txGrandChild[i].GetHash(), entry.FromTx(txGrandChild[i]));
    }
    // Now remove the parent, as might happen if a block-re-org occurs but the parent cannot be
    // put into the mempool (maybe because it is non-standard):
    testPool.remove(txParent, removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 6);
    BOOST_CHECK_EQUAL(testPool.size(), 0);
    removed.clear();
}

template<typename name>
void CheckSort(CTxMemPool &pool, std::vector<std::string> &sortedOrder)
{
    BOOST_CHECK_EQUAL(pool.size(), sortedOrder.size());
    typename CTxMemPool::indexed_transaction_set::index<name>::type::iterator it = pool.mapTx.get<name>().begin();
    int count=0;
    for (; it != pool.mapTx.get<name>().end(); ++it, ++count) {
        BOOST_CHECK_EQUAL(it->GetTx().GetHash().ToString(), sortedOrder[count]);
    }
}

BOOST_AUTO_TEST_CASE(MempoolIndexingTest)
{
    CTxMemPool pool(CFeeRate(0));
    TestMemPoolEntryHelper entry;
    entry.hadNoDependencies = true;

    /* 3rd highest fee */
    CMutableTransaction mtx1 = CMutableTransaction();
    mtx1.vout.resize(1);
    mtx1.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx1.vout[0].nValue = 10 * COIN;
    CTransaction tx1(mtx1);
    pool.addUnchecked(tx1.GetHash(), entry.Fee(10000LL).FromTx(tx1));

    /* highest fee */
    CMutableTransaction mtx2 = CMutableTransaction();
    mtx2.vout.resize(1);
    mtx2.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx2.vout[0].nValue = 2 * COIN;
    CTransaction tx2(mtx2);
    pool.addUnchecked(tx2.GetHash(), entry.Fee(20000LL).FromTx(tx2));

    /* lowest fee */
    CMutableTransaction mtx3 = CMutableTransaction();
    mtx3.vout.resize(1);
    mtx3.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx3.vout[0].nValue = 5 * COIN;
    CTransaction tx3(mtx3);
    pool.addUnchecked(tx3.GetHash(), entry.Fee(0LL).FromTx(tx3));

    /* 2nd highest fee */
    CMutableTransaction mtx4 = CMutableTransaction();
    mtx4.vout.resize(1);
    mtx4.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx4.vout[0].nValue = 6 * COIN;
    CTransaction tx4(mtx4);
    pool.addUnchecked(tx4.GetHash(), entry.Fee(15000LL).FromTx(tx4));

    /* equal fee rate to tx1, but newer */
    CMutableTransaction mtx5 = CMutableTransaction();
    mtx5.vout.resize(1);
    mtx5.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx5.vout[0].nValue = 11 * COIN;
    entry.nTime = 1;
    CTransaction tx5(mtx5);
    pool.addUnchecked(tx5.GetHash(), entry.Fee(10000LL).FromTx(tx5));
    BOOST_CHECK_EQUAL(pool.size(), 5);

    std::vector<std::string> sortedOrder;
    sortedOrder.resize(5);
    sortedOrder[0] = tx3.GetHash().ToString(); // 0
    sortedOrder[1] = tx5.GetHash().ToString(); // 10000
    sortedOrder[2] = tx1.GetHash().ToString(); // 10000
    sortedOrder[3] = tx4.GetHash().ToString(); // 15000
    sortedOrder[4] = tx2.GetHash().ToString(); // 20000
    CheckSort<descendant_score>(pool, sortedOrder);

    /* low fee but with high fee child */
    /* tx6 -> tx7 -> tx8, tx9 -> tx10 */
    CMutableTransaction mtx6 = CMutableTransaction();
    mtx6.vout.resize(1);
    mtx6.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx6.vout[0].nValue = 20 * COIN;
    CTransaction tx6(mtx6);
    pool.addUnchecked(tx6.GetHash(), entry.Fee(0LL).FromTx(tx6));
    BOOST_CHECK_EQUAL(pool.size(), 6);
    // Check that at this point, tx6 is sorted low
    sortedOrder.insert(sortedOrder.begin(), tx6.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    CTxMemPool::setEntries setAncestors;
    setAncestors.insert(pool.mapTx.find(tx6.GetHash()));
    CMutableTransaction mtx7 = CMutableTransaction();
    mtx7.vin.resize(1);
    mtx7.vin[0].prevout = COutPoint(tx6.GetHash(), 0);
    mtx7.vin[0].scriptSig = CScript() << OP_11;
    mtx7.vout.resize(2);
    mtx7.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx7.vout[0].nValue = 10 * COIN;
    mtx7.vout[1].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx7.vout[1].nValue = 1 * COIN;
    CTransaction tx7(mtx7);

    CTxMemPool::setEntries setAncestorsCalculated;
    std::string dummy;
    BOOST_CHECK_EQUAL(pool.CalculateMemPoolAncestors(entry.Fee(2000000LL).FromTx(tx7), setAncestorsCalculated, 100, 1000000, 1000, 1000000, dummy), true);
    BOOST_CHECK(setAncestorsCalculated == setAncestors);

    pool.addUnchecked(tx7.GetHash(), entry.FromTx(tx7), setAncestors);
    BOOST_CHECK_EQUAL(pool.size(), 7);

    // Now tx6 should be sorted higher (high fee child): tx7, tx6, tx2, ...
    sortedOrder.erase(sortedOrder.begin());
    sortedOrder.push_back(tx6.GetHash().ToString());
    sortedOrder.push_back(tx7.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    /* low fee child of tx7 */
    CMutableTransaction mtx8 = CMutableTransaction();
    mtx8.vin.resize(1);
    mtx8.vin[0].prevout = COutPoint(tx7.GetHash(), 0);
    mtx8.vin[0].scriptSig = CScript() << OP_11;
    mtx8.vout.resize(1);
    mtx8.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx8.vout[0].nValue = 10 * COIN;
    CTransaction tx8(mtx8);
    setAncestors.insert(pool.mapTx.find(tx7.GetHash()));
    pool.addUnchecked(tx8.GetHash(), entry.Fee(0LL).Time(2).FromTx(tx8), setAncestors);

    // Now tx8 should be sorted low, but tx6/tx both high
    sortedOrder.insert(sortedOrder.begin(), tx8.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    /* low fee child of tx7 */
    CMutableTransaction mtx9 = CMutableTransaction();
    mtx9.vin.resize(1);
    mtx9.vin[0].prevout = COutPoint(tx7.GetHash(), 1);
    mtx9.vin[0].scriptSig = CScript() << OP_11;
    mtx9.vout.resize(1);
    mtx9.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx9.vout[0].nValue = 1 * COIN;
    CTransaction tx9(mtx9);
    pool.addUnchecked(tx9.GetHash(), entry.Fee(0LL).Time(3).FromTx(tx9), setAncestors);

    // tx9 should be sorted low
    BOOST_CHECK_EQUAL(pool.size(), 9);
    sortedOrder.insert(sortedOrder.begin(), tx9.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    std::vector<std::string> snapshotOrder = sortedOrder;

    setAncestors.insert(pool.mapTx.find(tx8.GetHash()));
    setAncestors.insert(pool.mapTx.find(tx9.GetHash()));
    /* tx10 depends on tx8 and tx9 and has a high fee*/
    CMutableTransaction mtx10 = CMutableTransaction();
    mtx10.vin.resize(2);
    mtx10.vin[0].prevout = COutPoint(tx8.GetHash(), 0);
    mtx10.vin[0].scriptSig = CScript() << OP_11;
    mtx10.vin[1].prevout = COutPoint(tx9.GetHash(), 0);
    mtx10.vin[1].scriptSig = CScript() << OP_11;
    mtx10.vout.resize(1);
    mtx10.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    mtx10.vout[0].nValue = 10 * COIN;
    CTransaction tx10(mtx10);

    setAncestorsCalculated.clear();
    BOOST_CHECK_EQUAL(pool.CalculateMemPoolAncestors(entry.Fee(200000LL).Time(4).FromTx(tx10), setAncestorsCalculated, 100, 1000000, 1000, 1000000, dummy), true);
    BOOST_CHECK(setAncestorsCalculated == setAncestors);

    pool.addUnchecked(tx10.GetHash(), entry.FromTx(tx10), setAncestors);

    /**
     *  tx8 and tx9 should both now be sorted higher
     *  Final order after tx10 is added:
     *
     *  tx3 = 0 (1)
     *  tx5 = 10000 (1)
     *  tx1 = 10000 (1)
     *  tx4 = 15000 (1)
     *  tx2 = 20000 (1)
     *  tx9 = 200k (2 txs)
     *  tx8 = 200k (2 txs)
     *  tx10 = 200k (1 tx)
     *  tx6 = 2.2M (5 txs)
     *  tx7 = 2.2M (4 txs)
     */
    sortedOrder.erase(sortedOrder.begin(), sortedOrder.begin()+2); // take out tx9, tx8 from the beginning
    sortedOrder.insert(sortedOrder.begin()+5, tx9.GetHash().ToString());
    sortedOrder.insert(sortedOrder.begin()+6, tx8.GetHash().ToString());
    sortedOrder.insert(sortedOrder.begin()+7, tx10.GetHash().ToString()); // tx10 is just before tx6
    CheckSort<descendant_score>(pool, sortedOrder);

    // there should be 10 transactions in the mempool
    BOOST_CHECK_EQUAL(pool.size(), 10);

    // Now try removing tx10 and verify the sort order returns to normal
    std::list<CTransaction> removed;
    pool.remove(pool.mapTx.find(tx10.GetHash())->GetTx(), removed, true);
    CheckSort<descendant_score>(pool, snapshotOrder);

    pool.remove(pool.mapTx.find(tx9.GetHash())->GetTx(), removed, true);
    pool.remove(pool.mapTx.find(tx8.GetHash())->GetTx(), removed, true);
    /* Now check the sort on the mining score index.
     * Final order should be:
     *
     * tx7 (2M)
     * tx2 (20k)
     * tx4 (15000)
     * tx1/tx5 (10000)
     * tx3/6 (0)
     * (Ties resolved by hash)
     */
    sortedOrder.clear();
    sortedOrder.push_back(tx7.GetHash().ToString());
    sortedOrder.push_back(tx2.GetHash().ToString());
    sortedOrder.push_back(tx4.GetHash().ToString());
    if (tx1.GetHash() < tx5.GetHash()) {
        sortedOrder.push_back(tx5.GetHash().ToString());
        sortedOrder.push_back(tx1.GetHash().ToString());
    } else {
        sortedOrder.push_back(tx1.GetHash().ToString());
        sortedOrder.push_back(tx5.GetHash().ToString());
    }
    if (tx3.GetHash() < tx6.GetHash()) {
        sortedOrder.push_back(tx6.GetHash().ToString());
        sortedOrder.push_back(tx3.GetHash().ToString());
    } else {
        sortedOrder.push_back(tx3.GetHash().ToString());
        sortedOrder.push_back(tx6.GetHash().ToString());
    }
    CheckSort<mining_score>(pool, sortedOrder);
}

BOOST_AUTO_TEST_CASE(RemoveWithoutBranchId) {
    CTxMemPool pool(CFeeRate(0));
    TestMemPoolEntryHelper entry;
    entry.nFee = 10000LL;
    entry.hadNoDependencies = true;

    // Add some Sprout transactions
    for (auto i = 1; i < 11; i++) {
        CMutableTransaction mtx = CMutableTransaction();
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        mtx.vout[0].nValue = i * COIN;

        CTransaction tx(mtx);
        pool.addUnchecked(tx.GetHash(), entry.BranchId(NetworkUpgradeInfo[Consensus::BASE_SPROUT].nBranchId).FromTx(tx));
    }
    BOOST_CHECK_EQUAL(pool.size(), 10);

    // Check the pool only contains Sprout transactions
    for (CTxMemPool::indexed_transaction_set::const_iterator it = pool.mapTx.begin(); it != pool.mapTx.end(); it++) {
        BOOST_CHECK_EQUAL(it->GetValidatedBranchId(), NetworkUpgradeInfo[Consensus::BASE_SPROUT].nBranchId);
    }

    // Add some dummy transactions
    for (auto i = 1; i < 11; i++) {
        CMutableTransaction mtx = CMutableTransaction();
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        mtx.vout[0].nValue = i * COIN + 100;

        CTransaction tx(mtx);
        pool.addUnchecked(tx.GetHash(), entry.BranchId(NetworkUpgradeInfo[Consensus::UPGRADE_TESTDUMMY].nBranchId).FromTx(tx));
    }
    BOOST_CHECK_EQUAL(pool.size(), 20);

    // Add some Overwinter transactions
    for (auto i = 1; i < 11; i++) {
        CMutableTransaction mtx = CMutableTransaction();
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        mtx.vout[0].nValue = i * COIN + 200;

        CTransaction tx(mtx);
        pool.addUnchecked(tx.GetHash(), entry.BranchId(NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId).FromTx(tx));
    }
    BOOST_CHECK_EQUAL(pool.size(), 30);

    // Remove transactions that are not for Overwinter
    pool.removeWithoutBranchId(NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId);
    BOOST_CHECK_EQUAL(pool.size(), 10);

    // Check the pool only contains Overwinter transactions
    for (CTxMemPool::indexed_transaction_set::const_iterator it = pool.mapTx.begin(); it != pool.mapTx.end(); it++) {
        BOOST_CHECK_EQUAL(it->GetValidatedBranchId(), NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId);
    }

    // Roll back to Sprout
    pool.removeWithoutBranchId(NetworkUpgradeInfo[Consensus::BASE_SPROUT].nBranchId);
    BOOST_CHECK_EQUAL(pool.size(), 0);
}

// Test that nCheckFrequency is set correctly when calling setSanityCheck().
// https://github.com/zcash/zcash/issues/3134
BOOST_AUTO_TEST_CASE(SetSanityCheck) {
    CTxMemPool pool(CFeeRate(0));
    pool.setSanityCheck(1.0);
    BOOST_CHECK_EQUAL(pool.GetCheckFrequency(), 4294967295);
    pool.setSanityCheck(0);
    BOOST_CHECK_EQUAL(pool.GetCheckFrequency(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
