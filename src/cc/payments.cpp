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
 0) txidopret <- allocation, scriptPubKey, opret
 1) create <-  locked_blocks, minrelease, list of txidopret
 
 2) fund createtxid amount opretflag to global CC address with opret or txidaddr without
 
 3) release amount -> vout[i] will be scriptPubKeys[i] and (amount * allocations[i]) / sumallocations[] (only using vins that have been locked for locked_blocks+). 
 
 4) info txid -> display parameters, funds
 5) list -> all txids
 
 First step is to create txids with the info needed in their opreturns. this info is the weight, scriptPubKey and opret if needed. To do that txidopret is used:
 
 ./c is a script that invokes komodo-cli with the correct -ac_name
 
 ./c paymentstxidopret \"[9,%222102d6f13a8f745921cdb811e32237bb98950af1a5952be7b3d429abd9152f8e388dac%22]\" -> rawhex with txid 95d9fc8d8a3ef63693c7427e59ff5e177ef63b7345d5f6d6497ac262699a8def
 
 ./c paymentstxidopret \"[1,%2221039433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775ac%22]\" -> rawhex txid 00469695a08b975ceaf7258896abbf1455eb0f383e8a98fc650deace4cbf02a1
 
 now we have 2 txid with the required info in the opreturn. one of them has a 9 and the other a 1 for a 90%/10% split.
 
 ./c paymentscreate \"[0,0,%2295d9fc8d8a3ef63693c7427e59ff5e177ef63b7345d5f6d6497ac262699a8def%22,%2200469695a08b975ceaf7258896abbf1455eb0f383e8a98fc650deace4cbf02a1%22]\" -> created txid 318d827cc6d8f25f40517e7fb0982e3f707b4aa749d322483fc336686a87b28a that will be the createtxid that the other rpc calls will use.
 
 lets see if this appears in the list
 
 ./c paymentslist ->
 {
 "result": "success",
 "createtxids": [
 "318d827cc6d8f25f40517e7fb0982e3f707b4aa749d322483fc336686a87b28a"
 ]
 }
 
 It appeared! now lets get more info on it:
 ./c paymentsinfo \"[%22318d827cc6d8f25f40517e7fb0982e3f707b4aa749d322483fc336686a87b28a%22]\"
 {
 "lockedblocks": 0,
 "totalallocations": 10,
 "minrelease": 0,
 "RWRM36sC8jSctyFZtsu7CyDcHYPdZX7nPZ": 0.00000000,
 "REpyKi7avsVduqZ3eimncK4uKqSArLTGGK": 0.00000000,
 "totalfunds": 0.00000000,
 "result": "success"
 }
 
 There are 2 possible places the funds for this createtxid can be, the first is the special address that is derived from combining the globalCC address with the txidaddr. txidaddr is a non-spendable markeraddress created by converting the txid into a 33 byte pubkey by prefixing 0x02 to the txid. It is a 1of2 address, so it doesnt matter that nobody knows the privkey for this txidaddr. the second address is the global CC address and only utxo to that address with an opreturn containing the createtxid are funds valid for this payments CC createtxid
 
 next let us add some funds to it. the funds can be to either of the two addresses, controlled by useopret (defaults to 0)
 
 ./c paymentsfund \"[%22318d827cc6d8f25f40517e7fb0982e3f707b4aa749d322483fc336686a87b28a%22,1,0]\" -> txid 28f69b925bb7a21d2a3ba2327e85eb2031b014e976e43f5c2c6fb8a76767b221, which indeed sent funds to RWRM36sC8jSctyFZtsu7CyDcHYPdZX7nPZ without an opreturn and it appears on the payments info.
 
 ./c paymentsfund \"[%22318d827cc6d8f25f40517e7fb0982e3f707b4aa749d322483fc336686a87b28a%22,1,1]\" -> txid cc93330b5c951b724b246b3b138d00519c33f2a600a7c938bc9e51aff6e20e32, which indeed sent funds to REpyKi7avsVduqZ3eimncK4uKqSArLTGGK with an opreturn and it appears on the payments info.

 
./c paymentsrelease \"[%22318d827cc6d8f25f40517e7fb0982e3f707b4aa749d322483fc336686a87b28a%22,1.5]\" -> a8d5dbbb8ee94c05e75c4f3c5221091f59dcb86e0e9c4e1e3d2cf69e6fce6b81
 
 it used both fund utxos
 
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

CScript EncodePaymentsOpRet(int32_t lockedblocks,int32_t minrelease,int32_t totalallocations,std::vector<uint256> txidoprets)
{
    CScript opret; uint8_t evalcode = EVAL_PAYMENTS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'C' << lockedblocks << minrelease << totalallocations << txidoprets);
    return(opret);
}

uint8_t DecodePaymentsOpRet(CScript scriptPubKey,int32_t &lockedblocks,int32_t &minrelease,int32_t &totalallocations,std::vector<uint256> &txidoprets)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> lockedblocks; ss >> minrelease; ss >> totalallocations; ss >> txidoprets) != 0 )
    {
        if ( e == EVAL_PAYMENTS && f == 'C' )
            return(f);
    }
    return(0);
}

int64_t IsPaymentsvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v,char *cmpaddr)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && (cmpaddr[0] == 0 || strcmp(destaddr,cmpaddr) == 0) )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool PaymentsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    // one of two addresses
    // change must go to 1of2 txidaddr
    // only 'F' or 1of2 txidaddr can be spent
    // all vouts must match exactly
    return(true);
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddPaymentsInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey txidpk,int64_t total,int32_t maxinputs,uint256 createtxid,int32_t latestheight)
{
    char coinaddr[64]; CPubKey Paymentspk; int64_t nValue,threshold,price,totalinputs = 0; uint256 txid,checktxid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx,tx; int32_t iter,vout,ht,n = 0;
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
            fprintf(stderr,"iter.%d %s/v%d %s\n",iter,txid.GetHex().c_str(),vout,coinaddr);
            if ( vout == 0 && GetTransaction(txid,vintx,hashBlock,false) != 0 )
            {
                if ( latestheight != 0 )
                {
                    if ( (ht= komodo_blockheight(hashBlock)) == 0 )
                    {
                        fprintf(stderr,"null ht\n");
                        continue;
                    }
                    else if ( ht > latestheight )
                    {
                        fprintf(stderr,"ht.%d > lastheight.%d\n",ht,latestheight);
                        continue;
                    }
                }
                if ( iter == 0 )
                {
                    std::vector<uint8_t> scriptPubKey,opret;
                    if ( myGetTransaction(txid,tx,hashBlock) == 0 || tx.vout.size() < 2 || DecodePaymentsFundOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,checktxid) != 'F' || checktxid != createtxid )
                    {
                        fprintf(stderr,"bad opret %s vs %s\n",checktxid.GetHex().c_str(),createtxid.GetHex().c_str());
                        continue;
                    }
                }
                if ( (nValue= IsPaymentsvout(cp,vintx,vout,coinaddr)) > PAYMENTS_TXFEE && nValue >= threshold && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
                {
                    if ( total != 0 && maxinputs != 0 )
                        mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                    nValue = it->second.satoshis;
                    totalinputs += nValue;
                    n++;
                    fprintf(stderr,"iter.%d %s/v%d %s %.8f\n",iter,txid.GetHex().c_str(),vout,coinaddr,(double)nValue/COIN);
                    if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                        break;
                } //else fprintf(stderr,"nValue %.8f vs threshold %.8f\n",(double)nValue/COIN,(double)threshold/COIN);
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
    cJSON *params=0; char *newstr; int32_t i,j;
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
    }
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
    int32_t latestheight,nextheight = komodo_nextheight();
    CMutableTransaction tmpmtx,mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(),nextheight); UniValue result(UniValue::VOBJ); uint256 createtxid,hashBlock;
    CTransaction tx,txO; CPubKey mypk,txidpk,Paymentspk; int32_t i,n,numoprets=0,lockedblocks,minrelease,totalallocations,checkallocations=0,allocation; int64_t inputsum,amount,CCchange=0; CTxOut vout; CScript onlyopret; char txidaddr[64],destaddr[64]; std::vector<uint256> txidoprets; std::string rawtx;
    cJSON *params = payments_reparse(&n,jsonstr);
    mypk = pubkey2pk(Mypubkey());
    Paymentspk = GetUnspendable(cp,0);
    if ( params != 0 && n == 2 )
    {
        createtxid = payments_juint256(jitem(params,0));
        amount = jdouble(jitem(params,1),0) * SATOSHIDEN + 0.0000000049;
        if ( myGetTransaction(createtxid,tx,hashBlock) != 0 )
        {
            if ( tx.vout.size() > 0 && DecodePaymentsOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,lockedblocks,minrelease,totalallocations,txidoprets) != 0 )
            {
                if ( lockedblocks < 0 || minrelease < 0 || totalallocations <= 0 || txidoprets.size() < 2 )
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","negative parameter"));
                    return(result);
                }
                latestheight = (nextheight - lockedblocks - 1);
                if ( amount < minrelease*COIN )
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","amount too smal"));
                    result.push_back(Pair("amount",ValueFromAmount(amount)));
                    result.push_back(Pair("minrelease",ValueFromAmount(minrelease*COIN)));
                    return(result);
                }
                txidpk = CCtxidaddr(txidaddr,createtxid);
                mtx.vout.push_back(MakeCC1of2vout(EVAL_PAYMENTS,0,Paymentspk,txidpk));
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
                    if ( params != 0 )
                        free_json(params);
                    return(result);
                }
                else if ( checkallocations != totalallocations )
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","totalallocations mismatch"));
                    result.push_back(Pair("checkallocations",(int64_t)checkallocations));
                    result.push_back(Pair("totalallocations",(int64_t)totalallocations));
                    if ( params != 0 )
                        free_json(params);
                    return(result);
                }
                else if ( numoprets > 1 )
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","too many oprets"));
                    result.push_back(Pair("numoprets",(int64_t)numoprets));
                    if ( params != 0 )
                        free_json(params);
                    return(result);
                }
                for (i=0; i<txidoprets.size(); i++)
                {
                    mtx.vout[i+1].nValue *= amount;
                    mtx.vout[i+1].nValue /= totalallocations;
                }
                if ( (inputsum= AddPaymentsInputs(cp,mtx,txidpk,amount+PAYMENTS_TXFEE,60,createtxid,latestheight)) >= amount )
                {
                    if ( (CCchange= (inputsum - amount)) >= PAYMENTS_TXFEE )
                        mtx.vout[0].nValue = CCchange;
                    mtx.vout.push_back(CTxOut(0,CScript() << ParseHex(HexStr(txidpk)) << OP_CHECKSIG));
                    GetCCaddress1of2(cp,destaddr,Paymentspk,txidpk);
                    CCaddr1of2set(cp,Paymentspk,txidpk,cp->CCpriv,destaddr);
                    rawtx = FinalizeCCTx(0,cp,mtx,mypk,PAYMENTS_TXFEE,onlyopret);
                    if ( params != 0 )
                        free_json(params);
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
    if ( params != 0 )
        free_json(params);
    return(result);
}

UniValue PaymentsFund(struct CCcontract_info *cp,char *jsonstr)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); UniValue result(UniValue::VOBJ);
    CPubKey Paymentspk,mypk,txidpk; uint256 txid,hashBlock; int64_t amount; CScript opret; CTransaction tx; char txidaddr[64]; std::string rawtx; int32_t n,useopret = 0,lockedblocks,minrelease,totalallocations; std::vector<uint256> txidoprets;
    cJSON *params = payments_reparse(&n,jsonstr);
    mypk = pubkey2pk(Mypubkey());
    Paymentspk = GetUnspendable(cp,0);
    if ( params != 0 && n > 1 && n <= 3 )
    {
        txid = payments_juint256(jitem(params,0));
        amount = jdouble(jitem(params,1),0) * SATOSHIDEN + 0.0000000049;
        if ( n == 3 )
            useopret = jint(jitem(params,2),0) != 0;
        if ( myGetTransaction(txid,tx,hashBlock) == 0 || tx.vout.size() == 1 || DecodePaymentsOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,lockedblocks,minrelease,totalallocations,txidoprets) == 0 )
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","invalid createtxid"));
        }
        else if ( AddNormalinputs(mtx,mypk,amount+PAYMENTS_TXFEE,60) > 0 )
        {
            if ( lockedblocks < 0 || minrelease < 0 || totalallocations <= 0 || txidoprets.size() < 2 )
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","negative parameter"));
                if ( params != 0 )
                    free_json(params);
                return(result);
            }
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
            if ( params != 0 )
                free_json(params);
            return(payments_rawtxresult(result,rawtx,1));
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
    if ( params != 0 )
        free_json(params);
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
            if ( params != 0 )
                free_json(params);
            return(payments_rawtxresult(result,rawtx,1));
        }
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","invalid params or cant find txfee"));
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","parameters error"));
        result.push_back(Pair("n",(int64_t)n));
        fprintf(stderr,"(%s) %p\n",jsonstr,params);
    }
    if ( params != 0 )
        free_json(params);
    return(result);
}

UniValue PaymentsCreate(struct CCcontract_info *cp,char *jsonstr)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); CTransaction tx; CPubKey Paymentspk,mypk; char markeraddr[64]; std::vector<uint256> txidoprets; uint256 hashBlock; int32_t i,n,numoprets=0,lockedblocks,minrelease,totalallocations=0; std::string rawtx;
    cJSON *params = payments_reparse(&n,jsonstr);
    if ( params != 0 && n >= 4 )
    {
        lockedblocks = juint(jitem(params,0),0);
        minrelease = juint(jitem(params,1),0);
        if ( lockedblocks < 0 || minrelease < 0 )
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","negative parameter"));
            if ( params != 0 )
                free_json(params);
            return(result);
        }
        for (i=0; i<n-2; i++)
            txidoprets.push_back(payments_juint256(jitem(params,2+i)));
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
                if ( params != 0 )
                    free_json(params);
                return(result);
            }
        }
        if ( numoprets > 1 )
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","too many opreturns"));
            result.push_back(Pair("numoprets",(int64_t)numoprets));
            if ( params != 0 )
                free_json(params);
            return(result);
        }
        mypk = pubkey2pk(Mypubkey());
        Paymentspk = GetUnspendable(cp,0);
        if ( AddNormalinputs(mtx,mypk,2*PAYMENTS_TXFEE,60) > 0 )
        {
            mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,PAYMENTS_TXFEE,Paymentspk,Paymentspk));
            rawtx = FinalizeCCTx(0,cp,mtx,mypk,PAYMENTS_TXFEE,EncodePaymentsOpRet(lockedblocks,minrelease,totalallocations,txidoprets));
            if ( params != 0 )
                free_json(params);
            return(payments_rawtxresult(result,rawtx,1));
        }
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","not enough normal funds"));
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","parameters error"));
    }
    if ( params != 0 )
        free_json(params);
    return(result);
}

UniValue PaymentsInfo(struct CCcontract_info *cp,char *jsonstr)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); CTransaction tx,txO; CPubKey Paymentspk,txidpk; int32_t i,j,n,flag=0,allocation,numoprets=0,lockedblocks,minrelease,totalallocations; std::vector<uint256> txidoprets; int64_t funds,fundsopret; char fundsaddr[64],fundsopretaddr[64],txidaddr[64],*outstr; uint256 createtxid,hashBlock;
    cJSON *params = payments_reparse(&n,jsonstr);
    if ( params != 0 && n == 1 )
    {
        Paymentspk = GetUnspendable(cp,0);
        createtxid = payments_juint256(jitem(params,0));
        if ( myGetTransaction(createtxid,tx,hashBlock) != 0 )
        {
            if ( tx.vout.size() > 0 && DecodePaymentsOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,lockedblocks,minrelease,totalallocations,txidoprets) != 0 )
            {
                if ( lockedblocks < 0 || minrelease < 0 || totalallocations <= 0 || txidoprets.size() < 2 )
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","negative parameter"));
                    if ( params != 0 )
                        free_json(params);
                    return(result);
                }
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
    if ( params != 0 )
        free_json(params);
    return(result);
}

UniValue PaymentsList(struct CCcontract_info *cp,char *jsonstr)
{
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; uint256 txid,hashBlock;
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); char markeraddr[64],str[65]; CPubKey Paymentspk; CTransaction tx; int32_t lockedblocks,minrelease,totalallocations; std::vector<uint256> txidoprets;
    Paymentspk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,markeraddr,Paymentspk,Paymentspk);
    SetCCtxids(addressIndex,markeraddr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( it->first.index == 0 && myGetTransaction(txid,tx,hashBlock) != 0 )
        {
            if ( tx.vout.size() > 0 && DecodePaymentsOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,lockedblocks,minrelease,totalallocations,txidoprets) == 'C' )
            {
                if ( lockedblocks < 0 || minrelease < 0 || totalallocations <= 0 || txidoprets.size() < 2 )
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","negative parameter"));
                    return(result);
                }
                a.push_back(uint256_str(str,txid));
            }
        }
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("createtxids",a));
    return(result);
}
