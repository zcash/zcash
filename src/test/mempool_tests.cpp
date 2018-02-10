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
    testPool.addUnchecked(txParent.GetHash(), CTxMemPoolEntry(txParent, 0, 0, 0.0, 1));
    testPool.remove(txParent, removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 1);
    removed.clear();
    
    // Parent, children, grandchildren:
    testPool.addUnchecked(txParent.GetHash(), CTxMemPoolEntry(txParent, 0, 0, 0.0, 1));
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(txChild[i].GetHash(), CTxMemPoolEntry(txChild[i], 0, 0, 0.0, 1));
        testPool.addUnchecked(txGrandChild[i].GetHash(), CTxMemPoolEntry(txGrandChild[i], 0, 0, 0.0, 1));
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
        testPool.addUnchecked(txChild[i].GetHash(), CTxMemPoolEntry(txChild[i], 0, 0, 0.0, 1));
        testPool.addUnchecked(txGrandChild[i].GetHash(), CTxMemPoolEntry(txGrandChild[i], 0, 0, 0.0, 1));
    }
    // Now remove the parent, as might happen if a block-re-org occurs but the parent cannot be
    // put into the mempool (maybe because it is non-standard):
    testPool.remove(txParent, removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 6);
    BOOST_CHECK_EQUAL(testPool.size(), 0);
    removed.clear();
}

BOOST_AUTO_TEST_CASE(RemoveWithoutBranchId) {
    CTxMemPool pool(CFeeRate(0));

    // Add some Sprout transactions
    for (auto i = 1; i < 11; i++) {
        CMutableTransaction tx = CMutableTransaction();
        tx.vout.resize(1);
        tx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        tx.vout[0].nValue = i * COIN;
        pool.addUnchecked(tx.GetHash(), CTxMemPoolEntry(tx, 0, 0, 0.0, 1, true, NetworkUpgradeInfo[Consensus::BASE_SPROUT].nBranchId));
    }
    BOOST_CHECK_EQUAL(pool.size(), 10);

    // Check the pool only contains Sprout transactions
    for (std::map<uint256, CTxMemPoolEntry>::const_iterator it = pool.mapTx.begin(); it != pool.mapTx.end(); it++) {
        BOOST_CHECK_EQUAL(it->second.GetBranchId(), NetworkUpgradeInfo[Consensus::BASE_SPROUT].nBranchId);
    }

    // Add some dummy transactions
    for (auto i = 1; i < 11; i++) {
        CMutableTransaction tx = CMutableTransaction();
        tx.vout.resize(1);
        tx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        tx.vout[0].nValue = i * COIN + 100;
        pool.addUnchecked(tx.GetHash(), CTxMemPoolEntry(tx, 0, 0, 0.0, 1, true, NetworkUpgradeInfo[Consensus::UPGRADE_TESTDUMMY].nBranchId));
    }
    BOOST_CHECK_EQUAL(pool.size(), 20);

    // Add some Overwinter transactions
    for (auto i = 1; i < 11; i++) {
        CMutableTransaction tx = CMutableTransaction();
        tx.vout.resize(1);
        tx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        tx.vout[0].nValue = i * COIN + 200;
        pool.addUnchecked(tx.GetHash(), CTxMemPoolEntry(tx, 0, 0, 0.0, 1, true, NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId));
    }
    BOOST_CHECK_EQUAL(pool.size(), 30);

    // Remove transactions that are not for Overwinter
    pool.removeWithoutBranchId(NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId);
    BOOST_CHECK_EQUAL(pool.size(), 10);

    // Check the pool only contains Overwinter transactions
    for (std::map<uint256, CTxMemPoolEntry>::const_iterator it = pool.mapTx.begin(); it != pool.mapTx.end(); it++) {
        BOOST_CHECK_EQUAL(it->second.GetBranchId(), NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId);
    }

    // Roll back to Sprout
    pool.removeWithoutBranchId(NetworkUpgradeInfo[Consensus::BASE_SPROUT].nBranchId);
    BOOST_CHECK_EQUAL(pool.size(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
