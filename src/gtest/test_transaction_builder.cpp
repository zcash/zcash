#include "chainparams.h"
#include "consensus/params.h"
#include "consensus/validation.h"
#include "main.h"
#include "transaction_builder.h"
#include "zcash/Address.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

TEST(TransactionBuilder, Invoke) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    auto consensusParams = Params().GetConsensus();

    auto sk_from = libzcash::SaplingSpendingKey::random();
    auto fvk_from = sk_from.full_viewing_key();

    auto sk = libzcash::SaplingSpendingKey::random();
    auto xsk = sk.expanded_spending_key();
    auto fvk = sk.full_viewing_key();
    auto ivk = fvk.in_viewing_key();
    libzcash::diversifier_t d = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    auto pk = *ivk.address(d);

    // Create a shielding transaction from transparent to Sapling
    // TODO: Add transparent inputs :P
    auto builder1 = TransactionBuilder(consensusParams, 1);
    builder1.AddSaplingOutput(fvk_from, pk, 50, {});
    auto maybe_tx1 = builder1.Build();
    ASSERT_EQ(static_cast<bool>(maybe_tx1), true);
    auto tx1 = maybe_tx1.get();

    EXPECT_EQ(tx1.vin.size(), 0);
    EXPECT_EQ(tx1.vout.size(), 0);
    EXPECT_EQ(tx1.vjoinsplit.size(), 0);
    EXPECT_EQ(tx1.vShieldedSpend.size(), 0);
    EXPECT_EQ(tx1.vShieldedOutput.size(), 1);
    EXPECT_EQ(tx1.valueBalance, -50);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx1, state, 2, 0));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Prepare to spend the note that was just created
    auto maybe_pt = libzcash::SaplingNotePlaintext::decrypt(
        tx1.vShieldedOutput[0].encCiphertext, ivk, tx1.vShieldedOutput[0].ephemeralKey
    );
    ASSERT_EQ(static_cast<bool>(maybe_pt), true);
    auto maybe_note = maybe_pt.get().note(ivk);
    ASSERT_EQ(static_cast<bool>(maybe_note), true);
    auto note = maybe_note.get();
    ZCSaplingIncrementalMerkleTree tree;
    tree.append(tx1.vShieldedOutput[0].cm);
    auto anchor = tree.root();
    auto witness = tree.witness();

    // Create a Sapling-only transaction
    auto builder2 = TransactionBuilder(consensusParams, 2);
    builder2.AddSaplingSpend(xsk, note, anchor, witness);
    builder2.AddSaplingOutput(fvk, pk, 25, {});
    auto maybe_tx2 = builder2.Build();
    ASSERT_EQ(static_cast<bool>(maybe_tx2), true);
    auto tx2 = maybe_tx2.get();

    EXPECT_EQ(tx2.vin.size(), 0);
    EXPECT_EQ(tx2.vout.size(), 0);
    EXPECT_EQ(tx2.vjoinsplit.size(), 0);
    EXPECT_EQ(tx2.vShieldedSpend.size(), 1);
    EXPECT_EQ(tx2.vShieldedOutput.size(), 1);
    EXPECT_EQ(tx2.valueBalance, 25);

    EXPECT_TRUE(ContextualCheckTransaction(tx2, state, 3, 0));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}
