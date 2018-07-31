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
#include "../txmempool.h"

/*
 This file implements a simple CC faucet as an example of how to make a new CC contract. It wont have any fancy sybil protection but will serve the purpose of a fully automated faucet.
 
 In order to implement a faucet, we need to have it funded. Once it is funded, anybody should be able to get some reasonable small amount.
 
 This leads to needing to lock the funding in a CC protected output. And to put a spending limit. We can do a per transaction spending limit quite easily with vout constraints. However, that would allow anybody to issue thousands of transactions per block, so we also need to add a rate limiter.
 
 To implement this, we can simply make any faucet vout fund the faucet. Then we can limit the number of confirmed utxo by combining faucet outputs and then only using utxo which are confirmed. This combined with a vout size limit will drastically limit the funds that can be withdrawn from the faucet.
*/

// start of consensus code

uint64_t IsFaucetvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool FaucetExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
{
    static uint256 zerohash;
    CTransaction vinTx; uint256 hashBlock,activehash; int32_t i,numvins,numvouts; uint64_t inputs=0,outputs=0,assetoshis;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    for (i=0; i<numvins; i++)
    {
        //fprintf(stderr,"vini.%d\n",i);
        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) != 0 )
        {
            //fprintf(stderr,"vini.%d check mempool\n",i);
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("cant find vinTx");
            else
            {
                //fprintf(stderr,"vini.%d check hash and vout\n",i);
                if ( hashBlock == zerohash )
                    return eval->Invalid("cant faucet from mempool");
                if ( (assetoshis= IsFaucetvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsFaucetvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+COIN+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + COIN + txfee");
    }
    else return(true);
}

bool FaucetValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i; bool retval;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        //fprintf(stderr,"check vins\n");
        for (i=0; i<numvins; i++)
        {
            if ( IsCCInput(tx.vin[0].scriptSig) == 0 )
            {
                fprintf(stderr,"faucetget invalid vini\n");
                return eval->Invalid("illegal normal vini");
            }
        }
        //fprintf(stderr,"check amounts\n");
        if ( FaucetExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"faucetget invalid amount\n");
            return false;
        }
        else
        {
            preventCCvouts = 1;
            if ( IsFaucetvout(cp,tx,0) != 0 )
            {
                preventCCvouts++;
                i = 1;
            } else i = 0;
            if ( tx.vout[i].nValue != COIN )
                return eval->Invalid("invalid faucet output");
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"faucetget validated\n");
            else fprintf(stderr,"faucetget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

uint64_t AddFaucetInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint64_t total,int32_t maxinputs)
{
    char coinaddr[64]; uint64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        // prevent dup
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( (nValue= IsFaucetvout(cp,vintx,(int32_t)it->first.index)) > 1000000 )
            {
                if ( total != 0 && maxinputs != 0 )
                    mtx.vin.push_back(CTxIn(txid,(int32_t)it->first.index,CScript()));
                nValue = it->second.satoshis;
                totalinputs += nValue;
                n++;
                if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                    break;
            }
        }
    }
    return(totalinputs);
}

std::string FaucetGet(uint64_t txfee)
{
    CMutableTransaction mtx; CPubKey mypk,faucetpk; CScript opret; uint64_t inputs,CCchange=0,nValue=COIN; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_FAUCET);
    if ( txfee == 0 )
        txfee = 10000;
    faucetpk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    if ( (inputs= AddFaucetInputs(cp,mtx,faucetpk,nValue+txfee,60)) > 0 )
    {
        if ( inputs > nValue )
            CCchange = (inputs - nValue - txfee);
        if ( CCchange != 0 )
            mtx.vout.push_back(MakeCC1vout(EVAL_FAUCET,CCchange,faucetpk));
        mtx.vout.push_back(CTxOut(nValue,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        return(FinalizeCCTx(-1LL,cp,mtx,mypk,txfee,opret));
    } else fprintf(stderr,"cant find faucet inputs\n");
    return(0);
}

std::string FaucetFund(uint64_t txfee,uint64_t funds)
{
    CMutableTransaction mtx; CPubKey mypk,faucetpk; CScript opret; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_FAUCET);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    faucetpk = GetUnspendable(cp,0);
    if ( AddNormalinputs(mtx,mypk,funds+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_FAUCET,funds,faucetpk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,opret));
    }
    return(0);
}

UniValue FaucetInfo()
{
    UniValue result(UniValue::VOBJ); char numstr[64];
    CMutableTransaction mtx; CPubKey faucetpk; struct CCcontract_info *cp,C; uint64_t funding;
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","Faucet"));
    cp = CCinit(&C,EVAL_FAUCET);
    faucetpk = GetUnspendable(cp,0);
    funding = AddFaucetInputs(cp,mtx,faucetpk,0,0);
    sprintf(numstr,"%.8f",(double)funding/COIN);
    result.push_back(Pair("funding",numstr));
    return(result);
}

