#include "chainparams.h"
#include "consensus/params.h"
#include "consensus/validation.h"
#include "key_io.h"
#include "main.h"
#include "pubkey.h"
#include "transaction_builder.h"
#include "zcash/Address.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

static const std::string tSecretRegtest = "cND2ZvtabDbJ1gucx9GWH6XT9kgTAqfb6cotPt5Q5CyxVDhid2EN";

TEST(TransactionBuilder, Invoke)
{
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    auto consensusParams = Params().GetConsensus();

    CBasicKeyStore keystore;
    CKey tsk = DecodeSecret(tSecretRegtest);
    keystore.AddKey(tsk);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());

    auto sk_from = libzcash::SaplingSpendingKey::random();
    auto fvk_from = sk_from.full_viewing_key();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto fvk = sk.full_viewing_key();
    auto ivk = fvk.in_viewing_key();
    libzcash::diversifier_t d = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    auto pk = *ivk.address(d);

    // Create a shielding transaction from transparent to Sapling
    // 0.0005 t-ZEC in, 0.0004 z-ZEC out, 0.0001 t-ZEC fee
    auto builder1 = TransactionBuilder(consensusParams, 1, &keystore);
    builder1.AddTransparentInput(COutPoint(), scriptPubKey, 50000);
    builder1.AddSaplingOutput(fvk_from.ovk, pk, 40000, {});
    auto maybe_tx1 = builder1.Build();
    ASSERT_EQ(static_cast<bool>(maybe_tx1), true);
    auto tx1 = maybe_tx1.get();

    EXPECT_EQ(tx1.vin.size(), 1);
    EXPECT_EQ(tx1.vout.size(), 0);
    EXPECT_EQ(tx1.vjoinsplit.size(), 0);
    EXPECT_EQ(tx1.vShieldedSpend.size(), 0);
    EXPECT_EQ(tx1.vShieldedOutput.size(), 1);
    EXPECT_EQ(tx1.valueBalance, -40000);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(0,tx1, state, 2, 0));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Prepare to spend the note that was just created
    auto maybe_pt = libzcash::SaplingNotePlaintext::decrypt(
        tx1.vShieldedOutput[0].encCiphertext, ivk, tx1.vShieldedOutput[0].ephemeralKey, tx1.vShieldedOutput[0].cm);
    ASSERT_EQ(static_cast<bool>(maybe_pt), true);
    auto maybe_note = maybe_pt.get().note(ivk);
    ASSERT_EQ(static_cast<bool>(maybe_note), true);
    auto note = maybe_note.get();
    SaplingMerkleTree tree;
    tree.append(tx1.vShieldedOutput[0].cm);
    auto anchor = tree.root();
    auto witness = tree.witness();

    // Create a Sapling-only transaction
    // 0.0004 z-ZEC in, 0.00025 z-ZEC out, 0.0001 t-ZEC fee, 0.00005 z-ZEC change
    auto builder2 = TransactionBuilder(consensusParams, 2);
    ASSERT_TRUE(builder2.AddSaplingSpend(expsk, note, anchor, witness));
    // Check that trying to add a different anchor fails
    ASSERT_FALSE(builder2.AddSaplingSpend(expsk, note, uint256(), witness));

    builder2.AddSaplingOutput(fvk.ovk, pk, 25000, {});
    auto maybe_tx2 = builder2.Build();
    ASSERT_EQ(static_cast<bool>(maybe_tx2), true);
    auto tx2 = maybe_tx2.get();

    EXPECT_EQ(tx2.vin.size(), 0);
    EXPECT_EQ(tx2.vout.size(), 0);
    EXPECT_EQ(tx2.vjoinsplit.size(), 0);
    EXPECT_EQ(tx2.vShieldedSpend.size(), 1);
    EXPECT_EQ(tx2.vShieldedOutput.size(), 2);
    EXPECT_EQ(tx2.valueBalance, 10000);

    EXPECT_TRUE(ContextualCheckTransaction(0,tx2, state, 3, 0));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}

TEST(TransactionBuilder, ThrowsOnTransparentInputWithoutKeyStore)
{
    auto consensusParams = Params().GetConsensus();

    auto builder = TransactionBuilder(consensusParams, 1);
    ASSERT_THROW(builder.AddTransparentInput(COutPoint(), CScript(), 1), std::runtime_error);
}

TEST(TransactionBuilder, RejectsInvalidTransparentOutput)
{
    auto consensusParams = Params().GetConsensus();

    // Default CTxDestination type is an invalid address
    CTxDestination taddr;
    auto builder = TransactionBuilder(consensusParams, 1);
    EXPECT_FALSE(builder.AddTransparentOutput(taddr, 50));
}

TEST(TransactionBuilder, RejectsInvalidTransparentChangeAddress)
{
    auto consensusParams = Params().GetConsensus();

    // Default CTxDestination type is an invalid address
    CTxDestination taddr;
    auto builder = TransactionBuilder(consensusParams, 1);
    EXPECT_FALSE(builder.SendChangeTo(taddr));
}

TEST(TransactionBuilder, FailsWithNegativeChange)
{
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    auto consensusParams = Params().GetConsensus();

    // Generate dummy Sapling address
    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto fvk = sk.full_viewing_key();
    auto pk = sk.default_address();

    // Set up dummy transparent address
    CBasicKeyStore keystore;
    CKey tsk = DecodeSecret(tSecretRegtest);
    keystore.AddKey(tsk);
    auto tkeyid = tsk.GetPubKey().GetID();
    auto scriptPubKey = GetScriptForDestination(tkeyid);
    CTxDestination taddr = tkeyid;

    // Generate dummy Sapling note
    libzcash::SaplingNote note(pk, 59999);
    auto cm = note.cm().value();
    SaplingMerkleTree tree;
    tree.append(cm);
    auto anchor = tree.root();
    auto witness = tree.witness();

    // Fail if there is only a Sapling output
    // 0.0005 z-ZEC out, 0.0001 t-ZEC fee
    auto builder = TransactionBuilder(consensusParams, 1);
    builder.AddSaplingOutput(fvk.ovk, pk, 50000, {});
    EXPECT_FALSE(static_cast<bool>(builder.Build()));

    // Fail if there is only a transparent output
    // 0.0005 t-ZEC out, 0.0001 t-ZEC fee
    builder = TransactionBuilder(consensusParams, 1, &keystore);
    EXPECT_TRUE(builder.AddTransparentOutput(taddr, 50000));
    EXPECT_FALSE(static_cast<bool>(builder.Build()));

    // Fails if there is insufficient input
    // 0.0005 t-ZEC out, 0.0001 t-ZEC fee, 0.00059999 z-ZEC in
    EXPECT_TRUE(builder.AddSaplingSpend(expsk, note, anchor, witness));
    EXPECT_FALSE(static_cast<bool>(builder.Build()));

    // Succeeds if there is sufficient input
    builder.AddTransparentInput(COutPoint(), scriptPubKey, 1);
    EXPECT_TRUE(static_cast<bool>(builder.Build()));

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}

TEST(TransactionBuilder, ChangeOutput)
{
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    auto consensusParams = Params().GetConsensus();

    // Generate dummy Sapling address
    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto pk = sk.default_address();

    // Generate dummy Sapling note
    libzcash::SaplingNote note(pk, 25000);
    auto cm = note.cm().value();
    SaplingMerkleTree tree;
    tree.append(cm);
    auto anchor = tree.root();
    auto witness = tree.witness();

    // Generate change Sapling address
    auto sk2 = libzcash::SaplingSpendingKey::random();
    auto fvkOut = sk2.full_viewing_key();
    auto zChangeAddr = sk2.default_address();

    // Set up dummy transparent address
    CBasicKeyStore keystore;
    CKey tsk = DecodeSecret(tSecretRegtest);
    keystore.AddKey(tsk);
    auto tkeyid = tsk.GetPubKey().GetID();
    auto scriptPubKey = GetScriptForDestination(tkeyid);
    CTxDestination taddr = tkeyid;

    // No change address and no Sapling spends
    {
        auto builder = TransactionBuilder(consensusParams, 1, &keystore);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000);
        EXPECT_FALSE(static_cast<bool>(builder.Build()));
    }

    // Change to the same address as the first Sapling spend
    {
        auto builder = TransactionBuilder(consensusParams, 1, &keystore);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000);
        ASSERT_TRUE(builder.AddSaplingSpend(expsk, note, anchor, witness));
        auto maybe_tx = builder.Build();
        ASSERT_EQ(static_cast<bool>(maybe_tx), true);
        auto tx = maybe_tx.get();

        EXPECT_EQ(tx.vin.size(), 1);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vjoinsplit.size(), 0);
        EXPECT_EQ(tx.vShieldedSpend.size(), 1);
        EXPECT_EQ(tx.vShieldedOutput.size(), 1);
        EXPECT_EQ(tx.valueBalance, -15000);
    }

    // Change to a Sapling address
    {
        auto builder = TransactionBuilder(consensusParams, 1, &keystore);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000);
        builder.SendChangeTo(zChangeAddr, fvkOut.ovk);
        auto maybe_tx = builder.Build();
        ASSERT_EQ(static_cast<bool>(maybe_tx), true);
        auto tx = maybe_tx.get();

        EXPECT_EQ(tx.vin.size(), 1);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vjoinsplit.size(), 0);
        EXPECT_EQ(tx.vShieldedSpend.size(), 0);
        EXPECT_EQ(tx.vShieldedOutput.size(), 1);
        EXPECT_EQ(tx.valueBalance, -15000);
    }

    // Change to a transparent address
    {
        auto builder = TransactionBuilder(consensusParams, 1, &keystore);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000);
        ASSERT_TRUE(builder.SendChangeTo(taddr));
        auto maybe_tx = builder.Build();
        ASSERT_EQ(static_cast<bool>(maybe_tx), true);
        auto tx = maybe_tx.get();

        EXPECT_EQ(tx.vin.size(), 1);
        EXPECT_EQ(tx.vout.size(), 1);
        EXPECT_EQ(tx.vjoinsplit.size(), 0);
        EXPECT_EQ(tx.vShieldedSpend.size(), 0);
        EXPECT_EQ(tx.vShieldedOutput.size(), 0);
        EXPECT_EQ(tx.valueBalance, 0);
        EXPECT_EQ(tx.vout[0].nValue, 15000);
    }

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}

TEST(TransactionBuilder, SetFee)
{
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    auto consensusParams = Params().GetConsensus();

    // Generate dummy Sapling address
    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto fvk = sk.full_viewing_key();
    auto pk = sk.default_address();

    // Generate dummy Sapling note
    libzcash::SaplingNote note(pk, 50000);
    auto cm = note.cm().value();
    SaplingMerkleTree tree;
    tree.append(cm);
    auto anchor = tree.root();
    auto witness = tree.witness();

    // Default fee
    {
        auto builder = TransactionBuilder(consensusParams, 1);
        ASSERT_TRUE(builder.AddSaplingSpend(expsk, note, anchor, witness));
        builder.AddSaplingOutput(fvk.ovk, pk, 25000, {});
        auto maybe_tx = builder.Build();
        ASSERT_EQ(static_cast<bool>(maybe_tx), true);
        auto tx = maybe_tx.get();

        EXPECT_EQ(tx.vin.size(), 0);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vjoinsplit.size(), 0);
        EXPECT_EQ(tx.vShieldedSpend.size(), 1);
        EXPECT_EQ(tx.vShieldedOutput.size(), 2);
        EXPECT_EQ(tx.valueBalance, 10000);
    }

    // Configured fee
    {
        auto builder = TransactionBuilder(consensusParams, 1);
        ASSERT_TRUE(builder.AddSaplingSpend(expsk, note, anchor, witness));
        builder.AddSaplingOutput(fvk.ovk, pk, 25000, {});
        builder.SetFee(20000);
        auto maybe_tx = builder.Build();
        ASSERT_EQ(static_cast<bool>(maybe_tx), true);
        auto tx = maybe_tx.get();

        EXPECT_EQ(tx.vin.size(), 0);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vjoinsplit.size(), 0);
        EXPECT_EQ(tx.vShieldedSpend.size(), 1);
        EXPECT_EQ(tx.vShieldedOutput.size(), 2);
        EXPECT_EQ(tx.valueBalance, 20000);
    }

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}
