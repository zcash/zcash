#include "chainparams.h"
#include "consensus/params.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "key_io.h"
#include "main.h"
#include "pubkey.h"
#include "rpc/protocol.h"
#include "transaction_builder.h"
#include "gtest/test_transaction_builder.h"
#include "gtest/utils.h"
#include "util/test.h"
#include "zcash/Address.hpp"
#include "zcash/address/mnemonic.h"

#include <optional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(TransactionBuilder, TransparentToSapling)
{
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();

    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
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
    // 0.00005 t-ZEC in, 0.00004 z-ZEC out, default fee
    auto builder = TransactionBuilder(Params(), 1, std::nullopt, &keystore);
    builder.AddTransparentInput(COutPoint(uint256S("1234"), 0), scriptPubKey, 5000);
    builder.AddSaplingOutput(fvk_from.ovk, pk, 4000, {});
    auto tx = builder.Build().GetTxOrThrow();

    EXPECT_EQ(tx.vin.size(), 1);
    EXPECT_EQ(tx.vout.size(), 0);
    EXPECT_EQ(tx.vJoinSplit.size(), 0);
    EXPECT_EQ(tx.GetSaplingSpendsCount(), 0);
    EXPECT_EQ(tx.GetSaplingOutputsCount(), 1);
    EXPECT_EQ(tx.GetValueBalanceSapling(), -4000);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 2, true));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, SaplingToSapling) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();

    auto sk = GetTestMasterSaplingSpendingKey();
    auto extfvk = sk.ToXFVK();
    auto fvk = extfvk.fvk;
    auto pa = extfvk.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 4000);

    // Create a Sapling-only transaction
    // 0.00004 z-ZEC in, 0.000025 z-ZEC out, default fee, 0.000005 z-ZEC change
    auto builder = TransactionBuilder(Params(), 2, std::nullopt);
    builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());

    // Check that trying to add a different anchor fails
    // TODO: the following check can be split out in to another test
    testNote.tree.append(uint256());
    ASSERT_THROW(builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness()), rust::Error);

    builder.AddSaplingOutput(fvk.ovk, pa, 2500, {});
    auto tx = builder.Build().GetTxOrThrow();

    EXPECT_EQ(tx.vin.size(), 0);
    EXPECT_EQ(tx.vout.size(), 0);
    EXPECT_EQ(tx.vJoinSplit.size(), 0);
    EXPECT_EQ(tx.GetSaplingSpendsCount(), 1);
    EXPECT_EQ(tx.GetSaplingOutputsCount(), 2);
    EXPECT_EQ(tx.GetValueBalanceSapling(), 1000);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 3, true));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, SaplingToSprout) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();

    auto sk = GetTestMasterSaplingSpendingKey();
    auto extfvk = sk.ToXFVK();
    auto fvk = extfvk.fvk;
    auto pa = extfvk.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 4000);

    auto sproutSk = libzcash::SproutSpendingKey::random();
    auto sproutAddr = sproutSk.address();

    // Create a Sapling-to-Sprout transaction (reusing the note from above)
    // - 0.00004 Sapling-ZEC in     - 0.000025 Sprout-ZEC out
    //                              - 0.000005 Sapling-ZEC change
    //                              - default t-ZEC fee
    auto builder = TransactionBuilder(Params(), 2, std::nullopt, nullptr);
    builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
    builder.AddSproutOutput(sproutAddr, 2500, std::nullopt);
    auto tx = builder.Build().GetTxOrThrow();

    EXPECT_EQ(tx.vin.size(), 0);
    EXPECT_EQ(tx.vout.size(), 0);
    EXPECT_EQ(tx.vJoinSplit.size(), 1);
    EXPECT_EQ(tx.vJoinSplit[0].vpub_old, 2500);
    EXPECT_EQ(tx.vJoinSplit[0].vpub_new, 0);
    EXPECT_EQ(tx.GetSaplingSpendsCount(), 1);
    EXPECT_EQ(tx.GetSaplingOutputsCount(), 2);
    EXPECT_EQ(tx.GetValueBalanceSapling(), 3500);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 3, true));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, SproutToSproutAndSapling) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto fvk = sk.full_viewing_key();
    auto pa = sk.default_address();

    auto sproutSk = libzcash::SproutSpendingKey::random();
    auto sproutAddr = sproutSk.address();

    auto wtx = GetValidSproutReceive(sproutSk, 25000, true);
    auto sproutNote = GetSproutNote(sproutSk, wtx, 0, 1);

    SproutMerkleTree sproutTree;
    for (int i = 0; i < ZC_NUM_JS_OUTPUTS; i++) {
        sproutTree.append(wtx.vJoinSplit[0].commitments[i]);
    }
    SproutWitness sproutWitness = sproutTree.witness();
    // Fake a view with the Sprout note in it
    auto rt = sproutTree.root();
    TransactionBuilderCoinsViewDB fakeDB;
    fakeDB.sproutTrees.insert(std::pair<uint256, SproutMerkleTree>(rt, sproutTree));
    CCoinsViewCache view(&fakeDB);

    // Create a Sprout-to-[Sprout-and-Sapling] transaction
    // - 0.00025 Sprout-ZEC in      - 0.00006 Sprout-ZEC out
    //                              - 0.00004 Sprout-ZEC out
    //                              - 0.00005 Sprout-ZEC change
    //                              - 0.00005 Sapling-ZEC out
    //                              - 0.00005 t-ZEC fee
    auto builder = TransactionBuilder(Params(), 2, std::nullopt, nullptr, &view);
    builder.SetFee(5000);
    builder.AddSproutInput(sproutSk, sproutNote, sproutWitness);
    builder.AddSproutOutput(sproutAddr, 6000, std::nullopt);
    builder.AddSproutOutput(sproutAddr, 4000, std::nullopt);
    builder.AddSaplingOutput(fvk.ovk, pa, 5000, std::nullopt);
    auto tx = builder.Build().GetTxOrThrow();

    EXPECT_EQ(tx.vin.size(), 0);
    EXPECT_EQ(tx.vout.size(), 0);
    // TODO: This should be doable in two JoinSplits.
    // There's an inefficiency in the implementation.
    EXPECT_EQ(tx.vJoinSplit.size(), 3);
    EXPECT_EQ(tx.vJoinSplit[0].vpub_old, 0);
    EXPECT_EQ(tx.vJoinSplit[0].vpub_new, 0);
    EXPECT_EQ(tx.vJoinSplit[1].vpub_old, 0);
    EXPECT_EQ(tx.vJoinSplit[1].vpub_new, 0);
    EXPECT_EQ(tx.vJoinSplit[2].vpub_old, 0);
    EXPECT_EQ(tx.vJoinSplit[2].vpub_new, 10000);
    EXPECT_EQ(tx.GetSaplingSpendsCount(), 0);
    EXPECT_EQ(tx.GetSaplingOutputsCount(), 1);
    EXPECT_EQ(tx.GetValueBalanceSapling(), -5000);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 4, true));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, DuplicateOrchardNullifier)
{
    #include "data/tx-orchard-duplicate-nullifiers.h"

    RegtestActivateNU5();

    CDataStream ssin(
        ParseHex(txdataOrchardDuplicateNullifiersTestVector),
        SER_NETWORK,
        PROTOCOL_VERSION
    );

    CTransaction tx;
    ssin >> tx;

    CValidationState state;
    EXPECT_FALSE(CheckTransactionWithoutProofVerification(tx, state));
    EXPECT_EQ(state.GetRejectReason(), "bad-orchard-nullifiers-duplicate");

    // Revert to default
    RegtestDeactivateNU5();
}

TEST(TransactionBuilder, TransparentToOrchard)
{
    auto consensusParams = RegtestActivateNU5();

    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());

    auto coinType = Params().BIP44CoinType();
    auto seed = MnemonicSeed::Random(coinType);
    auto sk = libzcash::OrchardSpendingKey::ForAccount(seed, coinType, 0);
    auto fvk = sk.ToFullViewingKey();
    auto ivk = fvk.ToIncomingViewingKey();
    libzcash::diversifier_index_t j(0);
    auto recipient = ivk.Address(j);

    auto orchardAnchor = uint256();

    // Create a shielding transaction from transparent to Orchard
    // 0.00005 t-ZEC in, 0.00004 z-ZEC out, default fee
    auto builder = TransactionBuilder(Params(), 1, orchardAnchor, &keystore);
    builder.AddTransparentInput(COutPoint(uint256S("1234"), 0), scriptPubKey, 5000);
    builder.AddOrchardOutput(std::nullopt, recipient, 4000, std::nullopt);
    auto maybeTx = builder.Build();
    EXPECT_TRUE(maybeTx.IsTx());
    if (maybeTx.IsError()) {
        std::cerr << "Failed to build transaction: " << maybeTx.GetError() << std::endl;
        GTEST_FAIL();
    }
    auto tx = maybeTx.GetTxOrThrow();

    EXPECT_EQ(tx.vin.size(), 1);
    EXPECT_EQ(tx.vout.size(), 0);
    EXPECT_EQ(tx.vJoinSplit.size(), 0);
    EXPECT_EQ(tx.GetSaplingSpendsCount(), 0);
    EXPECT_EQ(tx.GetSaplingOutputsCount(), 0);
    EXPECT_TRUE(tx.GetOrchardBundle().IsPresent());
    EXPECT_EQ(tx.GetOrchardBundle().GetValueBalance(), -4000);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 2, true));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    RegtestDeactivateNU5();
}

TEST(TransactionBuilder, ThrowsOnTransparentInputWithoutKeyStore)
{
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& consensusParams = Params().GetConsensus();

    auto builder = TransactionBuilder(Params(), 1, std::nullopt);
    ASSERT_THROW(builder.AddTransparentInput(COutPoint(), CScript(), 1), std::runtime_error);
}

TEST(TransactionBuilder, RejectsInvalidTransparentOutput)
{
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& consensusParams = Params().GetConsensus();

    // Default CTxDestination type is an invalid address
    CTxDestination taddr;
    auto builder = TransactionBuilder(Params(), 1, std::nullopt);
    ASSERT_THROW(builder.AddTransparentOutput(taddr, 50), UniValue);
}

TEST(TransactionBuilder, FailsWithNegativeChange)
{
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();

    // Generate dummy Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto extfvk = sk.ToXFVK();
    auto fvk = extfvk.fvk;
    auto pa = extfvk.DefaultAddress();

    // Set up dummy transparent address
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto tkeyid = tsk.GetPubKey().GetID();
    auto scriptPubKey = GetScriptForDestination(tkeyid);
    CTxDestination taddr = tkeyid;

    auto testNote = GetTestSaplingNote(pa, 5999);

    // Fail if there is only a Sapling output
    // 0.00005 z-ZEC out, default fee
    auto builder = TransactionBuilder(Params(), 1, std::nullopt);
    builder.AddSaplingOutput(fvk.ovk, pa, 5000, {});
    EXPECT_EQ("Change cannot be negative: -0.00006 ZEC", builder.Build().GetError());

    // Fail if there is only a transparent output
    // 0.00005 t-ZEC out, default fee
    builder = TransactionBuilder(Params(), 1, std::nullopt, &keystore);
    builder.AddTransparentOutput(taddr, 5000);
    EXPECT_EQ("Change cannot be negative: -0.00006 ZEC", builder.Build().GetError());

    // Fails if there is insufficient input
    // 0.00005 t-ZEC out, default fee, 0.00005999 z-ZEC in
    builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
    EXPECT_EQ("Change cannot be negative: -0.00000001 ZEC", builder.Build().GetError());

    // Succeeds if there is sufficient input
    builder.AddTransparentInput(
        COutPoint(uint256S("7777777777777777777777777777777777777777777777777777777777777777"), 0),
        scriptPubKey, 1);
    EXPECT_TRUE(builder.Build().IsTx());

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, ChangeOutput)
{
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();

    // Generate dummy Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto extfvk = sk.ToXFVK();
    auto fvk = extfvk.fvk;
    auto pa = extfvk.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 2500);

    // Generate change Sapling address
    auto sk2 = libzcash::SaplingSpendingKey::random();
    auto fvkOut = sk2.full_viewing_key();
    auto zChangeAddr = sk2.default_address();

    // Set up dummy transparent address
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto tkeyid = tsk.GetPubKey().GetID();
    auto scriptPubKey = GetScriptForDestination(tkeyid);

    auto fakeCoin = COutPoint(uint256S("7777777777777777777777777777777777777777777777777777777777777777"), 0);

    // No change address and no Sapling spends
    {
        auto builder = TransactionBuilder(Params(), 1, std::nullopt, &keystore);
        builder.AddTransparentInput(fakeCoin, scriptPubKey, 2500);
        EXPECT_EQ("Could not determine change address", builder.Build().GetError());
    }

    // Change to the same address as the first Sapling spend
    {
        auto builder = TransactionBuilder(Params(), 1, std::nullopt, &keystore);
        builder.AddTransparentInput(fakeCoin, scriptPubKey, 2500);
        builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
        auto tx = builder.Build().GetTxOrThrow();

        EXPECT_EQ(tx.vin.size(), 1);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vJoinSplit.size(), 0);
        EXPECT_EQ(tx.GetSaplingSpendsCount(), 1);
        EXPECT_EQ(tx.GetSaplingOutputsCount(), 2);
        EXPECT_EQ(tx.GetValueBalanceSapling(), -1500);
    }

    // Change to a Sapling address
    {
        auto builder = TransactionBuilder(Params(), 1, std::nullopt, &keystore);
        builder.AddTransparentInput(fakeCoin, scriptPubKey, 2500);
        builder.SendChangeTo(zChangeAddr, fvkOut.ovk);
        auto tx = builder.Build().GetTxOrThrow();

        EXPECT_EQ(tx.vin.size(), 1);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vJoinSplit.size(), 0);
        EXPECT_EQ(tx.GetSaplingSpendsCount(), 0);
        EXPECT_EQ(tx.GetSaplingOutputsCount(), 1);
        EXPECT_EQ(tx.GetValueBalanceSapling(), -1500);
    }

    // Change to a transparent address
    {
        auto builder = TransactionBuilder(Params(), 1, std::nullopt, &keystore);
        builder.AddTransparentInput(fakeCoin, scriptPubKey, 2500);
        builder.SendChangeTo(tkeyid, {});
        auto tx = builder.Build().GetTxOrThrow();

        EXPECT_EQ(tx.vin.size(), 1);
        EXPECT_EQ(tx.vout.size(), 1);
        EXPECT_EQ(tx.vJoinSplit.size(), 0);
        EXPECT_EQ(tx.GetSaplingSpendsCount(), 0);
        EXPECT_EQ(tx.GetSaplingOutputsCount(), 0);
        EXPECT_EQ(tx.GetValueBalanceSapling(), 0);
        EXPECT_EQ(tx.vout[0].nValue, 1500);
    }

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, SetFee)
{
    LoadProofParameters();

    auto consensusParams = RegtestActivateSapling();

    // Generate dummy Sapling address
    auto sk = GetTestMasterSaplingSpendingKey();
    auto extfvk = sk.ToXFVK();
    auto fvk = extfvk.fvk;
    auto pa = extfvk.DefaultAddress();

    auto testNote = GetTestSaplingNote(pa, 50000);

    // Default fee
    {
        auto builder = TransactionBuilder(Params(), 1, std::nullopt);
        builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
        builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
        auto tx = builder.Build().GetTxOrThrow();

        EXPECT_EQ(tx.vin.size(), 0);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vJoinSplit.size(), 0);
        EXPECT_EQ(tx.GetSaplingSpendsCount(), 1);
        EXPECT_EQ(tx.GetSaplingOutputsCount(), 2);
        // the value balance is just the amount exposed as the miner fee
        EXPECT_EQ(tx.GetValueBalanceSapling(), 1000);
    }

    // Configured fee
    {
        auto builder = TransactionBuilder(Params(), 1, std::nullopt);
        builder.AddSaplingSpend(sk, testNote.note, testNote.tree.witness());
        builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
        builder.SetFee(20000);
        auto tx = builder.Build().GetTxOrThrow();

        EXPECT_EQ(tx.vin.size(), 0);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vJoinSplit.size(), 0);
        EXPECT_EQ(tx.GetSaplingSpendsCount(), 1);
        EXPECT_EQ(tx.GetSaplingOutputsCount(), 2);
        // the value balance is just the amount exposed as the miner fee
        EXPECT_EQ(tx.GetValueBalanceSapling(), 20000);
    }

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, CheckSaplingTxVersion)
{
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    const Consensus::Params& consensusParams = Params().GetConsensus();

    auto sk = GetTestMasterSaplingSpendingKey();
    auto pk = sk.ToXFVK().DefaultAddress();

    // Cannot add Sapling outputs to a non-Sapling transaction
    auto builder = TransactionBuilder(Params(), 1, std::nullopt);
    try {
        builder.AddSaplingOutput(uint256(), pk, 12345, {});
    } catch (std::runtime_error const & err) {
        EXPECT_EQ(err.what(), std::string("TransactionBuilder cannot add Sapling output to pre-Sapling transaction"));
    } catch(...) {
        FAIL() << "Expected std::runtime_error";
    }

    // Cannot add Sapling spends to a non-Sapling transaction
    libzcash::SaplingNote note(pk, 50000, libzcash::Zip212Enabled::BeforeZip212);
    SaplingMerkleTree tree;
    try {
        builder.AddSaplingSpend(sk, note, tree.witness());
    } catch (std::runtime_error const & err) {
        EXPECT_EQ(err.what(), std::string("TransactionBuilder cannot add Sapling spend to pre-Sapling transaction"));
    } catch(...) {
        FAIL() << "Expected std::runtime_error";
    }

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}
