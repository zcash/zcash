/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include <cryptoconditions.h>

#include "hash.h"
#include "main.h"
#include "chain.h"
#include "streams.h"
#include "script/cc.h"
#include "cc/betprotocol.h"
#include "cc/eval.h"
#include "cc/utils.h"
#include "primitives/transaction.h"

int32_t komodo_nextheight();

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
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

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
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

    CC *disputeCond = MakeDisputeCond();
    mtx.vin.push_back(CTxIn(signedSessionTxHash, 0, CScript()));

    std::vector<unsigned char> result(vmResultHash.begin(), vmResultHash.begin()+32);
    mtx.vout.push_back(CTxOut(0, CScript() << OP_RETURN << result));
    return mtx;
}


CMutableTransaction BetProtocol::MakePostEvidenceTx(uint256 signedSessionTxHash,
        int playerIdx, std::vector<unsigned char> state)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

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
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

    CC *payoutCond = MakePayoutCond(signedSessionTxHash);
    mtx.vout.push_back(CTxOut(totalPayout, CCPubKey(payoutCond)));
    cc_free(payoutCond);

    return mtx;
}


CMutableTransaction BetProtocol::MakeAgreePayoutTx(std::vector<CTxOut> payouts,
        uint256 signedStakeTxHash)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    mtx.vin.push_back(CTxIn(signedStakeTxHash, 0, CScript()));
    mtx.vout = payouts;
    return mtx;
}


CMutableTransaction BetProtocol::MakeImportPayoutTx(std::vector<CTxOut> payouts,
        CTransaction signedDisputeTx, uint256 signedStakeTxHash, MoMProof momProof)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
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


/*
 * Crypto-Condition EVAL method that verifies a payout against a transaction
 * notarised on another chain.
 *
 * IN: params - condition params
 * IN: importTx - Payout transaction on value chain (KMD)
 * IN: nIn  - index of input of stake
 *
 * importTx: Spends stakeTx with payouts from asset chain
 *
 *   in  0:      Spends Stake TX and contains ImportPayout CC
 *   out 0:      OP_RETURN MomProof, disputeTx
 *   out 1-:     arbitrary payouts
 *
 * disputeTx: Spends sessionTx.0 (opener on asset chain)
 *
 *   in 0:       spends sessionTx.0
 *   in 1-:      anything
 *   out 0:      OP_RETURN hash of payouts
 *   out 1-:     anything
 */
bool Eval::ImportPayout(const std::vector<uint8_t> params, const CTransaction &importTx, unsigned int nIn)
{
    if (importTx.vout.size() == 0) return Invalid("no-vouts");

    // load data from vout[0]
    MoMProof proof;
    CTransaction disputeTx;
    {
        std::vector<unsigned char> vopret;
        GetOpReturnData(importTx.vout[0].scriptPubKey, vopret);
        if (!E_UNMARSHAL(vopret, ss >> proof; ss >> disputeTx))
            return Invalid("invalid-payload");
    }

    // Check disputeTx.0 shows correct payouts
    {
        uint256 givenPayoutsHash;
        GetOpReturnHash(disputeTx.vout[0].scriptPubKey, givenPayoutsHash);
        std::vector<CTxOut> payouts(importTx.vout.begin() + 1, importTx.vout.end());
        if (givenPayoutsHash != SerializeHash(payouts))
            return Invalid("wrong-payouts");
    }

    // Check disputeTx spends sessionTx.0
    // condition ImportPayout params is session ID from other chain
    {
        uint256 sessionHash;
        if (!E_UNMARSHAL(params, ss >> sessionHash))
            return Invalid("malformed-params");
        if (disputeTx.vin[0].prevout != COutPoint(sessionHash, 0))
            return Invalid("wrong-session");
    }

    // Check disputeTx solves momproof from vout[0]
    {
        NotarisationData data(0);
        if (!GetNotarisationData(proof.notarisationHash, data))
            return Invalid("coudnt-load-mom");

        if (data.MoM != proof.branch.Exec(disputeTx.GetHash()))
            return Invalid("mom-check-fail");
    }

    return Valid();
}


/*
 * Crypto-Condition EVAL method that resolves a dispute of a session
 *
 * IN: vm - AppVM virtual machine to verify states
 * IN: params - condition params
 * IN: disputeTx - transaction attempting to resolve dispute
 * IN: nIn  - index of input of dispute tx
 *
 * disputeTx: attempt to resolve a dispute
 *
 *   in  0:      Spends Session TX first output, reveals DisputeHeader
 *   out 0:      OP_RETURN hash of payouts
 */
bool Eval::DisputePayout(AppVM &vm, std::vector<uint8_t> params, const CTransaction &disputeTx, unsigned int nIn)
{
    if (disputeTx.vout.size() == 0) return Invalid("no-vouts");

    // get payouts hash
    uint256 payoutHash;
    if (!GetOpReturnHash(disputeTx.vout[0].scriptPubKey, payoutHash))
        return Invalid("invalid-payout-hash");

    // load params
    uint16_t waitBlocks;
    std::vector<uint8_t> vmParams;
    if (!E_UNMARSHAL(params, ss >> VARINT(waitBlocks); ss >> vmParams))
        return Invalid("malformed-params");

    // ensure that enough time has passed
    {
        CTransaction sessionTx;
        CBlockIndex sessionBlock;
        
        // if unconformed its too soon
        if (!GetTxConfirmed(disputeTx.vin[0].prevout.hash, sessionTx, sessionBlock))
            return Error("couldnt-get-parent");

        if (GetCurrentHeight() < sessionBlock.GetHeight() + waitBlocks)
            return Invalid("dispute-too-soon");  // Not yet
    }

    // get spends
    std::vector<CTransaction> spends;
    if (!GetSpendsConfirmed(disputeTx.vin[0].prevout.hash, spends))
        return Error("couldnt-get-spends");

    // verify result from VM
    int maxLength = -1;
    uint256 bestPayout;
    for (int i=1; i<spends.size(); i++)
    {
        std::vector<unsigned char> vmState;
        if (spends[i].vout.size() == 0) continue;
        if (!GetOpReturnData(spends[i].vout[0].scriptPubKey, vmState)) continue;
        auto out = vm.evaluate(vmParams, vmState);
        uint256 resultHash = SerializeHash(out.second);
        if (out.first > maxLength) {
            maxLength = out.first;
            bestPayout = resultHash;
        }
        // The below means that if for any reason there is a draw, the first dispute wins
        else if (out.first == maxLength) {
            if (bestPayout != payoutHash) {
                fprintf(stderr, "WARNING: VM has multiple solutions of same length\n");
                bestPayout = resultHash;
            }
        }
    }

    if (maxLength == -1) return Invalid("no-evidence");

    return bestPayout == payoutHash ? Valid() : Invalid("wrong-payout");
}
