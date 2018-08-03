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

#include "CClotto.h"
#include "../txmempool.h"

/*
*/

// start of consensus code

uint64_t IsLottovout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool LottoExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
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
                    return eval->Invalid("cant Lotto from mempool");
                if ( (assetoshis= IsLottovout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsLottovout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+COIN+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + COIN + txfee");
    }
    else return(true);
}

bool LottoValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)
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
                fprintf(stderr,"Lottoget invalid vini\n");
                return eval->Invalid("illegal normal vini");
            }
        }
        //fprintf(stderr,"check amounts\n");
        if ( LottoExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Lottoget invalid amount\n");
            return false;
        }
        else
        {
            preventCCvouts = 1;
            if ( IsLottovout(cp,tx,0) != 0 )
            {
                preventCCvouts++;
                i = 1;
            } else i = 0;
            if ( tx.vout[i].nValue != COIN )
                return eval->Invalid("invalid Lotto output");
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Lottoget validated\n");
            else fprintf(stderr,"Lottoget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

uint64_t AddLottoInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint64_t total,int32_t maxinputs)
{
    char coinaddr[64]; uint64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        // prevent dup
        if ( it->second.satoshis < 1000000 )
            continue;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( (nValue= IsLottovout(cp,vintx,(int32_t)it->first.index)) > 0 )
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

std::string LottoTicket(uint64_t txfee,uint64_t numtickets)
{
    CMutableTransaction mtx; CPubKey mypk,Lottopk; CScript opret; uint64_t inputs,CCchange=0,nValue=COIN; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_LOTTO);
    if ( txfee == 0 )
        txfee = 10000;
    Lottopk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    if ( (inputs= AddLottoInputs(cp,mtx,Lottopk,nValue+txfee,60)) > 0 )
    {
        if ( inputs > nValue )
            CCchange = (inputs - nValue - txfee);
        if ( CCchange != 0 )
            mtx.vout.push_back(MakeCC1vout(EVAL_LOTTO,CCchange,Lottopk));
        mtx.vout.push_back(CTxOut(nValue,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        return(FinalizeCCTx(-1LL,cp,mtx,mypk,txfee,opret));
    } else fprintf(stderr,"cant find Lotto inputs\n");
    return(0);
}

std::string LottoWinner(uint64_t txfee)
{
    CMutableTransaction mtx; CPubKey mypk,Lottopk; uint64_t winnings = 0; CScript opret; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_LOTTO);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    Lottopk = GetUnspendable(cp,0);
    if ( AddNormalinputs(mtx,mypk,txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_LOTTO,winnings,Lottopk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,opret));
    }
    return(0);
}


