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
        NotarisationData data;
        if (!GetNotarisationData(proof.notarisationHash, data))
            return Invalid("coudnt-load-mom");

        if (data.MoM != proof.Exec(disputeTx.GetHash()))
            return Invalid("mom-check-fail");
    }

    return Valid();
}
