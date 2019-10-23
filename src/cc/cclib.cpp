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
#define EVAL_FAUCET2 EVAL_FIRSTUSER

#ifdef BUILD_ROGUE
#define EVAL_ROGUE 17
std::string MYCCLIBNAME = (char *)"rogue";


#elif BUILD_CUSTOMCC
#include "customcc.h"

#elif BUILD_GAMESCC
#include "gamescc.h"

#else
#define EVAL_SUDOKU 17
#define EVAL_MUSIG 18
#define EVAL_DILITHIUM 19
std::string MYCCLIBNAME = (char *)"sudoku";
#endif

#ifndef BUILD_GAMESCC
void komodo_netevent(std::vector<uint8_t> payload) {}
#endif

extern std::string MYCCLIBNAME;

char *CClib_name() { return((char *)MYCCLIBNAME.c_str()); }

struct CClib_rpcinfo
{
    char *CCname,*method,*help;
    int32_t numrequiredargs,maxargs;
    uint8_t funcid,evalcode;
}

CClib_methods[] =
{
    { (char *)"faucet2", (char *)"fund", (char *)"amount", 1, 1, 'F', EVAL_FAUCET2 },
    { (char *)"faucet2", (char *)"get", (char *)"<no args>", 0, 0, 'G', EVAL_FAUCET2 },
#ifdef BUILD_ROGUE
    { (char *)"rogue", (char *)"newgame", (char *)"maxplayers buyin", 0, 2, 'G', EVAL_ROGUE },
    { (char *)"rogue", (char *)"gameinfo", (char *)"gametxid", 1, 1, 'T', EVAL_ROGUE },
    { (char *)"rogue", (char *)"pending", (char *)"<no args>", 0, 0, 'P', EVAL_ROGUE },
    { (char *)"rogue", (char *)"register", (char *)"gametxid [playertxid]", 1, 2, 'R', EVAL_ROGUE },
    { (char *)"rogue", (char *)"keystrokes", (char *)"gametxid keystrokes", 2, 2, 'K', EVAL_ROGUE },
    { (char *)"rogue", (char *)"bailout", (char *)"gametxid", 1, 1, 'Q', EVAL_ROGUE },
    { (char *)"rogue", (char *)"highlander", (char *)"gametxid", 1, 1, 'H', EVAL_ROGUE },
    { (char *)"rogue", (char *)"playerinfo", (char *)"playertxid", 1, 1, 'I', EVAL_ROGUE },
    { (char *)"rogue", (char *)"players", (char *)"<no args>", 0, 0, 'D', EVAL_ROGUE },
    { (char *)"rogue", (char *)"games", (char *)"<no args>", 0, 0, 'F', EVAL_ROGUE },
    { (char *)"rogue", (char *)"setname", (char *)"pname", 1, 1, 'N', EVAL_ROGUE },
    { (char *)"rogue", (char *)"extract", (char *)"gametxid [pubkey]", 1, 2, 'X', EVAL_ROGUE },
#elif BUILD_CUSTOMCC
    RPC_FUNCS
#elif BUILD_GAMESCC
    RPC_FUNCS
#else
    { (char *)"sudoku", (char *)"gen", (char *)"<no args>", 0, 0, 'G', EVAL_SUDOKU },
    { (char *)"sudoku", (char *)"txidinfo", (char *)"txid", 1, 1, 'T', EVAL_SUDOKU },
    { (char *)"sudoku", (char *)"pending", (char *)"<no args>", 0, 0, 'U', EVAL_SUDOKU },
    { (char *)"sudoku", (char *)"solution", (char *)"txid solution timestamps[81]", 83, 83, 'S', EVAL_SUDOKU },
    { (char *)"musig", (char *)"calcmsg", (char *)"sendtxid scriptPubKey", 2, 2, 'C', EVAL_MUSIG },
    { (char *)"musig", (char *)"combine", (char *)"pubkeys ...", 2, 999999999, 'P', EVAL_MUSIG },
    { (char *)"musig", (char *)"session", (char *)"myindex,numsigners,combined_pk,pkhash,msg32", 5, 5, 'R', EVAL_MUSIG },
    { (char *)"musig", (char *)"commit", (char *)"pkhash,ind,commitment", 3, 3, 'H', EVAL_MUSIG },
    { (char *)"musig", (char *)"nonce", (char *)"pkhash,ind,nonce", 3, 3, 'N', EVAL_MUSIG },
    { (char *)"musig", (char *)"partialsig", (char *)"pkhash,ind,partialsig", 3, 3, 'S', EVAL_MUSIG },
    { (char *)"musig", (char *)"verify", (char *)"msg sig pubkey", 3, 3, 'V', EVAL_MUSIG },
    { (char *)"musig", (char *)"send", (char *)"combined_pk amount", 2, 2, 'x', EVAL_MUSIG },
    { (char *)"musig", (char *)"spend", (char *)"sendtxid sig scriptPubKey", 3, 3, 'y', EVAL_MUSIG },
    { (char *)"dilithium", (char *)"keypair", (char *)"[hexseed]", 0, 1, 'K', EVAL_DILITHIUM },
    { (char *)"dilithium", (char *)"register", (char *)"handle, [hexseed]", 1, 2, 'R', EVAL_DILITHIUM },
    { (char *)"dilithium", (char *)"handleinfo", (char *)"handle", 1, 1, 'I', EVAL_DILITHIUM },
    { (char *)"dilithium", (char *)"sign", (char *)"msg [hexseed]", 1, 2, 'S', EVAL_DILITHIUM },
    { (char *)"dilithium", (char *)"verify", (char *)"pubtxid msg sig", 3, 3, 'V', EVAL_DILITHIUM },
    { (char *)"dilithium", (char *)"send", (char *)"handle pubtxid amount", 3, 3, 'x', EVAL_DILITHIUM },
    { (char *)"dilithium", (char *)"spend", (char *)"sendtxid scriptPubKey [hexseed]", 2, 3, 'y', EVAL_DILITHIUM },
    { (char *)"dilithium", (char *)"Qsend", (char *)"mypubtxid hexseed/'mypriv' destpubtxid,amount, ...", 4, 66, 'Q', EVAL_DILITHIUM },
#endif
};

std::string CClib_rawtxgen(struct CCcontract_info *cp,uint8_t funcid,cJSON *params);

#ifdef BUILD_ROGUE
int32_t rogue_replay(uint64_t seed,int32_t sleepmillis);
bool rogue_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx);

UniValue rogue_newgame(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_gameinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_keystrokes(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_bailout(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_highlander(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_playerinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_players(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_games(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_setname(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue rogue_extract(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);

#else
bool sudoku_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx);
UniValue sudoku_txidinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue sudoku_generate(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue sudoku_solution(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue sudoku_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);

bool musig_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx);
UniValue musig_calcmsg(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue musig_combine(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue musig_session(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue musig_commit(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue musig_nonce(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue musig_partialsig(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue musig_verify(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue musig_send(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue musig_spend(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);

bool dilithium_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx);
UniValue dilithium_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue dilithium_handleinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue dilithium_send(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue dilithium_spend(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue dilithium_keypair(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue dilithium_sign(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue dilithium_verify(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);
UniValue dilithium_Qsend(uint64_t txfee,struct CCcontract_info *cp,cJSON *params);

#endif

cJSON *cclib_reparse(int32_t *nump,char *jsonstr) // assumes origparams will be freed by caller
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
        //free(origparams);
    } else params = 0;
    return(params);
}

UniValue CClib_method(struct CCcontract_info *cp,char *method,char *jsonstr)
{
    UniValue result(UniValue::VOBJ); uint64_t txfee = 10000; int32_t m; cJSON *params = cclib_reparse(&m,jsonstr);
    fprintf(stderr,"method.(%s) -> (%s)\n",jsonstr!=0?jsonstr:"",params!=0?jprint(params,0):"");
#ifdef BUILD_ROGUE
    if ( cp->evalcode == EVAL_ROGUE )
    {
        if ( strcmp(method,"newgame") == 0 )
            return(rogue_newgame(txfee,cp,params));
        else if ( strcmp(method,"pending") == 0 )
            return(rogue_pending(txfee,cp,params));
        else if ( strcmp(method,"gameinfo") == 0 )
            return(rogue_gameinfo(txfee,cp,params));
        else if ( strcmp(method,"register") == 0 )
            return(rogue_register(txfee,cp,params));
        else if ( strcmp(method,"keystrokes") == 0 )
            return(rogue_keystrokes(txfee,cp,params));
        else if ( strcmp(method,"bailout") == 0 )
            return(rogue_bailout(txfee,cp,params));
        else if ( strcmp(method,"highlander") == 0 )
            return(rogue_highlander(txfee,cp,params));
        else if ( strcmp(method,"extract") == 0 )
            return(rogue_extract(txfee,cp,params));
        else if ( strcmp(method,"playerinfo") == 0 )
            return(rogue_playerinfo(txfee,cp,params));
        else if ( strcmp(method,"players") == 0 )
            return(rogue_players(txfee,cp,params));
        else if ( strcmp(method,"games") == 0 )
            return(rogue_games(txfee,cp,params));
        else if ( strcmp(method,"setname") == 0 )
            return(rogue_setname(txfee,cp,params));
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","invalid rogue method"));
            result.push_back(Pair("method",method));
            return(result);
        }
    }
#elif BUILD_CUSTOMCC
    CUSTOM_DISPATCH
#elif BUILD_GAMESCC
    CUSTOM_DISPATCH
#else
    if ( cp->evalcode == EVAL_SUDOKU )
    {
        //printf("CClib_method params.%p\n",params);
        if ( strcmp(method,"txidinfo") == 0 )
            return(sudoku_txidinfo(txfee,cp,params));
        else if ( strcmp(method,"gen") == 0 )
            return(sudoku_generate(txfee,cp,params));
        else if ( strcmp(method,"solution") == 0 )
            return(sudoku_solution(txfee,cp,params));
        else if ( strcmp(method,"pending") == 0 )
            return(sudoku_pending(txfee,cp,params));
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","invalid sudoku method"));
            result.push_back(Pair("method",method));
            return(result);
        }
    }
    else if ( cp->evalcode == EVAL_MUSIG )
    {
        //printf("CClib_method params.%p\n",params);
        if ( strcmp(method,"combine") == 0 )
            return(musig_combine(txfee,cp,params));
        else if ( strcmp(method,"calcmsg") == 0 )
            return(musig_calcmsg(txfee,cp,params));
        else if ( strcmp(method,"session") == 0 )
            return(musig_session(txfee,cp,params));
        else if ( strcmp(method,"commit") == 0 )
            return(musig_commit(txfee,cp,params));
        else if ( strcmp(method,"nonce") == 0 ) // returns combined nonce if ready
            return(musig_nonce(txfee,cp,params));
        else if ( strcmp(method,"partialsig") == 0 )
            return(musig_partialsig(txfee,cp,params));
        else if ( strcmp(method,"verify") == 0 )
            return(musig_verify(txfee,cp,params));
        else if ( strcmp(method,"send") == 0 )
            return(musig_send(txfee,cp,params));
        else if ( strcmp(method,"spend") == 0 )
            return(musig_spend(txfee,cp,params));
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","invalid musig method"));
            result.push_back(Pair("method",method));
            return(result);
        }
    }
    else if ( cp->evalcode == EVAL_DILITHIUM )
    {
        if ( strcmp(method,"Qsend") == 0 )
            return(dilithium_Qsend(txfee,cp,params));
        else if ( strcmp(method,"send") == 0 )
            return(dilithium_send(txfee,cp,params));
        else if ( strcmp(method,"spend") == 0 )
            return(dilithium_spend(txfee,cp,params));
        else if ( strcmp(method,"keypair") == 0 )
            return(dilithium_keypair(txfee,cp,params));
        else if ( strcmp(method,"register") == 0 )
            return(dilithium_register(txfee,cp,params));
        else if ( strcmp(method,"handleinfo") == 0 )
            return(dilithium_handleinfo(txfee,cp,params));
        else if ( strcmp(method,"sign") == 0 )
            return(dilithium_sign(txfee,cp,params));
        else if ( strcmp(method,"verify") == 0 )
            return(dilithium_verify(txfee,cp,params));
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","invalid dilithium method"));
            result.push_back(Pair("method",method));
            return(result);
        }
    }
#endif
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","only sudoku supported for now"));
        result.push_back(Pair("evalcode",(int)cp->evalcode));
        return(result);
    }
}

UniValue CClib_info(struct CCcontract_info *cp)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int32_t i; char str[2];
    result.push_back(Pair("result","success"));
    result.push_back(Pair("CClib",CClib_name()));
    for (i=0; i<sizeof(CClib_methods)/sizeof(*CClib_methods); i++)
    {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("evalcode",CClib_methods[i].evalcode));
        if ( CClib_methods[i].funcid < ' ' || CClib_methods[i].funcid >= 128 )
            obj.push_back(Pair("funcid",CClib_methods[i].funcid));
        else
        {
            str[0] = CClib_methods[i].funcid;
            str[1] = 0;
            obj.push_back(Pair("funcid",str));
        }
        obj.push_back(Pair("name",CClib_methods[i].CCname));
        obj.push_back(Pair("method",CClib_methods[i].method));
        obj.push_back(Pair("help",CClib_methods[i].help));
        obj.push_back(Pair("params_required",CClib_methods[i].numrequiredargs));
        obj.push_back(Pair("params_max",CClib_methods[i].maxargs));
        a.push_back(obj);
    }
    result.push_back(Pair("methods",a));
    return(result);
}

UniValue CClib(struct CCcontract_info *cp,char *method,char *jsonstr)
{
    UniValue result(UniValue::VOBJ); int32_t i; std::string rawtx; cJSON *params;
//printf("CClib params.(%s)\n",jsonstr!=0?jsonstr:"");
    for (i=0; i<sizeof(CClib_methods)/sizeof(*CClib_methods); i++)
    {
        if ( cp->evalcode == CClib_methods[i].evalcode && strcmp(method,CClib_methods[i].method) == 0 )
        {
            if ( cp->evalcode == EVAL_FAUCET2 )
            {
                result.push_back(Pair("result","success"));
                result.push_back(Pair("method",CClib_methods[i].method));
                params = cJSON_Parse(jsonstr);
                rawtx = CClib_rawtxgen(cp,CClib_methods[i].funcid,params);
                free_json(params);
                result.push_back(Pair("rawtx",rawtx));
                return(result);
            } else return(CClib_method(cp,method,jsonstr));
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("method",method));
    result.push_back(Pair("error","method not found"));
    return(result);
}

int64_t IsCClibvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v,char *cmpaddr)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cmpaddr) == 0 )
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
                if ( (assetoshis= IsCClibvout(cp,vinTx,tx.vin[i].prevout.n,cp->unspendableCCaddr)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsCClibvout(cp,tx,i,cp->unspendableCCaddr)) != 0 )
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
    if ( cp->evalcode != EVAL_FAUCET2 )
    {
#ifdef BUILD_ROGUE
        return(rogue_validate(cp,height,eval,tx));
#elif BUILD_CUSTOMCC
        return(custom_validate(cp,height,eval,tx));
#elif BUILD_GAMESCC
        return(games_validate(cp,height,eval,tx));
#else
        if ( cp->evalcode == EVAL_SUDOKU )
            return(sudoku_validate(cp,height,eval,tx));
        else if ( cp->evalcode == EVAL_MUSIG )
            return(musig_validate(cp,height,eval,tx));
        else if ( cp->evalcode == EVAL_DILITHIUM )
            return(dilithium_validate(cp,height,eval,tx));
        else return eval->Invalid("invalid evalcode");
#endif
    }
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
            if ( IsCClibvout(cp,tx,0,cp->unspendableCCaddr) != 0 )
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
            SetCCtxids(txids,destaddr,tx.vout[i].scriptPubKey.IsPayToCryptoCondition());
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

int64_t AddCClibInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs,char *cmpaddr,int32_t CCflag)
{
    char coinaddr[64]; int64_t threshold,nValue,price,totalinputs = 0,txfee = 10000; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr,CCflag!=0?true:false);
    if ( maxinputs > CC_MAXVINS )
        maxinputs = CC_MAXVINS;
    if ( maxinputs != 0 )
        threshold = total/maxinputs;
    else threshold = total;
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f vs %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN,(double)threshold/COIN);
        if ( it->second.satoshis < threshold || it->second.satoshis == txfee )
            continue;
        // no need to prevent dup
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if ( (nValue= IsCClibvout(cp,vintx,vout,cmpaddr)) >= 1000000 && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
            {
                if ( total != 0 && maxinputs != 0 )
                    mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                nValue = it->second.satoshis;
                totalinputs += nValue;
                n++;
                if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                    break;
            } //else fprintf(stderr,"nValue %.8f too small or already spent in mempool\n",(double)nValue/COIN);
        } else fprintf(stderr,"couldnt get tx\n");
    }
    return(totalinputs);
}

int64_t AddCClibtxfee(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk)
{
    char coinaddr[64]; int64_t nValue,txfee = 10000; uint256 txid,hashBlock; CTransaction vintx; int32_t vout;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f vs %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN,(double)threshold/COIN);
        if ( it->second.satoshis < txfee )
            continue;
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if ( (nValue= IsCClibvout(cp,vintx,vout,coinaddr)) != 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
            {
                mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                return(it->second.satoshis);
            } //else fprintf(stderr,"nValue %.8f too small or already spent in mempool\n",(double)nValue/COIN);
        } else fprintf(stderr,"couldnt get tx\n");
    }
    return(0);
}

std::string Faucet2Fund(struct CCcontract_info *cp,uint64_t txfee,int64_t funds)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,cclibpk; CScript opret;
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    cclibpk = GetUnspendable(cp,0);
    if ( AddNormalinputs2(mtx,funds+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,funds,cclibpk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,opret));
    }
    return("");
}

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
    if ( (inputs= AddCClibInputs(cp,mtx,cclibpk,nValue+txfee,60,cp->unspendableCCaddr,1)) > 0 )
    {
        if ( inputs > nValue )
            CCchange = (inputs - nValue - txfee);
        if ( CCchange != 0 )
            mtx.vout.push_back(MakeCC1vout(EVAL_FAUCET2,CCchange,cclibpk));
        mtx.vout.push_back(CTxOut(nValue,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        fprintf(stderr,"start at %u\n",(uint32_t)time(NULL));
        j = rand() & 0xfffffff;
        for (i=0; i<1000000; i++,j++)
        {
            tmpmtx = mtx;
            rawhex = FinalizeCCTx(-1LL,cp,tmpmtx,mypk,txfee,CScript() << OP_RETURN << E_MARSHAL(ss << (uint8_t)EVAL_FAUCET2 << (uint8_t)'G' << j));
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

UniValue cclib_error(UniValue &result,const char *errorstr)
{
    result.push_back(Pair("status","error"));
    result.push_back(Pair("error",errorstr));
    return(result);
}

uint256 juint256(cJSON *obj)
{
    uint256 tmp; bits256 t = jbits256(obj,0);
    memcpy(&tmp,&t,sizeof(tmp));
    return(revuint256(tmp));
}

int32_t cclib_parsehash(uint8_t *hash32,cJSON *item,int32_t len)
{
    char *hexstr;
    if ( (hexstr= jstr(item,0)) != 0 && is_hexstr(hexstr,0) == len*2 )
    {
        decode_hex(hash32,len,hexstr);
        return(0);
    } else return(-1);
}

#ifdef BUILD_ROGUE
#include "rogue_rpc.cpp"
#include "rogue/cursesd.c"
#include "rogue/vers.c"
#include "rogue/extern.c"
#include "rogue/armor.c"
#include "rogue/chase.c"
#include "rogue/command.c"
#include "rogue/daemon.c"
#include "rogue/daemons.c"
#include "rogue/fight.c"
#include "rogue/init.c"
#include "rogue/io.c"
#include "rogue/list.c"
#include "rogue/mach_dep.c"
#include "rogue/rogue.c"
#include "rogue/xcrypt.c"
#include "rogue/mdport.c"
#include "rogue/misc.c"
#include "rogue/monsters.c"
#include "rogue/move.c"
#include "rogue/new_level.c"
#include "rogue/options.c"
#include "rogue/pack.c"
#include "rogue/passages.c"
#include "rogue/potions.c"
#include "rogue/rings.c"
#include "rogue/rip.c"
#include "rogue/rooms.c" 
#include "rogue/save.c"
#include "rogue/scrolls.c"
#include "rogue/state.c"
#include "rogue/sticks.c"
#include "rogue/things.c"
#include "rogue/weapons.c"
#include "rogue/wizard.c"

#elif BUILD_CUSTOMCC
#include "customcc.cpp"

#elif BUILD_GAMESCC
#include "rogue/cursesd.c"
#include "gamescc.cpp"

#else
#include "sudoku.cpp"
#include "musig.cpp"
#include "dilithium.c"
//#include "../secp256k1/src/modules/musig/example.c"
#endif

