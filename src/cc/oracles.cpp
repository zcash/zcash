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

#include "CCOracles.h"

/*
 An oracles CC has the purpose of converting offchain data into onchain data
 simplest would just be to have a pubkey(s) that are trusted to provide such data, but this wont need to have a CC involved at all and can just be done by convention
 
 That begs the question, "what would an oracles CC do?"
 A couple of things come to mind, ie. payments to oracles for future offchain data and maybe some sort of dispute/censoring ability
 
 first step is to define the data that the oracle is providing. A simple name:description tx can be created to define the name and description of the oracle data.
 linked to this txid would be two types of transactions:
    a) oracle providers
    b) oracle data users
 
 In order to be resistant to sybil attacks, the feedback mechanism needs to have a cost. combining with the idea of payments for data, the oracle providers will be ranked by actual payments made to each oracle for each data type.
 
 Required transactions:
 0) create oracle description -> just needs to create txid for oracle data
 1) register as oracle data provider with price -> become a registered oracle data provider
 2) pay provider for N oracle data points -> lock funds for oracle provider
 3) publish oracle data point -> publish data and collect payment
 
 create:
 vins.*: normal inputs
 vout.0: txfee tag to oracle normal address
 vout.1: opreturn with name and description and format for data
 
 register:
 vins.*: normal inputs
 vout.0: txfee tag to normal marker address
 vout.1: opreturn with createtxid, pubkey and price per data point
 
 subscribe:
 vins.*: normal inputs
 vout.0: subscription fee to publishers CC address
 vout.1: opreturn with createtxid, registered provider's pubkey, amount
 
 data:
 vin.0: normal input
 vin.1+: subscription vout.0
 vout.0: change to publishers CC address
 vout.1: payment for dataprovider
 vout.2: opreturn with data in proper format
 
*/

// start of consensus code


CScript EncodeOraclesCreateOpRet(uint8_t funcid,std::string name,std::string description,std::string format)
{
    CScript opret; uint8_t evalcode = EVAL_ORACLES;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << name << description << format);
    return(opret);
}

uint8_t DecodeOraclesCreateOpRet(const CScript &scriptPubKey,std::string &name,std::string &description,std::string &format)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( script[0] == EVAL_ORACLES )
    {
        if ( script[1] == 'C' )
        {
            if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> name; ss >> description; ss >> format) != 0 )
            {
                return(script[1]);
            } else fprintf(stderr,"DecodeOraclesCreateOpRet unmarshal error for C\n");
        }
    }
    return(0);
}

CScript EncodeOraclesOpRet(uint8_t funcid,uint256 oracletxid,CPubKey pk,int64_t num)
{
    CScript opret; uint8_t evalcode = EVAL_ORACLES;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << oracletxid << pk << num);
    return(opret);
}

uint8_t DecodeOraclesOpRet(const CScript &scriptPubKey,uint256 &oracletxid,CPubKey &pk,int64_t &num)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 1 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> oracletxid; ss >> pk; ss >> num) != 0 )
    {
        if ( e == EVAL_ORACLES && (f == 'R' || f == 'S') )
            return(f);
    }
    return(0);
}

CScript EncodeOraclesData(uint8_t funcid,uint256 oracletxid,CPubKey pk,std::vector <uint8_t>data)
{
    CScript opret; uint8_t evalcode = EVAL_ORACLES;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << oracletxid << pk << data);
    return(opret);
}

uint8_t DecodeOraclesData(const CScript &scriptPubKey,uint256 &oracletxid,CPubKey &pk,std::vector <uint8_t>&data)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 1 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> oracletxid; ss >> pk; ss >> data) != 0 )
    {
        if ( e == EVAL_ORACLES && f == 'D' )
            return(f);
    }
    return(0);
}

int64_t IsOraclesvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool OraclesExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
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
                    return eval->Invalid("cant Oracles from mempool");
                if ( (assetoshis= IsOraclesvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsOraclesvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool OraclesValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)
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
        if ( OraclesExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Oraclesget invalid amount\n");
            return false;
        }
        else
        {
            txid = tx.GetHash();
            memcpy(hash,&txid,sizeof(hash));
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Oraclesget validated\n");
            else fprintf(stderr,"Oraclesget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddOraclesInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs)
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
            if ( (nValue= IsOraclesvout(cp,vintx,vout)) > 1000000 && myIsutxo_spentinmempool(txid,vout) == 0 )
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

int64_t LifetimeOraclesFunds(struct CCcontract_info *cp,uint256 oracletxid,CPubKey regpk)
{
    char coinaddr[64]; CPubKey pk; int64_t total=0,num; uint256 txid,hashBlock,subtxid; CTransaction subtx;
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    GetCCaddress(cp,coinaddr,regpk);
    SetCCtxids(addressIndex,coinaddr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,subtx,hashBlock,false) != 0 )
        {
            if ( subtx.vout.size() > 0 && DecodeOraclesOpRet(subtx.vout[subtx.vout.size()-1].scriptPubKey,subtxid,pk,num) == 'S' && subtxid == oracletxid && regpk == pk )
            {
                total += subtx.vout[0].nValue;
            }
        }
    }
    return(total);
}

int64_t OracleDatafee(uint256 oracletxid,CPubKey pk)
{
    CTransaction oracletx; char markeraddr[64]; CPubKey markerpubkey; uint8_t buf33[33]; uint256 hashBlock; std::string name,description,format; int32_t numvouts; int64_t datafee = 0;
    if ( GetTransaction(oracletxid,oracletx,hashBlock,false) != 0 && (numvouts= oracletx.vout.size()) > 0 )
    {
        if ( DecodeOraclesCreateOpRet(oracletx.vout[numvouts-1].scriptPubKey,name,description,format) == 'C' )
        {
            buf33[0] = 0x02;
            endiancpy(&buf33[1],(uint8_t *)&oracletxid,32);
            markerpubkey = buf2pk(buf33);
            Getscriptaddress(markeraddr,CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG);
  
        }
    }
    return(datafee);
}

std::string OracleCreate(int64_t txfee,std::string name,std::string description,std::string format)
{
    CMutableTransaction mtx; CPubKey mypk,Oraclespk; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_ORACLES);
    if ( name.size() > 32 || description.size() > 4096 || format.size() > 4096 )
    {
        fprintf(stderr,"name.%d or description.%d is too big\n",(int32_t)name.size(),(int32_t)description.size());
        return("");
    }
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    Oraclespk = GetUnspendable(cp,0);
    if ( AddNormalinputs(mtx,mypk,2*txfee,1) > 0 )
    {
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(Oraclespk)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeOraclesCreateOpRet('C',name,description,format)));
    }
    return("");
}

std::string OracleRegister(int64_t txfee,uint256 oracletxid,int64_t datafee)
{
    CMutableTransaction mtx; CPubKey mypk,markerpubkey; struct CCcontract_info *cp,C; uint8_t buf33[33]; char markeraddr[64];
    cp = CCinit(&C,EVAL_ORACLES);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    buf33[0] = 0x02;
    endiancpy(&buf33[1],(uint8_t *)&oracletxid,32);
    markerpubkey = buf2pk(buf33);
    Getscriptaddress(markeraddr,CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG);
    if ( AddNormalinputs(mtx,mypk,2*txfee,1) > 0 )
    {
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeOraclesOpRet('R',oracletxid,mypk,datafee)));
    }
    return("");
}

std::string OracleSubscribe(int64_t txfee,uint256 oracletxid,CPubKey publisher,int64_t amount)
{
    CMutableTransaction mtx; CPubKey mypk,markerpubkey; struct CCcontract_info *cp,C; uint8_t buf33[33]; char markeraddr[64];
    cp = CCinit(&C,EVAL_ORACLES);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    buf33[0] = 0x02;
    endiancpy(&buf33[1],(uint8_t *)&oracletxid,32);
    markerpubkey = buf2pk(buf33);
    Getscriptaddress(markeraddr,CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG);
    if ( AddNormalinputs(mtx,mypk,amount + 2*txfee,1) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,publisher));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeOraclesOpRet('S',oracletxid,mypk,amount)));
    }
    return("");
}

std::string OracleData(int64_t txfee,uint256 oracletxid,std::vector <uint8_t> data)
{
    CMutableTransaction mtx; CPubKey mypk; int64_t datafee,inputs,CCchange = 0; struct CCcontract_info *cp,C; char coinaddr[64];
    cp = CCinit(&C,EVAL_ORACLES);
    mypk = pubkey2pk(Mypubkey());
    if ( data.size() > 8192 )
    {
        fprintf(stderr,"datasize %d is too big\n",(int32_t)data.size());
        return("");
    }
    if ( (datafee= OracleDatafee(oracletxid,mypk)) <= 0 )
    {
        fprintf(stderr,"datafee %.8f is illegal\n",(double)datafee/COIN);
        return("");
    }
    if ( txfee == 0 )
        txfee = 10000;
    GetCCaddress(cp,coinaddr,mypk);
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        if ( (inputs= AddOracleInputs(cp,mtx,mypk,oracletxid,datafee,60)) > 0 )
        {
            if ( inputs > datafee )
                CCchange = (inputs - datafee);
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange,mypk));
            mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeOraclesData('D',oracletxid,mypk,data)));
        }
    }
    return("");
}

UniValue OracleInfo(uint256 origtxid)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR),obj(UniValue::VOBJ);  std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; CTransaction regtx,tx; std::string name,description,format; uint256 hashBlock,txid,oracletxid; CMutableTransaction mtx; CPubKey Oraclespk,markerpubkey,pk; struct CCcontract_info *cp,C; uint8_t buf33[33]; int64_t datafee,funding; char str[67],markeraddr[64],numstr[64];
    cp = CCinit(&C,EVAL_ORACLES);
    Oraclespk = GetUnspendable(cp,0);
    buf33[0] = 0x02;
    endiancpy(&buf33[1],(uint8_t *)&origtxid,32);
    markerpubkey = buf2pk(buf33);
    Getscriptaddress(markeraddr,CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG);
    if ( GetTransaction(origtxid,tx,hashBlock,false) != 0 )
    {
        if ( tx.vout.size() > 0 && DecodeOraclesCreateOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,name,description,format) == 'C' )
        {
            result.push_back(Pair("txid",uint256_str(str,origtxid)));
            result.push_back(Pair("name",name));
            result.push_back(Pair("description",description));
            result.push_back(Pair("marker",markeraddr));
            SetCCtxids(addressIndex,markeraddr);
            for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
            {
                txid = it->first.txhash;
                if ( GetTransaction(txid,regtx,hashBlock,false) != 0 )
                {
                    if ( regtx.vout.size() > 0 && DecodeOraclesOpRet(regtx.vout[regtx.vout.size()-1].scriptPubKey,oracletxid,pk,datafee) == 'R' && oracletxid == origtxid )
                    {
                        result.push_back(Pair("provider",pubkey33_str(str,(uint8_t *)pk.begin())));
                        funding = LifetimeOraclesFunds(cp,oracletxid,pk);
                        sprintf(numstr,"%.8f",(double)funding/COIN);
                        obj.push_back(Pair("lifetime",numstr));
                        funding = AddOraclesInputs(cp,mtx,pk,0,0);
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

UniValue OraclesList()
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

