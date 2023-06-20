// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <gtest/gtest.h>
#include <iostream>

#include "arith_uint256.h"
#include "mempool_limit.h"
#include "gtest/utils.h"
#include "util/time.h"
#include "util/test.h"
#include "transaction_builder.h"
#include "zip317.h"
#include "core_memusage.h"


const uint256 TX_ID1 = ArithToUint256(1);
const uint256 TX_ID2 = ArithToUint256(2);
const uint256 TX_ID3 = ArithToUint256(3);

TEST(MempoolLimitTests, RecentlyEvictedListAddWrapsAfterMaxSize)
{
    FixedClock clock(std::chrono::seconds(1));
    RecentlyEvictedList recentlyEvicted(&clock, 2, 100);
    recentlyEvicted.add(TX_ID1);
    recentlyEvicted.add(TX_ID2);
    recentlyEvicted.add(TX_ID3);
    // tx 1 should be overwritten by tx 3 due to maxSize 2
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID1));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID2));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID3));
}

TEST(MempoolLimitTests, RecentlyEvictedListDoesNotContainAfterExpiry)
{
    FixedClock clock(std::chrono::seconds(1));
    // maxSize=3, timeToKeep=1
    RecentlyEvictedList recentlyEvicted(&clock, 3, 1);
    recentlyEvicted.add(TX_ID1);
    clock.Set(std::chrono::seconds(2));
    recentlyEvicted.add(TX_ID2);
    recentlyEvicted.add(TX_ID3);
    // After 1 second the txId will still be there
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID1));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID2));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID3));
    clock.Set(std::chrono::seconds(3));
    // After 2 seconds it is gone
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID1));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID2));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID3));
    clock.Set(std::chrono::seconds(4));
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID1));
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID2));
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID3));
}

TEST(MempoolLimitTests, RecentlyEvictedDropOneAtATime)
{
    FixedClock clock(std::chrono::seconds(1));
    RecentlyEvictedList recentlyEvicted(&clock, 3, 2);
    recentlyEvicted.add(TX_ID1);
    clock.Set(std::chrono::seconds(2));
    recentlyEvicted.add(TX_ID2);
    clock.Set(std::chrono::seconds(3));
    recentlyEvicted.add(TX_ID3);
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID1));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID2));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID3));
    clock.Set(std::chrono::seconds(4));
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID1));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID2));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID3));
    clock.Set(std::chrono::seconds(5));
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID1));
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID2));
    EXPECT_TRUE(recentlyEvicted.contains(TX_ID3));
    clock.Set(std::chrono::seconds(6));
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID1));
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID2));
    EXPECT_FALSE(recentlyEvicted.contains(TX_ID3));
}

TEST(MempoolLimitTests, MempoolLimitTxSetCheckSizeAfterDropping)
{
    std::set<uint256> testedDropping;
    // Run the test until we have tested dropping each of the elements
    int trialNum = 0;
    while (testedDropping.size() < 3) {
        MempoolLimitTxSet limitSet(MIN_TX_COST * 2);
        EXPECT_EQ(0, limitSet.getTotalWeight());
        limitSet.add(TX_ID1, MIN_TX_COST, MIN_TX_COST);
        EXPECT_EQ(10000, limitSet.getTotalWeight());
        limitSet.add(TX_ID2, MIN_TX_COST, MIN_TX_COST);
        EXPECT_EQ(20000, limitSet.getTotalWeight());
        EXPECT_FALSE(limitSet.maybeDropRandom().has_value());
        limitSet.add(TX_ID3, MIN_TX_COST, MIN_TX_COST + LOW_FEE_PENALTY);
        EXPECT_EQ(30000 + LOW_FEE_PENALTY, limitSet.getTotalWeight());
        std::optional<uint256> drop = limitSet.maybeDropRandom();
        ASSERT_TRUE(drop.has_value());
        uint256 txid = drop.value();
        testedDropping.insert(txid);
        // Do not continue to test if a particular trial fails
        ASSERT_EQ(txid == TX_ID3 ? 20000 : 20000 + LOW_FEE_PENALTY, limitSet.getTotalWeight());
    }
}

TEST(MempoolLimitTests, MempoolCostAndEvictionWeight)
{
    LoadProofParameters();

    // The transaction creation is based on the test:
    // test_transaction_builder.cpp/TEST(TransactionBuilder, SetFee)
    auto consensusParams = RegtestActivateSapling();

    auto sk = GetTestMasterSaplingSpendingKey();
    auto extfvk = sk.ToXFVK();
    auto fvk = extfvk.fvk;
    auto pa = extfvk.DefaultAddress();

    CAmount funds = 66000;
    auto testNote = GetTestSaplingNote(pa, funds);

    // Default fee
    {
        auto builder = TransactionBuilder(Params(), 1, std::nullopt);
        builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
        builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});

        auto [cost, evictionWeight] = MempoolCostAndEvictionWeight(builder.Build().GetTxOrThrow(), MINIMUM_FEE);
        EXPECT_EQ(MIN_TX_COST, cost);
        EXPECT_EQ(MIN_TX_COST, evictionWeight);
    }

    // Lower than standard fee
    {
        auto builder = TransactionBuilder(Params(), 1, std::nullopt);
        builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
        builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
        static_assert(MINIMUM_FEE == 10000);
        builder.SetFee(MINIMUM_FEE-1);

        auto [cost, evictionWeight] = MempoolCostAndEvictionWeight(builder.Build().GetTxOrThrow(), MINIMUM_FEE-1);
        EXPECT_EQ(MIN_TX_COST, cost);
        EXPECT_EQ(MIN_TX_COST + LOW_FEE_PENALTY, evictionWeight);
    }

    // Larger Tx
    {
        auto builder = TransactionBuilder(Params(), 1, std::nullopt);
        builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
        for (int i = 0; i < 10; i++) {
            builder.AddSaplingOutput(fvk.ovk, pa, 1000, {});
        }

        auto result = builder.Build();
        if (result.IsError()) {
            std::cerr << result.GetError() << std::endl;
        }
        // max(1 input, 10 outputs + 1 change output) => 11 logical actions.
        CAmount zip317_fee = CalculateConventionalFee(11);
        ASSERT_GT(funds, 1000*10 + zip317_fee);
        const CTransaction tx {result.GetTxOrThrow()};
        EXPECT_EQ(11, tx.GetLogicalActionCount());

        // For the test to be valid, we want the memory usage of this transaction to be more than MIN_TX_COST.
        // Avoid hard-coding the usage because it might be platform-dependent.
        ASSERT_GT(GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION), MIN_TX_COST);
        size_t tx_usage = RecursiveDynamicUsage(tx);
        EXPECT_GT(tx_usage, MIN_TX_COST);

        auto [cost, evictionWeight] = MempoolCostAndEvictionWeight(tx, zip317_fee);
        EXPECT_EQ(tx_usage, cost);
        EXPECT_EQ(tx_usage, evictionWeight);

        // If we pay less than the conventional fee for 11 actions, we should incur a low fee penalty.
        auto [cost2, evictionWeight2] = MempoolCostAndEvictionWeight(tx, zip317_fee-1);
        EXPECT_EQ(tx_usage, cost2);
        EXPECT_EQ(tx_usage + LOW_FEE_PENALTY, evictionWeight2);
    }

    RegtestDeactivateSapling();
}
