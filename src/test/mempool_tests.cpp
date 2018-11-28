// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/upgrades.h"
#include "main.h"
#include "txmempool.h"
#include "util.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>
#include <list>

BOOST_FIXTURE_TEST_SUITE(mempool_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(MempoolRemoveTest)
{
    // Test CTxMemPool::remove functionality

    TestMemPoolEntryHelper entry;
    // Parent transaction with three children,
    // and three grand-children:
    CMutableTransaction txParent;
    txParent.vin.resize(1);
    txParent.vin[0].scriptSig = CScript() << OP_11;
    txParent.vout.resize(3);
    for (int i = 0; i < 3; i++)
    {
        txParent.vout[i].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txParent.vout[i].nValue = 33000LL;
    }
    CMutableTransaction txChild[3];
    for (int i = 0; i < 3; i++)
    {
        txChild[i].vin.resize(1);
        txChild[i].vin[0].scriptSig = CScript() << OP_11;
        txChild[i].vin[0].prevout.hash = txParent.GetHash();
        txChild[i].vin[0].prevout.n = i;
        txChild[i].vout.resize(1);
        txChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txChild[i].vout[0].nValue = 11000LL;
    }
    CMutableTransaction txGrandChild[3];
    for (int i = 0; i < 3; i++)
    {
        txGrandChild[i].vin.resize(1);
        txGrandChild[i].vin[0].scriptSig = CScript() << OP_11;
        txGrandChild[i].vin[0].prevout.hash = txChild[i].GetHash();
        txGrandChild[i].vin[0].prevout.n = 0;
        txGrandChild[i].vout.resize(1);
        txGrandChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txGrandChild[i].vout[0].nValue = 11000LL;
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

BOOST_AUTO_TEST_CASE(MempoolIndexingTest)
{
    CTxMemPool pool(CFeeRate(0));
    TestMemPoolEntryHelper entry;
    entry.hadNoDependencies = true;

    /* 3rd highest fee */
    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx1.vout[0].nValue = 10 * COIN;
    pool.addUnchecked(tx1.GetHash(), entry.Fee(10000LL).Priority(10.0).FromTx(tx1));

    /* highest fee */
    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx2.vout[0].nValue = 2 * COIN;
    pool.addUnchecked(tx2.GetHash(), entry.Fee(20000LL).Priority(9.0).FromTx(tx2));

    /* lowest fee */
    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx3.vout[0].nValue = 5 * COIN;
    pool.addUnchecked(tx3.GetHash(), entry.Fee(0LL).Priority(100.0).FromTx(tx3));

    /* 2nd highest fee */
    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vout.resize(1);
    tx4.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx4.vout[0].nValue = 6 * COIN;
    pool.addUnchecked(tx4.GetHash(), entry.Fee(15000LL).Priority(1.0).FromTx(tx4));

    /* equal fee rate to tx1, but newer */
    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vout.resize(1);
    tx5.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx5.vout[0].nValue = 11 * COIN;
    entry.nTime = 1;
    entry.dPriority = 10.0;
    pool.addUnchecked(tx5.GetHash(), entry.Fee(10000LL).FromTx(tx5));
    BOOST_CHECK_EQUAL(pool.size(), 5);

    // Check the fee-rate index is in order, should be tx2, tx4, tx1, tx5, tx3
    CTxMemPool::indexed_transaction_set::nth_index<1>::type::iterator it = pool.mapTx.get<1>().begin();
    BOOST_CHECK_EQUAL(it++->GetTx().GetHash().ToString(), tx2.GetHash().ToString());
    BOOST_CHECK_EQUAL(it++->GetTx().GetHash().ToString(), tx4.GetHash().ToString());
    BOOST_CHECK_EQUAL(it++->GetTx().GetHash().ToString(), tx1.GetHash().ToString());
    BOOST_CHECK_EQUAL(it++->GetTx().GetHash().ToString(), tx5.GetHash().ToString());
    BOOST_CHECK_EQUAL(it++->GetTx().GetHash().ToString(), tx3.GetHash().ToString());
    BOOST_CHECK(it == pool.mapTx.get<1>().end());
}

BOOST_AUTO_TEST_CASE(RemoveWithoutBranchId) {
    CTxMemPool pool(CFeeRate(0));
    TestMemPoolEntryHelper entry;
    entry.nFee = 10000LL;
    entry.hadNoDependencies = true;

    // Add some Sprout transactions
    for (auto i = 1; i < 11; i++) {
        CMutableTransaction tx = CMutableTransaction();
        tx.vout.resize(1);
        tx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        tx.vout[0].nValue = i * COIN;
        pool.addUnchecked(tx.GetHash(), entry.BranchId(NetworkUpgradeInfo[Consensus::BASE_SPROUT].nBranchId).FromTx(tx));
    }
    BOOST_CHECK_EQUAL(pool.size(), 10);

    // Check the pool only contains Sprout transactions
    for (CTxMemPool::indexed_transaction_set::const_iterator it = pool.mapTx.begin(); it != pool.mapTx.end(); it++) {
        BOOST_CHECK_EQUAL(it->GetValidatedBranchId(), NetworkUpgradeInfo[Consensus::BASE_SPROUT].nBranchId);
    }

    // Add some dummy transactions
    for (auto i = 1; i < 11; i++) {
        CMutableTransaction tx = CMutableTransaction();
        tx.vout.resize(1);
        tx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        tx.vout[0].nValue = i * COIN + 100;
        pool.addUnchecked(tx.GetHash(), entry.BranchId(NetworkUpgradeInfo[Consensus::UPGRADE_TESTDUMMY].nBranchId).FromTx(tx));
    }
    BOOST_CHECK_EQUAL(pool.size(), 20);

    // Add some Overwinter transactions
    for (auto i = 1; i < 11; i++) {
        CMutableTransaction tx = CMutableTransaction();
        tx.vout.resize(1);
        tx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        tx.vout[0].nValue = i * COIN + 200;
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
