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
#include "chain.h"
#include "version.h"
#include "script/cc.h"
#include "cc/eval.h"
#include "cc/betprotocol.h"
#include "primitives/transaction.h"


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
