// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <gtest/gtest.h>
#include <iostream>

#include "coins.h"
#include "init.h"
#include "key.h"
#include "transaction_builder.h"
#include "gtest/utils.h"
#include "util/test.h"

#include <optional>

TEST(RecursiveDynamicUsageTests, TestTransactionTransparent)
{
    auto consensusParams = RegtestActivateSapling();

    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());
    CTxDestination taddr = tsk.GetPubKey().GetID();

    auto builder = TransactionBuilder(Params(), 1, std::nullopt, SaplingMerkleTree::empty_root(), &keystore);
    builder.SetFee(10000);
    builder.AddTransparentInput(
        COutPoint(uint256S("7777777777777777777777777777777777777777777777777777777777777777"), 0),
        scriptPubKey, 50000);
    builder.AddTransparentOutput(taddr, 40000);

    auto tx = builder.Build().GetTxOrThrow();
    // 1 vin + 1 vout
    EXPECT_EQ((96 + 128) + 64, RecursiveDynamicUsage(tx));

    RegtestDeactivateSapling();
}

TEST(RecursiveDynamicUsageTests, TestTransactionJoinSplit)
{
    auto consensusParams = RegtestActivateSapling();

    auto sproutSk = libzcash::SproutSpendingKey::random();

    auto wtx = GetValidSproutReceive(sproutSk, 25000, true);
    // 2 vin + 1 vJoinSplit + 1 vShieldedOutput
    EXPECT_EQ(0, wtx.GetSaplingSpendsCount());
    EXPECT_EQ(1, wtx.GetSaplingOutputsCount());
    EXPECT_EQ(160 + 1856 + 1200, RecursiveDynamicUsage(wtx));

    RegtestDeactivateSapling();
}

TEST(RecursiveDynamicUsageTests, TestTransactionSaplingToSapling)
{
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();

    auto sk = GetTestMasterSaplingSpendingKey();
    auto extfvk = sk.ToXFVK();
    auto fvk = extfvk.fvk;
    auto pa = extfvk.DefaultAddress();
    auto testNote = GetTestSaplingNote(pa, 50000);

    auto builder = TransactionBuilder(Params(), 1, std::nullopt, testNote.tree.root());
    builder.SetFee(10000);
    builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
    builder.AddSaplingOutput(fvk.ovk, pa, 5000, {});

    auto tx = builder.Build().GetTxOrThrow();
    // 1 vShieldedSpend + 2 vShieldedOutput
    EXPECT_EQ(1, tx.GetSaplingSpendsCount());
    EXPECT_EQ(2, tx.GetSaplingOutputsCount());
    EXPECT_EQ(400 + 32 + 2520, RecursiveDynamicUsage(tx));

    RegtestDeactivateSapling();
}

TEST(RecursiveDynamicUsageTests, TestTransactionTransparentToSapling)
{
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();

    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());
    auto sk = libzcash::SaplingSpendingKey::random();

    auto builder = TransactionBuilder(Params(), 1, std::nullopt, SaplingMerkleTree::empty_root(), &keystore);
    builder.SetFee(10000);
    builder.AddTransparentInput(
        COutPoint(uint256S("7777777777777777777777777777777777777777777777777777777777777777"), 0),
        scriptPubKey, 50000);
    builder.AddSaplingOutput(sk.full_viewing_key().ovk, sk.default_address(), 40000, {});

    auto tx = builder.Build().GetTxOrThrow();
    // 1 vin + 2 vShieldedOutput
    EXPECT_EQ(0, tx.GetSaplingSpendsCount());
    EXPECT_EQ(2, tx.GetSaplingOutputsCount());
    EXPECT_EQ((96 + 128) + 2280, RecursiveDynamicUsage(tx));

    RegtestDeactivateSapling();
}

TEST(RecursiveDynamicUsageTests, TestTransactionSaplingToTransparent)
{
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();

    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    CTxDestination taddr = tsk.GetPubKey().GetID();
    auto sk = GetTestMasterSaplingSpendingKey();
    auto testNote = GetTestSaplingNote(sk.ToXFVK().DefaultAddress(), 50000);

    auto builder = TransactionBuilder(Params(), 1, std::nullopt, testNote.tree.root(), &keystore);
    builder.SetFee(10000);
    builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
    builder.AddTransparentOutput(taddr, 40000);

    auto tx = builder.Build().GetTxOrThrow();
    // 1 vShieldedSpend + 2 vShieldedOutput + 1 vout
    EXPECT_EQ(1, tx.GetSaplingSpendsCount());
    EXPECT_EQ(2, tx.GetSaplingOutputsCount());
    EXPECT_EQ(400 + 2520 + 64 + 32, RecursiveDynamicUsage(tx));

    RegtestDeactivateSapling();
}
