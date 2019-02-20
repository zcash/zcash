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


#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>
#include <secp256k1_musig.h>

#define MUSIG_PREVN 0   // for now, just use vout0 for the musig output
#define MUSIG_TXFEE 10000

CScript musig_sendopret(uint8_t funcid,secp256k1_pubkey combined_pk)
{
    CScript opret; uint8_t evalcode = EVAL_MUSIG;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << combined_pk);
    return(opret);
}

uint8_t musig_sendopretdecode(secp256k1_pubkey &combined_pk,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> combined_pk) != 0 && e == EVAL_MUSIG && f == 'x' )
    {
        return(f);
    }
    return(0);
}

CScript musig_spendopret(uint8_t funcid,secp256k1_pubkey combined_pk,secp256k1_schnorrsig musig)
{
    CScript opret; uint8_t evalcode = EVAL_MUSIG;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << combined_pk << musig);
    return(opret);
}

uint8_t musig_spendopretdecode(secp256k1_pubkey &combined_pk,secp256k1_schnorrsig &musig,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> combined_pk; ss >> musig) != 0 && e == EVAL_MUSIG && f == 'y' )
    {
        return(f);
    }
    return(0);
}

void musig_msghash(uint8_t *msg,uint256 prevhash,int32_t prevn,CTxOut vout,secp256k1_pubkey combined_pk)
{
    CScript data; uint256 hash; int32_t len = 0;
    data << E_MARSHAL(ss << prevhash << prevn << vout << combined_pk);
fprintf(stderr,"data size %d\n",(int32_t)data.size());
    vcalc_sha256(0,msg,data.data(),data.size());
}

int32_t musig_prevoutmsg(uint8_t *msg,uint256 sendtxid,CScript scriptPubKey)
{
    CTransaction vintx; uint256 hashBlock; int32_t numvouts; CTxOut vout; secp256k1_pubkey combined_pk;
    if ( myGetTransaction(sendtxid,vintx,hashBlock) != 0 && (numvouts= vintx.vout.size()) > 1 )
    {
        if ( musig_sendopretdecode(combined_pk,vintx.vout[numvouts-1].scriptPubKey) == 'x' )
        {
            vout.nValue = vintx.vout[MUSIG_PREVN].nValue - MUSIG_TXFEE;
            vout.scriptPubKey = scriptPubKey;
            return(musig_msghash(msg,sendtxid,MUSIG_PREVN,vout,combined_pk));
        }
    }
    return(zeroid);
}

UniValue musig_calcmsg(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); uint256 sendtxid; int32_t i; uint8_t msg[32]; char *scriptstr,str[65]; int32_t n;
    if ( (params= cclib_reparse(&n,params)) != 0 )
    {
        if ( n == 2 )
        {
            sendtxid = juint256(jitem(params,0));
            scriptstr = jstr(jitem(params,1),0);
            if ( is_hexstr(scriptstr,0) != 0 )
            {
                CScript scriptPubKey(ParseHex(scriptstr));
                musig_prevoutmsg(msg,sendtxid,scriptPubKey);
                result.push_back(Pair("result","success"));
                for (i=0; i<32; i++)
                    sprintf(&str[i<<1],"%02x",msg[i]);
                str[64] = 0;
                result.push_back(Pair("msg",str));
                return(result);
            } else return(cclib_error(result,"script is not hex"));
        } else return(cclib_error(result,"need exactly 2 parameters: sendtxid, scriptPubKey"));
    } else return(cclib_error(result,"couldnt parse params"));
}

UniValue musig_combine(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    return(result);
}

UniValue musig_session(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    return(result);
}

UniValue musig_commit(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    return(result);
}

UniValue musig_nonce(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    return(result);
}

UniValue musig_partialsign(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    return(result);
}

UniValue musig_sigcombine(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    return(result);
}

UniValue musig_verify(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    return(result);
}

// helpers for rpc calls that generate/validate onchain tx

UniValue musig_rawtxresult(UniValue &result,std::string rawtx)
{
    CTransaction tx;
    if ( rawtx.size() > 0 )
    {
        result.push_back(Pair("hex",rawtx));
        if ( DecodeHexTx(tx,rawtx) != 0 )
        {
            //if ( broadcastflag != 0 && myAddtomempool(tx) != 0 )
            //    RelayTransaction(tx);
            result.push_back(Pair("txid",tx.GetHash().ToString()));
            result.push_back(Pair("result","success"));
        } else result.push_back(Pair("error","decode hex"));
    } else result.push_back(Pair("error","couldnt finalize CCtx"));
    return(result);
}

UniValue musig_send(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); int32_t n; char *hexstr; std::string rawtx; int64_t amount; CPubKey musigpk,mypk;
    if ( txfee == 0 )
        txfee = MUSIG_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    musigpk = GetUnspendable(cp,0);
    if ( (params= cclib_reparse(&n,params)) != 0 )
    {
        if ( n == 2 && (hexstr= jstr(jitem(params,0),0)) != 0 && is_hexstr(hexstr,0) == 66 )
        {
            secp256k1_pubkey combined_pk(ParseHex(hexstr));
            amount = jdouble(jitem(params,1),0) * COIN + 0.0000000049;
            if ( amount >= 3*txfee && AddNormalinputs(mtx,mypk,amount+2*txfee,64) >= amount+2*txfee )
            {
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount+txfee,musigpk));
                rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,musig_sendopret('x',combined_pk));
                return(musig_rawtxresult(result,rawtx));
            } else return(cclib_error(result,"couldnt find funds or less than 0.0003"));
        } else return(cclib_error(result,"must have 2 params: combined_pk, amount"));
    } else return(cclib_error(result,"not enough parameters"));
}

UniValue musig_spend(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; CPubKey mypk; secp256k1_pubkey combined_pk; char *scriptstr,*musigstr; uint8_t msg[32]; CTransaction vintx; uint256 prevhash,hashBlock; int32_t n,numvouts; CTxOut vout;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( (params= cclib_reparse(&n,params)) != 0 )
    {
        if ( n == 3 )
        {
            prevhash = juint256(jitem(params,0));
            scriptstr = jstr(jitem(params,1),0);
            musigstr = jstr(jitem(params,2),0);
            if ( is_hexstr(scriptstr,0) != 0 && is_hexstr(musigstr,0) != 0 )
            {
                if ( txfee == 0 )
                    txfee = MUSIG_TXFEE;
                mypk = pubkey2pk(Mypubkey());
                secp256k1_schnorrsig musig(ParseHex(musigstr));
                CScript scriptPubKey(ParseHex(scriptstr));
                if ( myGetTransaction(prevhash,vintx,hashBlock) != 0 && (numvouts= vintx.vout.size()) > 1 )
                {
                    vout.nValue = vintx.vout[0].nValue - txfee;
                    vout.scriptPubKey = scriptPubKey;
                    if ( musig_sendopretdecode(combined_pk,vintx.vout[numvouts-1].scriptPubKey) == 'x' )
                    {
                        musig_prevoutmsg(msg,prevhash,vout.scriptPubKey);
                        if ( !secp256k1_schnorrsig_verify(ctx,&musig,msg,&combined_pk) )
                            return(cclib_error(result,"musig didnt validate"));
                        mtx.vin.push_back(CTxIn(prevhash,MUSIG_PREVN));
                        mtx.vout.push_back(vout);
                        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,musig_spendopret('y',combined_pk,musig));
                        return(musig_rawtxresult(result,rawtx));
                    } else return(cclib_error(result,"couldnt decode send opret"));
                } else return(cclib_error(result,"couldnt find vin0"));
            } else return(cclib_error(result,"script or musig is not hex"));
        } else return(cclib_error(result,"need to have exactly 3 params prevhash, scriptPubKey, musig"));
    } else return(cclib_error(result,"params parse error"));
}

bool musig_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    static secp256k1_context *ctx;
    secp256k1_pubkey combined_pk,checkpk; secp256k1_schnorrsig musig; uint256 hashBlock; CTransaction vintx; int32_t numvouts; uint8_t msg[32];
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( tx.vout.size() != 2 )
        return eval->Invalid("numvouts != 2");
    else if ( tx.vin.size() != 1 )
        return eval->Invalid("numvins != 1");
    else if ( IsCCInput(tx.vin[0].scriptSig) == 0 )
        return eval->Invalid("illegal normal vin0");
    else if ( myGetTransaction(tx.vin[0].prevout.hash,vintx,hashBlock) != 0 && (numvouts= vintx.vout.size()) > 1 )
    {
        if ( musig_sendopretdecode(combined_pk,vintx.vout[numvouts-1].scriptPubKey) == 'x' )
        {
            if ( musig_spendopretdecode(checkpk,musig,tx.vout[tx.vout.size()-1].scriptPubKey) == 'y' )
            {
                if ( combined_pk == checkpk )
                {
                    musig_prevoutmsg(msg,tx.vin[0].prevout.hash,tx.vout[0].scriptPubKey);
                    if ( !secp256k1_schnorrsig_verify(ctx,&musig,msg,&combined_pk) )
                        return eval->Invalid("failed schnorrsig_verify");
                    else return(true);
                } else return eval->Invalid("combined_pk didnt match send opret");
            } else return eval->Invalid("failed decode musig spendopret");
        } else return eval->Invalid("couldnt decode send opret");
    } else return eval->Invalid("couldnt find vin0 tx");
}
