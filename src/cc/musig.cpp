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

/* first make a combined pk:

./komodo-cli -ac_name=MUSIG cclib combine 18 '["02fb6aa0b96cad24d46b5da93eba3864c45ce07a73bba12da530ae841e140fcf28","0255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4"]'
{
  "pkhash": "5cb5a225064ca6ffc1438cb2a6ac2ac65fe2d5055dc7f6c7ebffb9a231f8912b",
  "combined_pk": "03f016c348437c7422eed92d865aa9789614f75327cada463eefc566126b54785b",
  "result": "success"
}

 the combined_pk and pkhash will be needed for various other rpc calls

 second, send 1 coin to the combined_pk
 ./komodo-cli -ac_name=MUSIG  cclib send 18 '["03f016c348437c7422eed92d865aa9789614f75327cada463eefc566126b54785b",1]'
 {
   "hex": "0400008085202f8901a980664dffc810725a79ffb89ac48be4c7b6bade9b789732fcf871acf8e81a2e010000006a47304402207e52763661ecd2c34a65d6623950be11794825db71576dc11894c606ddc317800220028fef46dc20630d0fdf22647b5d4ff0f1c47cf75f48702d0a91d5589eff99d001210255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4ffffffff031008f60500000000302ea22c8020c71ddb3aac7f9b9e4bdacf032aaa8b8e4433c4ff9f8a43cebb9c1f5da96928a48103120c008203000401cce09aa4350000000023210255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4ac0000000000000000266a2412782103f016c348437c7422eed92d865aa9789614f75327cada463eefc566126b54785b00000000920500000000000000000000000000",
   "txid": "5ce74037a153ee210413b48d4e88638b99825a2de1a1f1aa0d36ebf93019824c",
   "result": "success"
 }
 
 sendrawtransaction of the above hex.
 ./komodo-cli -ac_name=MUSIG getrawtransaction 5ce74037a153ee210413b48d4e88638b99825a2de1a1f1aa0d36ebf93019824c 1
 "vout": [
    {
      "value": 1.00010000,
      "valueSat": 100010000,
      "n": 0,
      "scriptPubKey": {
        "asm": "a22c8020c71ddb3aac7f9b9e4bdacf032aaa8b8e4433c4ff9f8a43cebb9c1f5da96928a48103120c008203000401 OP_CHECKCRYPTOCONDITION",
        "hex": "2ea22c8020c71ddb3aac7f9b9e4bdacf032aaa8b8e4433c4ff9f8a43cebb9c1f5da96928a48103120c008203000401cc",
        "reqSigs": 1,
        "type": "cryptocondition",
        "addresses": [
          "RKWS7jxyjPX9iaJttk8iMKf1AumanKypez"
        ]
      }
    },
    {
      "value": 8.99980000,
      "valueSat": 899980000,
      "n": 1,
      "scriptPubKey": {
        "asm": "0255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4 OP_CHECKSIG",
        "hex": "210255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4ac",
        "reqSigs": 1,
        "type": "pubkey",
        "addresses": [
          "RVQjvGdRbYLJ49bfH4SAFseipvwE3UdoDw"
        ]
      }
 
 script: 210255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4ac
 
 sendtxid: 5ce74037a153ee210413b48d4e88638b99825a2de1a1f1aa0d36ebf93019824c
 
  get the msg we need to sign:
  
 ./komodo-cli -ac_name=MUSIG  cclib calcmsg 18 '["5ce74037a153ee210413b48d4e88638b99825a2de1a1f1aa0d36ebf93019824c","210255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4ac"]'
 
 {
  "msg": "f7fb85d1412814e3c2f98b990802af6ee33dad368c6ba05c2050e9e5506fcd75",
  "result": "success"
 }
 
the "msg" is what needs to be signed to create a valid spend
 
 now on each signing node, a session needs to be created:
 5 args: ind, numsigners, combined_pk, pkhash, message to be signed
 
 on node with pubkey: 02fb6aa0b96cad24d46b5da93eba3864c45ce07a73bba12da530ae841e140fcf28
 ./komodo-cli -ac_name=MUSIG cclib session 18 '[0,2,"03f016c348437c7422eed92d865aa9789614f75327cada463eefc566126b54785b","5cb5a225064ca6ffc1438cb2a6ac2ac65fe2d5055dc7f6c7ebffb9a231f8912b","f7fb85d1412814e3c2f98b990802af6ee33dad368c6ba05c2050e9e5506fcd75"]'
 {
  "myind": 0,
  "numsigners": 2,
  "commitment": "bbea1f2562eca01b9a1393c5dc188bdd44551aebf684f4459930f59dde01f7ae",
  "result": "success"
 }

 on node with pubkey: 0255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4
 ./komodo-cli -ac_name=MUSIG cclib session 18 '[1,2,"03f016c348437c7422eed92d865aa9789614f75327cada463eefc566126b54785b","5cb5a225064ca6ffc1438cb2a6ac2ac65fe2d5055dc7f6c7ebffb9a231f8912b","f7fb85d1412814e3c2f98b990802af6ee33dad368c6ba05c2050e9e5506fcd75"]'
 {
   "myind": 1,
   "numsigners": 2,
   "commitment": "c2291acb747a75b1a40014d8eb0cc90a1360f74d413f65f78e20a7de45eda851",
   "result": "success"
 }
  
 now we need to get the commitment from each node to the other one. the session already put the commitment for each node into the global struct. Keep in mind there is a single global struct with session unique to each cclib session call. that means no restarting any deamon in the middle of the process on any of the nodes and only call cclib session a single time. this is an artificial restriction just to simplify the initial implementation of musig
 ./komodo-cli -ac_name=MUSIG cclib commit 18 '["5cb5a225064ca6ffc1438cb2a6ac2ac65fe2d5055dc7f6c7ebffb9a231f8912b","1","c2291acb747a75b1a40014d8eb0cc90a1360f74d413f65f78e20a7de45eda851"]'
 {
  "added_index": 1,
  "myind": 0,
  "nonce": "02fec7a9310c959a0a97b86bc3f8c30d392d1fb51793915898c568f73f1f70476b",
  "result": "success"
 }
 
 ./komodo-cli -ac_name=MUSIG  cclib commit 18 '["5cb5a225064ca6ffc1438cb2a6ac2ac65fe2d5055dc7f6c7ebffb9a231f8912b",0,"d242cff13fa8c9b83248e4219fda459ada146b885f2171481f1b0f66c66d94ad"]'
 {
   "added_index": 0,
   "myind": 1,
   "nonce": "039365deaaaea089d509ba4c9f846de2baf4aa04cf6b26fa2c1cd818553e47f80c",
   "result": "success"
 }

 Now exchange the revealed nonces to each node:
 ./komodo-cli -ac_name=MUSIG cclib nonce 18 '["5cb5a225064ca6ffc1438cb2a6ac2ac65fe2d5055dc7f6c7ebffb9a231f8912b","1","039365deaaaea089d509ba4c9f846de2baf4aa04cf6b26fa2c1cd818553e47f80c"]'
{
  "added_index": 1,
  "myind": 0,
  "partialsig": "1d65c09cd9bffe4f0604227e66cd7cd221480bbb08262fe885563a9df7cf8f5b",
  "result": "success"
}

./komodo-cli -ac_name=MUSIG  cclib nonce 18 '["5cb5a225064ca6ffc1438cb2a6ac2ac65fe2d5055dc7f6c7ebffb9a231f8912b",0,"02fec7a9310c959a0a97b86bc3f8c30d392d1fb51793915898c568f73f1f70476b"]'
{
  "added_index": 0,
  "myind": 1,
  "partialsig": "4a3795e6801b355102c617390cf5a462061e082e35dc2ed8f8b1fab54cc0769e",
  "result": "success"
}
 
 Almost there! final step is to exchange the partial sigs between signers
 ./komodo-cli -ac_name=MUSIG cclib partialsig 18 '["5cb5a225064ca6ffc1438cb2a6ac2ac65fe2d5055dc7f6c7ebffb9a231f8912b","1","4a3795e6801b355102c617390cf5a462061e082e35dc2ed8f8b1fab54cc0769e"]'
 {
   "added_index": 1,
   "result": "success",
   "combinedsig": "a76f2790747ed2436a281f2660bdbee21bad9ee130b9cab6e542fa618fba1512679d568359db33a008ca39b773c32134276613e93e025ec17e083553449005f9"
 }
 
 ./komodo-cli -ac_name=MUSIG  cclib partialsig 18 '["5cb5a225064ca6ffc1438cb2a6ac2ac65fe2d5055dc7f6c7ebffb9a231f8912b",0,"1d65c09cd9bffe4f0604227e66cd7cd221480bbb08262fe885563a9df7cf8f5b"]'
 {
   "added_index": 0,
   "result": "success",
   "combinedsig": "a76f2790747ed2436a281f2660bdbee21bad9ee130b9cab6e542fa618fba1512679d568359db33a008ca39b773c32134276613e93e025ec17e083553449005f9"
 }

 Notice both nodes generated the same combined signature!
 
 Now for a sanity test, we can use the verify call to make sure this sig will work with the msg needed for the spend:
 
 ./komodo-cli -ac_name=MUSIG cclib verify 18 '["f7fb85d1412814e3c2f98b990802af6ee33dad368c6ba05c2050e9e5506fcd75","03f016c348437c7422eed92d865aa9789614f75327cada463eefc566126b54785b","a76f2790747ed2436a281f2660bdbee21bad9ee130b9cab6e542fa618fba1512679d568359db33a008ca39b773c32134276613e93e025ec17e083553449005f9"]'
 {
   "msg": "f7fb85d1412814e3c2f98b990802af6ee33dad368c6ba05c2050e9e5506fcd75",
   "combined_pk": "03f016c348437c7422eed92d865aa9789614f75327cada463eefc566126b54785b",
   "combinedsig": "a76f2790747ed2436a281f2660bdbee21bad9ee130b9cab6e542fa618fba1512679d568359db33a008ca39b773c32134276613e93e025ec17e083553449005f9",
   "result": "success"
 }
 
 and finally the spend: sendtxid, scriptPubKey, musig
 
 ./komodo-cli -ac_name=MUSIG cclib spend 18 '["5ce74037a153ee210413b48d4e88638b99825a2de1a1f1aa0d36ebf93019824c","210255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4ac","a76f2790747ed2436a281f2660bdbee21bad9ee130b9cab6e542fa618fba1512679d568359db33a008ca39b773c32134276613e93e025ec17e083553449005f9"]'
{
  "scriptpubkey": "210255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4ac",
  "msg": "f7fb85d1412814e3c2f98b990802af6ee33dad368c6ba05c2050e9e5506fcd75",
  "combined_pk": "03f016c348437c7422eed92d865aa9789614f75327cada463eefc566126b54785b",
  "combinedsig": "a76f2790747ed2436a281f2660bdbee21bad9ee130b9cab6e542fa618fba1512679d568359db33a008ca39b773c32134276613e93e025ec17e083553449005f9",
  "hex": "0400008085202f89014c821930f9eb360daaf1a1e12d5a82998b63884e8db4130421ee53a13740e75c000000007b4c79a276a072a26ba067a5658021032d29d6545a2aafad795d9cf50912ecade549137
163934dfb2895ebc0e211ce8a81409671a60db89b3bc58966f3acc80194479b1a43d868e95a11ebc5609646d18710341a8ff92a7817571980307f5d660cc00a2735ac6333e0a7191243f1263f1959a100af03800112
a10001ffffffff0200e1f5050000000023210255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4ac0000000000000000686a4c6512792103f016c348437c7422eed92d865aa9789614f
75327cada463eefc566126b54785b40a76f2790747ed2436a281f2660bdbee21bad9ee130b9cab6e542fa618fba1512679d568359db33a008ca39b773c32134276613e93e025ec17e083553449005f900000000a805
00000000000000000000000000",
  "txid": "910635bf69a047fc90567a83ff12e47b753f470658b6d0855ec96e07e7349a8a",
  "result": "success"
} 
*/


#define USE_BASIC_CONFIG
#define ENABLE_MODULE_MUSIG
#include "../secp256k1/src/basic-config.h"
#include "../secp256k1/include/secp256k1.h"
#include "../secp256k1/src/ecmult.h"
#include "../secp256k1/src/ecmult_gen.h"

typedef struct { unsigned char data[64]; } secp256k1_schnorrsig;
struct secp256k1_context_struct {
    secp256k1_ecmult_context ecmult_ctx;
    secp256k1_ecmult_gen_context ecmult_gen_ctx;
    secp256k1_callback illegal_callback;
    secp256k1_callback error_callback;
};


//#include "../secp256k1/include/secp256k1.h"
//#include "../secp256k1/include/secp256k1_schnorrsig.h"
#include "../secp256k1/include/secp256k1_musig.h"


extern "C" int secp256k1_ecmult_multi_var(const secp256k1_ecmult_context *ctx, secp256k1_scratch *scratch, secp256k1_gej *r, const secp256k1_scalar *inp_g_sc, secp256k1_ecmult_multi_callback cb, void *cbdata, size_t n);
extern "C" int secp256k1_schnorrsig_verify(const secp256k1_context* ctx, const secp256k1_schnorrsig *sig, const unsigned char *msg32, const secp256k1_pubkey *pk);
extern "C" int secp256k1_schnorrsig_parse(const secp256k1_context* ctx, secp256k1_schnorrsig* sig, const unsigned char *in64);
extern "C" int secp256k1_musig_pubkey_combine(const secp256k1_context* ctx, secp256k1_scratch_space *scratch, secp256k1_pubkey *combined_pk, unsigned char *pk_hash32, const secp256k1_pubkey *pubkeys, size_t n_pubkeys);
extern "C" int secp256k1_musig_session_initialize(const secp256k1_context* ctx, secp256k1_musig_session *session, secp256k1_musig_session_signer_data *signers, unsigned char *nonce_commitment32, const unsigned char *session_id32, const unsigned char *msg32, const secp256k1_pubkey *combined_pk, const unsigned char *pk_hash32, size_t n_signers, size_t my_index, const unsigned char *seckey);
extern "C" int secp256k1_schnorrsig_serialize(const secp256k1_context* ctx, unsigned char *out64, const secp256k1_schnorrsig* sig);

#define MUSIG_PREVN 0   // for now, just use vout0 for the musig output
#define MUSIG_TXFEE 10000

struct musig_info
{
    secp256k1_musig_session session;
    secp256k1_pubkey combined_pk;
    uint8_t *nonce_commitments,**commitment_ptrs; // 32*N_SIGNERS
    secp256k1_musig_session_signer_data *signer_data; //[N_SIGNERS];
    secp256k1_pubkey *nonces; //[N_SIGNERS];
    secp256k1_musig_partial_signature *partial_sig; //[N_SIGNERS];
    int32_t myind,num,numcommits,numnonces,numpartials;
    uint8_t msg[32],pkhash[32],combpk[33];
};

std::vector <struct musig_info *> MUSIG;

struct musig_info *musig_infocreate(int32_t myind,int32_t num)
{
    int32_t i; struct musig_info *mp = (struct musig_info *)calloc(1,sizeof(*mp));
    mp->myind = myind, mp->num = num;
    mp->nonce_commitments = (uint8_t *)calloc(num,32);
    mp->commitment_ptrs = (uint8_t **)calloc(num,sizeof(*mp->commitment_ptrs));
    for (i=0; i<num; i++)
        mp->commitment_ptrs[i] = &mp->nonce_commitments[i*32];
    mp->signer_data = (secp256k1_musig_session_signer_data *)calloc(num,sizeof(*mp->signer_data));
    mp->nonces = (secp256k1_pubkey *)calloc(num,sizeof(*mp->nonces));
    mp->partial_sig = (secp256k1_musig_partial_signature *)calloc(num,sizeof(*mp->partial_sig));
    return(mp);
}

void musig_infofree(struct musig_info *mp)
{
    if ( mp->partial_sig != 0 )
    {
        GetRandBytes((uint8_t *)mp->partial_sig,mp->num*sizeof(*mp->partial_sig));
        free(mp->partial_sig);
    }
    if ( mp->nonces != 0 )
    {
        GetRandBytes((uint8_t *)mp->nonces,mp->num*sizeof(*mp->nonces));
        free(mp->nonces);
    }
    if ( mp->signer_data != 0 )
    {
        GetRandBytes((uint8_t *)mp->signer_data,mp->num*sizeof(*mp->signer_data));
        free(mp->signer_data);
    }
    if ( mp->nonce_commitments != 0 )
    {
        GetRandBytes((uint8_t *)mp->nonce_commitments,mp->num*32);
        free(mp->nonce_commitments);
    }
    if ( mp->commitment_ptrs != 0 )
    {
        GetRandBytes((uint8_t *)mp->commitment_ptrs,mp->num*sizeof(*mp->commitment_ptrs));
        free(mp->commitment_ptrs);
    }
    GetRandBytes((uint8_t *)mp,sizeof(*mp));
    free(mp);
}

CScript musig_sendopret(uint8_t funcid,CPubKey pk)
{
    CScript opret; uint8_t evalcode = EVAL_MUSIG;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << pk);
    return(opret);
}

uint8_t musig_sendopretdecode(CPubKey &pk,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> pk) != 0 && e == EVAL_MUSIG && f == 'x' )
    {
        return(f);
    }
    return(0);
}

CScript musig_spendopret(uint8_t funcid,CPubKey pk,std::vector<uint8_t> musig64)
{
    CScript opret; uint8_t evalcode = EVAL_MUSIG;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << pk << musig64);
    return(opret);
}

uint8_t musig_spendopretdecode(CPubKey &pk,std::vector<uint8_t> &musig64,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> pk; ss >> musig64) != 0 && e == EVAL_MUSIG && f == 'y' )
    {
        return(f);
    }
    return(0);
}

int32_t musig_parsepubkey(secp256k1_context *ctx,secp256k1_pubkey &spk,cJSON *item)
{
    char *hexstr;
    if ( (hexstr= jstr(item,0)) != 0 && is_hexstr(hexstr,0) == 66 )
    {
        CPubKey pk(ParseHex(hexstr));
        if ( secp256k1_ec_pubkey_parse(ctx,&spk,pk.begin(),33) > 0 )
            return(1);
    } else return(-1);
}

int32_t musig_msghash(uint8_t *msg,uint256 prevhash,int32_t prevn,CTxOut vout,CPubKey pk)
{
    CScript data; uint256 hash; int32_t len = 0;
    data << E_MARSHAL(ss << prevhash << prevn << vout << pk);
    hash = Hash(data.begin(),data.end());
    memcpy(msg,&hash,sizeof(hash));
    return(0);
}

int32_t musig_prevoutmsg(uint8_t *msg,uint256 sendtxid,CScript scriptPubKey)
{
    CTransaction vintx; uint256 hashBlock; int32_t numvouts; CTxOut vout; CPubKey pk;
    memset(msg,0,32);
    if ( myGetTransaction(sendtxid,vintx,hashBlock) != 0 && (numvouts= vintx.vout.size()) > 1 )
    {
        if ( musig_sendopretdecode(pk,vintx.vout[numvouts-1].scriptPubKey) == 'x' )
        {
            vout.nValue = vintx.vout[MUSIG_PREVN].nValue - MUSIG_TXFEE;
            vout.scriptPubKey = scriptPubKey;
            return(musig_msghash(msg,sendtxid,MUSIG_PREVN,vout,pk));
        }
    }
    return(-1);
}

UniValue musig_calcmsg(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); uint256 sendtxid; int32_t i,zeros=0; uint8_t msg[32]; char *scriptstr,str[65]; int32_t n;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n == 2 )
        {
            sendtxid = juint256(jitem(params,0));
            scriptstr = jstr(jitem(params,1),0);
            if ( is_hexstr(scriptstr,0) != 0 )
            {
                CScript scriptPubKey;
                scriptPubKey.resize(strlen(scriptstr)/2);
                decode_hex(&scriptPubKey[0],strlen(scriptstr)/2,scriptstr);
                musig_prevoutmsg(msg,sendtxid,scriptPubKey);
                for (i=0; i<32; i++)
                {
                    sprintf(&str[i<<1],"%02x",msg[i]);
                    if ( msg[i] == 0 )
                        zeros++;
                }
                str[64] = 0;
                if ( zeros != 32 )
                {    
                    result.push_back(Pair("msg",str));
                    result.push_back(Pair("result","success"));
                    return(result);
                } else return(cclib_error(result,"null result, make sure params are sendtxid, scriptPubKey"));
            } else return(cclib_error(result,"script is not hex"));
        } else return(cclib_error(result,"need exactly 2 parameters: sendtxid, scriptPubKey"));
    } else return(cclib_error(result,"couldnt parse params"));
}

UniValue musig_combine(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    size_t clen = CPubKey::PUBLIC_KEY_SIZE;
    UniValue result(UniValue::VOBJ); CPubKey pk; int32_t i,n; uint8_t pkhash[32]; char *hexstr,str[67]; secp256k1_pubkey combined_pk,spk; std::vector<secp256k1_pubkey> pubkeys;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        //fprintf(stderr,"n.%d args.(%s)\n",n,jprint(params,0));
        for (i=0; i<n; i++)
        {
            if ( musig_parsepubkey(ctx,spk,jitem(params,i)) < 0 )
                return(cclib_error(result,"error parsing pk"));
            pubkeys.push_back(spk);
        }
        if ( secp256k1_musig_pubkey_combine(ctx,NULL,&combined_pk,pkhash,&pubkeys[0],n) > 0 )
        {
            if ( secp256k1_ec_pubkey_serialize(ctx,(uint8_t *)pk.begin(),&clen,&combined_pk,SECP256K1_EC_COMPRESSED) > 0 && clen == 33 )
            {
                for (i=0; i<32; i++)
                    sprintf(&str[i<<1],"%02x",pkhash[i]);
                str[64] = 0;
                result.push_back(Pair("pkhash",str));
                
                for (i=0; i<33; i++)
                    sprintf(&str[i<<1],"%02x",((uint8_t *)pk.begin())[i]);
                str[66] = 0;
                result.push_back(Pair("combined_pk",str));
                result.push_back(Pair("result","success"));
                return(result);
            } else return(cclib_error(result,"error serializeing combined_pk"));
        } else return(cclib_error(result,"error combining pukbeys"));
    } else return(cclib_error(result,"need pubkeys params"));
}

UniValue musig_session(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t i,n,myind,num,musiglocation; char *pkstr,*pkhashstr,*msgstr; uint8_t session[32],msg[32],pkhash[32],privkey[32],pub33[33]; CPubKey pk; char str[67];
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) >= 5 )
    {
        myind = juint(jitem(params,0),0);
        num = juint(jitem(params,1),0);
        if ( myind < 0 || myind >= num || num <= 0 )
            return(cclib_error(result,"illegal myindex and numsigners"));
        if ( n > 5 )
            musiglocation = juint(jitem(params,5),0);
        else if ( n == 5 )
            musiglocation = 0;
        //printf("number of params.%i musiglocation.%i\n",n,musiglocation);
        if ( MUSIG.size() > musiglocation )
        {
            for (int i = 0; i < MUSIG.size()-1; i++)
                musig_infofree(MUSIG[i]);
            MUSIG.clear();
        }
        struct musig_info *temp_musig = musig_infocreate(myind,num);
        MUSIG.push_back(temp_musig);
        if ( musig_parsepubkey(ctx,MUSIG[musiglocation]->combined_pk,jitem(params,2)) < 0 )
            return(cclib_error(result,"error parsing combined_pubkey"));
        else if ( cclib_parsehash(MUSIG[musiglocation]->pkhash,jitem(params,3),32) < 0 )
            return(cclib_error(result,"error parsing pkhash"));
        else if ( cclib_parsehash(MUSIG[musiglocation]->msg,jitem(params,4),32) < 0 )
            return(cclib_error(result,"error parsing msg"));
        Myprivkey(privkey);
        GetRandBytes(session,32);
            /** Initializes a signing session for a signer
             *
             *  Returns: 1: session is successfully initialized
             *           0: session could not be initialized: secret key or secret nonce overflow
             *  Args:         ctx: pointer to a context object, initialized for signing (cannot
             *                     be NULL)
             *  Out:      session: the session structure to initialize (cannot be NULL)
             *            signers: an array of signers' data to be initialized. Array length must
             *                     equal to `n_signers` (cannot be NULL)
             * nonce_commitment32: filled with a 32-byte commitment to the generated nonce
             *                     (cannot be NULL)
             *  In:  session_id32: a *unique* 32-byte ID to assign to this session (cannot be
             *                     NULL). If a non-unique session_id32 was given then a partial
             *                     signature will LEAK THE SECRET KEY.
             *              msg32: the 32-byte message to be signed. Shouldn't be NULL unless you
             *                     require sharing public nonces before the message is known
             *                     because it reduces nonce misuse resistance. If NULL, must be
             *                     set with `musig_session_set_msg` before signing and verifying.
             *        combined_pk: the combined public key of all signers (cannot be NULL)
             *          pk_hash32: the 32-byte hash of the signers' individual keys (cannot be
             *                     NULL)
             *          n_signers: length of signers array. Number of signers participating in
             *                     the MuSig. Must be greater than 0 and at most 2^32 - 1.
             *           my_index: index of this signer in the signers array
             *             seckey: the signer's 32-byte secret key (cannot be NULL)
             */
        //fprintf(stderr, "SESSION: struct_size.%li using struct %i\n",MUSIG.size(), musiglocation);
        if ( secp256k1_musig_session_initialize(ctx,&MUSIG[musiglocation]->session,MUSIG[musiglocation]->signer_data, &MUSIG[musiglocation]->nonce_commitments[MUSIG[musiglocation]->myind * 32],session,MUSIG[musiglocation]->msg,&MUSIG[musiglocation]->combined_pk,MUSIG[musiglocation]->pkhash,MUSIG[musiglocation]->num,MUSIG[musiglocation]->myind,privkey) > 0 )
        {
            memset(session,0,sizeof(session));
            result.push_back(Pair("myind",(int64_t)myind));
            result.push_back(Pair("numsigners",(int64_t)num));
            for (i=0; i<32; i++)
                sprintf(&str[i<<1],"%02x",MUSIG[musiglocation]->nonce_commitments[MUSIG[musiglocation]->myind*32 + i]);
            str[64] = 0;
            if ( n == 5 )
                MUSIG[musiglocation]->numcommits = 1;
            result.push_back(Pair("commitment",str));
            result.push_back(Pair("result","success"));
            memset(privkey,0,sizeof(privkey));
            return(result);
        }
        else
        {
            memset(privkey,0,sizeof(privkey));
            memset(session,0,sizeof(session));
            return(cclib_error(result,"couldnt initialize session"));
        }
    } else return(cclib_error(result,"wrong number of params, need 5: myindex, numsigners, combined_pk, pkhash, msg32"));
}

UniValue musig_commit(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    size_t clen = CPubKey::PUBLIC_KEY_SIZE;
    UniValue result(UniValue::VOBJ); int32_t i,n,ind,myind; uint8_t pkhash[32]; CPubKey pk; char str[67];
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) >= 3 )
    {
        if ( n > 3 )
            myind = juint(jitem(params,3),0);
        else if ( n == 3 )
            myind = 0;
        if ( cclib_parsehash(pkhash,jitem(params,0),32) < 0 )
            return(cclib_error(result,"error parsing pkhash"));
        else if ( memcmp(MUSIG[myind]->pkhash,pkhash,32) != 0 )
            return(cclib_error(result,"pkhash doesnt match session pkhash"));
        else if ( (ind= juint(jitem(params,1),0)) < 0 || ind >= MUSIG[myind]->num )
            return(cclib_error(result,"illegal ind for session"));
        else if ( cclib_parsehash(&MUSIG[myind]->nonce_commitments[ind*32],jitem(params,2),32) < 0 )
            return(cclib_error(result,"error parsing commitment"));
        /** Gets the signer's public nonce given a list of all signers' data with commitments
         *
         *  Returns: 1: public nonce is written in nonce
         *           0: signer data is missing commitments or session isn't initialized
         *              for signing
         *  Args:        ctx: pointer to a context object (cannot be NULL)
         *           session: the signing session to get the nonce from (cannot be NULL)
         *           signers: an array of signers' data initialized with
         *                    `musig_session_initialize`. Array length must equal to
         *                    `n_commitments` (cannot be NULL)
         *  Out:       nonce: the nonce (cannot be NULL)
         *  In:  commitments: array of 32-byte nonce commitments (cannot be NULL)
         *     n_commitments: the length of commitments and signers array. Must be the total
         *                    number of signers participating in the MuSig.
         */
        result.push_back(Pair("added_index",ind));
        //fprintf(stderr, "COMMIT: struct_size.%li using_struct.%i added_index.%i\n",MUSIG.size(), myind, ind);
        MUSIG[myind]->numcommits++;
        if ( MUSIG[myind]->numcommits >= MUSIG[myind]->num && secp256k1_musig_session_get_public_nonce(ctx,&MUSIG[myind]->session,MUSIG[myind]->signer_data,&MUSIG[myind]->nonces[MUSIG[myind]->myind],MUSIG[myind]->commitment_ptrs,MUSIG[myind]->num) > 0 )
        {
            if ( secp256k1_ec_pubkey_serialize(ctx,(uint8_t *)pk.begin(),&clen,&MUSIG[myind]->nonces[MUSIG[myind]->myind],SECP256K1_EC_COMPRESSED) > 0 && clen == 33 )
            {
                for (i=0; i<33; i++)
                    sprintf(&str[i<<1],"%02x",((uint8_t *)pk.begin())[i]);
                str[66] = 0;
                if ( n == 3 )
                    MUSIG[myind]->numnonces = 1;
                result.push_back(Pair("myind",MUSIG[myind]->myind));
                result.push_back(Pair("nonce",str));
                result.push_back(Pair("result","success"));
            } else return(cclib_error(result,"error serializing nonce (pubkey)"));
        }
        else
        {
            result.push_back(Pair("status","not enough commitments"));
            result.push_back(Pair("result","success"));
        }
        return(result);
    } else return(cclib_error(result,"wrong number of params, need 3: pkhash, ind, commitment"));
}

UniValue musig_nonce(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t i,n,ind,myind; uint8_t pkhash[32],psig[32]; CPubKey pk; char str[67];
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) >= 3 )
    {
        if ( n > 3 )
            myind = juint(jitem(params,3),0);
        else if ( n == 3 )
            myind = 0;
        if ( cclib_parsehash(pkhash,jitem(params,0),32) < 0 )
            return(cclib_error(result,"error parsing pkhash"));
        else if ( memcmp(MUSIG[myind]->pkhash,pkhash,32) != 0 )
            return(cclib_error(result,"pkhash doesnt match session pkhash"));
        else if ( (ind= juint(jitem(params,1),0)) < 0 || ind >= MUSIG[myind]->num )
            return(cclib_error(result,"illegal ind for session"));
        else if ( musig_parsepubkey(ctx,MUSIG[myind]->nonces[ind],jitem(params,2)) < 0 )
            return(cclib_error(result,"error parsing nonce"));
        result.push_back(Pair("added_index",ind));
        /** Checks a signer's public nonce against a commitment to said nonce, and update
         *  data structure if they match
         *
         *  Returns: 1: commitment was valid, data structure updated
         *           0: commitment was invalid, nothing happened
         *  Args:      ctx: pointer to a context object (cannot be NULL)
         *          signer: pointer to the signer data to update (cannot be NULL). Must have
         *                  been used with `musig_session_get_public_nonce` or initialized
         *                  with `musig_session_initialize_verifier`.
         *  In:     nonce: signer's alleged public nonce (cannot be NULL)
         */
        MUSIG[myind]->numnonces++;
        //fprintf(stderr, "NONCE: struct_size.%li using_struct.%i added_index.%i numnounces.%i num.%i\n",MUSIG.size(), myind, ind, MUSIG[myind]->numnonces, MUSIG[myind]->num);
        if ( MUSIG[myind]->numnonces < MUSIG[myind]->num )
        {
            result.push_back(Pair("status","not enough nonces"));
            result.push_back(Pair("result","success"));
            return(result);
        }
        for (i=0; i<MUSIG[myind]->num; i++)
        {
            if ( secp256k1_musig_set_nonce(ctx,&MUSIG[myind]->signer_data[i],&MUSIG[myind]->nonces[i]) == 0 )
                return(cclib_error(result,"error setting nonce"));
        }
        /** Updates a session with the combined public nonce of all signers. The combined
         * public nonce is the sum of every signer's public nonce.
         *
         *  Returns: 1: nonces are successfully combined
         *           0: a signer's nonce is missing
         *  Args:        ctx: pointer to a context object (cannot be NULL)
         *           session: session to update with the combined public nonce (cannot be
         *                    NULL)
         *           signers: an array of signers' data, which must have had public nonces
         *                    set with `musig_set_nonce`. Array length must equal to `n_signers`
         *                    (cannot be NULL)
         *         n_signers: the length of the signers array. Must be the total number of
         *                    signers participating in the MuSig.
         *  Out: nonce_is_negated: a pointer to an integer that indicates if the combined
         *                    public nonce had to be negated.
         *           adaptor: point to add to the combined public nonce. If NULL, nothing is
         *                    added to the combined nonce.
         */
        if ( secp256k1_musig_session_combine_nonces(ctx,&MUSIG[myind]->session,MUSIG[myind]->signer_data,MUSIG[myind]->num,NULL,NULL) > 0 )
        {
            if ( secp256k1_musig_partial_sign(ctx,&MUSIG[myind]->session,&MUSIG[myind]->partial_sig[MUSIG[myind]->myind]) > 0 )
            {
                if ( secp256k1_musig_partial_signature_serialize(ctx,psig,&MUSIG[myind]->partial_sig[MUSIG[myind]->myind]) > 0 )
                {
                    for (i=0; i<32; i++)
                        sprintf(&str[i<<1],"%02x",psig[i]);
                    str[64] = 0;
                    result.push_back(Pair("myind",MUSIG[myind]->myind));
                    result.push_back(Pair("partialsig",str));
                    result.push_back(Pair("result","success"));
                    if ( n == 3 )
                        MUSIG[myind]->numpartials = 1;
                    return(result);
                } else return(cclib_error(result,"error serializing partial sig"));
            } else return(cclib_error(result,"error making partial sig"));
        } else return(cclib_error(result,"error combining nonces"));
    } else return(cclib_error(result,"wrong number of params, need 3: pkhash, ind, nonce"));
}

UniValue musig_partialsig(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t i,ind,n,myind; uint8_t pkhash[32],psig[32],out64[64]; char str[129]; secp256k1_schnorrsig sig;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) >= 3 )
    {
        if ( n > 3 )
            myind = juint(jitem(params,3),0);
        else if ( n == 3 )
            myind = 0;
        if ( cclib_parsehash(pkhash,jitem(params,0),32) < 0 )
            return(cclib_error(result,"error parsing pkhash"));
        else if ( memcmp(MUSIG[myind]->pkhash,pkhash,32) != 0 )
            return(cclib_error(result,"pkhash doesnt match session pkhash"));
        else if ( (ind= juint(jitem(params,1),0)) < 0 || ind >= MUSIG[myind]->num )
            return(cclib_error(result,"illegal ind for session"));
        else if ( cclib_parsehash(psig,jitem(params,2),32) < 0 )
            return(cclib_error(result,"error parsing psig"));
        else if ( secp256k1_musig_partial_signature_parse(ctx,&MUSIG[myind]->partial_sig[ind],psig) == 0 )
            return(cclib_error(result,"error parsing partialsig"));
        result.push_back(Pair("added_index",ind));
        //fprintf(stderr, "SIG: struct_size.%li using_struct.%i added_index.%i\n",MUSIG.size(), myind, ind);
        MUSIG[myind]->numpartials++;
        if ( MUSIG[myind]->numpartials >= MUSIG[myind]->num && secp256k1_musig_partial_sig_combine(ctx,&MUSIG[myind]->session,&sig,MUSIG[myind]->partial_sig,MUSIG[myind]->num) > 0 )
        {
            if ( secp256k1_schnorrsig_serialize(ctx,out64,&sig) > 0 )
            {
                result.push_back(Pair("result","success"));
                for (i=0; i<64; i++)
                    sprintf(&str[i<<1],"%02x",out64[i]);
                str[128] = 0;
                result.push_back(Pair("combinedsig",str));
            } else return(cclib_error(result,"error serializing combinedsig"));
        }
        else
        {
            if ( secp256k1_musig_partial_signature_serialize(ctx,psig,&MUSIG[myind]->partial_sig[MUSIG[myind]->myind]) > 0 )
            {
                result.push_back(Pair("myind",ind));
                for (i=0; i<32; i++)
                    sprintf(&str[i<<1],"%02x",psig[i]);
                str[64] = 0;
                result.push_back(Pair("partialsig",str));
                result.push_back(Pair("result","success"));
                result.push_back(Pair("status","need more partialsigs"));
            } else return(cclib_error(result,"error generating my partialsig"));
        }
        return(result);
    } else return(cclib_error(result,"wrong number of params, need 3: pkhash, ind, partialsig"));
}

//int testmain(void);
UniValue musig_verify(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t i,n; uint8_t msg[32],musig64[64]; secp256k1_pubkey combined_pk; secp256k1_schnorrsig musig; char str[129];
    //testmain();
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 3 )
    {
        if ( cclib_parsehash(msg,jitem(params,0),32) < 0 )
            return(cclib_error(result,"error parsing pkhash"));
        else if ( musig_parsepubkey(ctx,combined_pk,jitem(params,1)) < 0 )
            return(cclib_error(result,"error parsing combined_pk"));
        else if ( cclib_parsehash(musig64,jitem(params,2),64) < 0 )
            return(cclib_error(result,"error parsing musig64"));
        for (i=0; i<32; i++)
            sprintf(&str[i*2],"%02x",msg[i]);
        str[64] = 0;
        result.push_back(Pair("msg",str));
        result.push_back(Pair("combined_pk",jstr(jitem(params,1),0)));
        for (i=0; i<64; i++)
            sprintf(&str[i*2],"%02x",musig64[i]);
        str[128] = 0;
        result.push_back(Pair("combinedsig",str));
        if ( secp256k1_schnorrsig_parse(ctx,&musig,&musig64[0]) > 0 )
        {
            if ( secp256k1_schnorrsig_verify(ctx,&musig,msg,&combined_pk) > 0 )
            {
                result.push_back(Pair("result","success"));
                return(result);
            } else return(cclib_error(result,"musig didnt verify"));
        } else return(cclib_error(result,"couldnt parse musig64"));
    } else return(cclib_error(result,"wrong number of params, need 3: msg, combined_pk, combinedsig"));
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
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n == 2 && (hexstr= jstr(jitem(params,0),0)) != 0 && is_hexstr(hexstr,0) == 66 )
        {
            CPubKey pk(ParseHex(hexstr));
            amount = jdouble(jitem(params,1),0) * COIN + 0.0000000049;
            if ( amount >= 3*txfee && AddNormalinputs(mtx,mypk,amount+2*txfee,64) >= amount+2*txfee )
            {
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount+txfee,musigpk));
                rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,musig_sendopret('x',pk));
                return(musig_rawtxresult(result,rawtx));
            } else return(cclib_error(result,"couldnt find funds or less than 0.0003"));
        } else return(cclib_error(result,"must have 2 params: pk, amount"));
    } else return(cclib_error(result,"not enough parameters"));
}

UniValue musig_spend(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; CPubKey mypk,pk; secp256k1_pubkey combined_pk; char *scriptstr,*musigstr; uint8_t msg[32]; CTransaction vintx; uint256 prevhash,hashBlock; int32_t i,n,numvouts; char str[129]; CTxOut vout; secp256k1_schnorrsig musig;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n == 3 )
        {
            prevhash = juint256(jitem(params,0));
            scriptstr = jstr(jitem(params,1),0);
            musigstr = jstr(jitem(params,2),0);
            if ( is_hexstr(scriptstr,0) != 0 && is_hexstr(musigstr,0) == 128 )
            {
                if ( txfee == 0 )
                    txfee = MUSIG_TXFEE;
                mypk = pubkey2pk(Mypubkey());
                std::vector<uint8_t> musig64(ParseHex(musigstr));
                CScript scriptPubKey;
                scriptPubKey.resize(strlen(scriptstr)/2);
                decode_hex(&scriptPubKey[0],strlen(scriptstr)/2,scriptstr);
                if ( myGetTransaction(prevhash,vintx,hashBlock) != 0 && (numvouts= vintx.vout.size()) > 1 )
                {
                    vout.nValue = vintx.vout[0].nValue - txfee;
                    vout.scriptPubKey = scriptPubKey;
                    if ( musig_sendopretdecode(pk,vintx.vout[numvouts-1].scriptPubKey) == 'x' )
                    {
                        if ( secp256k1_schnorrsig_parse((const secp256k1_context *)ctx,&musig,(const uint8_t *)&musig64[0]) > 0 &&
                            secp256k1_ec_pubkey_parse(ctx,&combined_pk,pk.begin(),33) > 0 )
                        {
                            musig_prevoutmsg(msg,prevhash,vout.scriptPubKey);
                            {
                                for (i=0; i<32; i++)
                                    sprintf(&str[i*2],"%02x",msg[i]);
                                str[64] = 0;
                                result.push_back(Pair("msg",str));
                                for (i=0; i<33; i++)
                                    sprintf(&str[i*2],"%02x",((uint8_t *)pk.begin())[i]);
                                str[66] = 0;
                                result.push_back(Pair("combined_pk",str));
                                for (i=0; i<64; i++)
                                    sprintf(&str[i*2],"%02x",musig64[i]);
                                str[128] = 0;
                                result.push_back(Pair("combinedsig",str));
                            }
                            if ( !secp256k1_schnorrsig_verify((const secp256k1_context *)ctx,&musig,(const uint8_t *)msg,(const secp256k1_pubkey *)&combined_pk) )
                            {
                                return(cclib_error(result,"musig didnt validate"));
                            }
                            mtx.vin.push_back(CTxIn(prevhash,MUSIG_PREVN));
                            mtx.vout.push_back(vout);
                            rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,musig_spendopret('y',pk,musig64));
                            return(musig_rawtxresult(result,rawtx));
                        } else return(cclib_error(result,"couldnt parse pk or musig"));
                    } else return(cclib_error(result,"couldnt decode send opret"));
                } else return(cclib_error(result,"couldnt find vin0"));
            } else return(cclib_error(result,"script or musig is not hex"));
        } else return(cclib_error(result,"need to have exactly 3 params sendtxid, scriptPubKey, musig"));
    } else return(cclib_error(result,"params parse error"));
}

bool musig_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    static secp256k1_context *ctx;
    secp256k1_pubkey combined_pk; CPubKey pk,checkpk; secp256k1_schnorrsig musig; uint256 hashBlock; CTransaction vintx; int32_t numvouts; std::vector<uint8_t> musig64; uint8_t msg[32];
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
        if ( musig_sendopretdecode(pk,vintx.vout[numvouts-1].scriptPubKey) == 'x' )
        {
            if ( musig_spendopretdecode(checkpk,musig64,tx.vout[tx.vout.size()-1].scriptPubKey) == 'y' )
            {
                if ( pk == checkpk )
                {
                    if ( secp256k1_schnorrsig_parse((const secp256k1_context *)ctx,&musig,(const uint8_t *)&musig64[0]) > 0 &&
                        secp256k1_ec_pubkey_parse(ctx,&combined_pk,pk.begin(),33) > 0 )
                    {
                        musig_prevoutmsg(msg,tx.vin[0].prevout.hash,tx.vout[0].scriptPubKey);
                        if ( !secp256k1_schnorrsig_verify((const secp256k1_context *)ctx,&musig,(const uint8_t *)msg,(const secp256k1_pubkey *)&combined_pk) )
                            return eval->Invalid("failed schnorrsig_verify");
                        else return(true);
                    } else return eval->Invalid("couldnt parse pk or musig");
                } else return eval->Invalid("combined_pk didnt match send opret");
            } else return eval->Invalid("failed decode musig spendopret");
        } else return eval->Invalid("couldnt decode send opret");
    } else return eval->Invalid("couldnt find vin0 tx");
}
