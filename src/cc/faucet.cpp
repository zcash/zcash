/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
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

#include "CCfaucet.h"

/*
 This file implements a simple CC faucet as an example of how to make a new CC contract. It wont have any fancy sybil protection but will serve the purpose of a fully automated faucet.
 
 In order to implement a faucet, we need to have it funded. Once it is funded, anybody should be able to get some reasonable small amount.
 
 This leads to needing to lock the funding in a CC protected output. And to put a spending limit. We can do a per transaction spending limit quite easily with vout constraints. However, that would allow anybody to issue thousands of transactions per block, so we also need to add a rate limiter.
 
 To implement this, we can simply make any faucet vout fund the faucet. Then we can limit the number of confirmed utxo by combining faucet outputs and then only using utxo which are confirmed. This combined with a vout size limit will drastically limit the funds that can be withdrawn from the faucet.
*/

CC *MakeFaucetCond(CPubKey pk)
{
    std::vector<CC*> pks; uint8_t evalcode = EVAL_ASSETS;
    pks.push_back(CCNewSecp256k1(pk));
    CC *assetCC = CCNewEval(E_MARSHAL(ss << evalcode));
    CC *Sig = CCNewThreshold(1, pks);
    return CCNewThreshold(2, {assetCC, Sig});
}

CTxOut MakeFaucetVout(CAmount nValue,CPubKey pk)
{
    CTxOut vout;
    CC *payoutCond = MakeFaucetCond(pk);
    vout = CTxOut(nValue,CCPubKey(payoutCond));
    cc_free(payoutCond);
    return(vout);
}

uint64_t IsFaucetvout(const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,FaucetCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool FaucetExactAmounts(Eval* eval,const CTransaction &tx)
{
    CTransaction vinTx; uint256 hashBlock; int32_t i,numvins,numvouts; uint64_t inputs=0,outputs=0,assetoshis;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    for (i=1; i<numvins; i++)
    {
        if ( IsFaucetInput(tx.vin[i].scriptSig) != 0 )
        {
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("always should find vin, but didnt");
            else if ( (assetoshis= IsFaucetvout(vinTx,tx.vin[i].prevout.n)) != 0 )
                inputs += assetoshis;
        }
    }
    for (i=0; i<numvouts; i++)
    {
        if ( (assetoshis= IsFaucetvout(tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs )
        return(false);
    else return(true);
}

bool FaucetValidate(Eval* eval,const CTransaction &tx)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts;
    fprintf(stderr,"FaucetValidate\n");
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    outputs = inputs = 0;
    preventCCvins = preventCCvouts = -1;
    if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
        return eval->Invalid("illegal asset vin0");
    else if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else if ( FaucetExactAmounts(eval,tx) == false )
        eval->Invalid("asset inputs != outputs");
    else return(PreventCC(tx,preventCCvins,numvins,preventCCvouts,numvouts));
}

bool ProcessFaucet(Eval* eval, std::vector<uint8_t> paramsNull,const CTransaction &ctx, unsigned int nIn)
{
    static uint256 zero,prevtxid;
    txid = ctx.GetHash();
    if ( txid == prevtxid )
        return(true);
    if ( paramsNull.size() != 0 ) // Don't expect params
        return eval->Invalid("Cannot have params");
    else if ( ctx.vout.size() == 0 )
        return eval->Invalid("no-vouts");
    if ( FaucetValidate(eval,ctx,n) != 0 )
    {
        prevtxid = txid;
        return(true);
    } else return(false);
}
#endif
