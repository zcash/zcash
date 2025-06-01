#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "consensus/validation.h"
#include "gtest/utils.h"
#include "random.h"
#include "transaction_builder.h"
#include "util/test.h"
#include "wallet/orchard.h"
#include "zcash/Address.hpp"

#include <optional>

using namespace libzcash;

OrchardSpendingKey RandomOrchardSpendingKey() {
    auto coinType = Params().BIP44CoinType() ;

    auto seed = MnemonicSeed::Random(coinType);
    return OrchardSpendingKey::ForAccount(seed, coinType, 0);
}

CTransaction FakeOrchardTx(const OrchardSpendingKey& sk, libzcash::diversifier_index_t j) {
    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());

    auto fvk = sk.ToFullViewingKey();
    auto ivk = fvk.ToIncomingViewingKey();
    auto recipient = ivk.Address(j);
    auto orchardAnchor = uint256();

    // Create a shielding transaction from transparent to Orchard
    // 0.0005 t-ZEC in, 0.0004 z-ZEC out, 0.0001 fee
    auto builder = TransactionBuilder(Params(), 1, orchardAnchor, SaplingMerkleTree::empty_root(), &keystore);
    builder.SetFee(10000);
    builder.AddTransparentInput(COutPoint(uint256S("1234"), 0), scriptPubKey, 50000);
    builder.AddOrchardOutput(std::nullopt, recipient, 40000, std::nullopt);

    auto maybeTx = builder.Build();
    EXPECT_TRUE(maybeTx.IsTx());
    return maybeTx.GetTxOrThrow();
}

TEST(OrchardWalletTests, TxInvolvesMyNotes) {
    auto consensusParams = RegtestActivateNU5();
    OrchardWallet wallet;

    // Add a new spending key to the wallet
    auto sk = RandomOrchardSpendingKey();
    wallet.AddSpendingKey(sk);

    // Create a transaction sending to the default address for that
    // spending key and add it to the wallet.
    auto tx = FakeOrchardTx(sk, libzcash::diversifier_index_t(0));
    wallet.AddNotesIfInvolvingMe(tx, nullptr);

    // Check that we detect the transaction as ours
    EXPECT_TRUE(wallet.TxInvolvesMyNotes(tx.GetHash()));

    // Create a transaction sending to a different diversified address
    auto tx1 = FakeOrchardTx(sk, libzcash::diversifier_index_t(0xffffffffffffffff));
    wallet.AddNotesIfInvolvingMe(tx1, nullptr);

    // Check that we also detect this transaction as ours
    EXPECT_TRUE(wallet.TxInvolvesMyNotes(tx1.GetHash()));

    // Now generate a new key, and send a transaction to it without adding
    // the key to the wallet; it should not be detected as ours.
    auto skNotOurs = RandomOrchardSpendingKey();
    auto tx2 = FakeOrchardTx(skNotOurs, libzcash::diversifier_index_t(0));
    wallet.AddNotesIfInvolvingMe(tx2, nullptr);
    EXPECT_FALSE(wallet.TxInvolvesMyNotes(tx2.GetHash()));

    RegtestDeactivateNU5();
}

// This test is here instead of test_transaction_builder.cpp because it depends
// on OrchardWallet, which only exists if the wallet is compiled in.
TEST(TransactionBuilder, OrchardToOrchard) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateNU5();
    OrchardWallet wallet;

    CBasicKeyStore keystore;
    CKey tsk = AddTestCKeyToKeyStore(keystore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());

    auto sk = RandomOrchardSpendingKey();
    wallet.AddSpendingKey(sk);

    // Create a transaction sending to the default address for that
    // spending key and add it to the wallet.
    libzcash::diversifier_index_t j(0);
    auto txRecv = FakeOrchardTx(sk, j);
    wallet.AddNotesIfInvolvingMe(txRecv, nullptr);

    // Generate a recipient.
    auto recipient = RandomOrchardSpendingKey()
        .ToFullViewingKey()
        .ToIncomingViewingKey()
        .Address(j);

    // Select the one note in the wallet for spending.
    std::vector<OrchardNoteMetadata> notes;
    wallet.GetFilteredNotes(
        notes, sk.ToFullViewingKey().ToIncomingViewingKey(), true, true);
    ASSERT_EQ(notes.size(), 1);

    // If we attempt to get spend info now, it will fail because the note hasn't
    // been witnessed in the Orchard commitment tree.
    EXPECT_THROW(wallet.GetSpendInfo(notes, 1, wallet.GetLatestAnchor()), std::logic_error);

    // Append the bundle to the wallet's commitment tree.
    CBlock fakeBlock;
    fakeBlock.vtx.resize(2);
    fakeBlock.vtx[1] = txRecv;
    ASSERT_TRUE(wallet.AppendNoteCommitments(2, fakeBlock));

    // Now we can get spend info for the note.
    auto spendInfo = wallet.GetSpendInfo(notes, 1, wallet.GetLatestAnchor());
    EXPECT_EQ(spendInfo[0].second.Value(), 40000);

    // Get the root of the commitment tree.
    OrchardMerkleFrontier tree;
    tree.AppendBundle(txRecv.GetOrchardBundle());
    auto orchardAnchor = tree.root();

    // Create an Orchard-only transaction
    // 0.0004 z-ZEC in, 0.00025 z-ZEC out, default fee, 0.00014 z-ZEC change
    auto builder = TransactionBuilder(Params(), 2, orchardAnchor, SaplingMerkleTree::empty_root());
    EXPECT_TRUE(builder.AddOrchardSpend(sk, std::move(spendInfo[0].second)));
    builder.AddOrchardOutput(std::nullopt, recipient, 25000, std::nullopt);
    auto maybeTx = builder.Build();
    EXPECT_TRUE(maybeTx.IsTx());
    if (maybeTx.IsError()) {
        std::cerr << "Failed to build transaction: " << maybeTx.GetError() << std::endl;
        GTEST_FAIL();
    }
    auto tx = maybeTx.GetTxOrThrow();

    EXPECT_EQ(tx.vin.size(), 0);
    EXPECT_EQ(tx.vout.size(), 0);
    EXPECT_EQ(tx.vJoinSplit.size(), 0);
    EXPECT_EQ(tx.GetSaplingSpendsCount(), 0);
    EXPECT_EQ(tx.GetSaplingOutputsCount(), 0);
    EXPECT_TRUE(tx.GetOrchardBundle().IsPresent());
    EXPECT_EQ(tx.GetOrchardBundle().GetValueBalance(), 1000);

    CValidationState state;
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 3, true));
    EXPECT_EQ(state.GetRejectReason(), "");

    // Revert to default
    RegtestDeactivateNU5();
}
