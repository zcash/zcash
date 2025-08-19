#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "consensus/validation.h"
#include "main.h"
#include "proof_verifier.h"
#include "util/test.h"
#include "zcash/Proof.hpp"

class MockCValidationState : public CValidationState {
public:
    MOCK_METHOD6(DoS, bool(int level, bool ret,
             unsigned int chRejectCodeIn, const std::string &strRejectReasonIn,
             bool corruptionIn,
             const std::string &strDebugMessageIn));
    MOCK_METHOD4(Invalid, bool(bool ret,
                 unsigned int _chRejectCode, const std::string _strRejectReason,
                 const std::string &_strDebugMessage));
    MOCK_METHOD1(Error, bool(std::string strRejectReasonIn));
    MOCK_CONST_METHOD0(IsValid, bool());
    MOCK_CONST_METHOD0(IsInvalid, bool());
    MOCK_CONST_METHOD0(IsError, bool());
    MOCK_CONST_METHOD1(IsInvalid, bool(int &nDoSOut));
    MOCK_CONST_METHOD0(CorruptionPossible, bool());
    MOCK_CONST_METHOD0(GetRejectCode, unsigned int());
    MOCK_CONST_METHOD0(GetRejectReason, std::string());
    MOCK_CONST_METHOD0(GetDebugMessage, std::string());
};

TEST(CheckBlock, VersionTooLow) {
    auto verifier = ProofVerifier::Strict();

    CBlock block;
    block.nVersion = 1;

    MockCValidationState state;

    SelectParams(CBaseChainParams::MAIN);

    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "version-too-low", false, "")).Times(1);
    EXPECT_FALSE(CheckBlock(block, state, Params(), verifier, false, false, true));
}


// Subclass of CTransaction which doesn't call UpdateHash when constructing
// from a CMutableTransaction.  This enables us to create a CTransaction
// with bad values which normally trigger an exception during construction.
class UNSAFE_CTransaction : public CTransaction {
    public:
        UNSAFE_CTransaction(const CMutableTransaction &tx) : CTransaction(tx, true) {}
};

// Test that a Sprout tx with negative version is still rejected
// by CheckBlock under Sprout consensus rules.
TEST(CheckBlock, BlockSproutRejectsBadVersion) {
    SelectParams(CBaseChainParams::MAIN);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 1 << OP_0;
    mtx.vout.resize(1);
    mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    mtx.vout[0].nValue = 0;
    mtx.vout.push_back(CTxOut(
        Params().GetConsensus().GetBlockSubsidy(1)/5,
        Params().GetFoundersRewardScriptAtHeight(1)));
    mtx.fOverwintered = false;
    mtx.nVersion = -1;
    mtx.nVersionGroupId = 0;

    EXPECT_THROW((CTransaction(mtx)), std::ios_base::failure);
    UNSAFE_CTransaction tx {mtx};
    CBlock block;
    block.vtx.push_back(tx);

    MockCValidationState state;
    CBlockIndex indexPrev {Params().GenesisBlock()};

    auto verifier = ProofVerifier::Strict();

    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-version-too-low", false, "")).Times(1);
    EXPECT_FALSE(CheckBlock(block, state, Params(), verifier, false, false, true));
}


class ContextualCheckBlockTest : public ::testing::Test {
protected:
    void SetUp() override {
        SelectParams(CBaseChainParams::MAIN);
    }

    void TearDown() override {
        // Revert to test default. No-op on mainnet params.
        RegtestDeactivateBlossom();
    }

    // Returns a valid but empty mutable transaction at block height 1.
    CMutableTransaction GetFirstBlockCoinbaseTx() {
        CMutableTransaction mtx;

        // No inputs.
        mtx.vin.resize(1);
        mtx.vin[0].prevout.SetNull();

        // Set height to 1.
        mtx.vin[0].scriptSig = CScript() << 1 << OP_0;

        // Give it a single zero-valued, always-valid output.
        mtx.vout.resize(1);
        mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
        mtx.vout[0].nValue = 0;

        // Give it a Founder's Reward vout for height 1.
        auto rewardScript = Params().GetFoundersRewardScriptAtHeight(1);
        mtx.vout.push_back(CTxOut(
                    Params().GetConsensus().GetBlockSubsidy(1)/5,
                    rewardScript));

        return mtx;
    }

    // Expects a height-1 block containing a given transaction to pass
    // ContextualCheckBlock. This is used in accepting (Sprout-Sprout,
    // Overwinter-Overwinter, ...) tests. You should not call it without
    // calling a SCOPED_TRACE macro first to usefully label any failures.
    void ExpectValidBlockFromTx(const CTransaction& tx) {
        // Create a block and add the transaction to it.
        CBlock block;
        block.vtx.push_back(tx);

        // Set the previous block index to the genesis block.
        CBlockIndex indexPrev {Params().GenesisBlock()};

        // We now expect this to be a valid block.
        MockCValidationState state;
        EXPECT_TRUE(ContextualCheckBlock(block, state, Params(), &indexPrev, true));
    }

    // Expects a height-1 block containing a given transaction to fail
    // ContextualCheckBlock. This is used in rejecting (Sprout-Overwinter,
    // Overwinter-Sprout, ...) tests. You should not call it without
    // calling a SCOPED_TRACE macro first to usefully label any failures.
    void ExpectInvalidBlockFromTx(const CTransaction& tx, int level, std::string reason) {
        // Create a block and add the transaction to it.
        CBlock block;
        block.vtx.push_back(tx);

        // Set the previous block index to the genesis block.
        CBlockIndex indexPrev {Params().GenesisBlock()};

        // We now expect this to be an invalid block, for the given reason.
        MockCValidationState state;
        EXPECT_CALL(state, DoS(level, false, REJECT_INVALID, reason, false, "")).Times(1);
        EXPECT_FALSE(ContextualCheckBlock(block, state, Params(), &indexPrev, true));
    }

};


TEST_F(ContextualCheckBlockTest, BadCoinbaseHeight) {
    // Put a transaction in a block with no height in scriptSig
    CMutableTransaction mtx = GetFirstBlockCoinbaseTx();
    mtx.vin[0].scriptSig = CScript() << OP_0;
    mtx.vout.pop_back(); // remove the FR output

    CBlock block;
    block.vtx.emplace_back(mtx);

    // Treating block as genesis should pass
    MockCValidationState state;
    EXPECT_TRUE(ContextualCheckBlock(block, state, Params(), NULL, true));

    // Give the transaction a Founder's Reward vout
    mtx.vout.push_back(CTxOut(
                Params().GetConsensus().GetBlockSubsidy(1)/5,
                Params().GetFoundersRewardScriptAtHeight(1)));

    // Treating block as non-genesis should fail
    CTransaction tx2 {mtx};
    block.vtx[0] = tx2;
    CBlock prev;
    CBlockIndex indexPrev {prev};
    indexPrev.nHeight = 0;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-height", false, "")).Times(1);
    EXPECT_FALSE(ContextualCheckBlock(block, state, Params(), &indexPrev, true));

    // Setting to an incorrect height should fail
    mtx.vin[0].scriptSig = CScript() << 2 << OP_0;
    CTransaction tx3 {mtx};
    block.vtx[0] = tx3;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-height", false, "")).Times(1);
    EXPECT_FALSE(ContextualCheckBlock(block, state, Params(), &indexPrev, true));

    // After correcting the scriptSig, should pass
    mtx.vin[0].scriptSig = CScript() << 1 << OP_0;
    CTransaction tx4 {mtx};
    block.vtx[0] = tx4;
    EXPECT_TRUE(ContextualCheckBlock(block, state, Params(), &indexPrev, true));
}

// TEST PLAN: first, check that each ruleset accepts its own transaction type.
// Currently (May 2018) this means we'll test Sprout-Sprout,
// Overwinter-Overwinter, and Sapling-Sapling.

// Test block evaluated under Sprout rules will accept Sprout transactions.
// This test assumes that mainnet Overwinter activation is at least height 2.
TEST_F(ContextualCheckBlockTest, BlockSproutRulesAcceptSproutTx) {
    CMutableTransaction mtx = GetFirstBlockCoinbaseTx();

    // Make it a Sprout transaction w/o JoinSplits
    mtx.fOverwintered = false;
    mtx.nVersion = 1;

    SCOPED_TRACE("BlockSproutRulesAcceptSproutTx");
    ExpectValidBlockFromTx(CTransaction(mtx));
}


// Test block evaluated under Overwinter rules will accept Overwinter transactions.
TEST_F(ContextualCheckBlockTest, BlockOverwinterRulesAcceptOverwinterTx) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, 1);

    CMutableTransaction mtx = GetFirstBlockCoinbaseTx();

    // Make it an Overwinter transaction
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;

    SCOPED_TRACE("BlockOverwinterRulesAcceptOverwinterTx");
    ExpectValidBlockFromTx(CTransaction(mtx));
}


// Test that a block evaluated under Sapling rules can contain Sapling transactions.
TEST_F(ContextualCheckBlockTest, BlockSaplingRulesAcceptSaplingTx) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, 1);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, 1);

    CMutableTransaction mtx = GetFirstBlockCoinbaseTx();

    // Make it a Sapling transaction
    mtx.fOverwintered = true;
    mtx.nVersion = SAPLING_TX_VERSION;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;

    SCOPED_TRACE("BlockSaplingRulesAcceptSaplingTx");
    ExpectValidBlockFromTx(CTransaction(mtx));
}


// Test that a block evaluated under Blossom rules can contain Blossom transactions.
TEST_F(ContextualCheckBlockTest, BlockBlossomRulesAcceptBlossomTx) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, 1);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, 1);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, 1);

    CMutableTransaction mtx = GetFirstBlockCoinbaseTx();

    // Make it a Blossom transaction (using Sapling version/group id).
    mtx.fOverwintered = true;
    mtx.nVersion = SAPLING_TX_VERSION;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;

    SCOPED_TRACE("BlockBlossomRulesAcceptBlossomTx");
    ExpectValidBlockFromTx(CTransaction(mtx));
}


// TEST PLAN: next, check that each ruleset will not accept other transaction
// types. Currently (February 2020) this means with each of four *branches* active
// (Sprout, Overwinter, Sapling, Blossom), we'll test that transactions for tx
// *versions* not valid on that branch are rejected.
//
// Note that Sapling and Blossom transactions use the same tx version and version
// group id, but different consensus branch ids. These tests use transactions with
// no inputs and only a transparent output, therefore they are not signed and do not
// depend on the consensus branch id. Testing that Sapling rejects transactions that
// are signed for Blossom, and vice versa, is outside the scope of these tests.
//
// TODO: Change the testing approach to not require O(branches * versions) code.


// Test that a block evaluated under Sprout rules cannot contain non-Sprout
// transactions which require Overwinter to be active.  This test assumes that
// mainnet Overwinter activation is at least height 2.
TEST_F(ContextualCheckBlockTest, BlockSproutRulesRejectOtherTx) {
    CMutableTransaction mtx = GetFirstBlockCoinbaseTx();

    // Make it an Overwinter transaction
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;

    {
        SCOPED_TRACE("BlockSproutRulesRejectOverwinterTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 100, "tx-overwinter-not-active");
    }

    // Make it a Sapling transaction
    mtx.fOverwintered = true;
    mtx.nVersion = SAPLING_TX_VERSION;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;

    {
        SCOPED_TRACE("BlockSproutRulesRejectSaplingTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 100, "tx-overwinter-not-active");
    }
};


// Test block evaluated under Overwinter rules cannot contain non-Overwinter
// transactions.
TEST_F(ContextualCheckBlockTest, BlockOverwinterRulesRejectOtherTx) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, 1);

    CMutableTransaction mtx = GetFirstBlockCoinbaseTx();

    // Set the version to Sprout+JoinSplit (but nJoinSplit will be 0).
    mtx.nVersion = 2;

    {
        SCOPED_TRACE("BlockOverwinterRulesRejectSproutTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 100, "tx-overwintered-flag-not-set");
    }

    // Make it a Sapling transaction
    mtx.fOverwintered = true;
    mtx.nVersion = SAPLING_TX_VERSION;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;

    {
        SCOPED_TRACE("BlockOverwinterRulesRejectSaplingTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 100, "bad-tx-overwinter-version-too-high");
    }
}


// Test block evaluated under Sapling rules cannot contain non-Sapling transactions.
TEST_F(ContextualCheckBlockTest, BlockSaplingRulesRejectOtherTx) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, 1);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, 1);

    CMutableTransaction mtx = GetFirstBlockCoinbaseTx();

    // Set the version to Sprout+JoinSplit (but nJoinSplit will be 0).
    mtx.nVersion = 2;

    {
        SCOPED_TRACE("BlockSaplingRulesRejectSproutTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 100, "tx-overwintered-flag-not-set");
    }

    // Make it an Overwinter transaction
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;

    {
        SCOPED_TRACE("BlockSaplingRulesRejectOverwinterTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 100, "bad-sapling-tx-version-group-id");
    }
}


// Test block evaluated under Blossom rules cannot contain non-Blossom transactions.
TEST_F(ContextualCheckBlockTest, BlockBlossomRulesRejectOtherTx) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, 1);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, 1);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, 1);

    CMutableTransaction mtx = GetFirstBlockCoinbaseTx();

    // Set the version to Sprout+JoinSplit (but nJoinSplit will be 0).
    mtx.nVersion = 2;

    {
        SCOPED_TRACE("BlockBlossomRulesRejectSproutTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 100, "tx-overwintered-flag-not-set");
    }

    // Make it an Overwinter transaction
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;

    {
        SCOPED_TRACE("BlockBlossomRulesRejectOverwinterTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 100, "bad-sapling-tx-version-group-id");
    }
}
