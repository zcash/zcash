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

TEST(CheckBlock, VersionTooLow) {
    auto verifier = libzcash::ProofVerifier::Strict();

    CBlock block;
    block.nVersion = 1;

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "version-too-low", false)).Times(1);
    EXPECT_FALSE(CheckBlock(block, state, verifier, false, false));
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


TEST(ContextualCheckBlock, BadCoinbaseHeight) {
    SelectParams(CBaseChainParams::MAIN);

    // Create a block with no height in scriptSig
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << OP_0;
    mtx.vout.resize(1);
    mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    mtx.vout[0].nValue = 0;
    CTransaction tx {mtx};
    CBlock block;
    block.vtx.push_back(tx);

    // Treating block as genesis should pass
    MockCValidationState state;
    EXPECT_TRUE(ContextualCheckBlock(block, state, NULL));

    // Treating block as non-genesis should fail
    mtx.vout.push_back(CTxOut(GetBlockSubsidy(1, Params().GetConsensus())/5, Params().GetFoundersRewardScriptAtHeight(1)));
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

// Test that a block evaluated under Sprout rules cannot contain Overwinter transactions.
// This test assumes that mainnet Overwinter activation is at least height 2.
TEST(ContextualCheckBlock, BlockSproutRulesRejectOverwinterTx) {
    SelectParams(CBaseChainParams::MAIN);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 1 << OP_0;
    mtx.vout.resize(1);
    mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    mtx.vout[0].nValue = 0;

    mtx.fOverwintered = true;
    mtx.nVersion = 3;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;

    CTransaction tx {mtx};
    CBlock block;
    block.vtx.push_back(tx);

    MockCValidationState state;
    CBlockIndex indexPrev {Params().GenesisBlock()};

    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "tx-overwinter-not-active", false)).Times(1);
    EXPECT_FALSE(ContextualCheckBlock(block, state, &indexPrev));
}


// Test block evaluated under Sprout rules will accept Sprout transactions
TEST(ContextualCheckBlock, BlockSproutRulesAcceptSproutTx) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);

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
    mtx.nVersion = 1;

    CTransaction tx {mtx};
    CBlock block;
    block.vtx.push_back(tx);
    MockCValidationState state;
    CBlockIndex indexPrev {Params().GenesisBlock()};

    EXPECT_TRUE(ContextualCheckBlock(block, state, &indexPrev));

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}


// Test block evaluated under Overwinter rules will accept Overwinter transactions
TEST(ContextualCheckBlock, BlockOverwinterRulesAcceptOverwinterTx) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, 1);

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
    mtx.fOverwintered = true;
    mtx.nVersion = 3;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;

    CTransaction tx {mtx};
    CBlock block;
    block.vtx.push_back(tx);
    MockCValidationState state;
    CBlockIndex indexPrev {Params().GenesisBlock()};

    EXPECT_TRUE(ContextualCheckBlock(block, state, &indexPrev));

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}



// Test block evaluated under Overwinter rules will reject Sprout transactions
TEST(ContextualCheckBlock, BlockOverwinterRulesRejectSproutTx) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, 1);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 1 << OP_0;
    mtx.vout.resize(1);
    mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    mtx.vout[0].nValue = 0;

    mtx.nVersion = 2;

    CTransaction tx {mtx};
    CBlock block;
    block.vtx.push_back(tx);

    MockCValidationState state;
    CBlockIndex indexPrev {Params().GenesisBlock()};

    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "tx-overwinter-active", false)).Times(1);
    EXPECT_FALSE(ContextualCheckBlock(block, state, &indexPrev));

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}