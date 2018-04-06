#include <cryptoconditions.h>

#include "main.h"
#include "chain.h"
#include "streams.h"
#include "cc/eval.h"
#include "cc/betprotocol.h"
#include "primitives/transaction.h"


/*
 * Crypto-Condition EVAL method that verifies a payout against a transaction
 * notarised on another chain.
 *
 * IN: cond - CC EVAL node
 * IN: payoutTx - Payout transaction on value chain (KMD)
 * IN: nIn  - index of input of stake
 *
 * payoutTx: Spends stakeTx with payouts from asset chain
 *
 *   in  0:      Spends Stake TX and contains ImportPayout CC
 *   out 0:      OP_RETURN MomProof
 *   out 1:      OP_RETURN serialized disputeTx from other chain
 *   out 2-:     arbitrary payouts
 *
 * disputeTx: Spends sessionTx.0 (opener on asset chain)
 *
 *   in 0:       spends sessionTx.0
 *   in 1-:      anything
 *   out 0:      OP_RETURN hash of payouts
 *   out 1-:     anything
 */
bool Eval::ImportPayout(const CC *cond, const CTransaction &payoutTx, unsigned int nIn)
{
    // TODO: Error messages!
    if (payoutTx.vout.size() < 2) return Invalid("need-2-vouts");

    // load disputeTx from vout[1]
    CTransaction disputeTx;
    std::vector<unsigned char> exportData;
    GetOpReturnData(payoutTx.vout[1].scriptPubKey, exportData);
    if (!CheckDeserialize(exportData, disputeTx))
        return Invalid("invalid-dispute-tx");

    // Check disputeTx.0 shows correct payouts
    {
        std::vector<CTxOut> payouts(payoutTx.vout.begin() + 2, payoutTx.vout.end());
        uint256 payoutsHash = SerializeHash(payouts);
        std::vector<unsigned char> vPayoutsHash(payoutsHash.begin(), payoutsHash.end());

        if (disputeTx.vout[0].scriptPubKey != CScript() << OP_RETURN << vPayoutsHash)
            return Invalid("wrong-payouts");
    }

    // Check disputeTx spends sessionTx.0
    // condition ImportPayout params is session ID from other chain
    {
        if (cond->paramsBinLength != 32) return Invalid("malformed-params");
        COutPoint prevout = disputeTx.vin[0].prevout;
        if (memcmp(prevout.hash.begin(), cond->paramsBin, 32) != 0 ||
                   prevout.n != 0) return Invalid("wrong-session");
    }

    // Check disputeTx solves momproof from vout[0]
    {
        std::vector<unsigned char> vProof;
        GetOpReturnData(payoutTx.vout[0].scriptPubKey, vProof);
        MoMProof proof;
        if (!CheckDeserialize(vProof, proof))
            return Invalid("invalid-mom-proof-payload");
        
        NotarisationData data;
        if (!GetNotarisationData(proof.notarisationHash, data)) return Invalid("coudnt-load-mom");

        if (data.MoM != proof.Exec(disputeTx.GetHash()))
            return Invalid("mom-check-fail");
    }

    return Valid();
}
