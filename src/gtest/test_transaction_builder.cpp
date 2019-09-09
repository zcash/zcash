#include "chainparams.h"
#include "consensus/params.h"
#include "consensus/validation.h"
#include "key_io.h"
#include "main.h"
#include "pubkey.h"
#include "rpc/protocol.h"
#include "transaction_builder.h"
#include "utiltest.h"
#include "zcash/Address.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern ZCJoinSplit* params;

// Fake an empty view
class TransactionBuilderCoinsViewDB : public CCoinsView {
public:
    std::map<uint256, SproutMerkleTree> sproutTrees;

    TransactionBuilderCoinsViewDB() {}

    bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const {
        auto it = sproutTrees.find(rt);
        if (it != sproutTrees.end()) {
            tree = it->second;
            return true;
        } else {
            return false;
        }
    }

    bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
        return false;
    }

    bool GetNullifier(const uint256 &nf, ShieldedType type) const {
        return false;
    }

    bool GetCoins(const uint256 &txid, CCoins &coins) const {
        return false;
    }

    bool HaveCoins(const uint256 &txid) const {
        return false;
    }

    uint256 GetBestBlock() const {
        uint256 a;
        return a;
    }

    uint256 GetBestAnchor(ShieldedType type) const {
        uint256 a;
        return a;
    }

    bool BatchWrite(CCoinsMap &mapCoins,
                    const uint256 &hashBlock,
                    const uint256 &hashSproutAnchor,
                    const uint256 &hashSaplingAnchor,
                    CAnchorsSproutMap &mapSproutAnchors,
                    CAnchorsSaplingMap &mapSaplingAnchors,
                    CNullifiersMap &mapSproutNullifiers,
                    CNullifiersMap saplingNullifiersMap) {
        return false;
    }

    bool GetStats(CCoinsStats &stats) const {
        return false;
    }
};

TEST(TransactionBuilder, TransparentToSapling)
{
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
    // 0.0005 t-ZEC in, 0.0004 z-ZEC out, 0.0001 t-ZEC fee
    auto builder = TransactionBuilder(consensusParams, 1, &keystore);
    builder.AddTransparentInput(COutPoint(), scriptPubKey, 50000);
    builder.AddSaplingOutput(fvk_from.ovk, pk, 40000, {});
    auto tx = builder.Build().GetTxOrThrow();

    EXPECT_EQ(tx.vin.size(), 1);
    EXPECT_EQ(tx.vout.size(), 0);
    EXPECT_EQ(tx.vJoinSplit.size(), 0);
    EXPECT_EQ(tx.vShieldedSpend.size(), 0);
    EXPECT_EQ(tx.vShieldedOutput.size(), 1);
    EXPECT_EQ(tx.valueBalance, -40000);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 2, 0));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, SaplingToSapling) {
    auto consensusParams = RegtestActivateSapling();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto fvk = sk.full_viewing_key();
    auto pa = sk.default_address();

    auto testNote = GetTestSaplingNote(pa, 40000);
    
    // Create a Sapling-only transaction
    // 0.0004 z-ZEC in, 0.00025 z-ZEC out, 0.0001 t-ZEC fee, 0.00005 z-ZEC change
    auto builder = TransactionBuilder(consensusParams, 2);
    builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());

    // Check that trying to add a different anchor fails
    // TODO: the following check can be split out in to another test
    ASSERT_THROW(builder.AddSaplingSpend(expsk, testNote.note, uint256(), testNote.tree.witness()), UniValue);

    builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
    auto tx = builder.Build().GetTxOrThrow();

    EXPECT_EQ(tx.vin.size(), 0);
    EXPECT_EQ(tx.vout.size(), 0);
    EXPECT_EQ(tx.vJoinSplit.size(), 0);
    EXPECT_EQ(tx.vShieldedSpend.size(), 1);
    EXPECT_EQ(tx.vShieldedOutput.size(), 2);
    EXPECT_EQ(tx.valueBalance, 10000);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 3, 0));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, SaplingToSprout) {
    auto consensusParams = RegtestActivateSapling();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto pa = sk.default_address();

    auto testNote = GetTestSaplingNote(pa, 40000);

    auto sproutSk = libzcash::SproutSpendingKey::random();
    auto sproutAddr = sproutSk.address();

    // Create a Sapling-to-Sprout transaction (reusing the note from above)
    // - 0.0004 Sapling-ZEC in      - 0.00025 Sprout-ZEC out
    //                              - 0.00005 Sapling-ZEC change
    //                              - 0.0001 t-ZEC fee
    auto builder = TransactionBuilder(consensusParams, 2, nullptr, params);
    builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
    builder.AddSproutOutput(sproutAddr, 25000);
    auto tx = builder.Build().GetTxOrThrow();

    EXPECT_EQ(tx.vin.size(), 0);
    EXPECT_EQ(tx.vout.size(), 0);
    EXPECT_EQ(tx.vJoinSplit.size(), 1);
    EXPECT_EQ(tx.vJoinSplit[0].vpub_old, 25000);
    EXPECT_EQ(tx.vJoinSplit[0].vpub_new, 0);
    EXPECT_EQ(tx.vShieldedSpend.size(), 1);
    EXPECT_EQ(tx.vShieldedOutput.size(), 1);
    EXPECT_EQ(tx.valueBalance, 35000);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 3, 0));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, SproutToSproutAndSapling) {
    auto consensusParams = RegtestActivateSapling();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto fvk = sk.full_viewing_key();
    auto pa = sk.default_address();

    auto sproutSk = libzcash::SproutSpendingKey::random();
    auto sproutAddr = sproutSk.address();

    auto wtx = GetValidSproutReceive(*params, sproutSk, 25000, true);
    auto sproutNote = GetSproutNote(*params, sproutSk, wtx, 0, 1);
    
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
    auto builder = TransactionBuilder(consensusParams, 2, nullptr, params, &view);
    builder.SetFee(5000);
    builder.AddSproutInput(sproutSk, sproutNote, sproutWitness);
    builder.AddSproutOutput(sproutAddr, 6000);
    builder.AddSproutOutput(sproutAddr, 4000);
    builder.AddSaplingOutput(fvk.ovk, pa, 5000);
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
    EXPECT_EQ(tx.vShieldedSpend.size(), 0);
    EXPECT_EQ(tx.vShieldedOutput.size(), 1);
    EXPECT_EQ(tx.valueBalance, -5000);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 4, 0));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, ThrowsOnSproutOutputWithoutParams)
{
    auto consensusParams = Params().GetConsensus();
    auto sk = libzcash::SproutSpendingKey::random();
    auto addr = sk.address();

    auto builder = TransactionBuilder(consensusParams, 1);
    ASSERT_THROW(builder.AddSproutOutput(addr, 10), std::runtime_error);
}

TEST(TransactionBuilder, ThrowsOnTransparentInputWithoutKeyStore)
{
    SelectParams(CBaseChainParams::REGTEST);
    auto consensusParams = Params().GetConsensus();

    auto builder = TransactionBuilder(consensusParams, 1);
    ASSERT_THROW(builder.AddTransparentInput(COutPoint(), CScript(), 1), std::runtime_error);
}

TEST(TransactionBuilder, RejectsInvalidTransparentOutput)
{
    SelectParams(CBaseChainParams::REGTEST);
    auto consensusParams = Params().GetConsensus();

    // Default CTxDestination type is an invalid address
    CTxDestination taddr;
    auto builder = TransactionBuilder(consensusParams, 1);
    ASSERT_THROW(builder.AddTransparentOutput(taddr, 50), UniValue);
}

TEST(TransactionBuilder, RejectsInvalidTransparentChangeAddress)
{
    SelectParams(CBaseChainParams::REGTEST);
    auto consensusParams = Params().GetConsensus();

    // Default CTxDestination type is an invalid address
    CTxDestination taddr;
    auto builder = TransactionBuilder(consensusParams, 1);
    ASSERT_THROW(builder.SendChangeTo(taddr), UniValue);
}

TEST(TransactionBuilder, FailsWithNegativeChange)
{
    auto consensusParams = RegtestActivateSapling();

    // Generate dummy Sapling address
    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto fvk = sk.full_viewing_key();
    auto pa = sk.default_address();

    // Set up dummy transparent address
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto tkeyid = tsk.GetPubKey().GetID();
    auto scriptPubKey = GetScriptForDestination(tkeyid);
    CTxDestination taddr = tkeyid;

    auto testNote = GetTestSaplingNote(pa, 59999);

    // Fail if there is only a Sapling output
    // 0.0005 z-ZEC out, 0.0001 t-ZEC fee
    auto builder = TransactionBuilder(consensusParams, 1);
    builder.AddSaplingOutput(fvk.ovk, pa, 50000, {});
    EXPECT_EQ("Change cannot be negative", builder.Build().GetError());

    // Fail if there is only a transparent output
    // 0.0005 t-ZEC out, 0.0001 t-ZEC fee
    builder = TransactionBuilder(consensusParams, 1, &keystore);
    builder.AddTransparentOutput(taddr, 50000);
    EXPECT_EQ("Change cannot be negative", builder.Build().GetError());

    // Fails if there is insufficient input
    // 0.0005 t-ZEC out, 0.0001 t-ZEC fee, 0.00059999 z-ZEC in
    builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
    EXPECT_EQ("Change cannot be negative", builder.Build().GetError());

    // Succeeds if there is sufficient input
    builder.AddTransparentInput(COutPoint(), scriptPubKey, 1);
    EXPECT_TRUE(builder.Build().IsTx());

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, ChangeOutput)
{
    auto consensusParams = RegtestActivateSapling();

    // Generate dummy Sapling address
    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto pa = sk.default_address();

    auto testNote = GetTestSaplingNote(pa, 25000);

    // Generate change Sapling address
    auto sk2 = libzcash::SaplingSpendingKey::random();
    auto fvkOut = sk2.full_viewing_key();
    auto zChangeAddr = sk2.default_address();

    // Set up dummy transparent address
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto tkeyid = tsk.GetPubKey().GetID();
    auto scriptPubKey = GetScriptForDestination(tkeyid);
    CTxDestination taddr = tkeyid;

    // No change address and no Sapling spends
    {
        auto builder = TransactionBuilder(consensusParams, 1, &keystore);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000);
        EXPECT_EQ("Could not determine change address", builder.Build().GetError());
    }

    // Change to the same address as the first Sapling spend
    {
        auto builder = TransactionBuilder(consensusParams, 1, &keystore);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000);
        builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
        auto tx = builder.Build().GetTxOrThrow();

        EXPECT_EQ(tx.vin.size(), 1);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vJoinSplit.size(), 0);
        EXPECT_EQ(tx.vShieldedSpend.size(), 1);
        EXPECT_EQ(tx.vShieldedOutput.size(), 1);
        EXPECT_EQ(tx.valueBalance, -15000);
    }

    // Change to a Sapling address
    {
        auto builder = TransactionBuilder(consensusParams, 1, &keystore);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000);
        builder.SendChangeTo(zChangeAddr, fvkOut.ovk);
        auto tx = builder.Build().GetTxOrThrow();

        EXPECT_EQ(tx.vin.size(), 1);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vJoinSplit.size(), 0);
        EXPECT_EQ(tx.vShieldedSpend.size(), 0);
        EXPECT_EQ(tx.vShieldedOutput.size(), 1);
        EXPECT_EQ(tx.valueBalance, -15000);
    }

    // Change to a transparent address
    {
        auto builder = TransactionBuilder(consensusParams, 1, &keystore);
        builder.AddTransparentInput(COutPoint(), scriptPubKey, 25000);
        builder.SendChangeTo(taddr);
        auto tx = builder.Build().GetTxOrThrow();

        EXPECT_EQ(tx.vin.size(), 1);
        EXPECT_EQ(tx.vout.size(), 1);
        EXPECT_EQ(tx.vJoinSplit.size(), 0);
        EXPECT_EQ(tx.vShieldedSpend.size(), 0);
        EXPECT_EQ(tx.vShieldedOutput.size(), 0);
        EXPECT_EQ(tx.valueBalance, 0);
        EXPECT_EQ(tx.vout[0].nValue, 15000);
    }

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, SetFee)
{
    auto consensusParams = RegtestActivateSapling();

    // Generate dummy Sapling address
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
        auto tx = builder.Build().GetTxOrThrow();

        EXPECT_EQ(tx.vin.size(), 0);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vJoinSplit.size(), 0);
        EXPECT_EQ(tx.vShieldedSpend.size(), 1);
        EXPECT_EQ(tx.vShieldedOutput.size(), 2);
        EXPECT_EQ(tx.valueBalance, 10000);
    }

    // Configured fee
    {
        auto builder = TransactionBuilder(consensusParams, 1);
        builder.AddSaplingSpend(expsk, testNote.note, testNote.tree.root(), testNote.tree.witness());
        builder.AddSaplingOutput(fvk.ovk, pa, 25000, {});
        builder.SetFee(20000);
        auto tx = builder.Build().GetTxOrThrow();

        EXPECT_EQ(tx.vin.size(), 0);
        EXPECT_EQ(tx.vout.size(), 0);
        EXPECT_EQ(tx.vJoinSplit.size(), 0);
        EXPECT_EQ(tx.vShieldedSpend.size(), 1);
        EXPECT_EQ(tx.vShieldedOutput.size(), 2);
        EXPECT_EQ(tx.valueBalance, 20000);
    }

    // Revert to default
    RegtestDeactivateSapling();
}

TEST(TransactionBuilder, CheckSaplingTxVersion)
{
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    auto consensusParams = Params().GetConsensus();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto expsk = sk.expanded_spending_key();
    auto pk = sk.default_address();

    // Cannot add Sapling outputs to a non-Sapling transaction
    auto builder = TransactionBuilder(consensusParams, 1);
    try {
        builder.AddSaplingOutput(uint256(), pk, 12345, {});
    } catch (std::runtime_error const & err) {
        EXPECT_EQ(err.what(), std::string("TransactionBuilder cannot add Sapling output to pre-Sapling transaction"));
    } catch(...) {
        FAIL() << "Expected std::runtime_error";
    }

    // Cannot add Sapling spends to a non-Sapling transaction
    libzcash::SaplingNote note(pk, 50000);
    SaplingMerkleTree tree;
    try {
        builder.AddSaplingSpend(expsk, note, uint256(), tree.witness());
    } catch (std::runtime_error const & err) {
        EXPECT_EQ(err.what(), std::string("TransactionBuilder cannot add Sapling spend to pre-Sapling transaction"));
    } catch(...) {
        FAIL() << "Expected std::runtime_error";
    }

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}
