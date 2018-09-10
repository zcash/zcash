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

/*
 UniValue PriceInfo(uint256 origtxid)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR),obj(UniValue::VOBJ);
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    CTransaction regtx,tx; std::string name,description,format; uint256 hashBlock,txid,oracletxid,batontxid; CPubKey markerpubkey,pk; struct CCcontract_info *cp,C; uint8_t buf33[33]; int64_t datafee,funding; char str[67],markeraddr[64],numstr[64],batonaddr[64]; std::vector <uint8_t> data;
    cp = CCinit(&C,EVAL_ORACLES);
    buf33[0] = 0x02;
    endiancpy(&buf33[1],(uint8_t *)&origtxid,32);
    markerpubkey = buf2pk(buf33);
    Getscriptaddress(markeraddr,CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG);
    if ( GetTransaction(origtxid,tx,hashBlock,false) != 0 )
    {
        if ( tx.vout.size() > 0 && DecodeOraclesCreateOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,name,description,format) == 'C' )
        {
            result.push_back(Pair("result","success"));
            result.push_back(Pair("txid",uint256_str(str,origtxid)));
            result.push_back(Pair("name",name));
            result.push_back(Pair("description",description));
            result.push_back(Pair("format",format));
            result.push_back(Pair("marker",markeraddr));
            SetCCunspents(unspentOutputs,markeraddr);
            for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
            {
                txid = it->first.txhash;
                if ( GetTransaction(txid,regtx,hashBlock,false) != 0 )
                {
                    if ( regtx.vout.size() > 0 && DecodeOraclesOpRet(regtx.vout[regtx.vout.size()-1].scriptPubKey,oracletxid,pk,datafee) == 'R' && oracletxid == origtxid )
                    {
                        obj.push_back(Pair("provider",pubkey33_str(str,(uint8_t *)pk.begin())));
                        Getscriptaddress(batonaddr,regtx.vout[1].scriptPubKey);
                        batontxid = OracleBatonUtxo(10000,cp,oracletxid,batonaddr,pk,data);
                        obj.push_back(Pair("baton",batonaddr));
                        obj.push_back(Pair("batontxid",uint256_str(str,batontxid)));
                        funding = LifetimeOraclesFunds(cp,oracletxid,pk);
                        sprintf(numstr,"%.8f",(double)funding/COIN);
                        obj.push_back(Pair("lifetime",numstr));
                        funding = AddOracleInputs(cp,mtx,pk,0,0);
                        sprintf(numstr,"%.8f",(double)funding/COIN);
                        obj.push_back(Pair("funds",numstr));
                        sprintf(numstr,"%.8f",(double)datafee/COIN);
                        obj.push_back(Pair("datafee",numstr));
                        a.push_back(obj);
                    }
                }
            }
            result.push_back(Pair("registered",a));
        }
    }
    return(result);
}

UniValue PricesList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; struct CCcontract_info *cp,C; uint256 txid,hashBlock; CTransaction createtx; std::string name,description,format; char str[65];
    cp = CCinit(&C,EVAL_ORACLES);
    SetCCtxids(addressIndex,cp->normaladdr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,createtx,hashBlock,false) != 0 )
        {
            if ( createtx.vout.size() > 0 && DecodeOraclesCreateOpRet(createtx.vout[createtx.vout.size()-1].scriptPubKey,name,description,format) == 'C' )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}
*/
