// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php 

#include <gtest/gtest.h>
#include <iostream>

#include "arith_uint256.h"
#include "mempoollimit.h"
#include "utiltime.h"
#include "utiltest.h"
#include "transaction_builder.h"


const uint256 TX_ID1 = ArithToUint256(1);
const uint256 TX_ID2 = ArithToUint256(2);
const uint256 TX_ID3 = ArithToUint256(3);

TEST(MempoolLimitTests, RecentlyEvictedList_AddWrapsAfterMaxSize)
{
    RecentlyEvictedList recentlyEvicted(2, 100);
    SetMockTime(1);
    recentlyEvicted.add(TX_ID1);
    recentlyEvicted.add(TX_ID2);
    recentlyEvicted.add(TX_ID3);
    // tx 1 should be overwritten by tx 3 due maxSize 2
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID1));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID2));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID3));
}

TEST(MempoolLimitTests, RecentlyEvictedList_DoesNotContainAfterExpiry)
{
    SetMockTime(1);
    // maxSize=3, timeToKeep=1
    RecentlyEvictedList recentlyEvicted(3, 1);
    recentlyEvicted.add(TX_ID1);
    SetMockTime(2);
    recentlyEvicted.add(TX_ID2);
    recentlyEvicted.add(TX_ID3);
    // After 1 second the txId will still be there
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID1));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID2));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID3));
    SetMockTime(3);
    // After 2 second it is gone
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID1));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID2));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID3));
    SetMockTime(4);
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID2));
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID3));
}

TEST(MempoolLimitTests, WeightedTransactionList_CheckSizeAfterDropping)
{
    std::set<uint256> testedDropping;
    // Run the test until we have tested dropping each of the elements
    int trialNum = 0;
    while (testedDropping.size() < 3) {
        WeightedTransactionList list(MIN_TX_COST * 2);
        EXPECT_EQ(0, list.getTotalCost());
        EXPECT_EQ(0, list.getTotalLowFeePenaltyCost());
        list.add(WeightedTxInfo(TX_ID1, MIN_TX_COST, MIN_TX_COST));
        EXPECT_EQ(4000, list.getTotalCost());
        EXPECT_EQ(4000, list.getTotalLowFeePenaltyCost());
        list.add(WeightedTxInfo(TX_ID2, MIN_TX_COST, MIN_TX_COST));
        EXPECT_EQ(8000, list.getTotalCost());
        EXPECT_EQ(8000, list.getTotalLowFeePenaltyCost());
        EXPECT_FALSE(list.maybeDropRandom().is_initialized());
        list.add(WeightedTxInfo(TX_ID3, MIN_TX_COST, MIN_TX_COST + LOW_FEE_PENALTY));
        EXPECT_EQ(12000, list.getTotalCost());
        EXPECT_EQ(12000 + LOW_FEE_PENALTY, list.getTotalLowFeePenaltyCost());
        boost::optional<WeightedTxInfo> drop = list.maybeDropRandom();
        ASSERT_TRUE(drop.is_initialized());
        uint256 txid = drop.get().txId;
        std::cerr << "Trial " << trialNum++ << ": dropped " << txid.ToString() << std::endl;
        testedDropping.insert(txid);
        // Do not continue to test if a particular trial fails
        ASSERT_EQ(8000, list.getTotalCost());
        ASSERT_EQ(txid == TX_ID3 ? 8000 : 8000 + LOW_FEE_PENALTY, list.getTotalLowFeePenaltyCost());
    }
    std::cerr << "All 3 scenarios tested in " << trialNum << " trials" << std::endl;
}

TEST(MempoolLimitTests, WeightedTXInfo_FromTx)
{
    // The transaction creation is based on the test:
    // test_transaction_builder.cpp/TEST(TransactionBuilder, SetFee)
    auto consensusParams = RegtestActivateSapling();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto fvk = sk.full_viewing_key();
    auto pa = sk.default_address();

    auto testNote = GetTestSaplingNote(pa, 50000);

    // Default fee
    {
        auto builder = TransactionBuilder(consensusParams, 1);
        builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
        builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
        
        WeightedTxInfo info = WeightedTxInfo::from(builder.Build().GetTxOrThrow());
        EXPECT_EQ(MIN_TX_COST, info.cost);
        EXPECT_EQ(MIN_TX_COST, info.lowFeePenaltyCost);
    }
    
    // Lower than standard fee
    {
        auto builder = TransactionBuilder(consensusParams, 1);
        builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
        builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
        builder.SetFee(1);

        WeightedTxInfo info = WeightedTxInfo::from(builder.Build().GetTxOrThrow());
        EXPECT_EQ(MIN_TX_COST, info.cost);
        EXPECT_EQ(MIN_TX_COST + LOW_FEE_PENALTY, info.lowFeePenaltyCost);
    }

    // Larger Tx
    {
        auto testNote2 = GetTestSaplingNote(pa, 50000);
        auto testNote3 = GetTestSaplingNote(pa, 50000);
        auto testNote4 = GetTestSaplingNote(pa, 50000);
        auto builder = TransactionBuilder(consensusParams, 1);
        builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
        builder.AddSaplingSpend(expsk, testNote2.note, testNote2.tree.root(), testNote2.tree.witness());
        builder.AddSaplingSpend(expsk, testNote3.note, testNote3.tree.root(), testNote3.tree.witness());
        builder.AddSaplingSpend(expsk, testNote4.note, testNote4.tree.root(), testNote4.tree.witness());
        builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
        
        WeightedTxInfo info = WeightedTxInfo::from(builder.Build().GetTxOrThrow());
        EXPECT_EQ(MIN_TX_COST, info.cost);
        EXPECT_EQ(MIN_TX_COST, info.lowFeePenaltyCost);
    }
    
    RegtestDeactivateSapling();
}
