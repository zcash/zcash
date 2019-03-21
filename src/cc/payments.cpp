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

#include "CCPayments.h"

/* 
 0) create <- update_allowed flag, locked_blocks, minrelease, list of scriptPubKeys, allocations
 
 1) lock amount <create txid> to global CC address
 
 2) release amount -> vout[i] will be scriptPubKeys[i] and (amount * allocations[i]) / sumallocations[] (only using vins that have been locked for locked_blocks+). will make a tx with less than amount if it can find enough vins for minrelease amount
 
 3) update (vins from all scriptPubkeys) new update_allowed flag, locked_blocks, minrelease, list of scriptPubKeys, allocations (only if update_allowed)
 
 4) info txid -> display parameters, funds
 5) list -> all txids
*/

// start of consensus code

CScript EncodePaymentsTxidOpRet(int32_t allocation,std::vector<uint8_t> scriptPubKey,std::vector<uint8_t> destopret)
{
    CScript opret; uint8_t evalcode = EVAL_PAYMENTS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'T' << allocation << scriptPubKey << destopret);
    return(opret);
}

uint8_t DecodePaymentsTxidOpRet(CScript scriptPubKey,int32_t &allocation,std::vector<uint8_t> &destscriptPubKey,std::vector<uint8_t> &destopret)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> allocation; ss >> destscriptPubKey; ss >> destopret) != 0 )
    {
        if ( e == EVAL_PAYMENTS && f == 'T' )
            return(f);
    }
    return(0);
}

CScript EncodePaymentsFundOpRet(uint256 checktxid)
{
    CScript opret; uint8_t evalcode = EVAL_PAYMENTS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'F' << checktxid);
    return(opret);
}

uint8_t DecodePaymentsFundOpRet(CScript scriptPubKey,uint256 &checktxid)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> checktxid) != 0 )
    {
        if ( e == EVAL_PAYMENTS && f == 'F' )
            return(f);
    }
    return(0);
}

CScript EncodePaymentsOpRet(int32_t updateflag,int32_t lockedblocks,int32_t minrelease,int32_t totalallocations,std::vector<uint256> txidoprets)
{
    CScript opret; uint8_t evalcode = EVAL_PAYMENTS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'C' << updateflag << lockedblocks << minrelease << totalallocations << txidoprets);
    return(opret);
}

uint8_t DecodePaymentsOpRet(CScript scriptPubKey,int32_t &updateflag,int32_t &lockedblocks,int32_t &minrelease,int32_t &totalallocations,std::vector<uint256> &txidoprets)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> updateflag; ss >> lockedblocks; ss >> minrelease; ss >> totalallocations; ss >> txidoprets) != 0 )
    {
        if ( e == EVAL_PAYMENTS && f == 'C' )
            return(f);
    }
    return(0);
}

int64_t IsPaymentsvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool PaymentsExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
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
                    return eval->Invalid("cant Payments from mempool");
                if ( (assetoshis= IsPaymentsvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsPaymentsvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool PaymentsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    return(true);
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddPaymentsInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey txidpk,int64_t total,int32_t maxinputs,uint256 createtxid)
{
    char coinaddr[64]; CPubKey Paymentspk; int64_t nValue,threshold,price,totalinputs = 0; uint256 txid,checktxid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx,tx; int32_t iter,vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    if ( maxinputs > CC_MAXVINS )
        maxinputs = CC_MAXVINS;
    if ( maxinputs > 0 )
        threshold = total/maxinputs;
    else threshold = total;
    Paymentspk = GetUnspendable(cp,0);
    for (iter=0; iter<2; iter++)
    {
        if ( iter == 0 )
            GetCCaddress(cp,coinaddr,Paymentspk);
        else GetCCaddress1of2(cp,coinaddr,Paymentspk,txidpk);
        SetCCunspents(unspentOutputs,coinaddr);
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
        {
            txid = it->first.txhash;
            vout = (int32_t)it->first.index;
            if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
            {
                if ( iter == 0 )
                {
                    std::vector<uint8_t> scriptPubKey,opret;
                    if ( myGetTransaction(txid,tx,hashBlock) == 0 || tx.vout.size() < 2 || DecodePaymentsFundOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,checktxid) != 'F' || checktxid != createtxid )
                        continue;
                }
                if ( (nValue= IsPaymentsvout(cp,vintx,vout)) > PAYMENTS_TXFEE && nValue >= threshold && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
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
    }
    return(totalinputs);
}

UniValue payments_rawtxresult(UniValue &result,std::string rawtx,int32_t broadcastflag)
{
    CTransaction tx;
    if ( rawtx.size() > 0 )
    {
        result.push_back(Pair("hex",rawtx));
        if ( DecodeHexTx(tx,rawtx) != 0 )
        {
            if ( broadcastflag != 0 && myAddtomempool(tx) != 0 )
                RelayTransaction(tx);
            result.push_back(Pair("txid",tx.GetHash().ToString()));
            result.push_back(Pair("result","success"));
        } else result.push_back(Pair("error","decode hex"));
    } else result.push_back(Pair("error","couldnt finalize payments CCtx"));
    return(result);
}

cJSON *payments_reparse(int32_t *nump,char *jsonstr)
{
    cJSON *params; char *newstr; int32_t i,j;
    *nump = 0;
    if ( jsonstr != 0 )
    {
        if ( jsonstr[0] == '"' && jsonstr[strlen(jsonstr)-1] == '"' )
        {
            jsonstr[strlen(jsonstr)-1] = 0;
            jsonstr++;
        }
        newstr = (char *)malloc(strlen(jsonstr)+1);
        for (i=j=0; jsonstr[i]!=0; i++)
        {
            if ( jsonstr[i] == '%' && jsonstr[i+1] == '2' && jsonstr[i+2] == '2' )
            {
                newstr[j++] = '"';
                i += 2;
            }
            else if ( jsonstr[i] == '\'' )
                newstr[j++] = '"';
            else newstr[j++] = jsonstr[i];
        }
        newstr[j] = 0;
        params = cJSON_Parse(newstr);
        if ( 0 && params != 0 )
            printf("new.(%s) -> %s\n",newstr,jprint(params,0));
        free(newstr);
        *nump = cJSON_GetArraySize(params);
    } else params = 0;
    return(params);
}

uint256 payments_juint256(cJSON *obj)
{
    uint256 tmp; bits256 t = jbits256(obj,0);
    memcpy(&tmp,&t,sizeof(tmp));
    return(revuint256(tmp));
}

int32_t payments_parsehexdata(std::vector<uint8_t> &hexdata,cJSON *item,int32_t len)
{
    char *hexstr; int32_t val;
    if ( (hexstr= jstr(item,0)) != 0 && ((val= is_hexstr(hexstr,0)) == len*2 || (val > 0 && len == 0)) )
    {
        val >>= 1;
        hexdata.resize(val);
        decode_hex(&hexdata[0],val,hexstr);
        return(0);
    } else return(-1);
}

UniValue PaymentsRelease(struct CCcontract_info *cp,char *jsonstr)
{
    CMutableTransaction tmpmtx,mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); UniValue result(UniValue::VOBJ); uint256 createtxid,hashBlock;
    CTransaction tx,txO; CPubKey mypk,txidpk,Paymentspk; int32_t i,n,numoprets=0,updateflag,lockedblocks,minrelease,totalallocations,checkallocations=0,allocation; int64_t inputsum,amount,CCchange=0; CTxOut vout; CScript onlyopret; char txidaddr[64]; std::vector<uint256> txidoprets; std::string rawtx;
    cJSON *params = payments_reparse(&n,jsonstr);
    mypk = pubkey2pk(Mypubkey());
    Paymentspk = GetUnspendable(cp,0);
    if ( params != 0 && n == 2 )
    {
        createtxid = payments_juint256(jitem(params,0));
        amount = jdouble(jitem(params,1),0) * SATOSHIDEN + 0.0000000049;
        if ( myGetTransaction(createtxid,tx,hashBlock) != 0 )
        {
            if ( tx.vout.size() > 0 && DecodePaymentsOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,updateflag,lockedblocks,minrelease,totalallocations,txidoprets) != 0 )
            {
                for (i=0; i<txidoprets.size(); i++)
                {
                    std::vector<uint8_t> scriptPubKey,opret;
                    vout.nValue = 0;
                    if ( myGetTransaction(txidoprets[i],txO,hashBlock) != 0 && txO.vout.size() > 1 && DecodePaymentsTxidOpRet(txO.vout[txO.vout.size()-1].scriptPubKey,allocation,scriptPubKey,opret) == 'T' )
                    {
                        vout.nValue = allocation;
                        vout.scriptPubKey.resize(scriptPubKey.size());
                        memcpy(&vout.scriptPubKey[0],&scriptPubKey[0],scriptPubKey.size());
                        checkallocations += allocation;
                        if ( opret.size() > 0 )
                        {
                            scriptPubKey.resize(opret.size());
                            memcpy(&onlyopret[0],&opret[0],opret.size());
                            numoprets++;
                        }
                    } else break;
                    mtx.vout.push_back(vout);
                }
                if ( i != txidoprets.size() )
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","invalid txidoprets[i]"));
                    result.push_back(Pair("txi",(int64_t)i));
                    return(result);
                }
                else if ( checkallocations != totalallocations )
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","totalallocations mismatch"));
                    result.push_back(Pair("checkallocations",(int64_t)checkallocations));
                    result.push_back(Pair("totalallocations",(int64_t)totalallocations));
                    return(result);
                }
                else if ( numoprets > 0 )
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","too many oprets"));
                    result.push_back(Pair("numoprets",(int64_t)numoprets));
                    return(result);
                }
                for (i=0; i<txidoprets.size(); i++)
                {
                    mtx.vout[i].nValue *= amount;
                    mtx.vout[i].nValue /= totalallocations;
                }
                txidpk = CCtxidaddr(txidaddr,createtxid);
                if ( (inputsum= AddPaymentsInputs(cp,mtx,txidpk,amount,60,createtxid)) >= amount )
                {
                    if ( (CCchange= (inputsum - amount)) > PAYMENTS_TXFEE )
                        mtx.vout.push_back(MakeCC1of2vout(EVAL_PAYMENTS,CCchange,Paymentspk,txidpk));
                    rawtx = FinalizeCCTx(0,cp,mtx,mypk,PAYMENTS_TXFEE,onlyopret);
                    return(payments_rawtxresult(result,rawtx,0));
                }
                else
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","couldnt find enough locked funds"));
                }
            }
            else
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","couldnt decode paymentscreate txid opret"));
            }
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","couldnt find paymentscreate txid"));
        }
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","parameters error"));
    }
    return(result);
}

UniValue PaymentsFund(struct CCcontract_info *cp,char *jsonstr)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); UniValue result(UniValue::VOBJ);
    CPubKey Paymentspk,mypk,txidpk; uint256 txid,hashBlock; int64_t amount; CScript opret; CTransaction tx; char txidaddr[64]; std::string rawtx; int32_t n,useopret = 0,updateflag,lockedblocks,minrelease,totalallocations; std::vector<uint256> txidoprets;
    cJSON *params = payments_reparse(&n,jsonstr);
    mypk = pubkey2pk(Mypubkey());
    Paymentspk = GetUnspendable(cp,0);
    if ( params != 0 && n > 1 && n <= 3 )
    {
        txid = payments_juint256(jitem(params,0));
        amount = jdouble(jitem(params,1),0) * SATOSHIDEN + 0.0000000049;
        if ( n == 3 )
            useopret = jint(jitem(params,2),0) != 0;
        if ( myGetTransaction(txid,tx,hashBlock) == 0 || tx.vout.size() == 1 || DecodePaymentsOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,updateflag,lockedblocks,minrelease,totalallocations,txidoprets) == 0 )
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","invalid createtxid"));
        }
        else if ( AddNormalinputs(mtx,mypk,amount+PAYMENTS_TXFEE,60) > 0 )
        {
            if ( useopret == 0 )
            {
                txidpk = CCtxidaddr(txidaddr,txid);
                mtx.vout.push_back(MakeCC1of2vout(EVAL_PAYMENTS,amount,Paymentspk,txidpk));
            }
            else
            {
                mtx.vout.push_back(MakeCC1vout(EVAL_PAYMENTS,amount,Paymentspk));
                opret = EncodePaymentsFundOpRet(txid);
            }
            rawtx = FinalizeCCTx(0,cp,mtx,mypk,PAYMENTS_TXFEE,opret);
            return(payments_rawtxresult(result,rawtx,0));
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","couldnt find enough funds"));
        }
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","parameters error"));
    }
    return(result);
}

UniValue PaymentsTxidopret(struct CCcontract_info *cp,char *jsonstr)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); UniValue result(UniValue::VOBJ); CPubKey mypk; std::string rawtx;
    std::vector<uint8_t> scriptPubKey,opret; int32_t allocation,n,retval0,retval1=0;
    cJSON *params = payments_reparse(&n,jsonstr);
    mypk = pubkey2pk(Mypubkey());
    if ( params != 0 && n > 1 && n <= 3 )
    {
        allocation = juint(jitem(params,0),0);
        retval0 = payments_parsehexdata(scriptPubKey,jitem(params,1),0);
        if ( n == 3 )
            retval1 = payments_parsehexdata(opret,jitem(params,2),0);
        if ( allocation > 0 && retval0 == 0 && retval1 == 0 && AddNormalinputs(mtx,mypk,PAYMENTS_TXFEE,10) > 0 )
        {
            rawtx = FinalizeCCTx(0,cp,mtx,mypk,PAYMENTS_TXFEE,EncodePaymentsTxidOpRet(allocation,scriptPubKey,opret));
            return(payments_rawtxresult(result,rawtx,0));
        }
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","invalid params or cant find txfee"));
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","parameters error"));
    }
    return(result);
}

UniValue PaymentsCreate(struct CCcontract_info *cp,char *jsonstr)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); CTransaction tx; CPubKey Paymentspk,mypk; char markeraddr[64]; std::vector<uint256> txidoprets; uint256 hashBlock; int32_t i,n,numoprets=0,updateflag,lockedblocks,minrelease,totalallocations=0; std::string rawtx;
    cJSON *params = payments_reparse(&n,jsonstr);
    if ( params != 0 && n >= 4 )
    {
        updateflag = juint(jitem(params,0),0);
        lockedblocks = juint(jitem(params,1),0);
        minrelease = juint(jitem(params,2),0);
        for (i=0; i<n-3; i++)
            txidoprets.push_back(payments_juint256(jitem(params,3+i)));
        for (i=0; i<txidoprets.size(); i++)
        {
            std::vector<uint8_t> scriptPubKey,opret; int32_t allocation;
            if ( myGetTransaction(txidoprets[i],tx,hashBlock) != 0 && tx.vout.size() > 1 && DecodePaymentsTxidOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,allocation,scriptPubKey,opret) == 'T' )
            {
                totalallocations += allocation;
                if ( opret.size() > 0 )
                    numoprets++;
            }
            else
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","invalid txidopret"));
                result.push_back(Pair("txid",txidoprets[i].GetHex()));
                result.push_back(Pair("txi",(int64_t)i));
                return(result);
            }
        }
        if ( numoprets > 1 )
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","too many opreturns"));
            result.push_back(Pair("numoprets",(int64_t)numoprets));
            return(result);
        }
        mypk = pubkey2pk(Mypubkey());
        Paymentspk = GetUnspendable(cp,0);
        if ( AddNormalinputs(mtx,mypk,2*PAYMENTS_TXFEE,60) > 0 )
        {
            mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,PAYMENTS_TXFEE,Paymentspk,Paymentspk));
            rawtx = FinalizeCCTx(0,cp,mtx,mypk,PAYMENTS_TXFEE,EncodePaymentsOpRet(updateflag,lockedblocks,minrelease,totalallocations,txidoprets));
            return(payments_rawtxresult(result,rawtx,0));
        }
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","not enough normal funds"));
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","parameters error"));
    }
    return(result);
}

UniValue PaymentsInfo(struct CCcontract_info *cp,char *jsonstr)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); CTransaction tx,txO; CPubKey Paymentspk,txidpk; int32_t i,j,n,flag=0,allocation,numoprets=0,updateflag,lockedblocks,minrelease,totalallocations; std::vector<uint256> txidoprets; int64_t funds,fundsopret; char fundsaddr[64],fundsopretaddr[64],txidaddr[64],*outstr; uint256 createtxid,hashBlock;
    cJSON *params = payments_reparse(&n,jsonstr);
    if ( params != 0 && n == 1 )
    {
        Paymentspk = GetUnspendable(cp,0);
        createtxid = payments_juint256(jitem(params,0));
        if ( myGetTransaction(createtxid,tx,hashBlock) != 0 )
        {
            if ( tx.vout.size() > 0 && DecodePaymentsOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,updateflag,lockedblocks,minrelease,totalallocations,txidoprets) != 0 )
            {
                result.push_back(Pair("updateable",updateflag!=0?"yes":"no"));
                result.push_back(Pair("lockedblocks",(int64_t)lockedblocks));
                result.push_back(Pair("totalallocations",(int64_t)totalallocations));
                result.push_back(Pair("minrelease",(int64_t)minrelease));
                for (i=0; i<txidoprets.size(); i++)
                {
                    UniValue obj(UniValue::VOBJ); std::vector<uint8_t> scriptPubKey,opret;
                    obj.push_back(Pair("txidopret",txidoprets[i].GetHex()));
                    if ( myGetTransaction(txidoprets[i],txO,hashBlock) != 0 && txO.vout.size() > 1 && DecodePaymentsTxidOpRet(txO.vout[txO.vout.size()-1].scriptPubKey,allocation,scriptPubKey,opret) == 'T' )
                    {
                        outstr = (char *)malloc(scriptPubKey.size() + opret.size() + 1);
                        for (j=0; j<scriptPubKey.size(); j++)
                            outstr[j] = scriptPubKey[j];
                        outstr[j] = 0;
                        obj.push_back(Pair("scriptPubKey",outstr));
                        if ( opret.size() != 0 )
                        {
                            for (j=0; j<opret.size(); j++)
                                outstr[j] = opret[j];
                            outstr[j] = 0;
                            obj.push_back(Pair("opreturn",outstr));
                            numoprets++;
                        }
                        free(outstr);
                    }
                }
                flag++;
                if ( numoprets > 1 )
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","too many opreturns"));
                    result.push_back(Pair("numoprets",(int64_t)numoprets));
                }
                else
                {
                    txidpk = CCtxidaddr(txidaddr,createtxid);
                    GetCCaddress1of2(cp,fundsaddr,Paymentspk,txidpk);
                    funds = CCaddress_balance(fundsaddr);
                    result.push_back(Pair(fundsaddr,ValueFromAmount(funds)));
                    GetCCaddress(cp,fundsopretaddr,Paymentspk);
                    fundsopret = CCaddress_balance(fundsopretaddr);
                    result.push_back(Pair(fundsopretaddr,ValueFromAmount(fundsopret)));
                    result.push_back(Pair("totalfunds",ValueFromAmount(funds+fundsopret)));
                    result.push_back(Pair("result","success"));
                }
            }
        }
        if ( flag == 0 )
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","couldnt find valid payments create txid"));
        }
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","parameters error"));
    }
    return(result);
}

UniValue PaymentsList(struct CCcontract_info *cp,char *jsonstr)
{
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; uint256 txid,hashBlock;
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); char markeraddr[64],str[65]; CPubKey Paymentspk; CTransaction tx; int32_t updateflag,lockedblocks,minrelease,totalallocations; std::vector<uint256> txidoprets;
    result.push_back(Pair("result","success"));
    Paymentspk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,markeraddr,Paymentspk,Paymentspk);
    SetCCtxids(addressIndex,markeraddr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( it->first.index == 0 && myGetTransaction(txid,tx,hashBlock) != 0 )
        {
            if ( tx.vout.size() > 0 && DecodePaymentsOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,updateflag,lockedblocks,minrelease,totalallocations,txidoprets) == 'C' )
            {
                a.push_back(uint256_str(str,txid));
            }
        }
    }
    result.push_back(Pair("createtxids",a));
    return(result);
}
