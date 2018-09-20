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
 
 PricesFunding oracletxid, margin, priceaveraging, maxleverage, funding, longtoken, shorttoken, N [pubkeys]
 
 PricesBet -> oracletxid start with 'L', leverage, funding, direction
    funds are locked into global CC address
    it can be closed at anytime by the trader for cash settlement
    the house account can close it if rekt
 
 Implementation Notes:
  In order to eliminate the need for worrying about sybil attacks, each prices plan would be able to specific pubkey(s?) for whitelisted publishers. It would be possible to have a non-whitelisted plan that would use 50% correlation between publishers. 
 
 delta neutral balancing of riskexposure: fabs(long exposure - short exposure)
 bet +B at leverage L
 absval(sum(+BLi) - sum(-Bli))
 
 validate: update riskexposure and it needs to be <= funds
 
 PricesProfits: limit withdraw to funds in excess of riskexposure
 PricesFinish: payout (if winning) and update riskexposure
 need long/short exposure assets
 
 funding -> 1of2 CC global CC address and dealer address, exposure tokens to global 1of2 assets CC address
 pricebet -> user funds and exposure token to 1of2 address.
 pricewin -> winnings from dealer funds, exposure token back to global address
 priceloss -> exposuretoken back to global address
 
 exposure address, funds address
 
*/

// start of consensus code

int64_t PricesOraclePrice(int64_t &rektprice,uint64_t mode,uint256 oracletxid,std::vector<CPubKey>pubkeys,int32_t dir,int64_t amount,int32_t leverage)
{
    int64_t price;
    // howto ensure price when block it confirms it not known
    // get price from oracle + current chaintip
    // normalize leveraged amount
    if ( dir > 0 )
        rektprice = price * leverage / (leverage-1);
    else rektprice = price * (leverage-1) / leverage;
    return(price);
}

CScript EncodePricesFundingOpRet(uint8_t funcid,CPubKey planpk,uint256 oracletxid,uint256 longtoken,uint256 shorttoken,int32_t millimargin,uint64_t mode,int32_t maxleverage,std::vector<CPubKey> pubkeys,uint256 bettoken)
{
    CScript opret;
    fprintf(stderr,"implement EncodePricesFundingOpRet\n");
    return(opret);
}

uint8_t DecodePricesFundingOpRet(CScript scriptPubKey,CPubKey &planpk,uint256 &oracletxid,uint256 &longtoken,uint256 &shorttoken,int32_t &millimargin,uint64_t &mode,int32_t &maxleverage,std::vector<CPubKey> &pubkeys,uint256 &bettoken)
{
    fprintf(stderr,"implement DecodePricesFundingOpRet\n");
    return(0);
}

bool PricesValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)
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
        //if ( PricesExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Pricesget invalid amount\n");
            return false;
        }
        //else
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

int64_t AddTokensInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,char *destaddr,uint256 tolenid,int64_t total,int32_t maxinputs)
{
    int64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs,destaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        // need to prevent dup
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 && vout < vintx.vout.size() )
        {
            // need to verify assetid
            if ( (nValue= vintx.vout[vout].nValue) > 10000 && myIsutxo_spentinmempool(txid,vout) == 0 )
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

UniValue PricesList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; struct CCcontract_info *cp,C; uint64_t mode; int32_t margin,maxleverage; std::vector<CPubKey>pubkeys; uint256 txid,hashBlock,oracletxid,longtoken,shorttoken,bettoken; CPubKey planpk,pricespk; char str[65]; CTransaction vintx;
    cp = CCinit(&C,EVAL_PRICES);
    pricespk = GetUnspendable(cp,0);
    SetCCtxids(addressIndex,cp->normaladdr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( vintx.vout.size() > 0 && DecodePricesFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,planpk,oracletxid,longtoken,shorttoken,margin,mode,maxleverage,pubkeys,bettoken) == 'F' )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}

// longtoken satoshis limits long exposure
// shorttoken satoshis limits short exposure
// both must be in the 1of2 CC address with its total supply
// bettoken
std::string PricesCreateFunding(uint64_t txfee,uint256 bettoken,uint256 oracletxid,uint64_t margin,uint64_t mode,uint256 longtoken,uint256 shorttoken,int32_t maxleverage,int64_t funding,std::vector<CPubKey> pubkeys)
{
    CMutableTransaction mtx; CTransaction oracletx; int64_t fullsupply,inputs,CCchange=0; uint256 hashBlock; char str[65],coinaddr[64],houseaddr[64]; CPubKey mypk,pricespk; int32_t i,N,numvouts; struct CCcontract_info *cp,C,*assetscp,C2;
    if ( funding < 100*COIN || maxleverage <= 0 || maxleverage > 10000 )
    {
        CCerror = "invalid parameter error";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    cp = CCinit(&C,EVAL_PRICES);
    assetscp = CCinit(&C2,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    if ( (N= (int32_t)pubkeys.size()) || N > 15 )
    {
        fprintf(stderr,"too many pubkeys N.%d\n",N);
        return("");
    }
    for (i=0; i<N; i++)
    {
        Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(pubkeys[i])) << OP_CHECKSIG);
        if ( CCaddress_balance(coinaddr) == 0 )
        {
            fprintf(stderr,"N.%d but pubkeys[%d] has no balance\n",N,i);
            return("");
        }
    }
    if ( GetCCaddress1of2(cp,houseaddr,pricespk,mypk) == 0 )
    {
        fprintf(stderr,"PricesCreateFunding cant create globaladdr\n");
        return("");
    }
    if ( CCtoken_balance(houseaddr,longtoken) != CCfullsupply(longtoken) )
    {
        fprintf(stderr,"PricesCreateFunding (%s) globaladdr.%s token balance %.8f != %.8f\n",uint256_str(str,longtoken),houseaddr,(double)CCtoken_balance(houseaddr,longtoken)/COIN,(double)CCfullsupply(longtoken)/COIN);
        return("");
    }
    if ( CCtoken_balance(houseaddr,shorttoken) != CCfullsupply(shorttoken) )
    {
        fprintf(stderr,"PricesCreateFunding (%s) globaladdr.%s token balance %.8f != %.8f\n",uint256_str(str,longtoken),houseaddr,(double)CCtoken_balance(houseaddr,longtoken)/COIN,(double)CCfullsupply(shorttoken)/COIN);
        return("");
    }
    if ( GetTransaction(oracletxid,oracletx,hashBlock,false) == 0 || (numvouts= oracletx.vout.size()) <= 0 )
    {
        fprintf(stderr,"cant find oracletxid %s\n",uint256_str(str,oracletxid));
        return("");
    }
    fprintf(stderr,"error check bettoken\n");
    if ( AddNormalinputs(mtx,mypk,3*txfee,3) > 0 )
    {
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(pricespk)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodePricesFundingOpRet('F',mypk,oracletxid,longtoken,shorttoken,margin,mode,maxleverage,pubkeys,bettoken)));
    }
    else
    {
        CCerror = "cant find enough inputs";
        fprintf(stderr,"%s\n", CCerror.c_str() );
    }
    return("");
}

UniValue PricesInfo(uint256 fundingtxid)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); CPubKey pricespk,planpk; uint256 hashBlock,oracletxid,longtoken,shorttoken,bettoken; CTransaction vintx; int64_t balance,supply,exposure; uint64_t funding,mode; int32_t i,margin,maxleverage; char numstr[65],houseaddr[64],exposureaddr[64],str[65]; std::vector<CPubKey>pubkeys; struct CCcontract_info *cp,C,*assetscp,C2;
    cp = CCinit(&C,EVAL_PRICES);
    assetscp = CCinit(&C2,EVAL_ASSETS);
    pricespk = GetUnspendable(cp,0);
    if ( GetTransaction(fundingtxid,vintx,hashBlock,false) == 0 )
    {
        fprintf(stderr,"cant find fundingtxid\n");
        ERR_RESULT("cant find fundingtxid");
        return(result);
    }
    if ( vintx.vout.size() > 0 && DecodePricesFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,planpk,oracletxid,longtoken,shorttoken,margin,mode,maxleverage,pubkeys,bettoken) == 'F' )
    {
        result.push_back(Pair("result","success"));
        result.push_back(Pair("fundingtxid",uint256_str(str,fundingtxid)));
        result.push_back(Pair("bettoken",uint256_str(str,bettoken)));
        result.push_back(Pair("oracletxid",uint256_str(str,oracletxid)));
        sprintf(numstr,"%.3f",(double)margin/1000);
        result.push_back(Pair("profitmargin",numstr));
        result.push_back(Pair("maxleverage",maxleverage));
        result.push_back(Pair("mode",(int64_t)mode));
        for (i=0; i<pubkeys.size(); i++)
            a.push_back(pubkey33_str(str,(uint8_t *)&pubkeys[i]));
        result.push_back(Pair("pubkeys",a));
        GetCCaddress1of2(assetscp,houseaddr,pricespk,planpk);
        GetCCaddress1of2(assetscp,exposureaddr,pricespk,pricespk); // assets addr
        result.push_back(Pair("houseaddr",houseaddr));
        result.push_back(Pair("betaddr",exposureaddr));
        result.push_back(Pair("longtoken",uint256_str(str,longtoken)));
        supply = CCfullsupply(longtoken);
        result.push_back(Pair("longsupply",supply));
        balance = CCtoken_balance(houseaddr,longtoken);
        result.push_back(Pair("longavail",balance));
        exposure = CCtoken_balance(exposureaddr,longtoken);
        result.push_back(Pair("longexposure",exposure));
        result.push_back(Pair("shorttoken",uint256_str(str,shorttoken)));
        supply = CCfullsupply(shorttoken);
        result.push_back(Pair("shortsupply",supply));
        balance = CCtoken_balance(houseaddr,shorttoken);
        result.push_back(Pair("shortavail",balance));
        exposure = CCtoken_balance(exposureaddr,shorttoken);
        result.push_back(Pair("shortexposure",exposure));
        sprintf(numstr,"%.8f",(double)CCtoken_balance(houseaddr,bettoken)/COIN);
        result.push_back(Pair("funds",numstr));
    }
    return(result);
}

std::string PricesAddFunding(uint64_t txfee,uint256 refbettoken,uint256 fundingtxid,int64_t amount)
{
    CMutableTransaction mtx; struct CCcontract_info *cp,C,*assetscp,C2; CPubKey pricespk,planpk,mypk; uint256 hashBlock,oracletxid,longtoken,shorttoken,bettoken; CTransaction tx; int64_t balance,supply,exposure,inputs,CCchange = 0; uint64_t funding,mode; int32_t margin,maxleverage; char houseaddr[64],myaddr[64]; std::vector<CPubKey>pubkeys;
    if ( amount < 10000 )
    {
        CCerror = "amount must be positive";
        fprintf(stderr,"%s\n", CCerror.c_str() );
        return("");
    }
    cp = CCinit(&C,EVAL_PRICES);
    assetscp = CCinit(&C2,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    GetCCaddress(assetscp,myaddr,mypk);
    if ( GetTransaction(fundingtxid,tx,hashBlock,false) == 0 )
    {
        fprintf(stderr,"cant find fundingtxid\n");
        return("");
    }
    if ( tx.vout.size() > 0 && DecodePricesFundingOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,planpk,oracletxid,longtoken,shorttoken,margin,mode,maxleverage,pubkeys,bettoken) == 'F' && bettoken == refbettoken )
    {
        GetCCaddress1of2(assetscp,houseaddr,pricespk,planpk);
        if ( AddNormalinputs(mtx,mypk,2*txfee,3) > 0 )
        {
            if ( (inputs= AddTokensInputs(assetscp,mtx,myaddr,bettoken,amount,60)) >= amount )
            {
                mtx.vout.push_back(MakeCC1of2vout(assetscp->evalcode,amount,pricespk,planpk));
                mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(planpk)) << OP_CHECKSIG));
                if ( inputs > amount+txfee )
                    CCchange = (inputs - amount);
                mtx.vout.push_back(MakeCC1vout(assetscp->evalcode,CCchange,mypk));
                // add addr2
                return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeAssetOpRet('t',bettoken,zeroid,0,Mypubkey())));
            }
            else
            {
                CCerror = "cant find enough bet inputs";
                fprintf(stderr,"%s\n", CCerror.c_str() );
            }
        }
        else
        {
            CCerror = "cant find enough inputs";
            fprintf(stderr,"%s\n", CCerror.c_str() );
        }
    }
    return("");
}

std::string PricesBet(uint64_t txfee,uint256 refbettoken,uint256 fundingtxid,int64_t amount,int32_t leverage)
{
    CMutableTransaction mtx; struct CCcontract_info *cp,C,*assetscp,C2; CPubKey pricespk,planpk,mypk; uint256 hashBlock,oracletxid,longtoken,shorttoken,tokenid,bettoken; CTransaction tx; int64_t balance,supply,exposure,inputs,inputs2,longexposure,netexposure,shortexposure,CCchange = 0,CCchange2 = 0; uint64_t funding,mode; int32_t dir,margin,maxleverage; char houseaddr[64],myaddr[64],exposureaddr[64]; std::vector<CPubKey>pubkeys;
    if ( amount < 0 )
    {
        amount = -amount;
        dir = -1;
    } else dir = 1;
    cp = CCinit(&C,EVAL_PRICES);
    assetscp = CCinit(&C2,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    GetCCaddress(assetscp,myaddr,mypk);
    if ( GetTransaction(fundingtxid,tx,hashBlock,false) == 0 )
    {
        fprintf(stderr,"cant find fundingtxid\n");
        return("");
    }
    if ( tx.vout.size() > 0 && DecodePricesFundingOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,planpk,oracletxid,longtoken,shorttoken,margin,mode,maxleverage,pubkeys,bettoken) == 'F' && bettoken == refbettoken )
    {
        if ( leverage > maxleverage || leverage < 1 )
        {
            fprintf(stderr,"illegal leverage\n");
            return("");
        }
        GetCCaddress1of2(assetscp,houseaddr,pricespk,planpk);
        GetCCaddress1of2(assetscp,exposureaddr,pricespk,pricespk);
        if ( dir < 0 )
            tokenid = shorttoken;
        else tokenid = longtoken;
        exposure = leverage * amount;
        longexposure = CCtoken_balance(exposureaddr,longtoken);
        shortexposure = CCtoken_balance(exposureaddr,shorttoken);
        netexposure = (longexposure - shortexposure + exposure*dir);
        if ( netexposure < 0 )
            netexposure = -netexposure;
        balance = CCtoken_balance(myaddr,bettoken) / COIN;
        if ( balance < netexposure*9/10 ) // 10% extra room for dynamically closed bets in wrong direction
        {
            fprintf(stderr,"balance %lld < 90%% netexposure %lld, refuse bet\n",(long long)balance,(long long)netexposure);
            return("");
        }
        if ( AddNormalinputs(mtx,mypk,txfee,3) > 0 )
        {
            if ( (inputs= AddTokensInputs(assetscp,mtx,houseaddr,tokenid,exposure,30)) >= exposure )
            {
                if ( (inputs2= AddTokensInputs(assetscp,mtx,myaddr,bettoken,amount,30)) >= amount )
                {
                    mtx.vout.push_back(MakeCC1of2vout(assetscp->evalcode,amount,pricespk,planpk));
                    mtx.vout.push_back(MakeCC1of2vout(assetscp->evalcode,exposure,pricespk,pricespk));
                    if ( inputs > exposure+txfee )
                        CCchange = (inputs - exposure);
                    if ( inputs2 > amount+txfee )
                        CCchange2 = (inputs2 - amount);
                    mtx.vout.push_back(MakeCC1of2vout(assetscp->evalcode,CCchange,pricespk,planpk));
                    mtx.vout.push_back(MakeCC1vout(assetscp->evalcode,CCchange2,mypk));
                    // add addr2 and addr3
                    //return(FinalizeCCTx(0,assetscp,mtx,mypk,txfee,EncodePricesExtra('T',tokenid,bettoken,zeroid,dir*leverage)));
                    CScript opret;
                    return(FinalizeCCTx(0,assetscp,mtx,mypk,txfee,opret));
                }
                else
                {
                    fprintf(stderr,"cant find enough bettoken inputs\n");
                    return("");
                }
            }
            else
            {
                fprintf(stderr,"cant find enough exposure inputs\n");
                return("");
            }
        }
        else
        {
            CCerror = "cant find enough inputsB";
            fprintf(stderr,"%s\n", CCerror.c_str() );
        }
    }
    return("");
}

UniValue PricesStatus(uint64_t txfee,uint256 refbettoken,uint256 fundingtxid,uint256 bettxid)
{
    UniValue result(UniValue::VOBJ);
    // get height of bettxid
    // get price and rekt
    // get current height and price
    // what about if rekt in the past?
    return(result);
}

std::string PricesFinish(uint64_t txfee,uint256 refbettoken,uint256 fundingtxid,uint256 bettxid)
{
    return("");
}



