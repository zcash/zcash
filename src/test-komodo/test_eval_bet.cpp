#include <cryptoconditions.h>
#include <gtest/gtest.h>

#include "cc/betprotocol.h"
#include "cc/eval.h"
#include "base58.h"
#include "key.h"
#include "main.h"
#include "script/cc.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "script/serverchecker.h"

#include "testutils.h"


extern Eval* EVAL_TEST;


namespace TestBet {


static std::vector<CKey> playerSecrets;
static std::vector<CPubKey> players;

static int Dealer  = 0, Player1 = 1, Player2 = 2;


int CCSign(CMutableTransaction &tx, unsigned int nIn, CC *cond, std::vector<int> keyIds) {
    PrecomputedTransactionData txdata(tx);
    uint256 sighash = SignatureHash(CCPubKey(cond), tx, nIn, SIGHASH_ALL, 0, 0, &txdata);
    int nSigned = 0;
    for (int i=0; i<keyIds.size(); i++)
        nSigned += cc_signTreeSecp256k1Msg32(cond, playerSecrets[keyIds[i]].begin(), sighash.begin());
    tx.vin[nIn].scriptSig = CCSig(cond);
    return nSigned;
}


int TestCC(CMutableTransaction &mtxTo, unsigned int nIn, CC *cond)
{
    CAmount amount;
    ScriptError error;
    CTransaction txTo(mtxTo);
    PrecomputedTransactionData txdata(txTo);
    auto checker = ServerTransactionSignatureChecker(&txTo, nIn, amount, false, txdata);
    return VerifyScript(txTo.vin[nIn].scriptSig, CCPubKey(cond), 0, checker, 0, &error);
}


#define ASSERT_CC(tx, nIn, cond) if (!TestCC(tx, nIn, cond)) FAIL();


class MockVM : public AppVM
{
public:
    std::pair<int,std::vector<CTxOut>> evaluate(
            std::vector<unsigned char> header, std::vector<unsigned char> body)
    {
        std::vector<CTxOut> outs;
        if (memcmp(header.data(), "BetHeader", 9)) {
            printf("Wrong VM header\n");
            return std::make_pair(0, outs);
        }
        outs.push_back(CTxOut(2, CScript() << OP_RETURN << body.size()));
        return std::make_pair(body.size(), outs);
    }
};

const EvalCode EVAL_DISPUTEBET = 0xf2;

class EvalMock : public Eval
{
public:
    uint256 MoM;
    int currentHeight;
    std::map<uint256, CTransaction> txs;
    std::map<uint256, CBlockIndex> blocks;
    std::map<uint256, std::vector<CTransaction>> spends;

    bool Dispatch(const CC *cond, const CTransaction &txTo, unsigned int nIn)
    {
        EvalCode ecode = cond->code[0];
        std::vector<uint8_t> vparams(cond->code+1, cond->code+cond->codeLength);

        if (ecode == EVAL_DISPUTEBET) {
            MockVM vm;
            return DisputePayout(vm, vparams, txTo, nIn);
        }
        if (ecode == EVAL_IMPORTPAYOUT) {
            return ImportPayout(vparams, txTo, nIn);
        }
        return Invalid("invalid-code");
    }

    bool GetSpendsConfirmed(uint256 hash, std::vector<CTransaction> &spendsOut) const
    {
        auto r = spends.find(hash);
        if (r != spends.end()) {
            spendsOut = r->second;
            return true;
        }
        return false;
    }

    bool GetTxUnconfirmed(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock) const
    {
        auto r = txs.find(hash);
        if (r != txs.end()) {
            txOut = r->second;
            if (blocks.count(hash) > 0)
                hashBlock = hash;
            return true;
        }
        return false;
    }

    unsigned int GetCurrentHeight() const { return currentHeight; }

    bool GetBlock(uint256 hash, CBlockIndex& blockIdx) const
    {
        auto r = blocks.find(hash);
        if (r == blocks.end()) return false;
        blockIdx = r->second;
        return true;
    }

    bool GetNotarisationData(uint256 notarisationHash, NotarisationData &data) const
    {
        if (notarisationHash == NotarisationHash()) {
            data.MoM = MoM;
            return true;
        }
        return false;
    }

    static uint256 NotarisationHash()
    {
        uint256 h;
        h.begin()[0] = 123;
        return h;
    }
};


/*
 * Generates example data that we will test with and shows how to call BetProtocol.
 */
class ExampleBet
{
public:
    BetProtocol bet;
    CAmount totalPayout;

    ExampleBet() : bet(BetProtocol(EVAL_DISPUTEBET, players, 2, VCH("BetHeader", 9))), totalPayout(100) {}
    ~ExampleBet() {};

    CTransaction SessionTx()
    {
        return CTransaction(bet.MakeSessionTx(1));
    }

    CC* DisputeCond()
    {
        return bet.MakeDisputeCond();
    }

    CC* PayoutCond()
    {
        return bet.MakePayoutCond(SessionTx().GetHash());
    }

    CTransaction StakeTx()
    {
        return CTransaction(bet.MakeStakeTx(totalPayout, SessionTx().GetHash()));
    }

    std::vector<unsigned char> PlayerState(int playerIdx)
    {
        std::vector<unsigned char> state;
        for (int i=0; i<playerIdx+1; i++) state.push_back(1);
        return state;
    }

    std::vector<CTxOut> Payouts(int playerIdx)
    {
        return MockVM().evaluate(bet.vmParams, PlayerState(playerIdx)).second;
    }

    CMutableTransaction DisputeTx(int playerIdx)
    {
        return bet.MakeDisputeTx(SessionTx().GetHash(), SerializeHash(Payouts(playerIdx)));
    }

    CMutableTransaction PostEvidenceTx(int playerIdx)
    {
        return bet.MakePostEvidenceTx(SessionTx().GetHash(), playerIdx, PlayerState(playerIdx));
    }

    CMutableTransaction AgreePayoutTx()
    {
        std::vector<CTxOut> v;
        return bet.MakeAgreePayoutTx(v, uint256());
    }

    MoMProof GetMoMProof()
    {
        int nIndex = 5;
        std::vector<uint256> vBranch;
        vBranch.resize(3);
        return {MerkleBranch(nIndex, vBranch), EvalMock::NotarisationHash()};
    }

    CMutableTransaction ImportPayoutTx()
    {
        CMutableTransaction disputeTx = DisputeTx(Player2);
        return bet.MakeImportPayoutTx(Payouts(Player2), disputeTx, uint256(), GetMoMProof());
    }

    EvalMock SetEvalMock(int currentHeight)
    {
        EvalMock eval;
        CTransaction sessionTx = SessionTx();

        eval.txs[sessionTx.GetHash()] = sessionTx;

        CBlockIndex sessionBlock;
        sessionBlock.SetHeight(10);
        eval.blocks[sessionTx.GetHash()] = sessionBlock;

        std::vector<CTransaction> sessionSpends;
        sessionSpends.push_back(CTransaction(PostEvidenceTx(Dealer)));
        sessionSpends.push_back(CTransaction());  // Invalid, should be ignored
        sessionSpends.push_back(CTransaction(PostEvidenceTx(Player2)));
        eval.spends[sessionTx.GetHash()] = sessionSpends;

        eval.currentHeight = currentHeight;

        MoMProof proof = GetMoMProof();
        eval.MoM = proof.branch.Exec(DisputeTx(Player2).GetHash());

        EVAL_TEST = &eval;
        return eval;
    }
};


ExampleBet ebet;


class TestBet : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        // Make playerSecrets
        CBitcoinSecret vchSecret;
        auto addKey = [&] (std::string k) { vchSecret.SetString(k); playerSecrets.push_back(vchSecret.GetKey()); };
        addKey("UwFBKf4d6wC3yqdnk3LoGrFjy7gwxrWerBT8jTFamrBbem8wSw9L");
        addKey("Up6GpWwrmx2VpqF8rD3snJXToKT56Dzc8YSoL24osXnfNdCucaMR");
        addKey("UxEHwki3A95PSHHVRzE2N67eHTeoUcqLkovxp6yDPVViv54skF8c");
        // Make playerpubkeys
        for (int i=0; i<playerSecrets.size(); i++) players.push_back(playerSecrets[i].GetPubKey());
        // enable CC
        ASSETCHAINS_CC = 1;
    }
    virtual void SetUp() {
        EVAL_TEST = 0;
        ebet = ExampleBet();
    }
};


TEST_F(TestBet, testMakeSessionTx)
{
    CTransaction sessionTx = ebet.SessionTx();
    EXPECT_EQ(0, sessionTx.vin.size());
    EXPECT_EQ(4, sessionTx.vout.size());
    EXPECT_EQ(CCPubKey(ebet.DisputeCond()), sessionTx.vout[0].scriptPubKey);
    for (int i=0; i<players.size(); i++)
        EXPECT_EQ(CCPubKey(CCNewSecp256k1(players[i])), sessionTx.vout[i+1].scriptPubKey);
}


TEST_F(TestBet, testMakeDisputeCond)
{
    CC *disputeCond = ebet.DisputeCond();
    EXPECT_EQ("(2 of 15,(1 of 5,5,5))", CCShowStructure(disputeCond));

    CC *evalCond = disputeCond->subconditions[0];
    uint8_t target[100];
    sprintf((char*)target, "%c\x02\tBetHeader", EVAL_DISPUTEBET);
    EXPECT_EQ(0, memcmp(target, evalCond->code, 12));

    for (int i=0; i<players.size(); i++)
        EXPECT_EQ(CCPubKey(CCNewSecp256k1(players[i])),
                  CCPubKey(disputeCond->subconditions[1]->subconditions[i]));
}


TEST_F(TestBet, testSignDisputeCond)
{
    // Only one key needed to dispute
    CMutableTransaction disputeTx = ebet.DisputeTx(Player1);
    CC *disputeCond = ebet.DisputeCond();
    EXPECT_EQ(1, CCSign(disputeTx, 0, disputeCond, {Player1}));

    EXPECT_EQ(1, cc_isFulfilled(disputeCond->subconditions[0]));
    EXPECT_EQ(1, cc_isFulfilled(disputeCond->subconditions[1]));
    EXPECT_EQ(0, cc_isFulfilled(disputeCond->subconditions[1]->subconditions[0]));
    EXPECT_EQ(1, cc_isFulfilled(disputeCond->subconditions[1]->subconditions[1]));
    EXPECT_EQ(0, cc_isFulfilled(disputeCond->subconditions[1]->subconditions[2]));
    EXPECT_EQ(1, cc_isFulfilled(disputeCond));
}


TEST_F(TestBet, testDispute)
{
    EvalMock eval = ebet.SetEvalMock(12);

    // Only one key needed to dispute
    CMutableTransaction disputeTx = ebet.DisputeTx(Player2);
    CC *disputeCond = ebet.DisputeCond();
    EXPECT_EQ(1, CCSign(disputeTx, 0, disputeCond, {Player2}));

    // Success
    EXPECT_TRUE(TestCC(disputeTx, 0, disputeCond));

    // Set result hash to 0 and check false
    uint256 nonsense;
    disputeTx.vout[0].scriptPubKey = CScript() << OP_RETURN << E_MARSHAL(ss << nonsense);
    EXPECT_EQ(1, CCSign(disputeTx, 0, disputeCond, {Player2}));
    EXPECT_FALSE(TestCC(disputeTx, 0, disputeCond));
    EXPECT_EQ("wrong-payout", eval.state.GetRejectReason());
}


TEST_F(TestBet, testDisputeInvalidOutput)
{
    EvalMock eval = ebet.SetEvalMock(11);

    // Only one key needed to dispute
    CMutableTransaction disputeTx = ebet.DisputeTx(Dealer);
    CC *disputeCond = ebet.DisputeCond();

    // invalid payout hash
    std::vector<unsigned char> invalidHash = {0,1,2};
    disputeTx.vout[0].scriptPubKey = CScript() << OP_RETURN << invalidHash;
    ASSERT_EQ(1, CCSign(disputeTx, 0, disputeCond, {Player1}));
    EXPECT_FALSE(TestCC(disputeTx, 0, disputeCond));
    EXPECT_EQ("invalid-payout-hash", eval.state.GetRejectReason());

    // no vout at all
    disputeTx.vout.resize(0);
    ASSERT_EQ(1, CCSign(disputeTx, 0, disputeCond, {Player1}));
    EXPECT_FALSE(TestCC(disputeTx, 0, disputeCond));
    EXPECT_EQ("no-vouts", eval.state.GetRejectReason());
}


TEST_F(TestBet, testDisputeEarly)
{
    EvalMock eval = ebet.SetEvalMock(11);

    // Only one key needed to dispute
    CMutableTransaction disputeTx = ebet.DisputeTx(Dealer);
    CC *disputeCond = ebet.DisputeCond();
    EXPECT_EQ(1, CCSign(disputeTx, 0, disputeCond, {Player1}));

    EXPECT_FALSE(TestCC(disputeTx, 0, disputeCond));
    EXPECT_EQ("dispute-too-soon", eval.state.GetRejectReason());
}


TEST_F(TestBet, testDisputeInvalidParams)
{
    EvalMock eval = ebet.SetEvalMock(12);

    CMutableTransaction disputeTx = ebet.DisputeTx(Player2);
    CC *disputeCond = ebet.DisputeCond();
    CC *evalCond = disputeCond->subconditions[0];

    // too long
    evalCond->code = (unsigned char*) realloc(evalCond->code, ++evalCond->codeLength);
    ASSERT_EQ(1, CCSign(disputeTx, 0, disputeCond, {Player2}));
    EXPECT_FALSE(TestCC(disputeTx, 0, disputeCond));
    EXPECT_EQ("malformed-params", eval.state.GetRejectReason());

    // too short
    eval.state = CValidationState();
    evalCond->codeLength = 1;
    ASSERT_EQ(1, CCSign(disputeTx, 0, disputeCond, {Player2}));
    EXPECT_FALSE(TestCC(disputeTx, 0, disputeCond));
    EXPECT_EQ("malformed-params", eval.state.GetRejectReason());

    // is fine
    eval.state = CValidationState();
    evalCond->codeLength = 12;
    ASSERT_EQ(1, CCSign(disputeTx, 0, disputeCond, {Player2}));
    EXPECT_TRUE(TestCC(disputeTx, 0, disputeCond));
}


TEST_F(TestBet, testDisputeInvalidEvidence)
{
    EvalMock eval = ebet.SetEvalMock(12);

    CMutableTransaction disputeTx = ebet.DisputeTx(Player2);
    CC *disputeCond = ebet.DisputeCond();
    CCSign(disputeTx, 0, disputeCond, {Player2});

    CMutableTransaction mtx;

    mtx.vout.resize(1);
    mtx.vout[0].scriptPubKey = CScript();
    eval.spends[ebet.SessionTx().GetHash()][1] = CTransaction(mtx);
    ASSERT_TRUE(TestCC(disputeTx, 0, disputeCond));

    mtx.vout[0].scriptPubKey << OP_RETURN;
    eval.spends[ebet.SessionTx().GetHash()][1] = CTransaction(mtx);
    ASSERT_TRUE(TestCC(disputeTx, 0, disputeCond));

    mtx.vout[0].scriptPubKey = CScript() << 0;
    eval.spends[ebet.SessionTx().GetHash()][1] = CTransaction(mtx);
    ASSERT_TRUE(TestCC(disputeTx, 0, disputeCond));

    eval.spends[ebet.SessionTx().GetHash()].resize(1);
    eval.spends[ebet.SessionTx().GetHash()][0] = CTransaction();
    ASSERT_FALSE(TestCC(disputeTx, 0, disputeCond));
    EXPECT_EQ("no-evidence", eval.state.GetRejectReason());
}


TEST_F(TestBet, testMakeStakeTx)
{
    CTransaction stakeTx = ebet.StakeTx();
    EXPECT_EQ(0, stakeTx.vin.size());
    EXPECT_EQ(1, stakeTx.vout.size());
    EXPECT_EQ(ebet.totalPayout, stakeTx.vout[0].nValue);
    EXPECT_EQ(CCPubKey(ebet.PayoutCond()), stakeTx.vout[0].scriptPubKey);
}


TEST_F(TestBet, testMakePayoutCond)
{
    CC *payoutCond = ebet.PayoutCond();
    EXPECT_EQ("(1 of (3 of 5,5,5),(2 of (1 of 5,5,5),15))", CCShowStructure(payoutCond));
    EXPECT_EQ(0, memcmp(payoutCond->subconditions[1]->subconditions[1]->code+1,
                        ebet.SessionTx().GetHash().begin(), 32));
}


TEST_F(TestBet, testSignPayout)
{

    CMutableTransaction payoutTx = ebet.AgreePayoutTx();
    CC *payoutCond = ebet.PayoutCond();

    EXPECT_EQ(0, cc_isFulfilled(payoutCond->subconditions[0]));
    EXPECT_EQ(0, cc_isFulfilled(payoutCond->subconditions[1]));
    EXPECT_EQ(0, cc_isFulfilled(payoutCond));

    EXPECT_EQ(2, CCSign(payoutTx, 0, payoutCond, {Player1}));
    EXPECT_EQ(0, cc_isFulfilled(payoutCond->subconditions[0]));
    EXPECT_EQ(1, cc_isFulfilled(payoutCond->subconditions[1]));
    EXPECT_EQ(1, cc_isFulfilled(payoutCond));

    EXPECT_EQ(2, CCSign(payoutTx, 0, payoutCond, {Player2}));
    EXPECT_EQ(0, cc_isFulfilled(payoutCond->subconditions[0]));

    EXPECT_EQ(2, CCSign(payoutTx, 0, payoutCond, {Dealer}));
    EXPECT_EQ(1, cc_isFulfilled(payoutCond->subconditions[0]));
}


TEST_F(TestBet, testAgreePayout)
{
    EvalMock eval = ebet.SetEvalMock(12);

    CMutableTransaction payoutTx = ebet.AgreePayoutTx();
    CC *payoutCond = ebet.PayoutCond();

    EXPECT_EQ(2, CCSign(payoutTx, 0, payoutCond, {Dealer}));
    EXPECT_FALSE(TestCC(payoutTx, 0, payoutCond));
    EXPECT_EQ("(1 of (2 of (1 of 5,A5,A5),15),A2)",
             CCShowStructure(CCPrune(payoutCond)));

    EXPECT_EQ(2, CCSign(payoutTx, 0, payoutCond, {Player1}));
    EXPECT_FALSE(TestCC(payoutTx, 0, payoutCond));
    EXPECT_EQ("(1 of (2 of (1 of 5,A5,A5),15),A2)",
             CCShowStructure(CCPrune(payoutCond)));

    EXPECT_EQ(2, CCSign(payoutTx, 0, payoutCond, {Player2}));
    EXPECT_TRUE( TestCC(payoutTx, 0, payoutCond));
    EXPECT_EQ("(1 of (3 of 5,5,5),A2)",
             CCShowStructure(CCPrune(payoutCond)));
}


TEST_F(TestBet, testImportPayout)
{
    EvalMock eval = ebet.SetEvalMock(12);

    CMutableTransaction importTx = ebet.ImportPayoutTx();
    CC *payoutCond = ebet.PayoutCond();
    EXPECT_EQ(2, CCSign(importTx, 0, payoutCond, {Player2}));
    EXPECT_TRUE(TestCC(importTx, 0, payoutCond));
}


TEST_F(TestBet, testImportPayoutFewVouts)
{
    EvalMock eval = ebet.SetEvalMock(12);

    CMutableTransaction importTx = ebet.ImportPayoutTx();
    importTx.vout.resize(0);
    CC *payoutCond = ebet.PayoutCond();
    EXPECT_EQ(2, CCSign(importTx, 0, payoutCond, {Player2}));
    EXPECT_FALSE(TestCC(importTx, 0, payoutCond));
    EXPECT_EQ("no-vouts", eval.state.GetRejectReason());
}


TEST_F(TestBet, testImportPayoutInvalidPayload)
{
    EvalMock eval = ebet.SetEvalMock(12);

    CMutableTransaction importTx = ebet.ImportPayoutTx();
    importTx.vout[0].scriptPubKey.pop_back();
    CC *payoutCond = ebet.PayoutCond();
    EXPECT_EQ(2, CCSign(importTx, 0, payoutCond, {Player2}));
    EXPECT_FALSE(TestCC(importTx, 0, payoutCond));
    EXPECT_EQ("invalid-payload", eval.state.GetRejectReason());
}


TEST_F(TestBet, testImportPayoutWrongPayouts)
{
    EvalMock eval = ebet.SetEvalMock(12);

    CMutableTransaction importTx = ebet.ImportPayoutTx();
    importTx.vout[1].nValue = 7;
    CC *payoutCond = ebet.PayoutCond();
    EXPECT_EQ(2, CCSign(importTx, 0, payoutCond, {Player2}));
    ASSERT_FALSE(TestCC(importTx, 0, payoutCond));
    EXPECT_EQ("wrong-payouts", eval.state.GetRejectReason());
}


TEST_F(TestBet, testImportPayoutMangleSessionId)
{
    EvalMock eval = ebet.SetEvalMock(12);

    CMutableTransaction importTx = ebet.ImportPayoutTx();
    CC *payoutCond = ebet.PayoutCond();
    payoutCond->subconditions[1]->subconditions[1]->codeLength = 31;
    EXPECT_EQ(2, CCSign(importTx, 0, payoutCond, {Player2}));
    ASSERT_FALSE(TestCC(importTx, 0, payoutCond));
    EXPECT_EQ("malformed-params", eval.state.GetRejectReason());

    payoutCond = ebet.PayoutCond();
    memset(payoutCond->subconditions[1]->subconditions[1]->code+1, 1, 32);
    EXPECT_EQ(2, CCSign(importTx, 0, payoutCond, {Player2}));
    ASSERT_FALSE(TestCC(importTx, 0, payoutCond));
    EXPECT_EQ("wrong-session", eval.state.GetRejectReason());
}


TEST_F(TestBet, testImportPayoutInvalidNotarisationHash)
{
    EvalMock eval = ebet.SetEvalMock(12);

    MoMProof proof = ebet.GetMoMProof();
    proof.notarisationHash = uint256();
    CMutableTransaction importTx = ebet.bet.MakeImportPayoutTx(
            ebet.Payouts(Player2), ebet.DisputeTx(Player2), uint256(), proof);

    CC *payoutCond = ebet.PayoutCond();
    EXPECT_EQ(2, CCSign(importTx, 0, payoutCond, {Player2}));
    EXPECT_FALSE(TestCC(importTx, 0, payoutCond));
    EXPECT_EQ("coudnt-load-mom", eval.state.GetRejectReason());
}


TEST_F(TestBet, testImportPayoutMomFail)
{
    EvalMock eval = ebet.SetEvalMock(12);

    MoMProof proof = ebet.GetMoMProof();
    proof.branch.nIndex ^= 1;
    CMutableTransaction importTx = ebet.bet.MakeImportPayoutTx(
            ebet.Payouts(Player2), ebet.DisputeTx(Player2), uint256(), proof);

    CC *payoutCond = ebet.PayoutCond();
    EXPECT_EQ(2, CCSign(importTx, 0, payoutCond, {Player2}));
    EXPECT_FALSE(TestCC(importTx, 0, payoutCond));
    EXPECT_EQ("mom-check-fail", eval.state.GetRejectReason());
}


} /* namespace TestBet */
