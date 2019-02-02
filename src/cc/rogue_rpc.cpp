
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

#include "cJSON.h"

#define ROGUE_REGISTRATION 5

//////////////////////// start of CClib interface
//./komodod -ac_name=ROGUE -ac_supply=1000000 -pubkey=<yourpubkey> -addnode=5.9.102.210  -ac_cclib=rogue -ac_perc=10000000 -ac_reward=100000000 -ac_cc=60001 -ac_script=2ea22c80203d1579313abe7d8ea85f48c65ea66fc512c878c0d0e6f6d54036669de940febf8103120c008203000401cc &

// cclib newgame 17
// cclib pending 17
// cclib txidinfo 17 \"35e99df53c981a937bfa2ce7bfb303cea0249dba34831592c140d1cb729cb19f\"
// ./rogue <seed> gui -> creates keystroke files

CScript rogue_newgameopret(int64_t buyin)
{
    CScript opret; uint8_t evalcode = EVAL_ROGUE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'G' << buyin);
    return(opret);
}

/*CScript rogue_solutionopret(char *solution,uint32_t timestamps[81])
{
    CScript opret; uint8_t evalcode = EVAL_ROGUE; std::string str(solution); std::vector<uint8_t> data; int32_t i;
    for (i=0; i<81; i++)
    {
        data.push_back((timestamps[i] >> 24) & 0xff);
        data.push_back((timestamps[i] >> 16) & 0xff);
        data.push_back((timestamps[i] >> 8) & 0xff);
        data.push_back(timestamps[i] & 0xff);
    }
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'S' << str << data);
    return(opret);
}

uint8_t rogue_solutionopreturndecode(char solution[82],uint32_t timestamps[81],CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f; std::string str; std::vector<uint8_t> data; int32_t i,ind; uint32_t x;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> str; ss >> data) != 0 && e == EVAL_ROGUE && f == 'S' )
    {
        if ( data.size() == 81*sizeof(uint32_t) && str.size() == 81 )
        {
            strcpy(solution,str.c_str());
            for (i=ind=0; i<81; i++)
            {
                if ( solution[i] < '1' || solution[i] > '9' )
                    break;
                x = data[ind++];
                x <<= 8, x |= (data[ind++] & 0xff);
                x <<= 8, x |= (data[ind++] & 0xff);
                x <<= 8, x |= (data[ind++] & 0xff);
                timestamps[i] = x;
            }
            if ( i == 81 )
                return(f);
        } else fprintf(stderr,"datasize %d sol[%d]\n",(int32_t)data.size(),(int32_t)str.size());
    }
    return(0);
}*/

uint8_t rogue_newgameopreturndecode(int64_t &buyin,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> buyin) != 0 && e == EVAL_ROGUE && f == 'G' )
    {
        return(f);
    }
    return(0);
}

UniValue rogue_newgame(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; CPubKey roguepk,mypk; char *jsonstr; uint64_t inputsum,amount = 0;
    if ( params != 0 )
    {
        if ( (jsonstr= jprint(params,0)) != 0 )
        {
            if ( jsonstr[0] == '"' && jsonstr[strlen(jsonstr)-1] == '"' )
            {
                jsonstr[strlen(jsonstr)-1] = 0;
                jsonstr++;
            }
            amount = atof(jsonstr) * COIN + 0.0000000049;
            free(jsonstr);
        }
    }
    mypk = pubkey2pk(Mypubkey());
    roguepk = GetUnspendable(cp,0);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","rogue"));
    result.push_back(Pair("method","newgame"));
    if ( amount == 0 )
        result.push_back(Pair("type","newbie"));
    else result.push_back(Pair("type","buyin"));
    result.push_back(Pair("amount",ValueFromAmount(amount)));
    if ( (inputsum= AddCClibInputs(cp,mtx,roguepk,3*txfee,16,cp->unspendableCCaddr)) >= 3*txfee )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,roguepk));
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,mypk));
        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,rogue_newgameopret(amount));
        if ( rawtx.size() > 0 )
        {
            CTransaction tx;
            result.push_back(Pair("hex",rawtx));
            if ( DecodeHexTx(tx,rawtx) != 0 )
                result.push_back(Pair("txid",tx.GetHash().ToString()));
        } else result.push_back(Pair("error","couldnt finalize CCtx"));
    }
    return(result);
}

UniValue rogue_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ);
    return(result);
}

UniValue rogue_progress(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    return(result);
}

UniValue rogue_claimwin(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    return(result);
}

UniValue rogue_extract(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    return(result);
}

UniValue rogue_txidinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t ht,numvouts; char CCaddr[64],str[65],*txidstr; uint256 txid,hashBlock; CTransaction tx; uint64_t seed; int64_t buyin; CBlockIndex *pindex;
    if ( params != 0 )
    {
        if ( (txidstr= jprint(params,0)) != 0 )
        {
            if ( txidstr[0] == '"' && txidstr[strlen(txidstr)-1] == '"' )
            {
                txidstr[strlen(txidstr)-1] = 0;
                txidstr++;
            }
            //printf("params -> (%s)\n",txidstr);
            decode_hex((uint8_t *)&txid,32,txidstr);
            txid = revuint256(txid);
            result.push_back(Pair("txid",txid.GetHex()));
            if ( GetTransaction(txid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 1 )
            {
                if ( rogue_newgameopreturndecode(buyin,tx.vout[numvouts-1].scriptPubKey) == 'G' )
                {
                    result.push_back(Pair("result","success"));
                    if ( (pindex= komodo_blockindex(hashBlock)) != 0 )
                    {
                        ht = pindex->GetHeight();
                        result.push_back(Pair("height",ht));
                        result.push_back(Pair("start",ht+ROGUE_REGISTRATION));
                        if ( komodo_nextheight() > ht+ROGUE_REGISTRATION )
                        {
                            if ( (pindex= komodo_chainactive(ht+ROGUE_REGISTRATION)) != 0 )
                            {
                                hashBlock = pindex->GetBlockHash();
                                result.push_back(Pair("starthash",hashBlock.ToString().c_str()));
                                memcpy(&seed,&hashBlock,sizeof(seed));
                                seed &= (1LL << 62) - 1;
                                result.push_back(Pair("seed",(int64_t)seed));
                            }
                        }
                    }
                    Getscriptaddress(CCaddr,tx.vout[1].scriptPubKey);
                    result.push_back(Pair("rogueaddr",CCaddr));
                    result.push_back(Pair("buyin",ValueFromAmount(buyin)));
                }
                else
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","couldnt extract rogue_generate opreturn"));
                }
            }
            else
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","couldnt find txid"));
            }
        }
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","missing txid in params"));
    }
    result.push_back(Pair("name","rogue"));
    result.push_back(Pair("method","txidinfo"));
    return(result);
}

UniValue rogue_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR);
    char coinaddr[64]; uint64_t seed; int64_t amount,nValue,total=0; uint256 txid,hashBlock; CTransaction tx; int32_t ht,vout,numvouts; CPubKey roguepk; CBlockIndex *pindex;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    roguepk = GetUnspendable(cp,0);
    GetCCaddress(cp,coinaddr,roguepk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( it->second.satoshis != txfee || vout != 0 )
            continue;
        if ( GetTransaction(txid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 1 )
        {
            if ( (nValue= IsCClibvout(cp,tx,vout,coinaddr)) == txfee && myIsutxo_spentinmempool(txid,vout) == 0 )
            {
                if ( rogue_newgameopreturndecode(amount,tx.vout[numvouts-1].scriptPubKey) == 'G' )
                {
                    UniValue obj(UniValue::VOBJ);
                    if ( (pindex= komodo_blockindex(hashBlock)) != 0 )
                    {
                        ht = pindex->GetHeight();
                        obj.push_back(Pair("height",ht));
                        obj.push_back(Pair("start",ht+ROGUE_REGISTRATION));
                        if ( komodo_nextheight() > ht+ROGUE_REGISTRATION )
                        {
                            if ( (pindex= komodo_chainactive(ht+ROGUE_REGISTRATION)) != 0 )
                            {
                                hashBlock = pindex->GetBlockHash();
                                obj.push_back(Pair("starthash",hashBlock.ToString().c_str()));
                                memcpy(&seed,&hashBlock,sizeof(seed));
                                seed &= (1LL << 62) - 1;
                                obj.push_back(Pair("seed",(int64_t)seed));
                            }
                        }
                    }
                    obj.push_back(Pair("buyin",ValueFromAmount(amount)));
                    obj.push_back(Pair("txid",txid.GetHex()));
                    a.push_back(obj);
                    total += amount;
                }
            }
        }
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","rogue"));
    result.push_back(Pair("method","pending"));
    result.push_back(Pair("pending",a));
    result.push_back(Pair("numpending",a.size()));
    result.push_back(Pair("total",ValueFromAmount(total)));
    return(result);
}

#ifdef notyest
UniValue rogue_solution(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); int32_t i,j,good,ind,n,numvouts; uint256 txid; char *jsonstr,*newstr,*txidstr,coinaddr[64],checkaddr[64],CCaddr[64],*solution=0,unsolved[82]; CPubKey pk,mypk; uint8_t vals9[9][9],priv32[32],pub33[33]; uint32_t timestamps[81]; uint64_t balance,inputsum; std::string rawtx; CTransaction tx; uint256 hashBlock;
    mypk = pubkey2pk(Mypubkey());
    memset(timestamps,0,sizeof(timestamps));
    result.push_back(Pair("name","rogue"));
    result.push_back(Pair("method","solution"));
    good = 0;
    if ( params != 0 )
    {
        if ( (jsonstr= jprint(params,0)) != 0 )
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
                } else newstr[j++] = jsonstr[i];
            }
            newstr[j] = 0;
            params = cJSON_Parse(newstr);
        } else params = 0;
        if ( params != 0 )
        {
            if ( (n= cJSON_GetArraySize(params)) > 2 && n <= (sizeof(timestamps)/sizeof(*timestamps))+2 )
            {
                for (i=2; i<n; i++)
                {
                    timestamps[i-2] = juinti(params,i);
                    //printf("%u ",timestamps[i]);
                }
                if ( (solution= jstri(params,1)) != 0 && strlen(solution) == 81 )
                {
                    for (i=ind=0; i<9; i++)
                        for (j=0; j<9; j++)
                        {
                            if ( solution[ind] < '1' || solution[ind] > '9' )
                            {
                                result.push_back(Pair("result","error"));
                                result.push_back(Pair("error","illegal solution"));
                                return(result);
                            }
                            vals9[i][j] = solution[ind++] - '0';
                        }
                    rogue_privkey(priv32,vals9);
                    priv2addr(coinaddr,pub33,priv32);
                    pk = buf2pk(pub33);
                    GetCCaddress(cp,CCaddr,pk);
                    result.push_back(Pair("rogueaddr",CCaddr));
                    balance = CCaddress_balance(CCaddr);
                    result.push_back(Pair("amount",ValueFromAmount(balance)));
                    if ( rogue_captcha(1,timestamps,komodo_nextheight()) < 0 )
                    {
                        result.push_back(Pair("result","error"));
                        result.push_back(Pair("error","captcha failure"));
                        return(result);
                    }
                    else
                    {
                        if ( (txidstr= jstri(params,0)) != 0 )
                        {
                            decode_hex((uint8_t *)&txid,32,txidstr);
                            txid = revuint256(txid);
                            result.push_back(Pair("txid",txid.GetHex()));
                            if ( CCgettxout(txid,0,1) < 0 )
                                result.push_back(Pair("error","already solved"));
                            else if ( GetTransaction(txid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 1 )
                            {
                                Getscriptaddress(checkaddr,tx.vout[1].scriptPubKey);
                                if ( strcmp(checkaddr,CCaddr) != 0 )
                                {
                                    result.push_back(Pair("result","error"));
                                    result.push_back(Pair("error","wrong solution"));
                                    result.push_back(Pair("yours",CCaddr));
                                    return(result);
                                }
                                if ( rogue_genopreturndecode(unsolved,tx.vout[numvouts-1].scriptPubKey) == 'G' )
                                {
                                    for (i=0; i<81; i++)
                                    {
                                        if ( unsolved[i] < '1' || unsolved[i] > '9')
                                            continue;
                                        else if ( unsolved[i] != solution[i] )
                                        {
                                            printf("i.%d [%c] != [%c]\n",i,unsolved[i],solution[i]);
                                            result.push_back(Pair("error","wrong rogue solved"));
                                            break;
                                        }
                                    }
                                    if ( i == 81 )
                                        good = 1;
                                } else result.push_back(Pair("error","cant decode rogue"));
                            } else result.push_back(Pair("error","couldnt find rogue"));
                        }
                        if ( good != 0 )
                        {
                            mtx.vin.push_back(CTxIn(txid,0,CScript()));
                            if ( (inputsum= AddCClibInputs(cp,mtx,pk,balance,16,CCaddr)) >= balance )
                            {
                                mtx.vout.push_back(CTxOut(balance,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                                CCaddr2set(cp,cp->evalcode,pk,priv32,CCaddr);
                                rawtx = FinalizeCCTx(0,cp,mtx,pubkey2pk(Mypubkey()),txfee,rogue_solutionopret(solution,timestamps));
                                if ( rawtx.size() > 0 )
                                {
                                    result.push_back(Pair("result","success"));
                                    result.push_back(Pair("hex",rawtx));
                                }
                                else result.push_back(Pair("error","couldnt finalize CCtx"));
                            } else result.push_back(Pair("error","couldnt find funds in solution address"));
                        }
                    }
                }
            }
            else
            {
                printf("n.%d params.(%s)\n",n,jprint(params,0));
                result.push_back(Pair("error","couldnt get all params"));
            }
            return(result);
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","couldnt parse parameters"));
            result.push_back(Pair("parameters",newstr));
            return(result);
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","missing parameters"));
    return(result);
}
#endif

