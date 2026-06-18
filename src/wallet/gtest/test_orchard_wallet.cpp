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
    wallet.AddNotesIfInvolvingMe(tx);

    // Check that we detect the transaction as ours
    EXPECT_TRUE(wallet.TxInvolvesMyNotes(tx.GetHash()));

    // Create a transaction sending to a different diversified address
    auto tx1 = FakeOrchardTx(sk, libzcash::diversifier_index_t(0xffffffffffffffff));
    wallet.AddNotesIfInvolvingMe(tx1);

    // Check that we also detect this transaction as ours
    EXPECT_TRUE(wallet.TxInvolvesMyNotes(tx1.GetHash()));

    // Now generate a new key, and send a transaction to it without adding
    // the key to the wallet; it should not be detected as ours.
    auto skNotOurs = RandomOrchardSpendingKey();
    auto tx2 = FakeOrchardTx(skNotOurs, libzcash::diversifier_index_t(0));
    wallet.AddNotesIfInvolvingMe(tx2);
    EXPECT_FALSE(wallet.TxInvolvesMyNotes(tx2.GetHash()));

    RegtestDeactivateNU5();
}

// Regression test for #7150. GetSpendInfo resolves the spend against the anchor at
// its absolute height, so it must still succeed when the wallet's latest checkpoint
// has advanced past that height — e.g. a block connecting between anchor selection
// and spend construction. Before the #7150 fix, GetSpendInfo derived the checkpoint
// depth from a fixed confirmation count relative to the latest checkpoint, so an
// advanced tip made it read the wrong checkpoint and spuriously fail even though the
// wallet's tree had not diverged.
TEST(OrchardWalletTests, SpendResolvesAnchorByAbsoluteHeight) {
    auto consensusParams = RegtestActivateNU5();
    OrchardWallet wallet;

    // Receive a note into the wallet.
    auto sk = RandomOrchardSpendingKey();
    wallet.AddSpendingKey(sk);
    libzcash::diversifier_index_t j(0);
    auto txRecv = FakeOrchardTx(sk, j);
    wallet.AddNotesIfInvolvingMe(txRecv);

    std::vector<OrchardNoteMetadata> notes;
    wallet.GetFilteredNotes(
        notes, sk.ToFullViewingKey().ToIncomingViewingKey(), true, true);
    ASSERT_EQ(notes.size(), 1);

    // Connect the note's block at height 2 and capture the anchor at that height.
    // This mirrors the production convention (CWallet::IncrementNoteWitnesses
    // checkpoints height H *before* appending block H's commitments), under which
    // the checkpoint identified by H records the tree state at the *end of block
    // H-1*. So the checkpoint we create as "2" here records the pre-block-2 (empty)
    // state, and the end-of-block-2 anchor is the current frontier root captured
    // after the append below.
    CBlock block2;
    block2.vtx.resize(2);
    block2.vtx[1] = txRecv;
    ASSERT_TRUE(wallet.CheckpointNoteCommitmentTree(2));
    ASSERT_TRUE(wallet.AppendNoteCommitments(2, block2));
    uint256 anchorAtHeight2 = wallet.GetLatestAnchor();

    // Connect several further blocks, each adding an unrelated Orchard commitment so
    // the tree root changes and the latest checkpoint moves well past height 2.
    for (int h = 3; h <= 7; h++) {
        CBlock block;
        block.vtx.resize(2);
        block.vtx[1] = FakeOrchardTx(RandomOrchardSpendingKey(), j);
        ASSERT_TRUE(wallet.CheckpointNoteCommitmentTree(h));
        ASSERT_TRUE(wallet.AppendNoteCommitments(h, block));
    }
    ASSERT_EQ(wallet.GetLastCheckpointHeight().value(), 7);

    // The tip's anchor now differs from the end-of-block-2 anchor, so the spend can
    // only succeed if GetSpendInfo resolves by absolute height: it computes
    // depth = lastCheckpointHeight - anchorHeight = 7 - 2 = 5, which resolves to
    // checkpoint id 3 — the checkpoint created before block 3, recording the
    // end-of-block-2 state — rather than against the latest checkpoint.
    ASSERT_NE(wallet.GetLatestAnchor(), anchorAtHeight2);
    auto spendInfo = wallet.GetSpendInfo(notes, anchorAtHeight2, 2);
    ASSERT_EQ(spendInfo.size(), 1);
    EXPECT_EQ(spendInfo[0].second.Value(), 40000);

    RegtestDeactivateNU5();
}

// Builds a fresh Orchard-spending transaction under the currently-active consensus
// parameters, so that its consensus branch id matches the active epoch (allowing the
// same flow to be exercised under both NU5 and NU6.1).
//
// Uses `ASSERT_*` internally (which expand to conditional `return;`, returning only from this
// helper, not the calling test); so callers must invoke it via `ASSERT_NO_FATAL_FAILURE(...)`
// for a failed assumption to abort the test rather than continue with an unset `outTx`.
void BuildOrchardSpend(CTransaction& outTx) {
    OrchardWallet wallet;
    auto sk = RandomOrchardSpendingKey();
    wallet.AddSpendingKey(sk);

    // Create a transaction sending to the default address for that
    // spending key and add it to the wallet.
    libzcash::diversifier_index_t j(0);
    auto txRecv = FakeOrchardTx(sk, j);
    wallet.AddNotesIfInvolvingMe(txRecv);

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

    // Checkpoint at height 2 before appending block 2's commitments, as in
    // production (CWallet::IncrementNoteWitnesses): the checkpoint identified by H
    // is created before block H and records the end-of-block-(H-1) state.
    // GetSpendInfo addresses the wallet's tree by absolute height (#7150), so a
    // checkpoint must exist for GetLastCheckpointHeight() to resolve against.
    ASSERT_TRUE(wallet.CheckpointNoteCommitmentTree(2));

    // If we attempt to get spend info now, it will fail because the note hasn't
    // been witnessed in the Orchard commitment tree.
    ASSERT_THROW(wallet.GetSpendInfo(notes, wallet.GetLatestAnchor(), 2), std::logic_error);

    // Append the bundle to the wallet's commitment tree.
    CBlock fakeBlock;
    fakeBlock.vtx.resize(2);
    fakeBlock.vtx[1] = txRecv;
    ASSERT_TRUE(wallet.AppendNoteCommitments(2, fakeBlock));

    // Now we can get spend info for the note.
    auto spendInfo = wallet.GetSpendInfo(notes, wallet.GetLatestAnchor(), 2);
    ASSERT_EQ(spendInfo[0].second.Value(), 40000);

    // Get the root of the commitment tree.
    OrchardMerkleFrontier tree;
    tree.AppendBundle(txRecv.GetOrchardBundle());

    // Create an Orchard-only transaction
    // 0.0004 z-ZEC in, 0.00025 z-ZEC out, default fee, 0.00014 z-ZEC change
    auto builder = TransactionBuilder(Params(), 2, tree.root(), SaplingMerkleTree::empty_root());
    ASSERT_TRUE(builder.AddOrchardSpend(sk, std::move(spendInfo[0].second)));
    builder.AddOrchardOutput(std::nullopt, recipient, 25000, std::nullopt);
    auto maybeTx = builder.Build();
    ASSERT_TRUE(maybeTx.IsTx());

    auto tx = maybeTx.GetTxOrThrow();
    ASSERT_EQ(tx.vin.size(), 0);
    ASSERT_EQ(tx.vout.size(), 0);
    ASSERT_EQ(tx.vJoinSplit.size(), 0);
    ASSERT_EQ(tx.GetSaplingSpendsCount(), 0);
    ASSERT_EQ(tx.GetSaplingOutputsCount(), 0);
    ASSERT_TRUE(tx.GetOrchardBundle().IsPresent());
    ASSERT_EQ(tx.GetOrchardBundle().GetValueBalance(), 1000);
    outTx = tx;
}

// Sets `outBytes` to the v5 serialization of `tx` with its Orchard proof padded by one byte,
// so that the proof length is no longer canonical for the number of actions. The returned
// bytes are not re-parsed here (parsing is what the callers exercise).
//
// The v5 transaction format encodes the Orchard proof as a CompactSize length followed by
// that many bytes. For the proof sizes in use (a few thousand bytes) the length is encoded
// in the 3-byte CompactSize form, and incrementing it by one cannot change that encoding's
// width, so the field can be rewritten in place and one byte inserted into the proof.
//
// Uses `ASSERT_*` internally (which expand to conditional `return;`, returning only from this
// helper, not the calling test); so callers must invoke it via `ASSERT_NO_FATAL_FAILURE(...)`
// for a failed assumption to abort the test rather than continue with an unset `outBytes`.
void MakeOrchardProofNonCanonicalBytes(const CTransaction& tx, std::vector<unsigned char>& outBytes) {
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    std::vector<unsigned char> bytes(ss.begin(), ss.end());

    // Walk the v5 serialization to the Orchard proof's CompactSize length field:
    //   header (nVersion, nVersionGroupId, nConsensusBranchId, nLockTime, nExpiryHeight)
    size_t pos = 4 + 4 + 4 + 4 + 4;
    // four empty vectors: transparent in, transparent out, Sapling spends, Sapling outputs.
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(bytes[pos], 0);
        pos += 1;
    }
    // nActionsOrchard, then that many 820-byte action descriptions. These tests build a small
    // number of actions, so nActionsOrchard is a single-byte CompactSize.
    size_t nActions = bytes[pos];
    ASSERT_GT(nActions, 0u);
    ASSERT_LT(nActions, 253u);
    pos += 1 + nActions * 820;
    // flagsOrchard (1) + valueBalanceOrchard (8) + anchorOrchard (32).
    pos += 1 + 8 + 32;

    // proofsOrchard: a 3-byte CompactSize length (0xfd followed by a little-endian u16).
    ASSERT_EQ(bytes[pos], 0xfd);
    size_t proofLen = bytes[pos + 1] | (bytes[pos + 2] << 8);
    size_t newProofLen = proofLen + 1;
    ASSERT_LT(newProofLen, 0x10000u); // still fits the 3-byte CompactSize form
    bytes[pos + 1] = newProofLen & 0xff;
    bytes[pos + 2] = (newProofLen >> 8) & 0xff;
    // Insert one byte at the start of the proof body.
    bytes.insert(bytes.begin() + pos + 3, 0);

    outBytes = bytes;
}

// Overwrites the nConsensusBranchId field (a little-endian uint32 at byte offset 8 in the v5
// header) in place.
void SetConsensusBranchId(std::vector<unsigned char>& bytes, uint32_t branchId) {
    for (int i = 0; i < 4; i++) {
        bytes[8 + i] = (branchId >> (8 * i)) & 0xff;
    }
}

// Deserializes a transaction from raw bytes, returning whether it parsed successfully. zcashd
// computes the transaction's hashes by reparsing through librustzcash (see
// CTransaction::UpdateHash), which is where the Orchard proof size is enforced for NU6.2.
bool TryDeserializeTx(const std::vector<unsigned char>& bytes) {
    CDataStream ss(bytes, SER_NETWORK, PROTOCOL_VERSION);
    try {
        CTransaction tx;
        ss >> tx;
        return true;
    } catch (const std::ios_base::failure&) {
        return false;
    }
}

// These tests are here instead of test_transaction_builder.cpp because they depend
// on OrchardWallet, which only exists if the wallet is compiled in.

TEST(TransactionBuilder, OrchardToOrchardNU5) {
    LoadProofParameters();

    // Under NU5, the Orchard-spending transaction is accepted.
    auto consensusParams = RegtestActivateNU5();
    CTransaction tx;
    ASSERT_NO_FATAL_FAILURE(BuildOrchardSpend(tx));

    {
        CValidationState state;
        EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 1, true));
        EXPECT_EQ(state.GetRejectReason(), "");
    }

    RegtestDeactivateNU5();
}

TEST(TransactionBuilder, OrchardToOrchardNU6point1) {
    LoadProofParameters();

    // Under NU6.1, with the temporary Orchard-disabling soft fork activating at height 3,
    // build a fresh (NU6.1) Orchard-spending transaction.
    auto consensusParams = RegtestActivateNU6point1(false, 1, 2);
    CTransaction tx;
    ASSERT_NO_FATAL_FAILURE(BuildOrchardSpend(tx));

    // Before the soft-fork height the spend is still accepted...
    {
        CValidationState state;
        EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 1, true));
        EXPECT_EQ(state.GetRejectReason(), "");
    }
    // ... and at the soft-fork height it is rejected: the soft fork disables Orchard
    // spends as well as outputs.
    {
        CValidationState state;
        EXPECT_FALSE(ContextualCheckTransaction(tx, state, Params(), 2, true));
        EXPECT_EQ(state.GetRejectReason(), "bad-tx-has-orchard-actions");
    }

    RegtestDeactivateNU6point1();
}

TEST(TransactionBuilder, OrchardToOrchardNU6point2) {
    LoadProofParameters();

    // Under NU6.2, build a fresh Orchard-spending transaction with a canonically-sized proof.
    // NU6.2 is the active epoch from height 1, so the transaction's consensus branch id matches
    // at the heights under test.
    auto consensusParams = RegtestActivateNU6point2(false, 1);
    CTransaction tx;
    ASSERT_NO_FATAL_FAILURE(BuildOrchardSpend(tx));

    // A canonical-proof Orchard transaction is accepted under NU6.2.
    {
        CValidationState state;
        EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 2, true));
        EXPECT_EQ(state.GetRejectReason(), "");
    }

    RegtestDeactivateNU6point2();
}

// From NU6.2, an Orchard proof must have the canonical length for its number of actions. zcashd
// enforces this through librustzcash when it reparses a transaction to compute its hashes (see
// CTransaction::UpdateHash), keyed on the transaction's own consensus branch id. This checks
// that a NU6.2 transaction with a padded (non-canonical) proof fails to deserialize, while the
// same transaction with a canonical proof deserializes successfully.
TEST(TransactionBuilder, OrchardNonCanonicalProofSizeRejectedFromNU6point2) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateNU6point2(false, 1);
    CTransaction tx;
    ASSERT_NO_FATAL_FAILURE(BuildOrchardSpend(tx));

    // The unmodified (canonical-proof) NU6.2 transaction deserializes.
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << tx;
        std::vector<unsigned char> bytes(ss.begin(), ss.end());
        EXPECT_TRUE(TryDeserializeTx(bytes));
    }

    // With the proof padded to a non-canonical length, it does not.
    std::vector<unsigned char> tampered;
    ASSERT_NO_FATAL_FAILURE(MakeOrchardProofNonCanonicalBytes(tx, tampered));
    EXPECT_FALSE(TryDeserializeTx(tampered));

    RegtestDeactivateNU6point2();
}

// The proof-size rule is keyed on the NU6.2 consensus branch id, so it does not apply to
// transactions from earlier epochs. A node must still be able to deserialize a pre-NU6.2
// transaction carrying a non-canonical proof, so that it can sync and reindex chain history
// (which may contain such a proof mined before the rule took effect). This takes the same
// padded-proof bytes and rewrites the consensus branch id to NU6.1, then checks they
// deserialize successfully.
TEST(TransactionBuilder, OrchardNonCanonicalProofSizeAllowedBeforeNU6point2) {
    LoadProofParameters();

    auto consensusParams = RegtestActivateNU6point2(false, 1);
    CTransaction tx;
    ASSERT_NO_FATAL_FAILURE(BuildOrchardSpend(tx));

    std::vector<unsigned char> tampered;
    ASSERT_NO_FATAL_FAILURE(MakeOrchardProofNonCanonicalBytes(tx, tampered));
    // Sanity check: under the NU6.2 branch id these bytes are rejected.
    ASSERT_FALSE(TryDeserializeTx(tampered));

    // Under the NU6.1 branch id the same non-canonical proof is accepted at deserialization.
    SetConsensusBranchId(tampered, 0x4dec4df0);
    EXPECT_TRUE(TryDeserializeTx(tampered));

    RegtestDeactivateNU6point2();
}
