#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "main.h"
#include "primitives/transaction.h"
#include "consensus/validation.h"
#include "transaction_builder.h"
#include "utiltest.h"
#include "zcash/JoinSplit.hpp"

#include <librustzcash.h>
#include <rust/ed25519.h>

// Subclass of CTransaction which doesn't call UpdateHash when constructing
// from a CMutableTransaction.  This enables us to create a CTransaction
// with bad values which normally trigger an exception during construction.
class UNSAFE_CTransaction : public CTransaction {
    public:
        UNSAFE_CTransaction(const CMutableTransaction &tx) : CTransaction(tx, true) {}
};

TEST(ChecktransactionTests, CheckVpubNotBothNonzero) {
    CMutableTransaction tx;
    tx.nVersion = 2;

    {
        // Ensure that values within the joinsplit are well-formed.
        CMutableTransaction newTx(tx);
        CValidationState state;

        newTx.vJoinSplit.push_back(JSDescription());

        JSDescription *jsdesc = &newTx.vJoinSplit[0];
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

CMutableTransaction GetValidTransaction(uint32_t consensusBranchId=SPROUT_BRANCH_ID) {
    CMutableTransaction mtx;
    if (consensusBranchId == NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId) {
        mtx.fOverwintered = true;
        mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
        mtx.nVersion = OVERWINTER_TX_VERSION;
    } else if (consensusBranchId == NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId) {
        mtx.fOverwintered = true;
        mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
        mtx.nVersion = SAPLING_TX_VERSION;
    } else if (consensusBranchId != SPROUT_BRANCH_ID) {
        // Unsupported consensus branch ID
        assert(false);
    }

    mtx.vin.resize(2);
    mtx.vin[0].prevout.hash = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    mtx.vin[0].prevout.n = 0;
    mtx.vin[1].prevout.hash = uint256S("0000000000000000000000000000000000000000000000000000000000000002");
    mtx.vin[1].prevout.n = 0;
    mtx.vout.resize(2);
    // mtx.vout[0].scriptPubKey =
    mtx.vout[0].nValue = 0;
    mtx.vout[1].nValue = 0;
    mtx.vJoinSplit.resize(2);
    mtx.vJoinSplit[0].nullifiers.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
    mtx.vJoinSplit[0].nullifiers.at(1) = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    mtx.vJoinSplit[1].nullifiers.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000002");
    mtx.vJoinSplit[1].nullifiers.at(1) = uint256S("0000000000000000000000000000000000000000000000000000000000000003");

    if (mtx.nVersion >= SAPLING_TX_VERSION) {
        libzcash::GrothProof emptyProof;
        mtx.vJoinSplit[0].proof = emptyProof;
        mtx.vJoinSplit[1].proof = emptyProof;
    }

    CreateJoinSplitSignature(mtx, consensusBranchId);
    return mtx;
}

void CreateJoinSplitSignature(CMutableTransaction& mtx, uint32_t consensusBranchId) {
    // Generate an ephemeral keypair.
    Ed25519SigningKey joinSplitPrivKey;
    ed25519_generate_keypair(&joinSplitPrivKey, &mtx.joinSplitPubKey);

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
    assert(ed25519_sign(
        &joinSplitPrivKey,
        dataToBeSigned.begin(), 32,
        &mtx.joinSplitSig));
}

TEST(ChecktransactionTests, ValidTransaction) {
    CMutableTransaction mtx = GetValidTransaction();
    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
}

TEST(ChecktransactionTests, BadVersionTooLow) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.nVersion = 0;

    EXPECT_THROW((CTransaction(mtx)), std::ios_base::failure);
    UNSAFE_CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-version-too-low", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsVinEmpty) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit.resize(0);
    mtx.vin.resize(0);

    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsVoutEmpty) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit.resize(0);
    mtx.vout.resize(0);

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsOversize) {
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
        EXPECT_FALSE(ContextualCheckTransaction(tx, state, Params(), 1, true));
    }

    {
        // But should be fine again once Sapling activates!
        RegtestActivateSapling();

        mtx.fOverwintered = true;
        mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
        mtx.nVersion = SAPLING_TX_VERSION;

        // Change the proof types (which requires re-signing the JoinSplit data)
        mtx.vJoinSplit[0].proof = libzcash::GrothProof();
        mtx.vJoinSplit[1].proof = libzcash::GrothProof();
        CreateJoinSplitSignature(mtx, NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId);

        CTransaction tx(mtx);
        EXPECT_EQ(::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION), 103713);

        MockCValidationState state;
        EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
        EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 1, true));

        // Revert to default
        RegtestDeactivateSapling();
    }
}

TEST(ChecktransactionTests, OversizeSaplingTxns) {
    RegtestActivateSapling();

    CMutableTransaction mtx = GetValidTransaction();
    mtx.fOverwintered = true;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.nVersion = SAPLING_TX_VERSION;

    // Change the proof types (which requires re-signing the JoinSplit data)
    mtx.vJoinSplit[0].proof = libzcash::GrothProof();
    mtx.vJoinSplit[1].proof = libzcash::GrothProof();
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
    RegtestDeactivateSapling();
}

TEST(ChecktransactionTests, BadTxnsVoutNegative) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = -1;

    EXPECT_THROW((CTransaction(mtx)), std::ios_base::failure);
    UNSAFE_CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsVoutToolarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = MAX_MONEY + 1;

    EXPECT_THROW((CTransaction(mtx)), std::ios_base::failure);
    UNSAFE_CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsTxouttotalToolargeOutputs) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = MAX_MONEY;
    mtx.vout[1].nValue = 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, ValueBalanceNonZero) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.valueBalanceSapling = 10;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-valuebalance-nonzero", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, PositiveValueBalanceTooLarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vShieldedSpend.resize(1);
    mtx.valueBalanceSapling = MAX_MONEY + 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-valuebalance-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, NegativeValueBalanceTooLarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vShieldedSpend.resize(1);
    mtx.valueBalanceSapling = -(MAX_MONEY + 1);

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-valuebalance-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, ValueBalanceOverflowsTotal) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vShieldedSpend.resize(1);
    mtx.vout[0].nValue = 1;
    mtx.valueBalanceSapling = -MAX_MONEY;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsTxouttotalToolargeJoinsplit) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = 1;
    mtx.vJoinSplit[0].vpub_old = MAX_MONEY;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsTxintotalToolargeJoinsplit) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit[0].vpub_new = MAX_MONEY - 1;
    mtx.vJoinSplit[1].vpub_new = MAX_MONEY - 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txintotal-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsVpubOldNegative) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit[0].vpub_old = -1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_old-negative", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsVpubNewNegative) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit[0].vpub_new = -1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_new-negative", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsVpubOldToolarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit[0].vpub_old = MAX_MONEY + 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_old-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsVpubNewToolarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit[0].vpub_new = MAX_MONEY + 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_new-toolarge", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsVpubsBothNonzero) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit[0].vpub_old = 1;
    mtx.vJoinSplit[0].vpub_new = 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpubs-both-nonzero", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsInputsDuplicate) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vin[1].prevout.hash = mtx.vin[0].prevout.hash;
    mtx.vin[1].prevout.n = mtx.vin[0].prevout.n;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadJoinsplitsNullifiersDuplicateSameJoinsplit) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit[0].nullifiers.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
    mtx.vJoinSplit[0].nullifiers.at(1) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-joinsplits-nullifiers-duplicate", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadJoinsplitsNullifiersDuplicateDifferentJoinsplit) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit[0].nullifiers.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
    mtx.vJoinSplit[1].nullifiers.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-joinsplits-nullifiers-duplicate", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadCbHasJoinsplits) {
    CMutableTransaction mtx = GetValidTransaction();
    // Make it a coinbase.
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    mtx.vJoinSplit.resize(1);

    CTransaction tx(mtx);
    EXPECT_TRUE(tx.IsCoinBase());

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-has-joinsplits", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadCbEmptyScriptsig) {
    CMutableTransaction mtx = GetValidTransaction();
    // Make it a coinbase.
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();

    mtx.vJoinSplit.resize(0);

    CTransaction tx(mtx);
    EXPECT_TRUE(tx.IsCoinBase());

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-length", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsPrevoutNull) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vin[1].prevout.SetNull();

    CTransaction tx(mtx);
    EXPECT_FALSE(tx.IsCoinBase());

    MockCValidationState state;
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "bad-txns-prevout-null", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

TEST(ChecktransactionTests, BadTxnsInvalidJoinsplitSignature) {
    SelectParams(CBaseChainParams::REGTEST);
    auto chainparams = Params();

    CMutableTransaction mtx = GetValidTransaction();
    mtx.joinSplitSig.bytes[0] += 1;
    CTransaction tx(mtx);

    MockCValidationState state;
    // during initial block download, for transactions being accepted into the
    // mempool (and thus not mined), DoS ban score should be zero, else 10
    EXPECT_CALL(state, DoS(0, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, false, [](const Consensus::Params&) { return true; });
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, false, [](const Consensus::Params&) { return false; });
    // for transactions that have been mined in a block, DoS ban score should
    // always be 100.
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, true, [](const Consensus::Params&) { return true; });
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, true, [](const Consensus::Params&) { return false; });
}

TEST(ChecktransactionTests, JoinsplitSignatureDetectsOldBranchId) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, 1);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, 1);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, 10);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_HEARTWOOD, 20);
    auto chainparams = Params();

    auto saplingBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId;
    auto blossomBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_BLOSSOM].nBranchId;

    // Create a valid transaction for the Sapling epoch.
    CMutableTransaction mtx = GetValidTransaction(saplingBranchId);
    CTransaction tx(mtx);

    MockCValidationState state;
    // Ensure that the transaction validates against Sapling.
    EXPECT_TRUE(ContextualCheckTransaction(
        tx, state, chainparams, 5, false,
        [](const Consensus::Params&) { return false; }));

    // Attempt to validate the inputs against Blossom. We should be notified
    // that an old consensus branch ID was used for an input.
    EXPECT_CALL(state, DoS(
        10, false, REJECT_INVALID,
        strprintf("old-consensus-branch-id (Expected %s, found %s)",
            HexInt(blossomBranchId),
            HexInt(saplingBranchId)),
        false)).Times(1);
    EXPECT_FALSE(ContextualCheckTransaction(
        tx, state, chainparams, 15, false,
        [](const Consensus::Params&) { return false; }));

    // Attempt to validate the inputs against Heartwood. All we should learn is
    // that the signature is invalid, because we don't check more than one
    // network upgrade back.
    EXPECT_CALL(state, DoS(
        10, false, REJECT_INVALID,
        "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    EXPECT_FALSE(ContextualCheckTransaction(
        tx, state, chainparams, 25, false,
        [](const Consensus::Params&) { return false; }));

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_HEARTWOOD, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    RegtestDeactivateBlossom();
}

TEST(ChecktransactionTests, NonCanonicalEd25519Signature) {
    SelectParams(CBaseChainParams::REGTEST);
    auto chainparams = Params();

    CMutableTransaction mtx = GetValidTransaction();

    // Check that the signature is valid before we add L
    {
        CTransaction tx(mtx);
        MockCValidationState state;
        EXPECT_TRUE(ContextualCheckTransaction(tx, state, chainparams, 0, true));
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
        s = mtx.joinSplitSig.bytes[32 + i] + L[i] + (s >> 8);
        mtx.joinSplitSig.bytes[32 + i] = s & 0xff;
    }

    CTransaction tx(mtx);

    MockCValidationState state;
    // during initial block download, for transactions being accepted into the
    // mempool (and thus not mined), DoS ban score should be zero, else 10
    EXPECT_CALL(state, DoS(0, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, false, [](const Consensus::Params&) { return true; });
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, false, [](const Consensus::Params&) { return false; });
    // for transactions that have been mined in a block, DoS ban score should
    // always be 100.
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, true, [](const Consensus::Params&) { return true; });
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-invalid-joinsplit-signature", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, true, [](const Consensus::Params&) { return false; });
}

TEST(ChecktransactionTests, OverwinterConstructors) {
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

TEST(ChecktransactionTests, OverwinterSerialization) {
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

TEST(ChecktransactionTests, OverwinterDefaultValues) {
    // Check default values (this will fail when defaults change; test should then be updated)
    CTransaction tx;
    EXPECT_EQ(tx.nVersion, 1);
    EXPECT_EQ(tx.fOverwintered, false);
    EXPECT_EQ(tx.nVersionGroupId, 0);
    EXPECT_EQ(tx.nExpiryHeight, 0);
}

// A valid v3 transaction with no joinsplits
TEST(ChecktransactionTests, OverwinterValidTx) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;
    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
}

TEST(ChecktransactionTests, OverwinterExpiryHeight) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit.resize(0);
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

TEST(checktransaction_tests, BlossomExpiryHeight) {
    const Consensus::Params& params = RegtestActivateBlossom(false, 100).GetConsensus();
    CMutableTransaction preBlossomMtx = CreateNewContextualCMutableTransaction(params, 99);
    EXPECT_EQ(preBlossomMtx.nExpiryHeight, 100 - 1);
    CMutableTransaction blossomMtx = CreateNewContextualCMutableTransaction(params, 100);
    EXPECT_EQ(blossomMtx.nExpiryHeight, 100 + 40);
    RegtestDeactivateBlossom();
}

// Test that a Sprout tx with a negative version number is detected
// given the new Overwinter logic
TEST(ChecktransactionTests, SproutTxVersionTooLow) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit.resize(0);
    mtx.fOverwintered = false;
    mtx.nVersion = -1;

    EXPECT_THROW((CTransaction(mtx)), std::ios_base::failure);
    UNSAFE_CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-version-too-low", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}



TEST(ChecktransactionTests, SaplingSproutInputSumsTooLarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = SAPLING_TX_VERSION;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    {
        // create JSDescription
        uint256 rt;
        Ed25519VerificationKey joinSplitPubKey;
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

        auto jsdesc = JSDescriptionInfo(
            joinSplitPubKey, rt,
            inputs, outputs,
            0, 0
        ).BuildRandomized(
            inputMap, outputMap,
            false);

        mtx.vJoinSplit.push_back(jsdesc);
    }

    mtx.vShieldedSpend.push_back(SpendDescription());

    mtx.vJoinSplit[0].vpub_new = (MAX_MONEY / 2) + 10;

    {
        UNSAFE_CTransaction tx(mtx);
        CValidationState state;
        EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
    }

    mtx.valueBalanceSapling = (MAX_MONEY / 2) + 10;

    {
        UNSAFE_CTransaction tx(mtx);
        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txintotal-toolarge", false)).Times(1);
        CheckTransactionWithoutProofVerification(tx, state);
    }
}

// Test bad Overwinter version number in CheckTransactionWithoutProofVerification
TEST(ChecktransactionTests, OverwinterVersionNumberLow) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_MIN_TX_VERSION - 1;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    EXPECT_THROW((CTransaction(mtx)), std::ios_base::failure);
    UNSAFE_CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-tx-overwinter-version-too-low", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

// Test bad Overwinter version number in ContextualCheckTransaction
TEST(ChecktransactionTests, OverwinterVersionNumberHigh) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_MAX_TX_VERSION + 1;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    EXPECT_THROW((CTransaction(mtx)), std::ios_base::failure);
    UNSAFE_CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-tx-overwinter-version-too-high", false)).Times(1);
    ContextualCheckTransaction(tx, state, Params(), 1, true);

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}


// Test bad Overwinter version group id
TEST(ChecktransactionTests, OverwinterBadVersionGroupId) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vJoinSplit.resize(0);
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nExpiryHeight = 0;
    mtx.nVersionGroupId = 0x12345678;

    EXPECT_THROW((CTransaction(mtx)), std::ios_base::failure);
    UNSAFE_CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-tx-version-group-id", false)).Times(1);
    CheckTransactionWithoutProofVerification(tx, state);
}

// This tests an Overwinter transaction checked against Sprout
TEST(ChecktransactionTests, OverwinterNotActive) {
    SelectParams(CBaseChainParams::TESTNET);
    auto chainparams = Params();

    CMutableTransaction mtx = GetValidTransaction();
    mtx.fOverwintered = true;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    CTransaction tx(mtx);
    MockCValidationState state;
    // during initial block download, for transactions being accepted into the
    // mempool (and thus not mined), DoS ban score should be zero, else 10
    EXPECT_CALL(state, DoS(0, false, REJECT_INVALID, "tx-overwinter-not-active", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, false, [](const Consensus::Params&) { return true; });
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "tx-overwinter-not-active", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, false, [](const Consensus::Params&) { return false; });
    // for transactions that have been mined in a block, DoS ban score should
    // always be 100.
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "tx-overwinter-not-active", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, true, [](const Consensus::Params&) { return true; });
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "tx-overwinter-not-active", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 0, true, [](const Consensus::Params&) { return false; });
}

// This tests a transaction without the fOverwintered flag set, against the Overwinter consensus rule set.
TEST(ChecktransactionTests, OverwinterFlagNotSet) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    CMutableTransaction mtx = GetValidTransaction();
    mtx.fOverwintered = false;
    mtx.nVersion = OVERWINTER_TX_VERSION;
    mtx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
    mtx.nExpiryHeight = 0;

    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "tx-overwintered-flag-not-set", false)).Times(1);
    ContextualCheckTransaction(tx, state, Params(), 1, true);

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}


// Overwinter (NU0) does not allow soft fork to version 4 Overwintered tx.
TEST(ChecktransactionTests, OverwinterInvalidSoftForkVersion) {
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

static void ContextualCreateTxCheck(const Consensus::Params& params, int nHeight,
    int expectedVersion, bool expectedOverwintered, int expectedVersionGroupId, int expectedExpiryHeight)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(params, nHeight);
    EXPECT_EQ(mtx.nVersion, expectedVersion);
    EXPECT_EQ(mtx.fOverwintered, expectedOverwintered);
    EXPECT_EQ(mtx.nVersionGroupId, expectedVersionGroupId);
    EXPECT_EQ(mtx.nExpiryHeight, expectedExpiryHeight);
}


// Test CreateNewContextualCMutableTransaction sets default values based on height
TEST(ChecktransactionTests, OverwinteredContextualCreateTx) {
    SelectParams(CBaseChainParams::REGTEST);
    const Consensus::Params& params = Params().GetConsensus();
    int overwinterActivationHeight = 5;
    int saplingActivationHeight = 30;
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, overwinterActivationHeight);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, saplingActivationHeight);

    ContextualCreateTxCheck(params, overwinterActivationHeight - 1,
        1, false, 0, 0);
    // Overwinter activates
    ContextualCreateTxCheck(params, overwinterActivationHeight,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, overwinterActivationHeight + DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA);
    // Close to Sapling activation
    ContextualCreateTxCheck(params, saplingActivationHeight - DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA - 2,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, saplingActivationHeight - 2);
    ContextualCreateTxCheck(params, saplingActivationHeight - DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA - 1,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, saplingActivationHeight - 1);
    ContextualCreateTxCheck(params, saplingActivationHeight - DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, saplingActivationHeight - 1);
    ContextualCreateTxCheck(params, saplingActivationHeight - DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA + 1,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, saplingActivationHeight - 1);
    ContextualCreateTxCheck(params, saplingActivationHeight - DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA + 2,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, saplingActivationHeight - 1);
    ContextualCreateTxCheck(params, saplingActivationHeight - DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA + 3,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, saplingActivationHeight - 1);
    // Just before Sapling activation
    ContextualCreateTxCheck(params, saplingActivationHeight - 4,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, saplingActivationHeight - 1);
    ContextualCreateTxCheck(params, saplingActivationHeight - 3,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, saplingActivationHeight - 1);
    ContextualCreateTxCheck(params, saplingActivationHeight - 2,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, saplingActivationHeight - 1);
    ContextualCreateTxCheck(params, saplingActivationHeight - 1,
        OVERWINTER_TX_VERSION, true, OVERWINTER_VERSION_GROUP_ID, saplingActivationHeight - 1);
    // Sapling activates
    ContextualCreateTxCheck(params, saplingActivationHeight,
        SAPLING_TX_VERSION, true, SAPLING_VERSION_GROUP_ID, saplingActivationHeight + DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA);

    // Revert to default
    RegtestDeactivateSapling();
}

// Test a v1 transaction which has a malformed header, perhaps modified in-flight
TEST(ChecktransactionTests, BadTxReceivedOverNetwork)
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

TEST(ChecktransactionTests, InvalidShieldedCoinbase) {
    RegtestActivateSapling();

    CMutableTransaction mtx = GetValidTransaction();
    mtx.fOverwintered = true;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.nVersion = SAPLING_TX_VERSION;

    // Make it an invalid shielded coinbase (no ciphertexts or commitments).
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vShieldedOutput.resize(1);
    mtx.vJoinSplit.resize(0);

    CTransaction tx(mtx);
    EXPECT_TRUE(tx.IsCoinBase());

    // Before Heartwood, output descriptions are rejected.
    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-has-output-description", false)).Times(1);
    ContextualCheckTransaction(tx, state, Params(), 10, 57);

    RegtestActivateHeartwood(false, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    // From Heartwood, the output description is allowed but invalid (undecryptable).
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-output-desc-invalid-outct", false)).Times(1);
    ContextualCheckTransaction(tx, state, Params(), 10, 57);

    RegtestDeactivateHeartwood();
}

TEST(ChecktransactionTests, HeartwoodAcceptsShieldedCoinbase) {
    RegtestActivateHeartwood(false, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    auto chainparams = Params();

    uint256 ovk;
    auto note = libzcash::SaplingNote(
        libzcash::SaplingSpendingKey::random().default_address(), CAmount(123456), libzcash::Zip212Enabled::BeforeZip212);
    auto output = OutputDescriptionInfo(ovk, note, {{0xF6}});

    auto ctx = librustzcash_sapling_proving_ctx_init();
    auto odesc = output.Build(ctx).value();
    librustzcash_sapling_proving_ctx_free(ctx);

    CMutableTransaction mtx = GetValidTransaction();
    mtx.fOverwintered = true;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.nVersion = SAPLING_TX_VERSION;

    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vJoinSplit.resize(0);
    mtx.vShieldedOutput.push_back(odesc);

    // Transaction should fail with a bad public cmu.
    {
        auto cmOrig = mtx.vShieldedOutput[0].cmu;
        mtx.vShieldedOutput[0].cmu = uint256S("1234");
        CTransaction tx(mtx);
        EXPECT_TRUE(tx.IsCoinBase());

        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-output-desc-invalid-outct", false)).Times(1);
        ContextualCheckTransaction(tx, state, chainparams, 10, 57);

        mtx.vShieldedOutput[0].cmu = cmOrig;
    }

    // Transaction should fail with a bad outCiphertext.
    {
        auto outCtOrig = mtx.vShieldedOutput[0].outCiphertext;
        mtx.vShieldedOutput[0].outCiphertext = {{}};
        CTransaction tx(mtx);
        EXPECT_TRUE(tx.IsCoinBase());

        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-output-desc-invalid-outct", false)).Times(1);
        ContextualCheckTransaction(tx, state, chainparams, 10, 57);

        mtx.vShieldedOutput[0].outCiphertext = outCtOrig;
    }

    // Transaction should fail with a bad encCiphertext.
    {
        auto encCtOrig = mtx.vShieldedOutput[0].encCiphertext;
        mtx.vShieldedOutput[0].encCiphertext = {{}};
        CTransaction tx(mtx);
        EXPECT_TRUE(tx.IsCoinBase());

        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-cb-output-desc-invalid-encct", false)).Times(1);
        ContextualCheckTransaction(tx, state, chainparams, 10, 57);

        mtx.vShieldedOutput[0].encCiphertext = encCtOrig;
    }

    // Test the success case. The unmodified transaction should fail signature
    // validation, which is an unrelated consensus rule and is checked after the
    // shielded coinbase rules have passed.
    {
        CTransaction tx(mtx);
        EXPECT_TRUE(tx.IsCoinBase());

        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-sapling-binding-signature-invalid", false)).Times(1);
        ContextualCheckTransaction(tx, state, chainparams, 10, 57);
    }

    RegtestDeactivateHeartwood();
}

// Check that the consensus rules relevant to valueBalanceSapling, vShieldedOutput, and
// bindingSig from https://zips.z.cash/protocol/protocol.pdf#txnencoding are
// applied to coinbase transactions.
TEST(ChecktransactionTests, HeartwoodEnforcesSaplingRulesOnShieldedCoinbase) {
    RegtestActivateHeartwood(false, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    auto chainparams = Params();

    uint256 ovk;
    auto note = libzcash::SaplingNote(
        libzcash::SaplingSpendingKey::random().default_address(), CAmount(123456), libzcash::Zip212Enabled::BeforeZip212);
    auto output = OutputDescriptionInfo(ovk, note, {{0xF6}});

    CMutableTransaction mtx = GetValidTransaction();
    mtx.fOverwintered = true;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.nVersion = SAPLING_TX_VERSION;

    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig << 123;
    mtx.vJoinSplit.resize(0);
    mtx.valueBalanceSapling = -1000;

    // Coinbase transaction should fail non-contextual checks with no shielded
    // outputs and non-zero valueBalanceSapling.
    {
        CTransaction tx(mtx);
        EXPECT_TRUE(tx.IsCoinBase());

        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-valuebalance-nonzero", false)).Times(1);
        EXPECT_FALSE(CheckTransactionWithoutProofVerification(tx, state));
    }

    // Add a Sapling output.
    auto ctx = librustzcash_sapling_proving_ctx_init();
    auto odesc = output.Build(ctx).value();
    librustzcash_sapling_proving_ctx_free(ctx);
    mtx.vShieldedOutput.push_back(odesc);

    // Coinbase transaction should fail non-contextual checks with valueBalanceSapling
    // out of range.
    {
        mtx.valueBalanceSapling = MAX_MONEY + 1;
        EXPECT_THROW((CTransaction(mtx)), std::ios_base::failure);
        UNSAFE_CTransaction tx(mtx);
        EXPECT_TRUE(tx.IsCoinBase());

        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-valuebalance-toolarge", false)).Times(1);
        EXPECT_FALSE(CheckTransactionWithoutProofVerification(tx, state));
    }
    {
        mtx.valueBalanceSapling = -MAX_MONEY - 1;
        EXPECT_THROW((CTransaction(mtx)), std::ios_base::failure);
        UNSAFE_CTransaction tx(mtx);
        EXPECT_TRUE(tx.IsCoinBase());

        MockCValidationState state;
        EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-valuebalance-toolarge", false)).Times(1);
        EXPECT_FALSE(CheckTransactionWithoutProofVerification(tx, state));
    }

    mtx.valueBalanceSapling = -1000;
    CTransaction tx(mtx);
    EXPECT_TRUE(tx.IsCoinBase());

    // Coinbase transaction should now pass non-contextual checks.
    MockCValidationState state;
    EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));

    // Coinbase transaction does not pass contextual checks, as bindingSig
    // consensus rule is enforced.
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-sapling-binding-signature-invalid", false)).Times(1);
    ContextualCheckTransaction(tx, state, chainparams, 10, 57);

    RegtestDeactivateHeartwood();
}


TEST(ChecktransactionTests, CanopyRejectsNonzeroVPubOld) {
    RegtestActivateSapling();

    CMutableTransaction mtx = GetValidTransaction(NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId);

    // Make a JoinSplit with nonzero vpub_old
    mtx.vJoinSplit.resize(1);
    mtx.vJoinSplit[0].vpub_old = 1;
    mtx.vJoinSplit[0].vpub_new = 0;
    mtx.vJoinSplit[0].proof = libzcash::GrothProof();
    CreateJoinSplitSignature(mtx, NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId);

    CTransaction tx(mtx);

    // Before Canopy, nonzero vpub_old is accepted in both non-contextual and contextual checks
    MockCValidationState state;
    EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 1, true));

    RegtestActivateCanopy(false, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    // After Canopy, nonzero vpub_old is accepted in non-contextual checks but rejected in contextual checks
    EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_old-nonzero", false)).Times(1);
    EXPECT_FALSE(ContextualCheckTransaction(tx, state, Params(), 10, true));

    RegtestDeactivateCanopy();

}

TEST(ChecktransactionTests, CanopyAcceptsZeroVPubOld) {

    CMutableTransaction mtx = GetValidTransaction(NetworkUpgradeInfo[Consensus::UPGRADE_SAPLING].nBranchId);

    // Make a JoinSplit with zero vpub_old
    mtx.vJoinSplit.resize(1);
    mtx.vJoinSplit[0].vpub_old = 0;
    mtx.vJoinSplit[0].vpub_new = 1;
    mtx.vJoinSplit[0].proof = libzcash::GrothProof();
    CreateJoinSplitSignature(mtx, NetworkUpgradeInfo[Consensus::UPGRADE_CANOPY].nBranchId);

    CTransaction tx(mtx);

    // After Canopy, zero value vpub_old (i.e. unshielding) is accepted in both non-contextual and contextual checks
    MockCValidationState state;

    RegtestActivateCanopy(false, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    EXPECT_TRUE(CheckTransactionWithoutProofVerification(tx, state));
    EXPECT_TRUE(ContextualCheckTransaction(tx, state, Params(), 10, true));

    RegtestDeactivateCanopy();

}
