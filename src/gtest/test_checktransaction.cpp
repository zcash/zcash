#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "main.h"
#include "primitives/transaction.h"
#include "consensus/validation.h"

TEST(checktransaction_tests, check_vpub_not_both_nonzero) {
    CMutableTransaction tx;
    tx.nVersion = 2;

    {
        // Ensure that values within the pour are well-formed.
        CMutableTransaction newTx(tx);
        CValidationState state;
        state.SetPerformPourVerification(false); // don't verify the snark

        newTx.vpour.push_back(CPourTx());

        CPourTx *pourtx = &newTx.vpour[0];
        pourtx->vpub_old = 1;
        pourtx->vpub_new = 1;

        EXPECT_FALSE(CheckTransaction(newTx, state));
        EXPECT_EQ(state.GetRejectReason(), "bad-txns-vpubs-both-nonzero");
    }
}

class MockCValidationState : public CValidationState {
public:
    MOCK_METHOD5(DoS, bool(int level, bool ret,
             unsigned char chRejectCodeIn, std::string strRejectReasonIn,
             bool corruptionIn));
    MOCK_METHOD1(SetPerformPourVerification, bool(bool pourVerifyIn));
    MOCK_METHOD0(PerformPourVerification, bool());
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


CMutableTransaction GetValidTransaction() {
    CMutableTransaction mtx;
    mtx.vin.resize(2);
    mtx.vin[0].prevout.hash = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
    mtx.vin[0].prevout.n = 0;
    mtx.vin[1].prevout.hash = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    mtx.vin[1].prevout.n = 0;
    mtx.vout.resize(2);
    // mtx.vout[0].scriptPubKey = 
    mtx.vout[0].nValue = 0;
    mtx.vout[1].nValue = 0;
    mtx.vpour.resize(2);
    mtx.vpour[0].serials.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
    mtx.vpour[0].serials.at(1) = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    mtx.vpour[1].serials.at(0) = uint256S("0000000000000000000000000000000000000000000000000000000000000002");
    mtx.vpour[1].serials.at(1) = uint256S("0000000000000000000000000000000000000000000000000000000000000003");
    return mtx;
}

TEST(checktransaction_tests, valid_transaction) {
    CMutableTransaction mtx = GetValidTransaction();
    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_TRUE(CheckTransaction(tx, state));
}

TEST(checktransaction_tests, bad_txns_vin_empty) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vpour.resize(0);
    mtx.vin.resize(0);

    CTransaction tx(mtx);
    MockCValidationState state;
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_vout_empty) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vpour.resize(0);
    mtx.vout.resize(0);

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_oversize) {
    CMutableTransaction mtx = GetValidTransaction();

    mtx.vin[0].scriptSig = CScript();
    // 18 * (520char + DROP) + OP_1 = 9433 bytes
    std::vector<unsigned char> vchData(520);
    for (unsigned int i = 0; i < 2000; ++i)
        mtx.vin[0].scriptSig << vchData << OP_DROP;
    mtx.vin[0].scriptSig << OP_1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-oversize", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_vout_negative) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = -1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_vout_toolarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = MAX_MONEY + 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_txouttotal_toolarge_outputs) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = MAX_MONEY;
    mtx.vout[1].nValue = 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_txouttotal_toolarge_joinsplit) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vout[0].nValue = 1;
    mtx.vpour[0].vpub_new = MAX_MONEY;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_vpub_old_negative) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vpour[0].vpub_old = -1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_old-negative", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_vpub_new_negative) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vpour[0].vpub_new = -1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_new-negative", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_vpub_old_toolarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vpour[0].vpub_old = MAX_MONEY + 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_old-toolarge", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_vpub_new_toolarge) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vpour[0].vpub_new = MAX_MONEY + 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpub_new-toolarge", false)).Times(1);
    CheckTransaction(tx, state);
}

TEST(checktransaction_tests, bad_txns_vpubs_both_nonzero) {
    CMutableTransaction mtx = GetValidTransaction();
    mtx.vpour[0].vpub_old = 1;
    mtx.vpour[0].vpub_new = 1;

    CTransaction tx(mtx);

    MockCValidationState state;
    EXPECT_CALL(state, DoS(100, false, REJECT_INVALID, "bad-txns-vpubs-both-nonzero", false)).Times(1);
    CheckTransaction(tx, state);
}
