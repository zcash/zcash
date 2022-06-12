#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "consensus/validation.h"
#include "main.h"
#include "zcash/Proof.hpp"

class MockCValidationState : public CValidationState {
public:
    MOCK_METHOD5(DoS, bool(int level, bool ret,
             unsigned char chRejectCodeIn, std::string strRejectReasonIn,
             bool corruptionIn));
    MOCK_METHOD3(Invalid, bool(bool ret,
                 unsigned char _chRejectCode, std::string _strRejectReason));
    MOCK_METHOD1(Error, bool(std::string strRejectReasonIn));
    MOCK_CONST_METHOD0(IsValid, bool());
    MOCK_CONST_METHOD0(IsInvalid, bool());
    MOCK_CONST_METHOD0(IsError, bool());
    MOCK_CONST_METHOD1(IsInvalid, bool(int &nDoSOut));
    MOCK_CONST_METHOD0(CorruptionPossible, bool());
    MOCK_CONST_METHOD0(GetRejectCode, unsigned char());
    MOCK_CONST_METHOD0(GetRejectReason, std::string());
};

int32_t futureblock;
TEST(CheckBlock, VersionTooLow) {
    auto verifier = libzcash::ProofVerifier::Strict();

    CBlock block;
    block.nVersion = 1;

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "version-too-low", false)).Times(1);
    EXPECT_FALSE(CheckBlock(&futureblock,0,0,block, state, verifier, false, false));
}


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
        GetBlockSubsidy(1, Params().GetConsensus())/5,
        Params().GetFoundersRewardScriptAtHeight(1)));
    mtx.fOverwintered = false;
    mtx.nVersion = -1;
    mtx.nVersionGroupId = 0;

    CTransaction tx {mtx};
    CBlock block;
    block.vtx.push_back(tx);

    MockCValidationState state;
    CBlockIndex indexPrev {Params().GenesisBlock()};

    auto verifier = libzcash::ProofVerifier::Strict();

    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-version-too-low", false)).Times(1);
    EXPECT_FALSE(CheckBlock(block, state, verifier, false, false));
}


class ContextualCheckBlockTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        SelectParams(CBaseChainParams::MAIN);
    }

    virtual void TearDown() {
        // Revert to test default. No-op on mainnet params.
        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
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
        mtx.vout.push_back(CTxOut(
                    GetBlockSubsidy(1, Params().GetConsensus())/5,
                    Params().GetFoundersRewardScriptAtHeight(1)));

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
        EXPECT_TRUE(ContextualCheckBlock(block, state, &indexPrev));
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
        EXPECT_CALL(state, DoS(level, false, REJECT_INVALID, reason, false)).Times(1);
        EXPECT_FALSE(ContextualCheckBlock(block, state, &indexPrev));
    }

};


TEST_F(ContextualCheckBlockTest, BadCoinbaseHeight) {
    // Put a transaction in a block with no height in scriptSig
    CMutableTransaction mtx = GetFirstBlockCoinbaseTx();
    mtx.vin[0].scriptSig = CScript() << OP_0;
    mtx.vout.pop_back(); // remove the FR output

    CBlock block;
    block.vtx.push_back(mtx);

    // Treating block as genesis should pass
    MockCValidationState state;
    EXPECT_TRUE(ContextualCheckBlock(block, state, NULL));

    // Give the transaction a Founder's Reward vout
    mtx.vout.push_back(CTxOut(
                GetBlockSubsidy(1, Params().GetConsensus())/5,
                Params().GetFoundersRewardScriptAtHeight(1)));

    // Treating block as non-genesis should fail
    CTransaction tx2 {mtx};
    block.vtx[0] = tx2;
    CBlock prev;
    CBlockIndex indexPrev {prev};
    indexPrev.nHeight = 0;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-height", false)).Times(1);
    EXPECT_FALSE(ContextualCheckBlock(block, state, &indexPrev));

    // Setting to an incorrect height should fail
    mtx.vin[0].scriptSig = CScript() << 2 << OP_0;
    CTransaction tx3 {mtx};
    block.vtx[0] = tx3;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-height", false)).Times(1);
    EXPECT_FALSE(ContextualCheckBlock(block, state, &indexPrev));

    // After correcting the scriptSig, should pass
    mtx.vin[0].scriptSig = CScript() << 1 << OP_0;
    CTransaction tx4 {mtx};
    block.vtx[0] = tx4;
    EXPECT_TRUE(ContextualCheckBlock(block, state, &indexPrev));
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

// TEST PLAN: next, check that each ruleset will not accept other transaction
// types. Currently (May 2018) this means we'll test Sprout-Overwinter,
// Sprout-Sapling, Overwinter-Sprout, Overwinter-Sapling, Sapling-Sprout, and
// Sapling-Overwinter.

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
        ExpectInvalidBlockFromTx(CTransaction(mtx), 0, "tx-overwinter-not-active");
    }

    // Make it a Sapling transaction
    mtx.fOverwintered = true;
    mtx.nVersion = SAPLING_TX_VERSION;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;

    {
        SCOPED_TRACE("BlockSproutRulesRejectSaplingTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 0, "tx-overwinter-not-active");
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
        ExpectInvalidBlockFromTx(CTransaction(mtx), 100, "tx-overwinter-active");
    }

    // Make it a Sapling transaction
    mtx.fOverwintered = true;
    mtx.nVersion = SAPLING_TX_VERSION;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;

    {
        SCOPED_TRACE("BlockOverwinterRulesRejectSaplingTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 0, "bad-overwinter-tx-version-group-id");
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
        ExpectInvalidBlockFromTx(CTransaction(mtx), 100, "tx-overwinter-active");
    }

    // Make it an Overwinter transaction
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;

    {
        SCOPED_TRACE("BlockSaplingRulesRejectOverwinterTx");
        ExpectInvalidBlockFromTx(CTransaction(mtx), 0, "bad-sapling-tx-version-group-id");
    }
}
