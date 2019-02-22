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
./c cclib combine 18 \"[%2202aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848%22,%22039433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775%22]\"
{
    "pkhash": "5be117f3c5ce87e7dc6882c24b8231e0652ee82054bf7b9f94aef1f45e055cba",
    "combined_pk": "032ddac56613cd0667b589bd7f32b665e2d2ce0247e337a5a0bca6c72e3d9d057b",
    "result": "success"
}
 the combined_pk and pkhash will be needed for various other rpc calls
*/

/* second, send 0.777 coins to the combined_pk
 ./c cclib send 18 \"[%22032ddac56613cd0667b589bd7f32b665e2d2ce0247e337a5a0bca6c72e3d9d057b%22,0.777]\"
 {
 "hex": "0400008085202f8908018bbc4a0acf0f896680b84cc1000ec5530042fa3cf7471ab5c11cde56f6d0e60000000048473044022041a900fa57e54f939b797d09f0cee81ab9a79d978cd473ef3fc9f09c7678ad6b02206386bd15ee7ee8224a984fdd1d2a094738d082aec06028a5846d7bc61ddf16ca01ffffffff0b969f0e1ea787f0cc4e81d128353bd1cb670ab89bd1db4b47fbb7e872cd39fb00000000494830450221008c5de4b196e57b0dd94aa415950adf274e3e6859b82cf218729af84c1f15e76c022024aeab7eda63e6a652ef488bf26a8dc4ef8d2d4aa5746726085bfe5f169a5db701ffffffff0b36b70b43457fab377f28fb22da5a3e9d8186a37daae18cf0f710a221ab26250000000048473044022004ec20ae7490e7adabf9a3f78e4a58df84a3245485bfdd40f421cafe61d19c340220456d2b6f3c6e88632027c02606a0af1c21208d05f2de0826fbf4dfe7391ec83901ffffffff0aaff3cfe4ca22b97b6179a6f7cfac91945e5440e9438b89d1ec09500167176a0000000048473044022074dcad30c8ab9ed79a3ac69169611fc9e5f4b76a561b183461d968249316997f022063b25decaa285f494d277b9c8c2bcf6445b7929a304542e89c0645828d30a1a901ffffffff090e1bb92e9bf404a0d6455701b21af3dbf6765e61a1dc28b7c0f608ec4f12da000000004847304402202f9182c532c66138a6bdfcbb85a06cf1bf1532f2bf8f63170ef20843e4a81d0202207612a4353eb9606e84621c444ec7db1b683ff29c56127bda2d5e9c0eb13dbbc001ffffffff08a57005c7a40a923b1a510820b07f7318d760fe2a233b077d918cce357ad3af00000000484730440220643d60c68634fb2e0f6656389fc70c9f84c7086fc6e35b0fa26297e844f6c5fc02201d79669e073efe738d47de0130fdcba875e284e18fd478c0e6834d46632d8b8101ffffffff068cfd0ea6c0f5d401c67ec38f92425a9e59b0d5ade55bb2971ea955675a17bd00000000484730440220747139724248da4bcc1e5e3828e0ea811756e1fad0ebc40aeb006fd8079d46e402200d8f1c229c79494b5617e4373a3e083966dcd74571323f9d334be901d53871fa01ffffffff0200382fb6984b6128bb75115346242809c6555274e0cacef822825a2b4d231700000000484730440220454fcac398f6913fb4d8ed330f110f9cf62eec6c8cdb67d5df1effd2cf8222d5022017f6323630669777573e342e870c88727a917cc06c33611ebbd9d1fccc1dcd3701ffffffff03b0c2a10400000000302ea22c8020c71ddb3aac7f9b9e4bdacf032aaa8b8e4433c4ff9f8a43cebb9c1f5da96928a48103120c008203000401cc40ca220000000000232102aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848ac0000000000000000266a24127821032ddac56613cd0667b589bd7f32b665e2d2ce0247e337a5a0bca6c72e3d9d057b00000000460100000000000000000000000000",
 "txid": "2c4159bb19212dcaa412ae37de7d72398f063194053e04a65b0facf767ebcc68",
 "result": "success"
 }
 
 {
 "value": 0.77710000,
 "valueZat": 77710000,
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
 change script: 2102aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848ac
 
 sendtxid: 2c4159bb19212dcaa412ae37de7d72398f063194053e04a65b0facf767ebcc68
 
 broadcast sendtxid and wait for it to be confirmed. then get the msg we need to sign:
 
 ./c cclib calcmsg 18 \"[%222c4159bb19212dcaa412ae37de7d72398f063194053e04a65b0facf767ebcc68%22,%222102aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848ac%22]\"
 
 {
 "result": "success",
 "msg": "caa64ba398ddfe5c33d8c70a61e556caa0e69b19d93110c5a458a1b37ad44cb0"
 }
 
the "msg" is what needs to be signed to create a valid spend
 
 now on each signing node, a session needs to be created:
 5 args: ind, numsigners, combined_pk, pkhash, message to be signed
 
 on node with pubkey: 02aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848
 ./c cclib session 18 \"[0,2,%22032ddac56613cd0667b589bd7f32b665e2d2ce0247e337a5a0bca6c72e3d9d057b%22,%225be117f3c5ce87e7dc6882c24b8231e0652ee82054bf7b9f94aef1f45e055cba%22,%22caa64ba398ddfe5c33d8c70a61e556caa0e69b19d93110c5a458a1b37ad44cb0%22]\"
 {
 "myind": 0,
 "numsigners": 2,
 "commitment": "e82228c10d0e100477630349150dea744d3b2790dcd347511a1a98199840cda4",
 "result": "success"
 }
 
 on node with pubkey: 039433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775
 ./c cclib session 18 \"[1,2,%22032ddac56613cd0667b589bd7f32b665e2d2ce0247e337a5a0bca6c72e3d9d057b%22,%225be117f3c5ce87e7dc6882c24b8231e0652ee82054bf7b9f94aef1f45e055cba%22,%22caa64ba398ddfe5c33d8c70a61e556caa0e69b19d93110c5a458a1b37ad44cb0%22]\"
 {
 "myind": 1,
 "numsigners": 2,
 "commitment": "6e426e850ddc45e7742cfb6321781c00ee69a995ab12fa1f9ded7fe43658babf",
 "result": "success"
 }
 
 now we need to get the commitment from each node to the other one. the session already put the commitment for each node into the global struct. Keep in mind there is a single global struct with session unique to each cclib session call. that means no restarting any deamon in the middle of the process on any of the nodes and only call cclib session a single time. this is an artificial restriction just to simplify the initial implementation of musig
 
 ./c cclib commit 18 \"[%225be117f3c5ce87e7dc6882c24b8231e0652ee82054bf7b9f94aef1f45e055cba%22,1,%226e426e850ddc45e7742cfb6321781c00ee69a995ab12fa1f9ded7fe43658babf%22]\"
 {
 "added_index": 1,
 "myind": 0,
 "nonce": "0261671b0a6de416938cf035c98f8af37c6ca88bbbd1bcce693d709d4919b010e1",
 "result": "success"
 }
 
 ./c cclib commit 18 \"[%225be117f3c5ce87e7dc6882c24b8231e0652ee82054bf7b9f94aef1f45e055cba%22,0,%22e82228c10d0e100477630349150dea744d3b2790dcd347511a1a98199840cda4%22]\"
 {
 "added_index": 0,
 "myind": 1,
 "nonce": "02570f62a625ceb19a754a053152b162810c3e403df63f3d443e85bdacc74bfdfe",
 "result": "success"
 }
 
 Now exchange the revealed nonces to each node:
 
 ./c cclib nonce 18 \"[%225be117f3c5ce87e7dc6882c24b8231e0652ee82054bf7b9f94aef1f45e055cba%22,1,%2202570f62a625ceb19a754a053152b162810c3e403df63f3d443e85bdacc74bfdfe%22]\"

 {
 "added_index": 1,
 "myind": 0,
 "partialsig": "3f21885e6d2d020e1473435ccd148a61cdcb1d1105867fed45913185dc0acf59",
 "result": "success"
 }
 
 ./c cclib nonce 18 \"[%225be117f3c5ce87e7dc6882c24b8231e0652ee82054bf7b9f94aef1f45e055cba%22,0,%220261671b0a6de416938cf035c98f8af37c6ca88bbbd1bcce693d709d4919b010e1%22]\"
 {
 "added_index": 0,
 "myind": 0,
 "myind": 1,
 "partialsig": "af7f28455fb2e988d81068cd9d800879cd334036a8300118dc307b777a38c1ed",
 "result": "success"
 }
 
 Almost there! final step is to exchange the partial sigs between signers
 ./c cclib partialsig 18 \"[%225be117f3c5ce87e7dc6882c24b8231e0652ee82054bf7b9f94aef1f45e055cba%22,1,%22af7f28455fb2e988d81068cd9d800879cd334036a8300118dc307b777a38c1ed%22]\"
 {
 "added_index": 1,
 "result": "success",
 "combinedsig": "5e64dc5dda93b2d3f25fe44b2aaca69b8f15d21f70e2bc1c2c53e17262d941bbeea0b0a3ccdfeb96ec83ac2a6a9492db9afe5d47adb6810621c1acfd56439146"
 }

 
 ./c cclib partialsig 18 \"[%225be117f3c5ce87e7dc6882c24b8231e0652ee82054bf7b9f94aef1f45e055cba%22,0,%223f21885e6d2d020e1473435ccd148a61cdcb1d1105867fed45913185dc0acf59%22]\"

 {
 "added_index": 0,
 "result": "success",
 "combinedsig": "5e64dc5dda93b2d3f25fe44b2aaca69b8f15d21f70e2bc1c2c53e17262d941bbeea0b0a3ccdfeb96ec83ac2a6a9492db9afe5d47adb6810621c1acfd56439146"
 }
 Notice both nodes generated the same combined signature!
 
 Now for a sanity test, we can use the verify call to make sure this sig will work with the msg needed for the spend:
 
 ./c cclib verify 18 \"[%22caa64ba398ddfe5c33d8c70a61e556caa0e69b19d93110c5a458a1b37ad44cb0%22,%22032ddac56613cd0667b589bd7f32b665e2d2ce0247e337a5a0bca6c72e3d9d057b%22,%225e64dc5dda93b2d3f25fe44b2aaca69b8f15d21f70e2bc1c2c53e17262d941bbeea0b0a3ccdfeb96ec83ac2a6a9492db9afe5d47adb6810621c1acfd56439146%22]\"
 
 
 and finally the spend: sendtxid, scriptPubKey, musig
 
./c cclib spend 18 \"[%222c4159bb19212dcaa412ae37de7d72398f063194053e04a65b0facf767ebcc68%22,%222102aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848ac%22,%225e64dc5dda93b2d3f25fe44b2aaca69b8f15d21f70e2bc1c2c53e17262d941bbeea0b0a3ccdfeb96ec83ac2a6a9492db9afe5d47adb6810621c1acfd56439146%22]\"
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
    int32_t myind,num;
    uint8_t msg[32],pkhash[32],combpk[33];
} *MUSIG;

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
    UniValue result(UniValue::VOBJ); uint256 sendtxid; int32_t i; uint8_t msg[32]; char *scriptstr,str[65]; int32_t n;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
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

int32_t musig_parsehash(uint8_t *hash32,cJSON *item,int32_t len)
{
    char *hexstr;
    if ( (hexstr= jstr(item,0)) != 0 && is_hexstr(hexstr,0) == len*2 )
    {
        decode_hex(hash32,len,hexstr);
        return(0);
    } else return(-1);
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
    UniValue result(UniValue::VOBJ); int32_t i,n,myind,num; char *pkstr,*pkhashstr,*msgstr; uint8_t session[32],msg[32],pkhash[32],privkey[32],pub33[33]; CPubKey pk; char str[67];
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 5 )
    {
        myind = juint(jitem(params,0),0);
        num = juint(jitem(params,1),0);
        if ( myind < 0 || myind >= num || num <= 0 )
            return(cclib_error(result,"illegal myindex and numsigners"));
        if ( MUSIG != 0 )
            musig_infofree(MUSIG), MUSIG = 0;
        MUSIG = musig_infocreate(myind,num);
        if ( musig_parsepubkey(ctx,MUSIG->combined_pk,jitem(params,2)) < 0 )
            return(cclib_error(result,"error parsing combined_pubkey"));
        else if ( musig_parsehash(MUSIG->pkhash,jitem(params,3),32) < 0 )
            return(cclib_error(result,"error parsing pkhash"));
        else if ( musig_parsehash(MUSIG->msg,jitem(params,4),32) < 0 )
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
        if ( secp256k1_musig_session_initialize(ctx,&MUSIG->session,MUSIG->signer_data, &MUSIG->nonce_commitments[MUSIG->myind * 32],session,MUSIG->msg,&MUSIG->combined_pk,MUSIG->pkhash,MUSIG->num,MUSIG->myind,privkey) > 0 )
        {
            memset(session,0,sizeof(session));
            result.push_back(Pair("myind",(int64_t)myind));
            result.push_back(Pair("numsigners",(int64_t)num));
            for (i=0; i<32; i++)
                sprintf(&str[i<<1],"%02x",MUSIG->nonce_commitments[MUSIG->myind*32 + i]);
            str[64] = 0;
            result.push_back(Pair("commitment",str));
            result.push_back(Pair("result","success"));
            return(result);
        }
        else
        {
            memset(session,0,sizeof(session));
            return(cclib_error(result,"couldnt initialize session"));
        }
    } else return(cclib_error(result,"wrong number of params, need 5: myindex, numsigners, combined_pk, pkhash, msg32"));
}

UniValue musig_commit(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    size_t clen = CPubKey::PUBLIC_KEY_SIZE;
    UniValue result(UniValue::VOBJ); int32_t i,n,ind; uint8_t pkhash[32]; CPubKey pk; char str[67];
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 3 )
    {
        if ( musig_parsehash(pkhash,jitem(params,0),32) < 0 )
            return(cclib_error(result,"error parsing pkhash"));
        else if ( memcmp(MUSIG->pkhash,pkhash,32) != 0 )
            return(cclib_error(result,"pkhash doesnt match session pkhash"));
        else if ( (ind= juint(jitem(params,1),0)) < 0 || ind >= MUSIG->num )
            return(cclib_error(result,"illegal ind for session"));
        else if ( musig_parsehash(&MUSIG->nonce_commitments[ind*32],jitem(params,2),32) < 0 )
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
        if ( secp256k1_musig_session_get_public_nonce(ctx,&MUSIG->session,MUSIG->signer_data,&MUSIG->nonces[MUSIG->myind],MUSIG->commitment_ptrs,MUSIG->num) > 0 )
        {
            if ( secp256k1_ec_pubkey_serialize(ctx,(uint8_t *)pk.begin(),&clen,&MUSIG->nonces[MUSIG->myind],SECP256K1_EC_COMPRESSED) > 0 && clen == 33 )
            {
                for (i=0; i<33; i++)
                    sprintf(&str[i<<1],"%02x",((uint8_t *)pk.begin())[i]);
                str[66] = 0;
                result.push_back(Pair("myind",MUSIG->myind));
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
    UniValue result(UniValue::VOBJ); int32_t i,n,ind; uint8_t pkhash[32],psig[32]; CPubKey pk; char str[67];
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 3 )
    {
        if ( musig_parsehash(pkhash,jitem(params,0),32) < 0 )
            return(cclib_error(result,"error parsing pkhash"));
        else if ( memcmp(MUSIG->pkhash,pkhash,32) != 0 )
            return(cclib_error(result,"pkhash doesnt match session pkhash"));
        else if ( (ind= juint(jitem(params,1),0)) < 0 || ind >= MUSIG->num )
            return(cclib_error(result,"illegal ind for session"));
        else if ( musig_parsepubkey(ctx,MUSIG->nonces[ind],jitem(params,2)) < 0 )
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
        for (i=0; i<MUSIG->num; i++)
        {
            if ( secp256k1_musig_set_nonce(ctx,&MUSIG->signer_data[i],&MUSIG->nonces[i]) == 0 )
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
        if ( secp256k1_musig_session_combine_nonces(ctx,&MUSIG->session,MUSIG->signer_data,MUSIG->num,NULL,NULL) > 0 )
        {
            if ( secp256k1_musig_partial_sign(ctx,&MUSIG->session,&MUSIG->partial_sig[MUSIG->myind]) > 0 )
            {
                if ( secp256k1_musig_partial_signature_serialize(ctx,psig,&MUSIG->partial_sig[MUSIG->myind]) > 0 )
                {
                    for (i=0; i<32; i++)
                        sprintf(&str[i<<1],"%02x",psig[i]);
                    str[64] = 0;
                    result.push_back(Pair("myind",MUSIG->myind));
                    result.push_back(Pair("partialsig",str));
                    result.push_back(Pair("result","success"));
                    return(result);
                } else return(cclib_error(result,"error serializing partial sig"));
            } else return(cclib_error(result,"error making partial sig"));
        } else return(cclib_error(result,"error combining nonces"));
    } else return(cclib_error(result,"wrong number of params, need 3: pkhash, ind, nonce"));
}

UniValue musig_partialsig(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t i,ind,n; uint8_t pkhash[32],psig[32],out64[64]; char str[129]; secp256k1_schnorrsig sig;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 3 )
    {
        if ( musig_parsehash(pkhash,jitem(params,0),32) < 0 )
            return(cclib_error(result,"error parsing pkhash"));
        else if ( memcmp(MUSIG->pkhash,pkhash,32) != 0 )
            return(cclib_error(result,"pkhash doesnt match session pkhash"));
        else if ( (ind= juint(jitem(params,1),0)) < 0 || ind >= MUSIG->num )
            return(cclib_error(result,"illegal ind for session"));
        else if ( musig_parsehash(psig,jitem(params,2),32) < 0 )
            return(cclib_error(result,"error parsing psig"));
        else if ( secp256k1_musig_partial_signature_parse(ctx,&MUSIG->partial_sig[ind],psig) == 0 )
            return(cclib_error(result,"error parsing partialsig"));
        result.push_back(Pair("added_index",ind));
        if ( secp256k1_musig_partial_sig_combine(ctx,&MUSIG->session,&sig,MUSIG->partial_sig,MUSIG->num) > 0 )
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
            if ( secp256k1_musig_partial_signature_serialize(ctx,psig,&MUSIG->partial_sig[MUSIG->myind]) > 0 )
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

UniValue musig_verify(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t i,n; uint8_t msg[32],musig64[64]; secp256k1_pubkey combined_pk; secp256k1_schnorrsig musig; char str[129];
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 3 )
    {
        if ( musig_parsehash(msg,jitem(params,0),32) < 0 )
            return(cclib_error(result,"error parsing pkhash"));
        else if ( musig_parsepubkey(ctx,combined_pk,jitem(params,1)) < 0 )
            return(cclib_error(result,"error parsing combined_pk"));
        else if ( musig_parsehash(musig64,jitem(params,2),64) < 0 )
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
