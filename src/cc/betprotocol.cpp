#include <cryptoconditions.h>

#include "streams.h"
#include "komodo_cc.h"
#include "cc/eval.h"
#include "cc/betprotocol.h"
#include "primitives/transaction.h"


static unsigned char* CopyPubKey(CPubKey pkIn)
{
    unsigned char* pk = (unsigned char*) malloc(33);
    memcpy(pk, pkIn.begin(), 33);  // TODO: compressed?
    return pk;
}


CC* CCNewThreshold(int t, std::vector<CC*> v)
{
    CC *cond = cc_new(CC_Threshold);
    cond->threshold = t;
    cond->size = v.size();
    cond->subconditions = (CC**) calloc(v.size(), sizeof(CC*));
    memcpy(cond->subconditions, v.data(), v.size() * sizeof(CC*));
    return cond;
}


CC* CCNewSecp256k1(CPubKey k)
{
    CC *cond = cc_new(CC_Secp256k1);
    cond->publicKey = CopyPubKey(k);
    return cond;
}


CC* CCNewEval(std::string method, std::vector<unsigned char> paramsBin)
{
    CC *cond = cc_new(CC_Eval);
    strcpy(cond->method, method.data());
    cond->paramsBin = (unsigned char*) malloc(paramsBin.size());
    memcpy(cond->paramsBin, paramsBin.data(), paramsBin.size());
    cond->paramsBinLength = paramsBin.size();
    return cond;
}


std::vector<CC*> BetProtocol::PlayerConditions()
{
    std::vector<CC*> subs;
    for (int i=0; i<players.size(); i++)
        subs.push_back(CCNewSecp256k1(players[i]));
    return subs;
}


CC* BetProtocol::MakeDisputeCond()
{
    CC *disputePoker = CCNewEval(disputeFunc, CheckSerialize(disputeHeader));

    CC *anySig = CCNewThreshold(1, PlayerConditions());

    return CCNewThreshold(2, {disputePoker, anySig});
}


CMutableTransaction BetProtocol::MakeSessionTx()
{
    CMutableTransaction mtx;

    CC *disputeCond = MakeDisputeCond();
    mtx.vout.push_back(CTxOut(MINFEE, CCPubKey(disputeCond)));
    cc_free(disputeCond);

    for (int i=0; i<players.size(); i++) {
        CC *cond = CCNewSecp256k1(players[i]);
        mtx.vout.push_back(CTxOut(MINFEE, CCPubKey(cond)));
        cc_free(cond);
    }
    return mtx;
}


CMutableTransaction BetProtocol::MakeDisputeTx(uint256 signedSessionTxHash, uint256 vmResultHash)
{
    CMutableTransaction mtx;

    CC *disputeCond = MakeDisputeCond();
    mtx.vin.push_back(CTxIn(signedSessionTxHash, 0, CScript()));

    std::vector<unsigned char> result(vmResultHash.begin(), vmResultHash.begin()+32);
    mtx.vout.push_back(CTxOut(0, CScript() << OP_RETURN << result));
    return mtx;
}


CMutableTransaction BetProtocol::MakePostEvidenceTx(uint256 signedSessionTxHash,
        int playerIdx, std::vector<unsigned char> state)
{
    CMutableTransaction mtx;

    mtx.vin.push_back(CTxIn(signedSessionTxHash, playerIdx+1, CScript()));
    mtx.vout.push_back(CTxOut(0, CScript() << OP_RETURN << state));

    return mtx;
}


CC* BetProtocol::MakePayoutCond(uint256 signedSessionTxHash)
{
    // TODO: 2/3 majority
    CC* agree = CCNewThreshold(players.size(), PlayerConditions());

    CC *import;
    {
        std::vector<unsigned char> vHash(signedSessionTxHash.begin(), signedSessionTxHash.end());
        CC *importEval = CCNewEval("ImportPayout", vHash);

        CC *oneof = CCNewThreshold(1, PlayerConditions());

        import = CCNewThreshold(2, {oneof, importEval});
    }

    return CCNewThreshold(1, {agree, import});
}


CMutableTransaction BetProtocol::MakeStakeTx(CAmount totalPayout, uint256 signedSessionTxHash)
{
    CMutableTransaction mtx;

    CC *payoutCond = MakePayoutCond(signedSessionTxHash);
    mtx.vout.push_back(CTxOut(totalPayout, CCPubKey(payoutCond)));
    cc_free(payoutCond);

    return mtx;
}


CMutableTransaction BetProtocol::MakeAgreePayoutTx(std::vector<CTxOut> payouts,
        uint256 signedStakeTxHash)
{
    CMutableTransaction mtx;
    mtx.vin.push_back(CTxIn(signedStakeTxHash, 0, CScript()));
    mtx.vout = payouts;
    return mtx;
}


CMutableTransaction BetProtocol::MakeImportPayoutTx(std::vector<CTxOut> payouts,
        CTransaction signedDisputeTx, uint256 signedStakeTxHash, MoMProof momProof)
{
    CMutableTransaction mtx;
    mtx.vin.push_back(CTxIn(signedStakeTxHash, 0, CScript()));
    mtx.vout = payouts;
    mtx.vout.insert(mtx.vout.begin(),   CTxOut(0, CScript() << OP_RETURN << CheckSerialize(momProof)));
    mtx.vout.insert(mtx.vout.begin()+1, CTxOut(0, CScript() << OP_RETURN << CheckSerialize(signedDisputeTx)));
    return mtx;
}
