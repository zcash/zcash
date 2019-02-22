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
*/

/* second, send 0.777 coins to the combined_pk
 ./c cclib send 18 \"[%22032ddac56613cd0667b589bd7f32b665e2d2ce0247e337a5a0bca6c72e3d9d057b%22,0.777]\"
 {
 "hex": "0400008085202f8901bf7afcbbab7a5e1ba20c8010325e199305afa7d15b4bc86a589a512201d39924000000004847304402203e0cff8aaa831dd990afd979bbc0a648316f77407cb5e02780d18e0670f5172a0220283d5c52d0406b10c508cc2b19dd6dbe2420e7c5cf431a1d41072d0eec28edb901ffffffff03b0c2a10400000000302ea22c8020c71ddb3aac7f9b9e4bdacf032aaa8b8e4433c4ff9f8a43cebb9c1f5da96928a48103120c008203000401cca91e223700000000232102aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848ac0000000000000000266a24127821032ddac56613cd0667b589bd7f32b665e2d2ce0247e337a5a0bca6c72e3d9d057b00000000900000000000000000000000000000",
 "txid": "cb5309ed249da95e2b5696eb763a8736e2fff1d14922ada737b931494ca3d2be",
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
 
 sendtxid: cb5309ed249da95e2b5696eb763a8736e2fff1d14922ada737b931494ca3d2be
 
 broadcast sendtxid and wait for it to be confirmed. then get the msg we need to sign:
 
 ./c cclib calcmsg 18 \"[%22cb5309ed249da95e2b5696eb763a8736e2fff1d14922ada737b931494ca3d2be%22,%222102aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848ac%22]\"
 
 {
 "result": "success",
 "msg": "63b799913d4c9487f321b32d6ae8614f653f38e0b50d4df4bc1d36339ea18485"
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

#define MUSIG_PREVN 0   // for now, just use vout0 for the musig output
#define MUSIG_TXFEE 10000

struct musig_info
{
    secp256k1_musig_session musig_session;
    secp256k1_pubkey combined_pk;
    uint8_t *nonce_commitments; // 32*N_SIGNERS
    secp256k1_musig_session_signer_data *signer_data; //[N_SIGNERS];
    secp256k1_pubkey *nonce; //[N_SIGNERS];
    secp256k1_musig_partial_signature *partial_sig; //[N_SIGNERS];
    int32_t myind,num;
    uint8_t msg[32],pkhash[32],combpk[33];
} *MUSIG;

struct musig_info *musig_infocreate(int32_t myind,int32_t num)
{
    struct musig_info *mp = (struct musig_info *)calloc(1,sizeof(*mp));
    mp->myind = myind, mp->num = num;
    mp->nonce_commitments = (uint8_t *)calloc(num,32);
    mp->signer_data = (secp256k1_musig_session_signer_data *)calloc(num,sizeof(*mp->signer_data));
    mp->nonce = (secp256k1_pubkey *)calloc(num,sizeof(*mp->nonce));
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
    if ( mp->nonce != 0 )
    {
        GetRandBytes((uint8_t *)mp->nonce,mp->num*sizeof(*mp->nonce));
        free(mp->nonce);
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
            if ( (hexstr= jstr(jitem(params,i),0)) != 0 && is_hexstr(hexstr,0) == 66 )
            {
                CPubKey pk(ParseHex(hexstr));
                if ( secp256k1_ec_pubkey_parse(ctx,&spk,pk.begin(),33) > 0 )
                    pubkeys.push_back(spk);
                else return(cclib_error(result,"error parsing pk"));
            } else return(cclib_error(result,"all pubkeys must be 33 bytes hexdata"));
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
        // set the 5 args: myind, num, pub33, pkhash32, msg32
        if ( MUSIG != 0 )
            musig_infofree(MUSIG), MUSIG = 0;
        MUSIG = musig_infocreate(myind,num);
        pk = buf2pk(pub33);
        if ( secp256k1_ec_pubkey_parse(ctx,&MUSIG->combined_pk,pk.begin(),33) > 0 )
        {
            memcpy(MUSIG->pkhash,pkhash,sizeof(pkhash));
            memcpy(MUSIG->msg,msg,sizeof(msg));
            GetRandBytes(session,32);
            Myprivkey(privkey);
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
            if ( secp256k1_musig_session_initialize(ctx,&MUSIG->musig_session,MUSIG->signer_data, &MUSIG->nonce_commitments[MUSIG->myind * 32],session,MUSIG->msg,&MUSIG->combined_pk,MUSIG->pkhash,MUSIG->num,MUSIG->myind,privkey) > 0 )
            {
                result.push_back(Pair("myind",(int64_t)myind));
                result.push_back(Pair("numsigners",(int64_t)num));
                for (i=0; i<32; i++)
                    sprintf(&str[i<<1],"%02x",MUSIG->nonce_commitments[MUSIG->myind*32 + i]);
                str[64] = 0;
                result.push_back(Pair("commitment",str));
                result.push_back(Pair("result","success"));
                return(result);
            } else return(cclib_error(result,"couldnt initialize session"));
        } else return(cclib_error(result,"couldnt parse combined pubkey"));
    } else return(cclib_error(result,"wrong number of params, need 5: myindex, numsigners, combined_pk, pkhash, msg32"));
}

UniValue musig_commit(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t n;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 3 )
    {
        // pkhash, ind, commitment
        // if all commitments, emit nonce, else just update MUSIG->

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
        // Set nonce commitments in the signer data and get the own public nonce
        //if (secp256k1_musig_session_get_public_nonce(ctx, &musig_session[i], signer_data[i], &nonce[i], nonce_commitment_ptr, N_SIGNERS) > 0 )
        {
            // publish nonce
            result.push_back(Pair("result","success"));
        }
        // return 0;
    } else return(cclib_error(result,"wrong number of params, need 3: pkhash, ind, commitment"));
}

UniValue musig_nonce(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t n;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        // pkhash, ind, nonce
        // if all nonces, combine nonce, else just update MUSIG->
        result.push_back(Pair("result","success"));
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
        //if (!secp256k1_musig_set_nonce(ctx, &signer_data[i][j], &nonce[j])) {
        
        
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
        // after all nonces: if (!secp256k1_musig_session_combine_nonces(ctx, &musig_session[i], signer_data[i], N_SIGNERS, NULL, NULL)) {
        return(result);
    } else return(cclib_error(result,"wrong number of params, need 3: pkhash, ind, nonce"));
}

UniValue musig_partialsign(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t n;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        // similar to commit/nonce
    }
    result.push_back(Pair("result","success"));
    /** Produces a partial signature
     *
     *  Returns: 1: partial signature constructed
     *           0: session in incorrect or inconsistent state
     *  Args:         ctx: pointer to a context object (cannot be NULL)
     *            session: active signing session for which the combined nonce has been
     *                     computed (cannot be NULL)
     *  Out:  partial_sig: partial signature (cannot be NULL)
     */
    //if (!secp256k1_musig_partial_sign(ctx, &musig_session[i], &partial_sig[i])) {
    /** Parse and verify a MuSig partial signature.
     *
     *  Returns: 1 when the signature could be parsed, 0 otherwise.
     *  Args:    ctx: a secp256k1 context object
     *  Out:     sig: pointer to a signature object
     *  In:     in32: pointer to the 32-byte signature to be parsed
     *
     *  After the call, sig will always be initialized. If parsing failed or the
     *  encoded numbers are out of range, signature verification with it is
     *  guaranteed to fail for every message and public key.
     */
    //SECP256K1_API int secp256k1_musig_partial_signature_parse(

    return(result);
}

UniValue musig_sigcombine(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t n;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        // finally!
    }
    result.push_back(Pair("result","success"));
    /** Checks that an individual partial signature verifies
     *
     *  This function is essential when using protocols with adaptor signatures.
     *  However, it is not essential for regular MuSig's, in the sense that if any
     *  partial signatures does not verify, the full signature will also not verify, so the
     *  problem will be caught. But this function allows determining the specific party
     *  who produced an invalid signature, so that signing can be restarted without them.
     *
     *  Returns: 1: partial signature verifies
     *           0: invalid signature or bad data
     *  Args:         ctx: pointer to a context object (cannot be NULL)
     *            session: active session for which the combined nonce has been computed
     *                     (cannot be NULL)
     *             signer: data for the signer who produced this signature (cannot be NULL)
     *  In:   partial_sig: signature to verify (cannot be NULL)
     *             pubkey: public key of the signer who produced the signature (cannot be NULL)
     */
    //if (!secp256k1_musig_partial_sig_verify(ctx, &musig_session[i], &signer_data[i][j], &partial_sig[j], &pubkeys[j])) {
        return 0;
    /** Combines partial signatures
     *
     *  Returns: 1: all partial signatures have values in range. Does NOT mean the
     *              resulting signature verifies.
     *           0: some partial signature had s/r out of range
     *  Args:         ctx: pointer to a context object (cannot be NULL)
     *            session: initialized session for which the combined nonce has been
     *                     computed (cannot be NULL)
     *  Out:          sig: complete signature (cannot be NULL)
     *  In:  partial_sigs: array of partial signatures to combine (cannot be NULL)
     *             n_sigs: number of signatures in the partial_sigs array
     */
   // after all partials: return secp256k1_musig_partial_sig_combine(ctx, &musig_session[0], sig, partial_sig, N_SIGNERS return(result);
}

UniValue musig_verify(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    static secp256k1_context *ctx;
    UniValue result(UniValue::VOBJ); int32_t n;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        // can code this out of order
    }
    result.push_back(Pair("result","success"));
    /** Verify a Schnorr signature.
     *
     *  Returns: 1: correct signature
     *           0: incorrect or unparseable signature
     *  Args:    ctx: a secp256k1 context object, initialized for verification.
     *  In:      sig: the signature being verified (cannot be NULL)
     *         msg32: the 32-byte message hash being verified (cannot be NULL)
     *        pubkey: pointer to a public key to verify with (cannot be NULL)
     */
    // if (!secp256k1_schnorrsig_verify(ctx, &sig, msg, &combined_pk)) {
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
    UniValue result(UniValue::VOBJ); std::string rawtx; CPubKey mypk,pk; secp256k1_pubkey combined_pk; char *scriptstr,*musigstr; uint8_t msg[32]; CTransaction vintx; uint256 prevhash,hashBlock; int32_t n,numvouts; CTxOut vout; secp256k1_schnorrsig musig;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n == 3 )
        {
            prevhash = juint256(jitem(params,0));
            scriptstr = jstr(jitem(params,1),0);
            musigstr = jstr(jitem(params,2),0);
            if ( is_hexstr(scriptstr,0) != 0 && is_hexstr(musigstr,0) != 128 )
            {
                if ( txfee == 0 )
                    txfee = MUSIG_TXFEE;
                mypk = pubkey2pk(Mypubkey());
                std::vector<uint8_t> musig64(ParseHex(musigstr));
                CScript scriptPubKey(ParseHex(scriptstr));
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
                            if ( !secp256k1_schnorrsig_verify((const secp256k1_context *)ctx,&musig,(const uint8_t *)msg,(const secp256k1_pubkey *)&combined_pk) )
                                return(cclib_error(result,"musig didnt validate"));
                            mtx.vin.push_back(CTxIn(prevhash,MUSIG_PREVN));
                            mtx.vout.push_back(vout);
                            rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,musig_spendopret('y',pk,musig64));
                            return(musig_rawtxresult(result,rawtx));
                        } else return(cclib_error(result,"couldnt parse pk or musig"));
                    } else return(cclib_error(result,"couldnt decode send opret"));
                } else return(cclib_error(result,"couldnt find vin0"));
            } else return(cclib_error(result,"script or musig is not hex"));
        } else return(cclib_error(result,"need to have exactly 3 params prevhash, scriptPubKey, musig"));
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
