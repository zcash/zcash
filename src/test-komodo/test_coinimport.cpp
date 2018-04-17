
#include <cryptoconditions.h>
#include <gtest/gtest.h>

#include "cc/importcoin.h"
#include "cc/eval.h"
#include "base58.h"
#include "core_io.h"
#include "key.h"
#include "main.h"
#include "script/cc.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "script/serverchecker.h"
#include "txmempool.h"

#include "testutils.h"


extern Eval* EVAL_TEST;

namespace TestCoinImport {


static uint8_t testNum = 0;

class TestCoinImport : public ::testing::Test, public Eval {
public:
    CMutableTransaction burnTx;
    std::vector<CTxOut> payouts;
    MomoProof proof;
    uint256 MoMoM;
    CMutableTransaction importTx;
    uint32_t chainId = 2;
    CAmount amount = 100;

    void SetImportTx() {
        burnTx.vout.resize(0);
        burnTx.vout.push_back(MakeBurnOutput(amount, chainId, payouts));
        MoMoM = burnTx.GetHash();  // TODO: an actual branch
        importTx = CMutableTransaction(MakeImportCoinTransaction(proof, CTransaction(burnTx), payouts));
    }

    uint32_t GetCurrentLedgerID() const { return chainId; }

    bool GetNotarisationData(int notarisationHeight, NotarisationData &data, bool verifyCanonical) const
    {
        if (MoMoM.IsNull()) return false;
        data.MoMoM = MoMoM;
        return true;
    }

protected:
    static void SetUpTestCase() { setupChain(); }
    virtual void SetUp() {
        ASSETCHAINS_CC = 1;
        EVAL_TEST = this;
        
        std::vector<uint8_t> fakepk;
        fakepk.resize(33);
        fakepk.begin()[0] = testNum++;
        payouts.push_back(CTxOut(amount, CScript() << fakepk << OP_CHECKSIG));
        SetImportTx();
    }


    void TestRunCCEval(CMutableTransaction mtx)
    {
        CTransaction importTx(mtx);
        PrecomputedTransactionData txdata(importTx);
        ServerTransactionSignatureChecker checker(&importTx, 0, 0, false, txdata);
        CValidationState verifystate;
        if (!VerifyCoinImport(importTx.vin[0].scriptSig, checker, verifystate))
            printf("TestRunCCEval: %s\n", verifystate.GetRejectReason().data());
    }
};


TEST_F(TestCoinImport, testProcessImportThroughPipeline)
{
    CValidationState mainstate;
    CTransaction tx(importTx);

    // first should work
    acceptTxFail(tx);
    
    // should fail in mempool
    ASSERT_FALSE(acceptTx(tx, mainstate));
    EXPECT_EQ("already in mempool", mainstate.GetRejectReason());

    // should be in persisted UTXO set
    generateBlock();
    ASSERT_FALSE(acceptTx(tx, mainstate));
    EXPECT_EQ("already have coins", mainstate.GetRejectReason());
    ASSERT_TRUE(pcoinsTip->HaveCoins(tx.GetHash()));

    // Now disconnect the block
    CValidationState invalstate;
    if (!InvalidateBlock(invalstate, chainActive.Tip())) {
        FAIL() << invalstate.GetRejectReason();
    }
    ASSERT_FALSE(pcoinsTip->HaveCoins(tx.GetHash()));

    // should be back in mempool
    ASSERT_FALSE(acceptTx(tx, mainstate));
    EXPECT_EQ("already in mempool", mainstate.GetRejectReason());
}


TEST_F(TestCoinImport, testImportTombstone)
{
    CValidationState mainstate;
    // By setting an unspendable output, there will be no addition to UTXO
    // Nonetheless, we dont want to be able to import twice
    payouts[0].scriptPubKey = CScript() << OP_RETURN;
    SetImportTx();
    MoMoM = burnTx.GetHash();  // TODO: an actual branch
    CTransaction tx(importTx);

    // first should work
    acceptTxFail(tx);

    // should be in persisted UTXO set
    generateBlock();
    ASSERT_FALSE(acceptTx(tx, mainstate));
    EXPECT_EQ("import tombstone exists", mainstate.GetRejectReason());
    ASSERT_TRUE(pcoinsTip->HaveCoins(burnTx.GetHash()));

    // Now disconnect the block
    CValidationState invalstate;
    if (!InvalidateBlock(invalstate, chainActive.Tip())) {
        FAIL() << invalstate.GetRejectReason();
    }
    // Tombstone should be gone from utxo set
    ASSERT_FALSE(pcoinsTip->HaveCoins(burnTx.GetHash()));

    // should be back in mempool
    ASSERT_FALSE(acceptTx(tx, mainstate));
    EXPECT_EQ("already in mempool", mainstate.GetRejectReason());
}


TEST_F(TestCoinImport, testNoVouts)
{
    importTx.vout.resize(0);
    TestRunCCEval(importTx);
    EXPECT_EQ("no-vouts", state.GetRejectReason());
}


TEST_F(TestCoinImport, testInvalidParams)
{
    std::vector<uint8_t> payload = E_MARSHAL(ss << EVAL_IMPORTCOIN; ss << 'a');
    importTx.vin[0].scriptSig = CScript() << payload;
    TestRunCCEval(importTx);
    EXPECT_EQ("invalid-params", state.GetRejectReason());
}


TEST_F(TestCoinImport, testNonCanonical)
{
    importTx.nLockTime = 10;
    TestRunCCEval(importTx);
    EXPECT_EQ("non-canonical", state.GetRejectReason());
}


TEST_F(TestCoinImport, testInvalidBurnOutputs)
{
    burnTx.vout.resize(0);
    MoMoM = burnTx.GetHash();  // TODO: an actual branch
    CTransaction tx = MakeImportCoinTransaction(proof, CTransaction(burnTx), payouts);
    TestRunCCEval(tx);
    EXPECT_EQ("invalid-burn-outputs", state.GetRejectReason());
}


TEST_F(TestCoinImport, testInvalidBurnParams)
{
    burnTx.vout[0].scriptPubKey = CScript() << OP_RETURN << E_MARSHAL(ss << VARINT(chainId));
    MoMoM = burnTx.GetHash();  // TODO: an actual branch
    CTransaction tx = MakeImportCoinTransaction(proof, CTransaction(burnTx), payouts);
    TestRunCCEval(tx);
    EXPECT_EQ("invalid-burn-params", state.GetRejectReason());
}


TEST_F(TestCoinImport, testWrongChainId)
{
    chainId = 0;
    TestRunCCEval(importTx);
    EXPECT_EQ("importcoin-wrong-chain", state.GetRejectReason());
}


TEST_F(TestCoinImport, testInvalidBurnAmount)
{
    burnTx.vout[0].nValue = 0;
    MoMoM = burnTx.GetHash();  // TODO: an actual branch
    CTransaction tx = MakeImportCoinTransaction(proof, CTransaction(burnTx), payouts);
    TestRunCCEval(tx);
    EXPECT_EQ("invalid-burn-amount", state.GetRejectReason());
}


TEST_F(TestCoinImport, testPayoutTooHigh)
{
    importTx.vout[0].nValue = 101;
    TestRunCCEval(importTx);
    EXPECT_EQ("payout-too-high", state.GetRejectReason());
}


TEST_F(TestCoinImport, testInvalidPayouts)
{
    importTx.vout[0].nValue = 40;
    importTx.vout.push_back(importTx.vout[0]);
    TestRunCCEval(importTx);
    EXPECT_EQ("wrong-payouts", state.GetRejectReason());
}


TEST_F(TestCoinImport, testCouldntLoadMomom)
{
    MoMoM.SetNull();
    TestRunCCEval(importTx);
    EXPECT_EQ("coudnt-load-momom", state.GetRejectReason());
}


TEST_F(TestCoinImport, testMomomCheckFail)
{
    MoMoM.SetNull();
    MoMoM.begin()[0] = 1;
    TestRunCCEval(importTx);
    EXPECT_EQ("momom-check-fail", state.GetRejectReason());
}

} /* namespace TestCoinImport */
