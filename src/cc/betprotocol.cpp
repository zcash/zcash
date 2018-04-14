#include <cryptoconditions.h>

#include "streams.h"
#include "script/cc.h"
#include "cc/eval.h"
#include "cc/betprotocol.h"
#include "primitives/transaction.h"


std::vector<CC*> BetProtocol::PlayerConditions()
{
    std::vector<CC*> subs;
    for (int i=0; i<players.size(); i++)
        subs.push_back(CCNewSecp256k1(players[i]));
    return subs;
}


CC* BetProtocol::MakeDisputeCond()
{
    CC *disputePoker = CCNewEval(E_MARSHAL(
        ss << disputeCode << VARINT(waitBlocks) << vmParams;
    ));

    CC *anySig = CCNewThreshold(1, PlayerConditions());

    return CCNewThreshold(2, {disputePoker, anySig});
}


/*
 * spendFee is the amount assigned to each output, for the purposes of posting
 * dispute / evidence.
 */
CMutableTransaction BetProtocol::MakeSessionTx(CAmount spendFee)
{
    CMutableTransaction mtx;

    CC *disputeCond = MakeDisputeCond();
    mtx.vout.push_back(CTxOut(spendFee, CCPubKey(disputeCond)));
    cc_free(disputeCond);

    for (int i=0; i<players.size(); i++) {
        CC *cond = CCNewSecp256k1(players[i]);
        mtx.vout.push_back(CTxOut(spendFee, CCPubKey(cond)));
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
        CC *importEval = CCNewEval(E_MARSHAL(
            ss << EVAL_IMPORTPAYOUT << signedSessionTxHash;
        ));

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
    CScript proofData;
    proofData << OP_RETURN << E_MARSHAL(ss << momProof << signedDisputeTx);
    mtx.vout.insert(mtx.vout.begin(), CTxOut(0, proofData));
    return mtx;
}


bool GetOpReturnHash(CScript script, uint256 &hash)
{
    std::vector<unsigned char> vHash;
    GetOpReturnData(script, vHash);
    if (vHash.size() != 32) return false;
    hash = uint256(vHash);
    return true;
}
