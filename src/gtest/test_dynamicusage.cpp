// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php 

#include <gtest/gtest.h>
#include <iostream>

#include "coins.h"
#include "init.h"
#include "key.h"
#include "transaction_builder.h"
#include "utiltest.h"

extern ZCJoinSplit* params;

TEST(RecursiveDynamicUsageTests, TestTransactionTransparent)
{
    auto consensusParams = RegtestActivateSapling();

    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());
    CTxDestination taddr = tsk.GetPubKey().GetID();
    
    auto builder = TransactionBuilder(consensusParams, 1, &keystore);
    builder.AddTransparentInput(COutPoint(), scriptPubKey, 50000);
    builder.AddTransparentOutput(taddr, 40000);

    auto tx = builder.Build().GetTxOrThrow();
    EXPECT_EQ(288, RecursiveDynamicUsage(tx));

    RegtestDeactivateSapling();
}

TEST(RecursiveDynamicUsageTests, TestTransactionJoinSplit)
{
    auto consensusParams = RegtestActivateSapling();

    auto sproutSk = libzcash::SproutSpendingKey::random();

    auto wtx = GetValidSproutReceive(*params, sproutSk, 25000, true);
    EXPECT_EQ(2806, RecursiveDynamicUsage(wtx));

    RegtestDeactivateSapling();
}

TEST(RecursiveDynamicUsageTests, TestTransactionSaplingToSapling)
{
    auto consensusParams = RegtestActivateSapling();
    
    auto sk = libzcash::SaplingSpendingKey::random();
    auto testNote = GetTestSaplingNote(sk.default_address(), 50000);

    auto builder = TransactionBuilder(consensusParams, 1);
    builder.AddSaplingSpend(sk.expanded_spending_key(), testNote.note, testNote.tree.root(), testNote.tree.witness());
    builder.AddSaplingOutput(sk.full_viewing_key().ovk, sk.default_address(), 5000, {});
    
    auto tx = builder.Build().GetTxOrThrow();
    EXPECT_EQ(2280, RecursiveDynamicUsage(tx));

    RegtestDeactivateSapling();
}

TEST(RecursiveDynamicUsageTests, TestTransactionTransparentToSapling)
{
    auto consensusParams = RegtestActivateSapling();
    
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());
    auto sk = libzcash::SaplingSpendingKey::random();

    auto builder = TransactionBuilder(consensusParams, 1, &keystore);
    builder.AddTransparentInput(COutPoint(), scriptPubKey, 50000);
    builder.AddSaplingOutput(sk.full_viewing_key().ovk, sk.default_address(), 40000, {});
    
    auto tx = builder.Build().GetTxOrThrow();
    EXPECT_EQ(1172, RecursiveDynamicUsage(tx));

    RegtestDeactivateSapling();
}

TEST(RecursiveDynamicUsageTests, TestTransactionSaplingToTransparent)
{
    auto consensusParams = RegtestActivateSapling();
    
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    CTxDestination taddr = tsk.GetPubKey().GetID();
    auto sk = libzcash::SaplingSpendingKey::random();
    auto testNote = GetTestSaplingNote(sk.default_address(), 50000);

    auto builder = TransactionBuilder(consensusParams, 1, &keystore);
    builder.AddSaplingSpend(sk.expanded_spending_key(), testNote.note, testNote.tree.root(), testNote.tree.witness());
    builder.AddTransparentOutput(taddr, 40000);

    auto tx = builder.Build().GetTxOrThrow();
    EXPECT_EQ(448, RecursiveDynamicUsage(tx));

    RegtestDeactivateSapling();
}
