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

#include "CCPrices.h"

/*
 Prices CC would best build on top of the oracles CC, ie. to combine payments for multiple oracles and to calculate a 51% protected price feed.
 
 We need to assume there is an oracle for a specific price. In the event there are more than one provider, the majority need to be within correlation distance to update a pricepoint.

 int64_t OraclePrice(int32_t height,uint256 reforacletxid,char *markeraddr,char *format);

 Using the above function, a consensus price can be obtained for a datasource.
 
 given an oracletxid, the marketaddr and format can be extracted to be used for future calls to OraclePrice. This allows to set a starting price and that in turn allows cash settled leveraged trading!
 
 Funds work like with dice, ie. there is a Prices plan that traders bet against.
 
 PricesOpen -> oracletxid start with 'L' price, leverage, amount
 funds are locked into global CC address
    it can be closed at anytime by the trader for cash settlement
    the house account can close it if rekt
 
 Implementation Notes:
  In order to eliminate the need for worrying about sybil attacks, each prices plan would be able to specific pubkey(s?) for whitelisted publishers. It would be possible to have a non-whitelisted plan that would use 50% correlation between publishers. 
 
 delta neutral balancing of risk exposure
 
*/

// start of consensus code

int64_t IsPricesvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool PricesExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
{
    static uint256 zerohash;
    CTransaction vinTx; uint256 hashBlock,activehash; int32_t i,numvins,numvouts; int64_t inputs=0,outputs=0,assetoshis;
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
                    return eval->Invalid("cant Prices from mempool");
                if ( (assetoshis= IsPricesvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsPricesvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool PricesValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numblocks; bool retval; uint256 txid; uint8_t hash[32]; char str[65],destaddr[64];
    return(false);
    std::vector<std::pair<CAddressIndexKey, CAmount> > txids;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        for (i=0; i<numvins; i++)
        {
            if ( IsCCInput(tx.vin[0].scriptSig) == 0 )
            {
                return eval->Invalid("illegal normal vini");
            }
        }
        //fprintf(stderr,"check amounts\n");
        if ( PricesExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Pricesget invalid amount\n");
            return false;
        }
        else
        {
            txid = tx.GetHash();
            memcpy(hash,&txid,sizeof(hash));
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Pricesget validated\n");
            else fprintf(stderr,"Pricesget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddPricesInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs)
{
    char coinaddr[64]; int64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        // no need to prevent dup
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( (nValue= IsPricesvout(cp,vintx,vout)) > 1000000 && myIsutxo_spentinmempool(txid,vout) == 0 )
            {
                if ( total != 0 && maxinputs != 0 )
                    mtx.vin.push_back(CTxIn(txid,vout,CScript()));
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

#ifdef later
UniValue PricesInfo(uint256 pricesid)
{
    UniValue result(UniValue::VOBJ); CPubKey pricepk; uint256 hashBlock,oracletxid; CTransaction vintx; int64_t minbet,maxbet,maxodds; uint64_t funding; char numstr[65]; struct CCcontract_info *cp,C;
    if ( GetTransaction(pricesid,vintx,hashBlock,false) == 0 )
    {
        fprintf(stderr,"cant find fundingtxid\n");
        ERR_RESULT("cant find fundingtxid");
        return(result);
    }
    if ( vintx.vout.size() > 0 && DecodePricesFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,oracletxid,minbet,maxbet,maxodds) == 0 )
    {
        fprintf(stderr,"fundingtxid isnt price creation txid\n");
        ERR_RESULT("fundingtxid isnt price creation txid");
        return(result);
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("pricesid",uint256_str(str,pricesid)));
    result.push_back(Pair("oracletxid",uint256_str(str,oracletxid)));
    sprintf(numstr,"%.8f",(double)minbet/COIN);
    result.push_back(Pair("minbet",numstr));
    sprintf(numstr,"%.8f",(double)maxbet/COIN);
    result.push_back(Pair("maxbet",numstr));
    result.push_back(Pair("maxodds",maxodds));
    cp = CCinit(&C,EVAL_PRICES);
    pricepk = GetUnspendable(cp,0);
    funding = PricePlanFunds(cp,pricepk,pricesid);
    sprintf(numstr,"%.8f",(double)funding/COIN);
    result.push_back(Pair("funding",numstr));
    return(result);
}

UniValue PricesList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; struct CCcontract_info *cp,C; uint256 txid,hashBlock,oracletxid; CTransaction vintx; int64_t minbet,maxbet,maxodds; char str[65];
    cp = CCinit(&C,EVAL_PRICES);
    SetCCtxids(addressIndex,cp->normaladdr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( vintx.vout.size() > 0 && DecodePricesFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,oracletxid,minbet,maxbet,maxodds) != 0 )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}

std::string PricesCreateFunding(uint64_t txfee,char *planstr,int64_t funds,int64_t minbet,int64_t maxbet,int64_t maxodds,int64_t timeoutblocks)
{
    CMutableTransaction mtx; uint256 zero; CScript fundingPubKey; CPubKey mypk,pricepk; int64_t a,b,c,d; uint64_t sbits; struct CCcontract_info *cp,C;
    if ( funds < 0 || minbet < 0 || maxbet < 0 || maxodds < 1 || maxodds > 9999 || timeoutblocks < 0 || timeoutblocks > 1440 )
    {
        CCerror = "invalid parameter error";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    if ( funds < 100*COIN )
    {
        CCerror = "price plan needs at least 100 coins";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    memset(&zero,0,sizeof(zero));
    if ( (cp= Pricesinit(fundingPubKey,zero,&C,planstr,txfee,mypk,pricepk,sbits,a,b,c,d)) == 0 )
    {
        CCerror = "Priceinit error in create funding";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,funds+3*txfee,60) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,funds,pricepk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(pricepk)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodePricesFundingOpRet('F',sbits,minbet,maxbet,maxodds,timeoutblocks)));
    }
    CCerror = "cant find enough inputs";
    fprintf(stderr,"%s\n", CCerror.c_str() );
    return("");
}

std::string PricesAddfunding(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t amount)
{
    CMutableTransaction mtx; CScript fundingPubKey,scriptPubKey; CPubKey mypk,pricepk; struct CCcontract_info *cp,C; int64_t minbet,maxbet,maxodds;
    if ( amount < 0 )
    {
        CCerror = "amount must be positive";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    if ( (cp= Pricesinit(fundingPubKey,fundingtxid,&C,planstr,txfee,mypk,pricepk,sbits,minbet,maxbet,maxodds,timeoutblocks)) == 0 )
        return("");
    scriptPubKey = CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG;
    if ( scriptPubKey == fundingPubKey )
    {
        if ( AddNormalinputs(mtx,mypk,amount+2*txfee,60) > 0 )
        {
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,pricepk));
            mtx.vout.push_back(CTxOut(txfee,fundingPubKey));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodePricesOpRet('E',sbits,fundingtxid,hentropy,zeroid)));
        }
        else
        {
            CCerror = "cant find enough inputs";
            fprintf(stderr,"%s\n", CCerror.c_str() );
        }
    }
    else
    {
        CCerror = "only fund creator can add more funds (entropy)";
        fprintf(stderr,"%s\n", CCerror.c_str() );
    }
    return("");
}

std::string PricesBet(uint64_t txfee,uint256 pricesid,int64_t bet,int32_t odds)
{
    CMutableTransaction mtx; CScript fundingPubKey; CPubKey mypk,pricepk; int64_t funding,minbet,maxbet,maxodds; struct CCcontract_info *cp,C;
    if ( bet < 0 )
    {
        CCerror = "bet must be positive";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    if ( odds < 1 || odds > 9999 )
    {
        CCerror = "odds must be between 1 and 9999";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    if ( (cp= Pricesinit(fundingPubKey,pricesid,&C,txfee,mypk,pricepk,minbet,maxbet,maxodds)) == 0 )
        return("");
    if ( bet < minbet || bet > maxbet || odds > maxodds )
    {
        CCerror = strprintf("Price plan %s illegal bet %.8f: minbet %.8f maxbet %.8f or odds %d vs max.%d\n",planstr,(double)bet/COIN,(double)minbet/COIN,(double)maxbet/COIN,(int32_t)odds,(int32_t)maxodds);
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    if ( (funding= PricesPlanFunds(cp,pricepk,pricesid)) >= 2*bet*odds+txfee )
    {
        if ( myIsutxo_spentinmempool(entropytxid,0) != 0 )
        {
            CCerror = "entropy txid is spent";
            fprintf(stderr,"%s\n", CCerror.c_str() );
            return("");
        }
        if ( AddNormalinputs(mtx,mypk,bet+2*txfee+odds,60) > 0 )
        {
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,entropyval,pricepk));
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,bet,pricepk));
            mtx.vout.push_back(CTxOut(txfee+odds,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodePricesOpRet('B',pricesid)));
        } else fprintf(stderr,"cant find enough normal inputs for %.8f, plan funding %.8f\n",(double)bet/COIN,(double)funding/COIN);
    }
    if ( entropyval == 0 && funding != 0 )
        CCerror = "cant find price entropy inputs";
    else CCerror = "cant find price input";
    fprintf(stderr,"%s\n", CCerror.c_str() );
    return("");
}

std::string PricesBetFinish(int32_t *resultp,uint64_t txfee,uint256 pricesid,uint256 bettxid)
{
     *resultp = -1;
    CCerror = "couldnt find bettx or entropytx";
    fprintf(stderr,"%s\n", CCerror.c_str() );
    return("");
}
#endif

