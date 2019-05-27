/**********************************************************************
 * Copyright (c) 2018 Jonas Nick                                      *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

/**
 * This file demonstrates how to use the MuSig module to create a multisignature.
 * Additionally, see the documentation in include/secp256k1_musig.h.
 */

/*#include <stdio.h>
#include <assert.h>
#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>
#include <secp256k1_musig.h>*/

 /* Number of public keys involved in creating the aggregate signature */
#define N_SIGNERS 3
 /* Create a key pair and store it in seckey and pubkey */
int create_key(const secp256k1_context* ctx, unsigned char* seckey, secp256k1_pubkey* pubkey) {
    int ret;
    FILE *frand = fopen("/dev/urandom", "r");
    if (frand == NULL) {
        return 0;
    }
    do {
         if(!fread(seckey, 32, 1, frand)) {
             fclose(frand);
             return 0;
         }
    /* The probability that this not a valid secret key is approximately 2^-128 */
    } while (!secp256k1_ec_seckey_verify(ctx, seckey));
    fclose(frand);
    ret = secp256k1_ec_pubkey_create(ctx, pubkey, seckey);
    return ret;
}

/* Sign a message hash with the given key pairs and store the result in sig */
int sign(const secp256k1_context* ctx, unsigned char seckeys[][32], const secp256k1_pubkey* pubkeys, const unsigned char* msg32, secp256k1_schnorrsig *sig) {
    secp256k1_musig_session musig_session[N_SIGNERS];
    unsigned char nonce_commitment[N_SIGNERS][32];
    const unsigned char *nonce_commitment_ptr[N_SIGNERS];
    secp256k1_musig_session_signer_data signer_data[N_SIGNERS][N_SIGNERS];
    secp256k1_pubkey nonce[N_SIGNERS];
    int i, j;
    secp256k1_musig_partial_signature partial_sig[N_SIGNERS];

    for (i = 0; i < N_SIGNERS; i++) {
        FILE *frand;
        unsigned char session_id32[32];
        unsigned char pk_hash[32];
        secp256k1_pubkey combined_pk;

        /* Create combined pubkey and initialize signer data */
        if (!secp256k1_musig_pubkey_combine(ctx, NULL, &combined_pk, pk_hash, pubkeys, N_SIGNERS)) {
            return 0;
        }
        /* Create random session ID. It is absolutely necessary that the session ID
         * is unique for every call of secp256k1_musig_session_initialize. Otherwise
         * it's trivial for an attacker to extract the secret key! */
        frand = fopen("/dev/urandom", "r");
        if(frand == NULL) {
            return 0;
        }
        if (!fread(session_id32, 32, 1, frand)) {
            fclose(frand);
            return 0;
        }
        fclose(frand);
        /* Initialize session */
        if (!secp256k1_musig_session_initialize(ctx, &musig_session[i], signer_data[i], nonce_commitment[i], session_id32, msg32, &combined_pk, pk_hash, N_SIGNERS, i, seckeys[i])) {
            return 0;
        }
        nonce_commitment_ptr[i] = &nonce_commitment[i][0];
    }
    /* Communication round 1: Exchange nonce commitments */
    for (i = 0; i < N_SIGNERS; i++) {
        /* Set nonce commitments in the signer data and get the own public nonce */
        if (!secp256k1_musig_session_get_public_nonce(ctx, &musig_session[i], signer_data[i], &nonce[i], nonce_commitment_ptr, N_SIGNERS)) {
            return 0;
        }
    }
    /* Communication round 2: Exchange nonces */
    for (i = 0; i < N_SIGNERS; i++) {
        for (j = 0; j < N_SIGNERS; j++) {
            if (!secp256k1_musig_set_nonce(ctx, &signer_data[i][j], &nonce[j])) {
                /* Signer j's nonce does not match the nonce commitment. In this case
                 * abort the protocol. If you make another attempt at finishing the
                 * protocol, create a new session (with a fresh session ID!). */
                return 0;
            }
        }
        if (!secp256k1_musig_session_combine_nonces(ctx, &musig_session[i], signer_data[i], N_SIGNERS, NULL, NULL)) {
            return 0;
        }
    }
    for (i = 0; i < N_SIGNERS; i++) {
        if (!secp256k1_musig_partial_sign(ctx, &musig_session[i], &partial_sig[i])) {
            return 0;
        }
    }
    /* Communication round 3: Exchange partial signatures */
    for (i = 0; i < N_SIGNERS; i++) {
        for (j = 0; j < N_SIGNERS; j++) {
            /* To check whether signing was successful, it suffices to either verify
             * the the combined signature with the combined public key using
             * secp256k1_schnorrsig_verify, or verify all partial signatures of all
             * signers individually. Verifying the combined signature is cheaper but
             * verifying the individual partial signatures has the advantage that it
             * can be used to determine which of the partial signatures are invalid
             * (if any), i.e., which of the partial signatures cause the combined
             * signature to be invalid and thus the protocol run to fail. It's also
             * fine to first verify the combined sig, and only verify the individual
             * sigs if it does not work.
             */
            if (!secp256k1_musig_partial_sig_verify(ctx, &musig_session[i], &signer_data[i][j], &partial_sig[j], &pubkeys[j])) {
                return 0;
            }
        }
    }
    return secp256k1_musig_partial_sig_combine(ctx, &musig_session[0], sig, partial_sig, N_SIGNERS);
}

 int testmain(void) {
    secp256k1_context* ctx;
    int i;
    unsigned char seckeys[N_SIGNERS][32];
    secp256k1_pubkey pubkeys[N_SIGNERS];
    secp256k1_pubkey combined_pk;
    unsigned char msg[32] = "this_could_be_the_hash_of_a_msg";
    secp256k1_schnorrsig sig;

    /* Create a context for signing and verification */
    ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    fprintf(stderr,"Creating key pairs......");
    for (i = 0; i < N_SIGNERS; i++) {
        if (!create_key(ctx, seckeys[i], &pubkeys[i])) {
            fprintf(stderr,"FAILED\n");
            return 1;
        }
    }
    fprintf(stderr,"ok\n");
    fprintf(stderr,"Combining public keys...");
    if (!secp256k1_musig_pubkey_combine(ctx, NULL, &combined_pk, NULL, pubkeys, N_SIGNERS)) {
        fprintf(stderr,"FAILED\n");
        return 1;
    }
    fprintf(stderr,"ok\n");
    fprintf(stderr,"Signing message.........");
    if (!sign(ctx, seckeys, pubkeys, msg, &sig)) {
        fprintf(stderr,"FAILED\n");
        return 1;
    }
    fprintf(stderr,"ok\n");
    fprintf(stderr,"Verifying signature.....");
    if (!secp256k1_schnorrsig_verify(ctx, &sig, msg, &combined_pk)) {
        fprintf(stderr,"FAILED\n");
        return 1;
    }
    fprintf(stderr,"ok\n");
    secp256k1_context_destroy(ctx);
    return 0;
}


