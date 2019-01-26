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

#include <assert.h>
#include <cryptoconditions.h>

#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/cc.h"
#include "cc/eval.h"
#include "cc/utils.h"
#include "cc/CCinclude.h"
#include "main.h"
#include "chain.h"
#include "core_io.h"
#include "crosschain.h"

#define FAUCET2SIZE COIN

struct CClib_rpcinfo
{
    char *method,*help;
    int32_t numrequiredargs,maxargs; // frontloaded with required
    uint8_t funcid;
}
CClib_methods[] =
{
    { (char *)"faucet2_fund", (char *)"amount", 1, 1, 'F' },
    { (char *)"faucet2_get", (char *)"<no args>", 0, 0, 'G' },
};

std::string MYCCLIBNAME = (char *)"faucet2";

char *CClib_name() { return((char *)MYCCLIBNAME.c_str()); }

std::string CClib_rawtxgen(struct CCcontract_info *cp,uint8_t funcid,cJSON *params);

UniValue CClib_info(struct CCcontract_info *cp)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int32_t i; char str[2];
    result.push_back(Pair("result","success"));
    result.push_back(Pair("CClib",CClib_name()));
    for (i=0; i<sizeof(CClib_methods)/sizeof(*CClib_methods); i++)
    {
        UniValue obj(UniValue::VOBJ);
        if ( CClib_methods[i].funcid < ' ' || CClib_methods[i].funcid >= 128 )
            obj.push_back(Pair("funcid",CClib_methods[i].funcid));
        else
        {
            str[0] = CClib_methods[i].funcid;
            str[1] = 0;
            obj.push_back(Pair("funcid",str));
        }
        obj.push_back(Pair("name",CClib_methods[i].method));
        obj.push_back(Pair("help",CClib_methods[i].help));
        obj.push_back(Pair("params_required",CClib_methods[i].numrequiredargs));
        obj.push_back(Pair("params_max",CClib_methods[i].maxargs));
        a.push_back(obj);
    }
    result.push_back(Pair("methods",a));
    return(result);
}

UniValue CClib(struct CCcontract_info *cp,char *method,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t i; std::string rawtx;
    for (i=0; i<sizeof(CClib_methods)/sizeof(*CClib_methods); i++)
    {
        if ( strcmp(method,CClib_methods[i].method) == 0 )
        {
            result.push_back(Pair("result","success"));
            result.push_back(Pair("method",CClib_methods[i].method));
            rawtx = CClib_rawtxgen(cp,CClib_methods[i].funcid,params);
            result.push_back(Pair("rawtx",rawtx));
            return(result);
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("method",CClib_methods[i].method));
    result.push_back(Pair("error","method not found"));
    return(result);
}

int64_t IsCClibvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool CClibExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
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
                    return eval->Invalid("cant faucet2 from mempool");
                if ( (assetoshis= IsCClibvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsCClibvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+FAUCET2SIZE+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + FAUCET2SIZE + txfee");
    }
    else return(true);
}

bool CClib_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx,unsigned int nIn)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numblocks; bool retval; uint256 txid; uint8_t hash[32]; char str[65],destaddr[64];
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
                fprintf(stderr,"faucetget invalid vini\n");
                return eval->Invalid("illegal normal vini");
            }
        }
        //fprintf(stderr,"check amounts\n");
        if ( CClibExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"faucetget invalid amount\n");
            return false;
        }
        else
        {
            preventCCvouts = 1;
            if ( IsCClibvout(cp,tx,0) != 0 )
            {
                preventCCvouts++;
                i = 1;
            } else i = 0;
            txid = tx.GetHash();
            memcpy(hash,&txid,sizeof(hash));
            fprintf(stderr,"check faucetget txid %s %02x/%02x\n",uint256_str(str,txid),hash[0],hash[31]);
            if ( tx.vout[i].nValue != FAUCET2SIZE )
                return eval->Invalid("invalid faucet output");
            else if ( (hash[0] & 0xff) != 0 || (hash[31] & 0xff) != 0 )
                return eval->Invalid("invalid faucetget txid");
            Getscriptaddress(destaddr,tx.vout[i].scriptPubKey);
            SetCCtxids(txids,destaddr);
            for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=txids.begin(); it!=txids.end(); it++)
            {
                //int height = it->first.blockHeight;
                if ( CCduration(numblocks,it->first.txhash) > 0 && numblocks > 3 )
                {
                    //fprintf(stderr,"would return error %s numblocks.%d ago\n",uint256_str(str,it->first.txhash),numblocks);
                    return eval->Invalid("faucet2 is only for brand new addresses");
                }
            }
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"faucet2get validated\n");
            else fprintf(stderr,"faucet2get invalid\n");
            return(retval);
        }
    }
}

int64_t AddCClibInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs)
{
    char coinaddr[64]; int64_t threshold,nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    threshold = total/(maxinputs+1);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( it->second.satoshis < threshold )
            continue;
        //char str[65]; fprintf(stderr,"check %s/v%d %.8f`\n",uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        // no need to prevent dup
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( (nValue= IsCClibvout(cp,vintx,vout)) > 1000000 && myIsutxo_spentinmempool(txid,vout) == 0 )
            {
                if ( total != 0 && maxinputs != 0 )
                    mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                nValue = it->second.satoshis;
                totalinputs += nValue;
                n++;
                if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                    break;
            } else fprintf(stderr,"nValue too small or already spent in mempool\n");
        } else fprintf(stderr,"couldnt get tx\n");
    }
    return(totalinputs);
}


std::string Faucet2Fund(struct CCcontract_info *cp,uint64_t txfee,int64_t funds)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,cclibpk; CScript opret;
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    cclibpk = GetUnspendable(cp,0);
    if ( AddNormalinputs(mtx,mypk,funds+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,funds,cclibpk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,opret));
    }
    return("");
}

/*UniValue FaucetInfo()
{
    UniValue result(UniValue::VOBJ); char numstr[64];
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey faucetpk; struct CCcontract_info *cp,C; int64_t funding;
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","Faucet"));
    cp = CCinit(&C,EVAL_FAUCET);
    faucetpk = GetUnspendable(cp,0);
    funding = AddFaucetInputs(cp,mtx,faucetpk,0,0);
    sprintf(numstr,"%.8f",(double)funding/COIN);
    result.push_back(Pair("funding",numstr));
    return(result);
}*/

std::string CClib_rawtxgen(struct CCcontract_info *cp,uint8_t funcid,cJSON *params)
{
    CMutableTransaction tmpmtx,mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,cclibpk; int64_t funds,txfee=0,inputs,CCchange=0,nValue=FAUCET2SIZE; std::string rawhex; uint32_t j; int32_t i,len; uint8_t buf[32768]; bits256 hash;
    if ( txfee == 0 )
        txfee = 10000;
    if ( funcid == 'F' )
    {
        if ( cJSON_GetArraySize(params) > 0 )
        {
            funds = (int64_t)jdouble(jitem(params,0),0)*COIN + 0.0000000049;
            return(Faucet2Fund(cp,0,funds));
        } else return("");
    }
    else if ( funcid != 'G' )
        return("");
    cclibpk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    if ( (inputs= AddCClibInputs(cp,mtx,cclibpk,nValue+txfee,60)) > 0 )
    {
        if ( inputs > nValue )
            CCchange = (inputs - nValue - txfee);
        if ( CCchange != 0 )
            mtx.vout.push_back(MakeCC1vout(EVAL_FIRSTUSER,CCchange,cclibpk));
        mtx.vout.push_back(CTxOut(nValue,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        fprintf(stderr,"start at %u\n",(uint32_t)time(NULL));
        j = rand() & 0xfffffff;
        for (i=0; i<1000000; i++,j++)
        {
            tmpmtx = mtx;
            rawhex = FinalizeCCTx(-1LL,cp,tmpmtx,mypk,txfee,CScript() << OP_RETURN << E_MARSHAL(ss << (uint8_t)EVAL_FIRSTUSER << (uint8_t)'G' << j));
            if ( (len= (int32_t)rawhex.size()) > 0 && len < 65536 )
            {
                len >>= 1;
                decode_hex(buf,len,(char *)rawhex.c_str());
                hash = bits256_doublesha256(0,buf,len);
                if ( (hash.bytes[0] & 0xff) == 0 && (hash.bytes[31] & 0xff) == 0 )
                {
                    fprintf(stderr,"found valid txid after %d iterations %u\n",i,(uint32_t)time(NULL));
                    return(rawhex);
                }
                //fprintf(stderr,"%02x%02x ",hash.bytes[0],hash.bytes[31]);
            }
        }
        fprintf(stderr,"couldnt generate valid txid %u\n",(uint32_t)time(NULL));
        return("");
    } else fprintf(stderr,"cant find faucet inputs\n");
    return("");
}
