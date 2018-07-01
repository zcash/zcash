#include <cryptoconditions.h>
#include <gtest/gtest.h>

#include "base58.h"
#include "key.h"
#include "script/cc.h"
#include "cc/eval.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "script/serverchecker.h"

#include "testutils.h"



class CCTest : public ::testing::Test {
public:
    void CCSign(CMutableTransaction &tx, CC *cond) {
        tx.vin.resize(1);
        PrecomputedTransactionData txdata(tx);
        uint256 sighash = SignatureHash(CCPubKey(cond), tx, 0, SIGHASH_ALL, 0, 0, &txdata);

        int out = cc_signTreeSecp256k1Msg32(cond, notaryKey.begin(), sighash.begin());
        tx.vin[0].scriptSig = CCSig(cond);
    }
protected:
    virtual void SetUp() {
        // enable CC
        ASSETCHAINS_CC = 1;
    }
};


TEST_F(CCTest, testIsPayToCryptoCondition)
{
    CScript s = CScript() << VCH("a", 1);
    ASSERT_FALSE(s.IsPayToCryptoCondition());
        
    s = CScript() << VCH("a", 1) << OP_CHECKCRYPTOCONDITION;
    ASSERT_TRUE(s.IsPayToCryptoCondition());

    s = CScript() << OP_CHECKCRYPTOCONDITION;
    ASSERT_FALSE(s.IsPayToCryptoCondition());
}


TEST_F(CCTest, testMayAcceptCryptoCondition)
{
    CC *cond;

    // ok
    CCFromJson(cond, R"!!(
    { "type": "threshold-sha-256",
      "threshold": 2,
      "subfulfillments": [
          { "type": "secp256k1-sha-256", "publicKey": "0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47" }
      ]
    })!!");
    ASSERT_TRUE(CCPubKey(cond).MayAcceptCryptoCondition());


    // prefix not allowed
    CCFromJson(cond, R"!!(
    { "type": "prefix-sha-256",
      "prefix": "abc",
      "maxMessageLength": 10,
      "subfulfillment":
          { "type": "secp256k1-sha-256", "publicKey": "0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47" }
      })!!");
    ASSERT_FALSE(CCPubKey(cond).MayAcceptCryptoCondition());


    // has no signature nodes
    CCFromJson(cond, R"!!(
    { "type": "threshold-sha-256",
      "threshold": 1,
      "subfulfillments": [
          { "type": "eval-sha-256", "code": "" },
          { "type": "eval-sha-256", "code": "" }
      ]
    })!!");
    ASSERT_FALSE(CCPubKey(cond).MayAcceptCryptoCondition());
}


static bool CCVerify(const CMutableTransaction &mtxTo, const CC *cond) {
    CAmount amount;
    ScriptError error;
    CTransaction txTo(mtxTo);
    PrecomputedTransactionData txdata(txTo);
    auto checker = ServerTransactionSignatureChecker(&txTo, 0, amount, false, txdata);
    return VerifyScript(CCSig(cond), CCPubKey(cond), 0, checker, 0, &error);
};


TEST_F(CCTest, testVerifyCryptoCondition)
{
    CC *cond;
    CMutableTransaction mtxTo;

    // ok
    cond = CCNewSecp256k1(notaryKey.GetPubKey());
    CCFromJson(cond, R"!!({
      "type": "secp256k1-sha-256",
      "publicKey": "0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47"
    })!!");
    CCSign(mtxTo, cond);
    ASSERT_TRUE(CCVerify(mtxTo, cond));

    
    // has signature nodes
    CCFromJson(cond, R"!!({
      "type": "threshold-sha-256",
      "threshold": 1,
      "subfulfillments": [
          { "type": "preimage-sha-256", "preimage": "" },
          { "type": "secp256k1-sha-256", "publicKey": "0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47" }
      ]
    })!!");
    cond->threshold = 2;
    CCSign(mtxTo, cond);
    ASSERT_TRUE(CCVerify(mtxTo, cond));

    // no signatures; the preimage will get encoded as a fulfillment because it's cheaper
    // and the secp256k1 node will get encoded as a condition
    cond->threshold = 1;
    ASSERT_FALSE(CCVerify(mtxTo, cond));

    // here the signature is set wrong
    cond->threshold = 2;
    ASSERT_TRUE(CCVerify(mtxTo, cond));
    memset(cond->subconditions[1]->signature, 0, 32); 
    ASSERT_FALSE(CCVerify(mtxTo, cond));
}

extern Eval* EVAL_TEST;

TEST_F(CCTest, testVerifyEvalCondition)
{

    class EvalMock : public Eval
    {
    public:
        bool Dispatch(const CC *cond, const CTransaction &txTo, unsigned int nIn)
        { return cond->code[0] ? Valid() : Invalid(""); }
    };

    EvalMock eval;
    EVAL_TEST = &eval;


    CC *cond;
    CMutableTransaction mtxTo;

    // ok
    cond = CCNewThreshold(2, { CCNewSecp256k1(notaryKey.GetPubKey()), CCNewEval({1}) });
    CCSign(mtxTo, cond);
    ASSERT_TRUE(CCVerify(mtxTo, cond));

    cond->subconditions[1]->code[0] = 0;
    ASSERT_FALSE(CCVerify(mtxTo, cond));
}


TEST_F(CCTest, testCryptoConditionsDisabled)
{
    CC *cond;
    ScriptError error;
    CMutableTransaction mtxTo;

    // ok
    CCFromJson(cond, R"!!({
      "type": "secp256k1-sha-256",
      "publicKey": "0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47"
    })!!");
    CCSign(mtxTo, cond);
    ASSERT_TRUE(CCVerify(mtxTo, cond));

    ASSETCHAINS_CC = 0;
    ASSERT_FALSE(CCVerify(mtxTo, cond));
}


TEST_F(CCTest, testLargeCondition)
{
    CC *cond;
    ScriptError error;
    CMutableTransaction mtxTo;

    std::vector<CC*> ccs;
    for (int i=0; i<18; i++) {
        ccs.push_back(CCNewSecp256k1(notaryKey.GetPubKey()));
    }
    cond = CCNewThreshold(16, ccs);
    CCSign(mtxTo, cond);
    EXPECT_EQ("(16 of 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,A5,A5)",
             CCShowStructure(CCPrune(cond)));
    EXPECT_EQ(1744, CCSig(cond).size());
    ASSERT_TRUE(CCVerify(mtxTo, cond));
}
