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

#include "CCGateways.h"

/*
 Uses MofN CC's normal msig handling to create automated deposits -> token issuing. And partial signing by the selected pubkeys for releasing the funds. A user would be able to select which pubkeys to use to construct the automated deposit/redeem multisigs.
 
 the potential pubkeys to be used would be based on active oracle data providers with recent activity.
 
 bind asset <-> KMD gateway deposit address
 KMD deposit -> globally spendable marker utxo
 spend marker utxo and spend linked/locked asset to user's CC address
 
 redeem -> asset to global CC address with withdraw address -> gateway spendable marker utxo
 spend market utxo and withdraw from gateway deposit address
 
 rpc calls:
 GatewayList
 GatewayInfo bindtxid
 GatewayBind coin tokenid M N pubkey(s)
 external: deposit to depositaddr with claimpubkey
 GatewayDeposit coin tokenid external.deposittxid -> markertxid
 GatewayClaim coin tokenid external.deposittxid markertxid -> spend marker and deposit asset
 
 GatewayWithdraw coin tokenid withdrawaddr
 external: do withdraw to withdrawaddr and spend marker, support for partial signatures and autocomplete
 
 deposit addr can be 1 to MofN pubkeys
 1:1 gateway with native coin
 
*/

// start of consensus code

CScript EncodeGatewaysBindOpRet(uint8_t funcid,std::string coin,uint256 tokenid,int64_t totalsupply,uint256 oracletxid,uint8_t M,uint8_t N,std::vector<CPubKey> pubkeys,uint8_t taddr,uint8_t prefix,uint8_t prefix2)
{
    CScript opret; uint8_t evalcode = EVAL_GATEWAYS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << coin << prefix << prefix2 << taddr << tokenid << totalsupply << M << N << pubkeys);
    return(opret);
}

CScript EncodeGatewaysOpRet(uint8_t funcid,std::string coin,uint256 bindtxid,std::vector<struct oracle_merklepair> publishers,int32_t height,uint256 cointxid,std::string deposithex,std::vector<uint256>proof,std::vector<uint8_t> redeemscript,int64_t amount)
{
    CScript opret; uint8_t evalcode = EVAL_GATEWAYS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << coin << bindtxid << publishers << height << cointxid << deposithex << proof << redeemscript << amount);
    return(opret);
}

uint8_t DecodeGatewaysOpRet(const CScript &scriptPubKey,std::string &coin,uint256 &bindtxid,std::vector<struct oracle_merklepair> &publishers,int32_t &height,uint256 &cointxid,std::string &deposithex,std::vector<uint256> &proof,std::vector<uint8_t> &redeemscript,int64_t &amount)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> coin; ss >> bindtxid; ss >> publishers; ss >> height; ss >> cointxid; ss >> deposithex; ss >> proof; ss >> redeemscript; ss >> amount) != 0 )
    {
        return(f);
    }
    return(0);
}

uint8_t DecodeGatewaysBindOpRet(char *depositaddr,const CScript &scriptPubKey,std::string &coin,uint256 &tokenid,int64_t &totalsupply,uint256 &oracletxid,uint8_t &M,uint8_t &N,std::vector<CPubKey> &pubkeys,uint8_t &taddr,uint8_t &prefix,uint8_t &prefix2)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    depositaddr[0] = 0;
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> coin; ss >> prefix; ss >> prefix2; ss >> taddr; ss >> tokenid; ss >> totalsupply; ss >> M; ss >> N; ss >> pubkeys) != 0 )
    {
        if ( prefix == 60 )
        {
            if ( N > 1 )
                Getscriptaddress(depositaddr,GetScriptForMultisig(M,pubkeys));
            else Getscriptaddress(depositaddr,CScript() << ParseHex(HexStr(pubkeys[0])) << OP_CHECKSIG);
        }
        else
        {
            fprintf(stderr,"need to generate non-KMD addresses\n");
        }
        return(f);
    }
    return(0);
}

int64_t IsGatewaysvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool GatewaysExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
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
                    return eval->Invalid("cant Gateways from mempool");
                if ( (assetoshis= IsGatewaysvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsGatewaysvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool GatewaysValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)
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
        if ( GatewaysExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Gatewaysget invalid amount\n");
            return false;
        }
        else
        {
            txid = tx.GetHash();
            memcpy(hash,&txid,sizeof(hash));
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Gatewaysget validated\n");
            else fprintf(stderr,"Gatewaysget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddGatewaysInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs)
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
            if ( (nValue= IsGatewaysvout(cp,vintx,vout)) > 10000 && myIsutxo_spentinmempool(txid,vout) == 0 )
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

UniValue GatewaysInfo(uint256 bindtxid)
{
    UniValue result(UniValue::VOBJ); std::string coin; char str[65],numstr[65],depositaddr[64]; uint8_t M,N; std::vector<CPubKey> pubkeys; uint8_t taddr,prefix,prefix2; uint256 tokenid,oracletxid,hashBlock; CTransaction tx; CMutableTransaction mtx; CPubKey Gatewayspk; struct CCcontract_info *cp,C; int64_t totalsupply,remaining;
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","Gateways"));
    cp = CCinit(&C,EVAL_GATEWAYS);
    Gatewayspk = GetUnspendable(cp,0);
    if ( GetTransaction(bindtxid,tx,hashBlock,false) != 0 )
    {
        if ( tx.vout.size() > 0 && DecodeGatewaysBindOpRet(depositaddr,tx.vout[tx.vout.size()-1].scriptPubKey,coin,tokenid,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2) != 0 && M <= N && N > 0 )
        {
            depositaddr[0] = 0;
            if ( N > 1 )
            {
                result.push_back(Pair("M",M));
                result.push_back(Pair("N",N));
            }
            result.push_back(Pair("coin",coin));
            result.push_back(Pair("oracletxid",uint256_str(str,oracletxid)));
            result.push_back(Pair("taddr",taddr));
            result.push_back(Pair("prefix",prefix));
            result.push_back(Pair("prefix2",prefix2));
            result.push_back(Pair("deposit",depositaddr));
            result.push_back(Pair("tokenid",uint256_str(str,tokenid)));
            sprintf(numstr,"%.8f",(double)totalsupply/COIN);
            result.push_back(Pair("totalsupply",numstr));
            remaining = CCaddress_balance(depositaddr);
            sprintf(numstr,"%.8f",(double)remaining/COIN);
            result.push_back(Pair("remaining",numstr));
            sprintf(numstr,"%.8f",(double)(totalsupply - remaining)/COIN);
            result.push_back(Pair("issued",numstr));
        }
    }
    return(result);
}

UniValue GatewaysList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; struct CCcontract_info *cp,C; uint256 txid,hashBlock,oracletxid,tokenid; CTransaction vintx; std::string coin; int64_t totalsupply; char str[65],depositaddr[64]; uint8_t M,N,taddr,prefix,prefix2; std::vector<CPubKey> pubkeys;
    cp = CCinit(&C,EVAL_GATEWAYS);
    SetCCtxids(addressIndex,cp->unspendableCCaddr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( vintx.vout.size() > 0 && DecodeGatewaysBindOpRet(depositaddr,vintx.vout[vintx.vout.size()-1].scriptPubKey,coin,tokenid,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2) != 0 )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}

std::string GatewaysBind(uint64_t txfee,std::string coin,uint256 tokenid,int64_t totalsupply,uint256 oracletxid,uint8_t M,uint8_t N,std::vector<CPubKey> pubkeys)
{
    CMutableTransaction mtx; CTransaction oracletx; uint8_t taddr,prefix,prefix2; CPubKey mypk,gatewayspk; CScript opret; uint256 hashBlock; struct CCcontract_info *cp,C; std::string name,description,format; int32_t i,numvouts; int64_t fullsupply; char destaddr[64],coinaddr[64],str[65],*fstr;
    cp = CCinit(&C,EVAL_GATEWAYS);
    if ( N == 0 || N > 15 || M > N )
    {
        fprintf(stderr,"illegal M.%d or N.%d\n",M,N);
        return("");
    }
    if ( strcmp((char *)"KMD",coin.c_str()) != 0 )
    {
        fprintf(stderr,"only KMD supported for now\n");
        return("");
    }
    taddr = 0;
    prefix = 60;
    prefix2 = 85;
    if ( pubkeys.size() != N )
    {
        fprintf(stderr,"M.%d N.%d but pubkeys[%d]\n",M,N,(int32_t)pubkeys.size());
        return("");
    }
    for (i=0; i<N; i++)
    {
        Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(pubkeys[i])) << OP_CHECKSIG);
        if ( CCaddress_balance(coinaddr) == 0 )
        {
            fprintf(stderr,"M.%d N.%d but pubkeys[%d] has no balance\n",M,N,i);
            return("");
        }
    }
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    gatewayspk = GetUnspendable(cp,0);
    if ( _GetCCaddress(destaddr,EVAL_ASSETS,gatewayspk) == 0 )
    {
        fprintf(stderr,"Gateway bind.%s (%s) cant create globaladdr\n",coin.c_str(),uint256_str(str,tokenid));
        return("");
    }
    if ( (fullsupply= CCfullsupply(tokenid)) != totalsupply )
    {
        fprintf(stderr,"Gateway bind.%s (%s) globaladdr.%s totalsupply %.8f != fullsupply %.8f\n",coin.c_str(),uint256_str(str,tokenid),cp->unspendableCCaddr,(double)totalsupply/COIN,(double)fullsupply/COIN);
        return("");
    }
    if ( CCtoken_balance(destaddr,tokenid) != totalsupply )
    {
        fprintf(stderr,"Gateway bind.%s (%s) globaladdr.%s token balance %.8f != %.8f\n",coin.c_str(),uint256_str(str,tokenid),cp->unspendableCCaddr,(double)CCtoken_balance(destaddr,tokenid)/COIN,(double)totalsupply/COIN);
        return("");
    }
    if ( GetTransaction(oracletxid,oracletx,hashBlock,false) == 0 || (numvouts= oracletx.vout.size()) <= 0 )
    {
        fprintf(stderr,"cant find oracletxid %s\n",uint256_str(str,oracletxid));
        return("");
    }
    if ( DecodeOraclesCreateOpRet(oracletx.vout[numvouts-1].scriptPubKey,name,description,format) != 'C' )
    {
        fprintf(stderr,"mismatched oracle name %s != %s\n",name.c_str(),coin.c_str());
        return("");
    }
    if ( (fstr= (char *)format.c_str()) == 0 || strncmp(fstr,"Ihh",3) != 0 )
    {
        fprintf(stderr,"illegal format (%s) != (%s)\n",fstr,(char *)"Ihh");
        return("");
    }
    fprintf(stderr,"implement GatewaysBindExists\n");
    /*if ( GatewaysBindExists(cp,gatewayspk,coin,tokenid) != 0 ) // dont forget to check mempool!
    {
        fprintf(stderr,"Gateway bind.%s (%s) already exists\n",coin.c_str(),uint256_str(str,tokenid));
        return("");
    }*/
    if ( AddNormalinputs(mtx,mypk,2*txfee,60) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,gatewayspk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeGatewaysBindOpRet('B',coin,tokenid,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2)));
    }
    fprintf(stderr,"cant find enough inputs\n");
    return("");
}

uint256 GatewaysReverseScan(uint256 &txid,int32_t height,uint256 reforacletxid,uint256 batontxid)
{
    CTransaction tx; uint256 hash,mhash,hashBlock,oracletxid; int64_t val; int32_t numvouts; int64_t merkleht; CPubKey pk; std::vector<uint8_t>data;
    txid = zeroid;
    while ( GetTransaction(batontxid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 0 )
    {
        if ( DecodeOraclesData(tx.vout[numvouts-1].scriptPubKey,oracletxid,hash,pk,data) == 'D' && oracletxid == reforacletxid )
        {
            if ( oracle_format(&hash,&merkleht,0,'I',(uint8_t *)data.data(),0,(int32_t)data.size()) == sizeof(int32_t) && merkleht == height )
            {
                if ( oracle_format(&hash,&val,0,'h',(uint8_t *)data.data(),sizeof(int32_t),(int32_t)data.size()) == sizeof(hash) &&
                oracle_format(&mhash,&val,0,'h',(uint8_t *)data.data(),(int32_t)(sizeof(int32_t)+sizeof(uint256)),(int32_t)data.size()) == sizeof(hash) && mhash != zeroid )
                {
                    txid = batontxid;
                    return(mhash);
                } else return(zeroid);
            }
            batontxid = hash;
        } else break;
    }
    return(zeroid);
}

int64_t GatewaysVerify(char *refdepositaddr,uint256 oracletxid,std::string refcoin,uint256 cointxid,const std::string deposithex,std::vector<uint256>proof,uint256 merkleroot,std::vector<uint8_t>redeemscript)
{
    uint256 hashBlock,txid = zeroid; CTransaction tx; std::string name,description,format; CScript scriptPubKey; char destaddr[64],str[65]; int32_t numvouts; int64_t nValue = 0;
    if ( GetTransaction(oracletxid,tx,hashBlock,false) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        fprintf(stderr,"GatewaysVerify cant find oracletxid %s\n",uint256_str(str,oracletxid));
        return(0);
    }
    if ( DecodeOraclesCreateOpRet(tx.vout[numvouts-1].scriptPubKey,name,description,format) != 'C' || name != refcoin )
    {
        fprintf(stderr,"GatewaysVerify mismatched oracle name %s != %s\n",name.c_str(),refcoin.c_str());
        return(0);
    }
    if ( DecodeHexTx(tx,deposithex) != 0 )
    {
        scriptPubKey = CScript() << redeemscript;
        Getscriptaddress(destaddr,tx.vout[0].scriptPubKey);
        if ( strcmp(refdepositaddr,destaddr) == 0 && scriptPubKey == tx.vout[1].scriptPubKey )
        {
            txid = tx.GetHash();
            nValue = tx.vout[0].nValue;
        }
        else
        {
            Getscriptaddress(destaddr,tx.vout[1].scriptPubKey);
            if ( strcmp(refdepositaddr,destaddr) == 0 && scriptPubKey == tx.vout[0].scriptPubKey )
            {
                txid = tx.GetHash();
                nValue = tx.vout[1].nValue;
            }
        }
    }
    if ( txid == cointxid )
    {
        fprintf(stderr,"verify proof for cointxid in merkleroot\n");
        return(nValue);
    } else fprintf(stderr,"(%s) != (%s) or txid mismatch.%d or script mismatch.%d\n",refdepositaddr,destaddr,txid != cointxid,scriptPubKey != tx.vout[1].scriptPubKey);
    return(0);
}

int64_t GatewaysDepositval(CTransaction tx)
{
    int32_t numvouts,height; int64_t amount; std::string coin,deposithex; std::vector<struct oracle_merklepair> publishers; uint256 bindtxid,cointxid; std::vector<uint256> proof; std::vector<uint8_t> claimpubkey;
    if ( (numvouts= tx.vout.size()) > 0 )
    {
        if ( DecodeGatewaysOpRet(tx.vout[numvouts-1].scriptPubKey,coin,bindtxid,publishers,height,cointxid,deposithex,proof,claimpubkey,amount) == 'D' )
        {
            // coin, bindtxid, publishers
            fprintf(stderr,"need to validate deposittxid more\n");
            return(amount);
        }
    }
    return(0);
}

std::string GatewaysDeposit(uint64_t txfee,uint256 bindtxid,std::vector<CPubKey>pubkeys,int32_t height,std::string refcoin,uint256 cointxid,std::string deposithex,std::vector<uint256>proof,std::vector<uint8_t> redeemscript,int64_t amount)
{
    CMutableTransaction mtx; CTransaction bindtx; CPubKey mypk,gatewayspk; uint256 oracletxid,merkleroot,mhash,hashBlock,tokenid; int64_t totalsupply; int32_t i,m,n,numvouts; uint8_t M,N,taddr,prefix,prefix2; std::string coin; struct CCcontract_info *cp,C; std::vector<CPubKey> msigpubkeys; std::vector<struct oracle_merklepair> publishers; struct oracle_merklepair P; char str[65],depositaddr[64];
    cp = CCinit(&C,EVAL_GATEWAYS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    gatewayspk = GetUnspendable(cp,0);
    if ( GetTransaction(bindtxid,bindtx,hashBlock,false) == 0 || (numvouts= bindtx.vout.size()) <= 0 )
    {
        fprintf(stderr,"cant find bindtxid %s\n",uint256_str(str,bindtxid));
        return("");
    }
    if ( DecodeGatewaysBindOpRet(depositaddr,bindtx.vout[numvouts-1].scriptPubKey,coin,tokenid,totalsupply,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2) != 'B' || refcoin != coin )
    {
        fprintf(stderr,"invalid bindtxid %s coin.%s\n",uint256_str(str,bindtxid),coin.c_str());
        return("");
    }
    n = (int32_t)pubkeys.size();
    merkleroot = zeroid;
    for (i=m=0; i<n; i++)
    {
        if ( (mhash= GatewaysReverseScan(txid,height,oracletxid,OraclesBatontxid(oracletxid,pubkeys[i].pk))) != zeroid )
        {
            if ( merkleroot == zeroid )
                merkleroot = mhash, m = 1;
            else if ( mhash == merkleroot )
                m++;
            P.pk = pubkeys[i].pk;
            P.txid = txid;
            publishers.push_back(P);
        }
    }
    if ( merkleroot == zeroid || m < n/2 )
    {
        fprintf(stderr,"couldnt find merkleroot for ht.%d %s oracle.%s m.%d vs n.%d\n",height,coin.c_str(),uint256_str(str,oracletxid),m,n);
        return("");
    }
    if ( GatewaysVerify(depositaddr,oracletxid,coin,cointxid,deposithex,proof,merkleroot,redeemscript) != amount )
    {
        fprintf(stderr,"deposittxid didnt validate\n");
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,2*txfee,60) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,mypk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeGatewaysOpRet('D',coin,bindtxid,publishers,height,cointxid,deposithex,proof,redeemscript,amount)));
    }
    fprintf(stderr,"cant find enough inputs\n");
    return("");
}

std::string GatewaysClaim(uint64_t txfee,uint256 bindtxid,std::string refcoin,uint256 deposittxid,std::vector<uint8_t> claimpubkey,int64_t amount)
{
    CMutableTransaction mtx; CTransaction tx; CPubKey mypk,gatewayspk; struct CCcontract_info *cp,C,*assetscp,C2; uint8_t M,N,taddr,prefix,prefix2; std::string coin; std::vector<CPubKey> msigpubkeys; int64_t totalsupply,inputs,CCchange=0; int32_t numvouts; uint256 assetid,oracletxid; char str[65],depositaddr[64];
    cp = CCinit(&C,EVAL_GATEWAYS);
    assetscp = CCinit(&C2,EVAL_ASSETS);
    memcpy(cp->unspendablepriv2,assetscp->CCpriv,32);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    gatewayspk = GetUnspendable(cp,0);
    if ( GetTransaction(bindtxid,tx,hashBlock,false) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        fprintf(stderr,"cant find bindtxid %s\n",uint256_str(str,bindtxid));
        return("");
    }
    if ( DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,coin,assetid,totalsupply,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2) != 'B' || coin != refcoin )
    {
        fprintf(stderr,"invalid bindtxid %s coin.%s\n",uint256_str(str,bindtxid),coin.c_str());
        return("");
    }
    if ( GetTransaction(deposittxid,tx,hashBlock,false) == 0 )
    {
        fprintf(stderr,"cant find bindtxid %s\n",uint256_str(str,bindtxid));
        return("");
    }
    if ( (total= GatewaysDepositval(tx)) == 0 )
    {
        fprintf(stderr,"invalid Gateways deposittxid %s\n",uint256_str(str,deposittxid));
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        if ( (inputs= AddAssetInputs(assetscp,mtx,gatewayspk,assetid,total,60)) > 0 )
        {
            if ( inputs > total )
                CCchange = (inputs - total);
            mtx.vin.push_back(CTxIn(deposittxid,0,CScript()));
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,total,mypk));
            if ( CCchange != 0 )
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,gatewayspk));
            return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,EncodeAssetOpRet('t',assetid,zeroid,0,Mypubkey())));
        }
    }
    fprintf(stderr,"cant find enough inputs or mismatched total\n");
    return("");
}

std::string GatewaysWithdraw(uint64_t txfee,uint256 bindtxid,std::string refcoin,std::vector<uint8_t> withdrawpub,int64_t amount)
{
    CMutableTransaction mtx; CTransaction tx; CPubKey mypk,gatewayspk; struct CCcontract_info *cp,C,*assetscp,C2; uint256 assetid; int64_t totalsupply,inputs,CCchange=0; uint8_t M,N,taddr,prefix,prefix2; std::string coin; std::vector<CPubKey> msigpubkeys; char depositaddr[64];
    cp = CCinit(&C,EVAL_GATEWAYS);
    assetscp = CCinit(&C2,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    gatewayspk = GetUnspendable(cp,0);
    if ( GetTransaction(bindtxid,tx,hashBlock,false) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        fprintf(stderr,"cant find bindtxid %s\n",uint256_str(str,bindtxid));
        return("");
    }
    if ( DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,coin,assetid,totalsupply,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2) != 'B' || coin != refcoin )
    {
        fprintf(stderr,"invalid bindtxid %s coin.%s\n",uint256_str(str,bindtxid),coin.c_str());
        return("");
    }
   if ( AddNormalinputs(mtx,mypk,2*txfee,1) > 0 )
    {
        if ( (inputs= AddAssetInputs(assetscp,mtx,mypk,assetid,amount,60)) > 0 )
        {
            if ( inputs > amount )
                CCchange = (inputs - amount);
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,amount,gatewayspk));
            if ( CCchange != 0 )
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,mypk));
            mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(withdrawpub)) << OP_CHECKSIG));
            return(FinalizeCCTx(mask,assetscp,mtx,mypk,txfee,EncodeAssetOpRet('t',assetid,zeroid,0,Mypubkey())));
        }
    }
    fprintf(stderr,"cant find enough inputs or mismatched total\n");
    return("");
}

// withdrawtxid used on external chain to create baton address, its existence in mempool (along with the withdraw) proof that the withdraw is pending


