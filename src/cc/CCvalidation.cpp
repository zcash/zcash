/******************************************************************************
 * Copyright © 2014-2020 The SuperNET Developers.                             *
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

/*
 CCValidaton has common validation functions that should be used to validate some basic things in all CCs.
 */

#include "CCinclude.h"

bool FetchCCtx(uint256 txid, CTransaction& tx, struct CCcontract_info *cp)
{
    EvalRef eval; uint256 hashBlock;
    if (myGetTransaction(txid,tx,hashBlock)==0) return (false);
    return (ValidateCCtx(tx,cp));
}

bool ValidateCCtx(const CTransaction& tx, struct CCcontract_info *cp)
{
    EvalRef eval;
    if (cp->validate(cp,eval.get(),tx,0)) return (true);
    return (false);
}

bool ExactAmounts(Eval* eval, const CTransaction &tx, uint64_t txfee)
{
    CTransaction vinTx; uint256 hashBlock; int32_t i,numvins,numvouts; int64_t inputs=0,outputs=0;

    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    for (i=0; i<numvins; i++)
    {
        if ( myGetTransaction(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
            return eval->Invalid("ExactAmounts - cannot find tx for vin."+std::to_string(i));
        inputs += vinTx.vout[tx.vin[i].prevout.n].nValue;
    }
    for (i=0; i<numvouts; i++) outputs+=tx.vout[i].nValue;
    if ( inputs != outputs+txfee ) return eval->Invalid("invalid total amounts - inputs != outputs + txfee!");
    return (true);
}

//no_burn - every OP_RETURN vout must not have >0 nValue, 
//no_multi - transaction cannot have multiple OP_RETURN vouts, 
//last_vout - no OP_RETURN vout is valid anywhere except vout[-1]
bool CCOpretCheck(Eval* eval, const CTransaction &tx, bool no_burn, bool no_multi, bool last_vout)
{ 
    int count=0,i=0;
    int numvouts = tx.vout.size();

    for (i=0;i<numvouts;i++)
    {
        if ( tx.vout[i].scriptPubKey[0] == OP_RETURN )
        {
            count++;
            if ( no_burn && tx.vout[i].nValue != 0 ) return eval->Invalid("invalid OP_RETURN vout, its value must be 0!");
            if ( last_vout && i != numvouts-1 ) return eval->Invalid("invalid OP_RETURN vout, it must be the last vout in tx!");  
        } 
    }
    if (count == 0) return eval->Invalid("invalid CC tx, no OP_RETURN vout!");
    if ( no_multi && count > 1) return eval->Invalid("multiple OP_RETURN vouts are not allowed in single tx!");  
    return true;
}
