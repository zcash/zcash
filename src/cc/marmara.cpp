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

#include "CCMarmara.h"

/*
 Marmara CC is for the MARMARA project
 
*/

// start of consensus code

int64_t IsMarmaravout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool MarmaraExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
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
                    return eval->Invalid("cant Marmara from mempool");
                if ( (assetoshis= IsMarmaravout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsMarmaravout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

int32_t MarmaraRandomize(uint32_t ind)
{
    uint64_t val64; uint32_t val,range = (MARMARA_MAXLOCK - MARMARA_MINLOCK);
    val64 = komodo_block_prg(ind);
    val = (uint32_t)(val64 >> 32);
    val ^= (uint32_t)val64;
    return((val % range) + MARMARA_MINLOCK);
}

int32_t MarmaraUnlockht(int32_t height)
{
    uint32_t ind = height / MARMARA_GROUPSIZE;
    height = (height / MARMARA_GROUPSIZE) * MARMARA_GROUPSIZE;
    return(height + MarmaraRandomize(ind));
}

CScript EncodeMarmaraCoinbaseOpRet(uint8_t funcid,CPubKey pk,int32_t ht)
{
    CScript opret; int32_t unlockht; uint8_t evalcode = EVAL_MARMARA;
    unlockht = MarmaraUnlockht(ht);
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << pk << ht << unlockht);
    if ( 0 )
    {
        std::vector<uint8_t> vopret; uint8_t *script,i;
        GetOpReturnData(opret,vopret);
        script = (uint8_t *)vopret.data();
        {
            for (i=0; i<vopret.size(); i++)
                fprintf(stderr,"%02x",script[i]);
            fprintf(stderr," <- gen opret.%c\n",funcid);
        }
    }
    return(opret);
}

uint8_t DecodeMaramaraCoinbaseOpRet(const CScript scriptPubKey,CPubKey &pk,int32_t &height,int32_t &unlockht)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( 0 )
    {
        int32_t i;
        for (i=0; i<vopret.size(); i++)
            fprintf(stderr,"%02x",script[i]);
        fprintf(stderr," <- opret\n");
    }
    if ( vopret.size() > 2 && script[0] == EVAL_MARMARA )
    {
        if ( script[1] == 'C' || script[1] == 'P' )
        {
            if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> pk; ss >> height; ss >> unlockht) != 0 )
            {
                return(script[1]);
            } else fprintf(stderr,"DecodeMaramaraCoinbaseOpRet unmarshal error for %c\n",script[1]);
        } else fprintf(stderr,"script[1] is %d != 'C' %d or 'P' %d\n",script[1],'C','P');
    } else fprintf(stderr,"vopret.size() is %d\n",(int32_t)vopret.size());
    return(0);
}

CScript Marmara_scriptPubKey(int32_t height,CPubKey pk)
{
    CTxOut ccvout;
    if ( height > 0 && (height & 1) == 0 && pk.size() == 33 )
        ccvout = MakeCC1vout(EVAL_MARMARA,0,pk);
    return(ccvout.scriptPubKey);
}

CScript MarmaraCoinbaseOpret(uint8_t funcid,int32_t height,CPubKey pk)
{
    uint8_t *ptr;
    //fprintf(stderr,"height.%d pksize.%d\n",height,(int32_t)pk.size());
    if ( height > 0 && (height & 1) == 0 && pk.size() == 33 )
        return(EncodeMarmaraCoinbaseOpRet(funcid,pk,height));
    return(CScript());
}

int32_t MarmaraValidateCoinbase(int32_t height,CTransaction tx)
{
    struct CCcontract_info *cp,C; CPubKey pk; int32_t ht,unlockht; CTxOut ccvout;
    cp = CCinit(&C,EVAL_MARMARA);
    if ( 0 )
    {
        int32_t d,histo[365*2+30];
        memset(histo,0,sizeof(histo));
        for (ht=2; ht<100; ht++)
            fprintf(stderr,"%d ",MarmaraUnlockht(ht));
        fprintf(stderr," <- first 100 unlock heights\n");
        for (ht=2; ht<1000000; ht+=MARMARA_GROUPSIZE)
        {
            d = (MarmaraUnlockht(ht) - ht) / 1440;
            if ( d < 0 || d > sizeof(histo)/sizeof(*histo) )
                fprintf(stderr,"d error.%d at ht.%d\n",d,ht);
            else histo[d]++;
        }
        for (ht=0; ht<sizeof(histo)/sizeof(*histo); ht++)
            fprintf(stderr,"%d ",histo[ht]);
        fprintf(stderr,"<- unlock histogram[%d] by days locked\n",(int32_t)(sizeof(histo)/sizeof(*histo)));
    }
    if ( (height & 1) != 0 )
        return(0);
    if ( tx.vout.size() == 2 && tx.vout[1].nValue == 0 )
    {
        if ( DecodeMaramaraCoinbaseOpRet(tx.vout[1].scriptPubKey,pk,ht,unlockht) == 'C' )
        {
            if ( ht == height && MarmaraUnlockht(height) == unlockht )
            {
                fprintf(stderr,"ht.%d -> unlock.%d\n",ht,unlockht);
                ccvout = MakeCC1vout(EVAL_MARMARA,0,pk);
                if ( ccvout.scriptPubKey == tx.vout[0].scriptPubKey )
                    return(0);
                fprintf(stderr,"ht.%d mismatched CCvout scriptPubKey\n",height);
            } else fprintf(stderr,"ht.%d %d vs %d unlock.%d\n",height,MarmaraUnlockht(height),ht,unlockht);
        } else fprintf(stderr,"ht.%d error decoding coinbase opret\n",height);
    }
    return(-1);
}

bool MarmaraValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
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
        if ( MarmaraExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Marmaraget invalid amount\n");
            return false;
        }
        else
        {
            txid = tx.GetHash();
            memcpy(hash,&txid,sizeof(hash));
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Marmaraget validated\n");
            else fprintf(stderr,"Marmaraget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddMarmaraCoinbases(struct CCcontract_info *cp,CMutableTransaction &mtx,int32_t firstheight,CPubKey poolpk,int32_t maxinputs)
{
    char coinaddr[64]; CPubKey pk; int64_t nValue,totalinputs = 0; uint256 txid,hashBlock; CTransaction vintx; int32_t unlockht,ht,vout,unlocks,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,poolpk);
    SetCCunspents(unspentOutputs,coinaddr);
    unlocks = MarmaraUnlockht(firstheight);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 && vintx.IsCoinBase() != 0 )
        {
            if ( DecodeMaramaraCoinbaseOpRet(vintx.vout[1].scriptPubKey,pk,ht,unlockht) == 'C' && unlockht == unlocks && pk == poolpk )
            {
                if ( (nValue= IsMarmaravout(cp,vintx,vout)) > 0 && myIsutxo_spentinmempool(txid,vout) == 0 )
                {
                    if ( maxinputs != 0 )
                        mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                    nValue = it->second.satoshis;
                    totalinputs += nValue;
                    n++;
                    if ( maxinputs > 0 && n >= maxinputs )
                        break;
                }
            }
        }
    }
    return(totalinputs);
}

std::string MarmaraFund(uint64_t txfee,int64_t funds)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,Marmarapk; CScript opret; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_MARMARA);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    Marmarapk = GetUnspendable(cp,0);
    if ( AddNormalinputs(mtx,mypk,funds+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA,funds,Marmarapk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,opret));
    }
    return("");
}

UniValue MarmaraInfo()
{
    UniValue result(UniValue::VOBJ); char numstr[64];
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey Marmarapk; struct CCcontract_info *cp,C; int64_t funding;
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","Marmara"));
    cp = CCinit(&C,EVAL_MARMARA);
    Marmarapk = GetUnspendable(cp,0);
    return(result);
}

UniValue MarmaraPoolPayout(uint64_t txfee,int32_t firstheight,double perc,char *jsonstr) // [[pk0, shares0], [pk1, shares1], ...]
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); cJSON *item,*array; std::string rawtx; int32_t i,n; uint8_t buf[33]; CPubKey Marmarapk,pk,poolpk; int64_t payout,total,totalpayout=0; double poolshares,share,shares = 0.; char *pkstr,*errorstr=0; struct CCcontract_info *cp,C;
    poolpk = pubkey2pk(Mypubkey());
    if ( txfee == 0 )
        txfee = 10000;
    cp = CCinit(&C,EVAL_MARMARA);
    Marmarapk = GetUnspendable(cp,0);
    if ( (array= cJSON_Parse(jsonstr)) != 0 && (n= cJSON_GetArraySize(array)) > 0 )
    {
        for (i=0; i<n; i++)
        {
            item = jitem(array,i);
            if ( (pkstr= jstr(jitem(item,0),0)) != 0 && strlen(pkstr) == 66 )
                shares += jdouble(jitem(item,1),0);
            else
            {
                errorstr = (char *)"all items must be of the form [<pubke>, <shares>]";
                break;
            }
        }
        if ( errorstr == 0 && shares > SMALLVAL )
        {
            shares += shares * perc;
            if ( (total= AddMarmaraCoinbases(cp,mtx,firstheight,poolpk,60)) > 0 )
            {
                for (i=0; i<n; i++)
                {
                    item = jitem(array,i);
                    if ( (share= jdouble(jitem(item,1),0)) > SMALLVAL )
                    {
                        payout = (share * (total - txfee)) / shares;
                        if ( payout > 0 )
                        {
                            if ( (pkstr= jstr(jitem(item,0),0)) != 0 && strlen(pkstr) == 66 )
                            {
                                totalpayout += payout;
                                decode_hex(buf,33,pkstr);
                                mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA,payout,Marmarapk,buf2pk(buf)));
                            }
                        }
                    }
                }
                if ( totalpayout > 0 && total > totalpayout-txfee )
                {
                    mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA,total - totalpayout - txfee,Marmarapk,poolpk));
                }
                rawtx = FinalizeCCTx(0,cp,mtx,poolpk,txfee,MarmaraCoinbaseOpret('P',firstheight,poolpk));
                if ( rawtx.size() == 0 )
                    errorstr = (char *)"couldnt finalize CCtx";
            } else errorstr = (char *)"couldnt find any coinbases to payout";
        }
        else if ( errorstr == 0 )
            errorstr = (char *)"no valid shares submitted";
        free(array);
    } else errorstr = (char *)"couldnt parse poolshares jsonstr";
    if ( rawtx.size() == 0 || errorstr != 0 )
    {
        result.push_back(Pair("result","error"));
        if ( errorstr != 0 )
            result.push_back(Pair("error",errorstr));
    }
    else
    {
        result.push_back(Pair("result",(char *)"success"));
        result.push_back(Pair("rawtx",rawtx));
        if ( totalpayout > 0 && total > totalpayout-txfee )
        {
            result.push_back(Pair("totalpayout",(double)totalpayout/COIN));
            result.push_back(Pair("totalshares",shares));
            result.push_back(Pair("poolfee",(double)(total - totalpayout)/COIN));
            result.push_back(Pair("perc",100. * (double)(total - totalpayout)/totalpayout));
        }
    }
    return(result);
}

