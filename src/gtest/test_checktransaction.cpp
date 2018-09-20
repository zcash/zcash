#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sodium.h>

#include "main.h"
#include "primitives/transaction.h"
#include "consensus/validation.h"

extern ZCJoinSplit* params;

TEST(checktransaction_tests, check_vpub_not_both_nonzero) {
    CMutableTransaction tx;
    tx.nVersion = 2;

    {
        // Ensure that values within the joinsplit are well-formed.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.vjoinsplit.push_back(JSDescription());

        JSDescription *jsdesc = &newTx.vjoinsplit[0];
        jsdesc->vpub_old = 1;
        jsdesc->vpub_new = 1;

        EXPECT_FALSE(CheckTransactionWithoutProofVerification(newTx, state));
        EXPECT_EQ(state.GetRejectReason(), "bad-txns-vpubs-both-nonzero");
    }
}

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

void CreateJoinSplitSignature(CMutableTransaction& mtx, uint32_t consensusBranchId);

CMutableTransaction GetValidTransaction() {
    uint32_t consensusBranchId = SPROUT_BRANCH_ID;

    CMutableTransaction mtx;
    mtx.vin.resize(2);
    mtx.vin[0].prevout.hash = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    mtx.vin[0].prevout.n = 0;
    mtx.vin[1].prevout.hash = uint256S("0000000000000000000000000000000000000000000000000000000000000002");
    mtx.vin[1].prevout.n = 0;
    mtx.vout.resize(2);
    // mtx.vout[0].scriptPubKey = 
    mtx.vout[0].nValue = 0;
    mtx.vout[1].nValue = 0;
    mtx.vjoinsplit.resize(2);
    mtx.vjoinsplit[0].nullifiers.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
    mtx.vjoinsplit[0].nullifiers.at(1) = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    mtx.vjoinsplit[1].nullifiers.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000002");
    mtx.vjoinsplit[1].nullifiers.at(1) = uint256S("0000000000000000000000000000000000000000000000000000000000000003");

    CreateJoinSplitSignature(mtx, consensusBranchId);
    return mtx;
}

void CreateJoinSplitSignature(CMutableTransaction& mtx, uint32_t consensusBranchId) {
    // Generate an ephemeral keypair.
    uint256 joinSplitPubKey;
    unsigned char joinSplitPrivKey[crypto_sign_SECRETKEYBYTES];
    crypto_sign_keypair(joinSplitPubKey.begin(), joinSplitPrivKey);
    mtx.joinSplitPubKey = joinSplitPubKey;

    // Compute the correct hSig.
    // TODO: #966.
    static const uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    // Empty output script.
    CScript scriptCode;
    CTransaction signTx(mtx);
    uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);
    if (dataToBeSigned == one) {
        throw std::runtime_error("SignatureHash failed");
    }

    // Add the signature
    assert(crypto_sign_detached(&mtx.joinSplitSig[0], NULL,
                         dataToBeSigned.begin(), 32,
                         joinSplitPrivKey
                        ) == 0);
}

TEST(checktransaction_tests, valid_transaction) {
    CMutableTransaction mtx = GetValidTransaction();
    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
}

TEST(checktransaction_tests, BadVersionTooLow) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.nVersion = 0;

    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-version-too-low", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_vin_empty) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0);
    mtx.vin.resize(0);

    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_vout_empty) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0);
    mtx.vout.resize(0);

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, BadTxnsOversize) {
    SelectParams(CBaseChainParams::REGTEST);
    CMutableTransaction mtx = GetValidTransaction();

    mtx.vin[0].scriptSig = CScript();
    std::vector<unsigned char> vchData(520);
    for (unsigned int i = 0; i < 190; ++i)
        mtx.vin[0].scriptSig << vchData << OP_DROP;
    mtx.vin[0].scriptSig << OP_1;

    {
        // Transaction is just under the limit...
        CTransaction tx(mtx);
        CValidationState state;
        ASSERT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
    }

    // Not anymore!
    mtx.vin[1].scriptSig << vchData << OP_DROP;
    mtx.vin[1].scriptSig << OP_1;

    {
        CTransaction tx(mtx);
        ASSERT_EQ(::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION), 100202);

        // Passes non-contextual checks...
        MockCValidationState state;
        EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));

        // ... but fails contextual ones!
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-oversize", false)).Times(1);
        EXPECT_FALSE(ContextualCheckTransaction(tx, state, 1, 100));
    }

    {
        // But should be fine again once Sapling activates!
        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

        mtx.fOverwintered = true;
        mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
        mtx.nVersion = SAPLING_TX_VERSION;

        // Change the proof types (which requires re-signing the JoinSplit data)
        mtx.vjoinsplit[0].proof = libzcash::GrothProof();
        mtx.vjoinsplit[1].proof = libzcash::GrothProof();
        CreateJoinSplitSignature(mtx, NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId);

        CTransaction tx(mtx);
        EXPECT_EQ(::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION), 103713);

        MockCValidationState state;
        EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
        EXPECT_TRUE(ContextualCheckTransaction(tx, state, 1, 100));

        // Revert to default
        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
        UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    }
}

TEST(checktransaction_tests, OversizeSaplingTxns) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    CMutableTransaction mtx = GetValidTransaction();
    mtx.fOverwintered = true;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.nVersion = SAPLING_TX_VERSION;

    // Change the proof types (which requires re-signing the JoinSplit data)
    mtx.vjoinsplit[0].proof = libzcash::GrothProof();
    mtx.vjoinsplit[1].proof = libzcash::GrothProof();
    CreateJoinSplitSignature(mtx, NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId);

    // Transaction just under the limit
    mtx.vin[0].scriptSig = CScript();
    std::vector<unsigned char> vchData(520);
    for (unsigned int i = 0; i < 3809; ++i)
        mtx.vin[0].scriptSig << vchData << OP_DROP;
    std::vector<unsigned char> vchDataRemainder(453);
    mtx.vin[0].scriptSig << vchDataRemainder << OP_DROP;
    mtx.vin[0].scriptSig << OP_1;

    {
        CTransaction tx(mtx);
        EXPECT_EQ(::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION), MAX_TX_SIZE_AFTER_SAPLING - 1);

        CValidationState state;
        EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
    }

    // Transaction equal to the limit
    mtx.vin[1].scriptSig << OP_1;

    {
        CTransaction tx(mtx);
        EXPECT_EQ(::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION), MAX_TX_SIZE_AFTER_SAPLING);

        CValidationState state;
        EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
    }

    // Transaction just over the limit
    mtx.vin[1].scriptSig << OP_1;

    {
        CTransaction tx(mtx);
        EXPECT_EQ(::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION), MAX_TX_SIZE_AFTER_SAPLING + 1);

        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-oversize", false)).Times(1);
        EXPECT_FALSE(CheckTransactionWithoutProofVerification(tx, state));
    }

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}

TEST(checktransaction_tests, bad_txns_vout_negative) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = -1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_vout_toolarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = MAX_MONEY + 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_txouttotal_toolarge_outputs) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = MAX_MONEY;
    mtx.vout[1].nValue = 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, ValueBalanceNonZero) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.valueBalance = 10;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-valuebalance-nonzero", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, PositiveValueBalanceTooLarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vShieldedSpend.resize(1);
    mtx.valueBalance = MAX_MONEY + 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-valuebalance-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, NegativeValueBalanceTooLarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vShieldedSpend.resize(1);
    mtx.valueBalance = -(MAX_MONEY + 1);

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-valuebalance-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, ValueBalanceOverflowsTotal) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vShieldedSpend.resize(1);
    mtx.vout[0].nValue = 1;
    mtx.valueBalance = -MAX_MONEY;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_txouttotal_toolarge_joinsplit) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = 1;
    mtx.vjoinsplit[0].vpub_old = MAX_MONEY;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_txintotal_toolarge_joinsplit) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit[0].vpub_new = MAX_MONEY - 1;
    mtx.vjoinsplit[1].vpub_new = MAX_MONEY - 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txintotal-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_vpub_old_negative) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit[0].vpub_old = -1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_old-negative", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_vpub_new_negative) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit[0].vpub_new = -1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_new-negative", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_vpub_old_toolarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit[0].vpub_old = MAX_MONEY + 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_old-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_vpub_new_toolarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit[0].vpub_new = MAX_MONEY + 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_new-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_vpubs_both_nonzero) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit[0].vpub_old = 1;
    mtx.vjoinsplit[0].vpub_new = 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpubs-both-nonzero", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_inputs_duplicate) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vin[1].prevout.hash = mtx.vin[0].prevout.hash;
    mtx.vin[1].prevout.n = mtx.vin[0].prevout.n;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_joinsplits_nullifiers_duplicate_same_joinsplit) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit[0].nullifiers.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
    mtx.vjoinsplit[0].nullifiers.at(1) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-joinsplits-nullifiers-duplicate", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_joinsplits_nullifiers_duplicate_different_joinsplit) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit[0].nullifiers.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
    mtx.vjoinsplit[1].nullifiers.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-joinsplits-nullifiers-duplicate", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_cb_has_joinsplits) {
    CMutableTransaction mtx = GetValidTransaction();
    // Make it a coinbase.
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    mtx.vjoinsplit.resize(1);

    CTransaction tx(mtx);
    EXPECT_TRUE(tx.IsCoinBase());

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-has-joinsplits", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_cb_empty_scriptsig) {
    CMutableTransaction mtx = GetValidTransaction();
    // Make it a coinbase.
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    mtx.vjoinsplit.resize(0);

    CTransaction tx(mtx);
    EXPECT_TRUE(tx.IsCoinBase());

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-length", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_prevout_null) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vin[1].prevout.SetNull();

    CTransaction tx(mtx);
    EXPECT_FALSE(tx.IsCoinBase());

    MockCValidationState state;
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "bad-txns-prevout-null", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(checktransaction_tests, bad_txns_invalid_joinsplit_signature) {
    SelectParams(CBaseChainParams::REGTEST);

    CMutableTransaction mtx = GetValidTransaction();
    mtx.joinSplitSig[0] += 1;
    CTransaction tx(mtx);

    MockCValidationState state;
    // during initial block download, DoS ban score should be zero, else 100
    EXPECT_CALL(state, DoS(0, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, 0, 100, []() { return true; });
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, 0, 100, []() { return false; });
}

TEST(checktransaction_tests, non_canonical_ed25519_signature) {
    SelectParams(CBaseChainParams::REGTEST);

    CMutableTransaction mtx = GetValidTransaction();

    // Check that the signature is valid before we add L
    {
        CTransaction tx(mtx);
        MockCValidationState state;
        EXPECT_TRUE(ContextualCheckTransaction(tx, state, 0, 100));
    }

    // Copied from libsodium/crypto_sign/ed25519/ref10/open.c
    static const unsigned char L[32] =
      { 0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
        0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 };

    // Add L to S, which starts at mtx.joinSplitSig[32].
    unsigned int s = 0;
    for (size_t i = 0; i < 32; i++) {
        s = mtx.joinSplitSig[32 + i] + L[i] + (s >> 8);
        mtx.joinSplitSig[32 + i] = s & 0xff;
    }

    CTransaction tx(mtx);

    MockCValidationState state;
    // during initial block download, DoS ban score should be zero, else 100
    EXPECT_CALL(state, DoS(0, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, 0, 100, []() { return true; });
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, 0, 100, []() { return false; });
}

TEST(checktransaction_tests, OverwinterConstructors) {
    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 20;

    // Check constructor with overwinter fields
    CTransaction tx(mtx);
    EXPECT_EQ(tx.nVersion, mtx.nVersion);
    EXPECT_EQ(tx.fOverwintered, mtx.fOverwintered);
    EXPECT_EQ(tx.nVersionGroupId, mtx.nVersionGroupId);
    EXPECT_EQ(tx.nExpiryHeight, mtx.nExpiryHeight);

    // Check constructor of mutable transaction struct
    CMutableTransaction mtx2(tx);
    EXPECT_EQ(mtx2.nVersion, mtx.nVersion);
    EXPECT_EQ(mtx2.fOverwintered, mtx.fOverwintered);
    EXPECT_EQ(mtx2.nVersionGroupId, mtx.nVersionGroupId);
    EXPECT_EQ(mtx2.nExpiryHeight, mtx.nExpiryHeight);
    EXPECT_TRUE(mtx2.GetHash() == mtx.GetHash());

    // Check assignment of overwinter fields
    CTransaction tx2 = tx;
    EXPECT_EQ(tx2.nVersion, mtx.nVersion);
    EXPECT_EQ(tx2.fOverwintered, mtx.fOverwintered);
    EXPECT_EQ(tx2.nVersionGroupId, mtx.nVersionGroupId);
    EXPECT_EQ(tx2.nExpiryHeight, mtx.nExpiryHeight);
    EXPECT_TRUE(tx2 == tx);
}

TEST(checktransaction_tests, OverwinterSerialization) {
    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 99;

    // Check round-trip serialization and deserialization from mtx to tx.
    {
        CDataStream ss(SER_DISK, PROTOCOL_VERSION);
        ss << mtx;
        CTransaction tx;
        ss >> tx;
        EXPECT_EQ(mtx.nVersion, tx.nVersion);
        EXPECT_EQ(mtx.fOverwintered, tx.fOverwintered);
        EXPECT_EQ(mtx.nVersionGroupId, tx.nVersionGroupId);
        EXPECT_EQ(mtx.nExpiryHeight, tx.nExpiryHeight);

        EXPECT_EQ(mtx.GetHash(), CMutableTransaction(tx).GetHash());
        EXPECT_EQ(tx.GetHash(), CTransaction(mtx).GetHash());
    }

    // Also check mtx to mtx
    {
        CDataStream ss(SER_DISK, PROTOCOL_VERSION);
        ss << mtx;
        CMutableTransaction mtx2;
        ss >> mtx2;
        EXPECT_EQ(mtx.nVersion, mtx2.nVersion);
        EXPECT_EQ(mtx.fOverwintered, mtx2.fOverwintered);
        EXPECT_EQ(mtx.nVersionGroupId, mtx2.nVersionGroupId);
        EXPECT_EQ(mtx.nExpiryHeight, mtx2.nExpiryHeight);

        EXPECT_EQ(mtx.GetHash(), mtx2.GetHash());
    }

    // Also check tx to tx
    {
        CTransaction tx(mtx);
        CDataStream ss(SER_DISK, PROTOCOL_VERSION);
        ss << tx;
        CTransaction tx2;
        ss >> tx2;
        EXPECT_EQ(tx.nVersion, tx2.nVersion);
        EXPECT_EQ(tx.fOverwintered, tx2.fOverwintered);
        EXPECT_EQ(tx.nVersionGroupId, tx2.nVersionGroupId);
        EXPECT_EQ(tx.nExpiryHeight, tx2.nExpiryHeight);

        EXPECT_EQ(mtx.GetHash(), CMutableTransaction(tx).GetHash());
        EXPECT_EQ(tx.GetHash(), tx2.GetHash());
    }
}

TEST(checktransaction_tests, OverwinterDefaultValues) {
    // Check default values (this will fail when defaults change; test should then be updated)
    CTransaction tx;
    EXPECT_EQ(tx.nVersion, 1);
    EXPECT_EQ(tx.fOverwintered, false);
    EXPECT_EQ(tx.nVersionGroupId, 0);
    EXPECT_EQ(tx.nExpiryHeight, 0);
}

// A valid v3 transaction with no joinsplits
TEST(checktransaction_tests, OverwinterValidTx) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;
    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
}

TEST(checktransaction_tests, OverwinterExpiryHeight) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    {
        CTransaction tx(mtx);
        MockCValidationState state;
        EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
    }

    {
        mtx.nExpiryHeight = TX_EXPIRY_HEIGHT_THRESHOLD - 1;
        CTransaction tx(mtx);
        MockCValidationState state;
        EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
    }

    {
        mtx.nExpiryHeight = TX_EXPIRY_HEIGHT_THRESHOLD;
        CTransaction tx(mtx);
        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-tx-expiry-height-too-high", false)).Times(1);
        CheckTransactionWithoutProofVerification(tx, state);
    }

    {
        mtx.nExpiryHeight = std::numeric_limits<uint32_t>::max();
        CTransaction tx(mtx);
        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-tx-expiry-height-too-high", false)).Times(1);
        CheckTransactionWithoutProofVerification(tx, state);
    }
}


// Test that a Sprout tx with a negative version number is detected
// given the new Overwinter logic
TEST(checktransaction_tests, SproutTxVersionTooLow) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0);
    mtx.fOverwintered = false;
    mtx.nVersion = -1;

    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-version-too-low", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}



// Subclass of CTransaction which doesn't call UpdateHash when constructing
// from a CMutableTransaction.  This enables us to create a CTransaction
// with bad values which normally trigger an exception during construction.
class UNSAFE_CTransaction : public CTransaction {
    public:
        UNSAFE_CTransaction(const CMutableTransaction &tx) : CTransaction(tx, true) {}
};

TEST(checktransaction_tests, SaplingSproutInputSumsTooLarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = SAPLING_TX_VERSION;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    {
        // create JSDescription
        uint256 rt;
        uint256 joinSplitPubKey;
        std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs = {
            libzcash::JSInput(),
            libzcash::JSInput()
        };
        std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs = {
            libzcash::JSOutput(),
            libzcash::JSOutput()
        };
        std::array<size_t, ZC_NUM_JS_INPUTS> inputMap;
        std::array<size_t, ZC_NUM_JS_OUTPUTS> outputMap;

        auto jsdesc = JSDescription::Randomized(
            true,
            *params, joinSplitPubKey, rt,
            inputs, outputs,
            inputMap, outputMap,
            0, 0, false);

        mtx.vjoinsplit.push_back(jsdesc);
    }

    mtx.vShieldedSpend.push_back(SpendDescription());

    mtx.vjoinsplit[0].vpub_new = (MAX_MONEY / 2) + 10;

    {
        UNSAFE_CTransaction tx(mtx);
        CValidationState state;
        EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
    }

    mtx.valueBalance = (MAX_MONEY / 2) + 10;

    {
        UNSAFE_CTransaction tx(mtx);
        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txintotal-toolarge", false)).Times(1);
        CheckTransactionWithoutProofVerification(tx, state);
    }
}

// Test bad Overwinter version number in CheckTransactionWithoutProofVerification
TEST(checktransaction_tests, OverwinterVersionNumberLow) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_MIN_TX_VERSION - 1;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    UNSAFE_CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-tx-overwinter-version-too-low", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

// Test bad Overwinter version number in ContextualCheckTransaction
TEST(checktransaction_tests, OverwinterVersionNumberHigh) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_MAX_TX_VERSION + 1;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    UNSAFE_CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-tx-overwinter-version-too-high", false)).Times(1);
    ContextualCheckTransaction(tx, state, 1, 100);

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}


// Test bad Overwinter version group id
TEST(checktransaction_tests, OverwinterBadVersionGroupId) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vjoinsplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nExpiryHeight = 0;
    mtx.nVersionGroupId = 0x12345678;

    UNSAFE_CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-tx-version-group-id", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

// This tests an Overwinter transaction checked against Sprout
TEST(checktransaction_tests, OverwinterNotActive) {
    SelectParams(CBaseChainParams::TESTNET);

    CMutableTransaction mtx = GetValidTransaction();
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    CTransaction tx(mtx);
    MockCValidationState state;
    // during initial block download, DoS ban score should be zero, else 100
    EXPECT_CALL(state, DoS(0, false, REJECT_INVALID, "tx-overwinter-not-active", false)).Times(1);
    ContextualCheckTransaction(tx, state, 1, 100, []() { return true; });
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "tx-overwinter-not-active", false)).Times(1);
    ContextualCheckTransaction(tx, state, 1, 100, []() { return false; });
}

// This tests a transaction without the fOverwintered flag set, against the Overwinter consensus rule set.
TEST(checktransaction_tests, OverwinterFlagNotSet) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    CMutableTransaction mtx = GetValidTransaction();
    mtx.fOverwintered = false;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "tx-overwinter-flag-not-set", false)).Times(1);
    ContextualCheckTransaction(tx, state, 1, 100);

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}


// Overwinter (NU0) does not allow soft fork to version 4 Overwintered tx.
TEST(checktransaction_tests, OverwinterInvalidSoftForkVersion) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.fOverwintered = true;
    mtx.nVersion = 4; // This is not allowed
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    CDataStream ss(SER_DISK, PROTOCOL_VERSION);
    try {
        ss << mtx;
        FAIL() << "Expected std::ios_base::failure 'Unknown transaction format'";
    }
    catch(std::ios_base::failure & err) {
        EXPECT_THAT(err.what(), testing::HasSubstr(std::string("Unknown transaction format")));
    }
    catch(...) {
        FAIL() << "Expected std::ios_base::failure 'Unknown transaction format', got some other exception";
    }
}


// Test CreateNewContextualCMutableTransaction sets default values based on height
TEST(checktransaction_tests, OverwinteredContextualCreateTx) {
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& consensusParams = Params().GetConsensus();
    int activationHeight = 5;
    int saplingActivationHeight = 30;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, activationHeight);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, saplingActivationHeight);

    {
        CMutableTransaction mtx = CreateNewContextualCMutableTransaction(
            consensusParams, activationHeight - 1);

        EXPECT_EQ(mtx.nVersion, 1);
        EXPECT_EQ(mtx.fOverwintered, false);
        EXPECT_EQ(mtx.nVersionGroupId, 0);
        EXPECT_EQ(mtx.nExpiryHeight, 0);
    }

    // Overwinter activates
    {
        CMutableTransaction mtx = CreateNewContextualCMutableTransaction(
            consensusParams, activationHeight );

        EXPECT_EQ(mtx.nVersion, 3);
        EXPECT_EQ(mtx.fOverwintered, true);
        EXPECT_EQ(mtx.nVersionGroupId, OVERWINTER_VERSION_GROUP_ID);
        EXPECT_EQ(mtx.nExpiryHeight, activationHeight + expiryDelta);
    }

    // Close to Sapling activation
    {
        CMutableTransaction mtx = CreateNewContextualCMutableTransaction(
            consensusParams, saplingActivationHeight - expiryDelta - 2);

        EXPECT_EQ(mtx.fOverwintered, true);
        EXPECT_EQ(mtx.nVersionGroupId, OVERWINTER_VERSION_GROUP_ID);
        EXPECT_EQ(mtx.nVersion, OVERWINTER_TX_VERSION);
        EXPECT_EQ(mtx.nExpiryHeight, saplingActivationHeight - 2);
    }

    {
        CMutableTransaction mtx = CreateNewContextualCMutableTransaction(
            consensusParams, saplingActivationHeight - expiryDelta - 1);

        EXPECT_EQ(mtx.fOverwintered, true);
        EXPECT_EQ(mtx.nVersionGroupId, OVERWINTER_VERSION_GROUP_ID);
        EXPECT_EQ(mtx.nVersion, OVERWINTER_TX_VERSION);
        EXPECT_EQ(mtx.nExpiryHeight, saplingActivationHeight - 1);
    }

    {
        CMutableTransaction mtx = CreateNewContextualCMutableTransaction(
            consensusParams, saplingActivationHeight - expiryDelta);

        EXPECT_EQ(mtx.fOverwintered, true);
        EXPECT_EQ(mtx.nVersionGroupId, OVERWINTER_VERSION_GROUP_ID);
        EXPECT_EQ(mtx.nVersion, OVERWINTER_TX_VERSION);
        EXPECT_EQ(mtx.nExpiryHeight, saplingActivationHeight - 1);
    }

    // Just before Sapling activation
    {
        CMutableTransaction mtx = CreateNewContextualCMutableTransaction(
            consensusParams, saplingActivationHeight - 1);

        EXPECT_EQ(mtx.fOverwintered, true);
        EXPECT_EQ(mtx.nVersionGroupId, OVERWINTER_VERSION_GROUP_ID);
        EXPECT_EQ(mtx.nVersion, OVERWINTER_TX_VERSION);
        EXPECT_EQ(mtx.nExpiryHeight, saplingActivationHeight - 1);
    }

    // Sapling activates
    {
        CMutableTransaction mtx = CreateNewContextualCMutableTransaction(
            consensusParams, saplingActivationHeight);

        EXPECT_EQ(mtx.fOverwintered, true);
        EXPECT_EQ(mtx.nVersionGroupId, SAPLING_VERSION_GROUP_ID);
        EXPECT_EQ(mtx.nVersion, SAPLING_TX_VERSION);
        EXPECT_EQ(mtx.nExpiryHeight, saplingActivationHeight + expiryDelta);
    }

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}

// Test a v1 transaction which has a malformed header, perhaps modified in-flight
TEST(checktransaction_tests, BadTxReceivedOverNetwork)
{
    // First four bytes <01 00 00 00> have been modified to be <FC FF FF FF> (-4 as an int32)
    std::string goodPrefix = "01000000";
    std::string badPrefix = "fcffffff";
    std::string hexTx = "0176c6541939b95f8d8b7779a77a0863b2a0267e281a050148326f0ea07c3608fb000000006a47304402207c68117a6263486281af0cc5d3bee6db565b6dce19ffacc4cb361906eece82f8022007f604382dee2c1fde41c4e6e7c1ae36cfa28b5b27350c4bfaa27f555529eace01210307ff9bef60f2ac4ceb1169a9f7d2c773d6c7f4ab6699e1e5ebc2e0c6d291c733feffffff02c0d45407000000001976a9145eaaf6718517ec8a291c6e64b16183292e7011f788ac5ef44534000000001976a91485e12fb9967c96759eae1c6b1e9c07ce977b638788acbe000000";

    // Good v1 tx
    {
        std::vector<unsigned char> txData(ParseHex(goodPrefix + hexTx ));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        CTransaction tx;
        ssData >> tx;
        EXPECT_EQ(tx.nVersion, 1);
        EXPECT_EQ(tx.fOverwintered, false);
    }

    // Good v1 mutable tx
    {
        std::vector<unsigned char> txData(ParseHex(goodPrefix + hexTx ));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        CMutableTransaction mtx;
        ssData >> mtx;
        EXPECT_EQ(mtx.nVersion, 1);
    }

    // Bad tx
    {
        std::vector<unsigned char> txData(ParseHex(badPrefix + hexTx ));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            CTransaction tx;
            ssData >> tx;
            FAIL() << "Expected std::ios_base::failure 'Unknown transaction format'";
        }
        catch(std::ios_base::failure & err) {
            EXPECT_THAT(err.what(), testing::HasSubstr(std::string("Unknown transaction format")));
        }
        catch(...) {
            FAIL() << "Expected std::ios_base::failure 'Unknown transaction format', got some other exception";
        }
    }

    // Bad mutable tx
    {
        std::vector<unsigned char> txData(ParseHex(badPrefix + hexTx ));
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            CMutableTransaction mtx;
            ssData >> mtx;
            FAIL() << "Expected std::ios_base::failure 'Unknown transaction format'";
        }
        catch(std::ios_base::failure & err) {
            EXPECT_THAT(err.what(), testing::HasSubstr(std::string("Unknown transaction format")));
        }
        catch(...) {
            FAIL() << "Expected std::ios_base::failure 'Unknown transaction format', got some other exception";
        }
    }
}
