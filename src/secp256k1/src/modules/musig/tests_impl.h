/**********************************************************************
 * Copyright (c) 2018 Andrew Poelstra                                 *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _SECP256K1_MODULE_MUSIG_TESTS_
#define _SECP256K1_MODULE_MUSIG_TESTS_

#include "secp256k1_musig.h"

void musig_api_tests(secp256k1_scratch_space *scratch) {
    secp256k1_scratch_space *scratch_small;
    secp256k1_musig_session session[2];
    secp256k1_musig_session verifier_session;
    secp256k1_musig_session_signer_data signer0[2];
    secp256k1_musig_session_signer_data signer1[2];
    secp256k1_musig_session_signer_data verifier_signer_data[2];
    secp256k1_musig_partial_signature partial_sig[2];
    secp256k1_musig_partial_signature partial_sig_adapted[2];
    secp256k1_musig_partial_signature partial_sig_overflow;
    secp256k1_schnorrsig final_sig;
    secp256k1_schnorrsig final_sig_cmp;

    unsigned char buf[32];
    unsigned char sk[2][32];
    unsigned char ones[32];
    unsigned char session_id[2][32];
    unsigned char nonce_commitment[2][32];
    int nonce_is_negated;
    const unsigned char *ncs[2];
    unsigned char msg[32];
    unsigned char msghash[32];
    secp256k1_pubkey combined_pk;
    unsigned char pk_hash[32];
    secp256k1_pubkey pk[2];

    unsigned char sec_adaptor[32];
    unsigned char sec_adaptor1[32];
    secp256k1_pubkey adaptor;

    /** setup **/
    secp256k1_context *none = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    secp256k1_context *sign = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_context *vrfy = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    int ecount;

    secp256k1_context_set_error_callback(none, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_error_callback(sign, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_error_callback(vrfy, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(none, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(sign, counting_illegal_callback_fn, &ecount);
    secp256k1_context_set_illegal_callback(vrfy, counting_illegal_callback_fn, &ecount);

    memset(ones, 0xff, 32);

    secp256k1_rand256(session_id[0]);
    secp256k1_rand256(session_id[1]);
    secp256k1_rand256(sk[0]);
    secp256k1_rand256(sk[1]);
    secp256k1_rand256(msg);
    secp256k1_rand256(sec_adaptor);

    CHECK(secp256k1_ec_pubkey_create(ctx, &pk[0], sk[0]) == 1);
    CHECK(secp256k1_ec_pubkey_create(ctx, &pk[1], sk[1]) == 1);
    CHECK(secp256k1_ec_pubkey_create(ctx, &adaptor, sec_adaptor) == 1);

    /** main test body **/

    /* Key combination */
    ecount = 0;
    CHECK(secp256k1_musig_pubkey_combine(none, scratch, &combined_pk, pk_hash, pk, 2) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_pubkey_combine(sign, scratch, &combined_pk, pk_hash, pk, 2) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, pk, 2) == 1);
    CHECK(ecount == 2);
    /* pubkey_combine does not require a scratch space */
    CHECK(secp256k1_musig_pubkey_combine(vrfy, NULL, &combined_pk, pk_hash, pk, 2) == 1);
    CHECK(ecount == 2);
    /* If a scratch space is given it shouldn't be too small */
    scratch_small = secp256k1_scratch_space_create(ctx, 1);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch_small, &combined_pk, pk_hash, pk, 2) == 0);
    secp256k1_scratch_space_destroy(scratch_small);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, NULL, pk_hash, pk, 2) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, NULL, pk, 2) == 1);
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, NULL, 2) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, pk, 0) == 0);
    CHECK(ecount == 5);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, NULL, 0) == 0);
    CHECK(ecount == 6);

    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, pk, 2) == 1);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, pk, 2) == 1);
    CHECK(secp256k1_musig_pubkey_combine(vrfy, scratch, &combined_pk, pk_hash, pk, 2) == 1);

    /** Session creation **/
    ecount = 0;
    CHECK(secp256k1_musig_session_initialize(none, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_session_initialize(vrfy, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 1);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_session_initialize(sign, NULL, signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], NULL, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, NULL, session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 5);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], NULL, msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 6);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], NULL, &combined_pk, pk_hash, 2, 0, sk[0]) == 1);
    CHECK(ecount == 6);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, NULL, pk_hash, 2, 0, sk[0]) == 0);
    CHECK(ecount == 7);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, NULL, 2, 0, sk[0]) == 0);
    CHECK(ecount == 8);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 0, 0, sk[0]) == 0);
    CHECK(ecount == 8);
    /* If more than UINT32_MAX fits in a size_t, test that session_initialize
     * rejects n_signers that high. */
    if (SIZE_MAX > UINT32_MAX) {
        CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, ((size_t) UINT32_MAX) + 2, 0, sk[0]) == 0);
    }
    CHECK(ecount == 8);
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, NULL) == 0);
    CHECK(ecount == 9);
    /* secret key overflows */
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, ones) == 0);
    CHECK(ecount == 9);


    {
        secp256k1_musig_session session_without_msg;
        CHECK(secp256k1_musig_session_initialize(sign, &session_without_msg, signer0, nonce_commitment[0], session_id[0], NULL, &combined_pk, pk_hash, 2, 0, sk[0]) == 1);
        CHECK(secp256k1_musig_session_set_msg(none, &session_without_msg, msg) == 1);
        CHECK(secp256k1_musig_session_set_msg(none, &session_without_msg, msg) == 0);
    }
    CHECK(secp256k1_musig_session_initialize(sign, &session[0], signer0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 1);
    CHECK(secp256k1_musig_session_initialize(sign, &session[1], signer1, nonce_commitment[1], session_id[1], msg, &combined_pk, pk_hash, 2, 1, sk[1]) == 1);
    ncs[0] = nonce_commitment[0];
    ncs[1] = nonce_commitment[1];

    ecount = 0;
    CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, msg, &combined_pk, pk_hash, ncs, 2) == 1);
    CHECK(ecount == 0);
    CHECK(secp256k1_musig_session_initialize_verifier(none, NULL, verifier_signer_data, msg, &combined_pk, pk_hash, ncs, 2) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, NULL, &combined_pk, pk_hash, ncs, 2) == 1);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, msg, NULL, pk_hash, ncs, 2) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, msg, &combined_pk, NULL, ncs, 2) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, msg, &combined_pk, pk_hash, NULL, 2) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, msg, &combined_pk, pk_hash, ncs, 0) == 0);
    CHECK(ecount == 4);
    if (SIZE_MAX > UINT32_MAX) {
        CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, msg, &combined_pk, pk_hash, ncs, ((size_t) UINT32_MAX) + 2) == 0);
    }
    CHECK(ecount == 4);
    CHECK(secp256k1_musig_session_initialize_verifier(none, &verifier_session, verifier_signer_data, msg, &combined_pk, pk_hash, ncs, 2) == 1);

    CHECK(secp256k1_musig_compute_messagehash(none, msghash, &verifier_session) == 0);
    CHECK(secp256k1_musig_compute_messagehash(none, msghash, &session[0]) == 0);

    /** Signing step 0 -- exchange nonce commitments */
    ecount = 0;
    {
        secp256k1_pubkey nonce;

        /* Can obtain public nonce after commitments have been exchanged; still can't sign */
        CHECK(secp256k1_musig_session_get_public_nonce(none, &session[0], signer0, &nonce, ncs, 2) == 1);
        CHECK(secp256k1_musig_partial_sign(none, &session[0], &partial_sig[0]) == 0);
        CHECK(ecount == 0);
    }

    /** Signing step 1 -- exchange nonces */
    ecount = 0;
    {
        secp256k1_pubkey public_nonce[3];

        CHECK(secp256k1_musig_session_get_public_nonce(none, &session[0], signer0, &public_nonce[0], ncs, 2) == 1);
        CHECK(ecount == 0);
        CHECK(secp256k1_musig_session_get_public_nonce(none, NULL, signer0, &public_nonce[0], ncs, 2) == 0);
        CHECK(ecount == 1);
        CHECK(secp256k1_musig_session_get_public_nonce(none, &session[0], NULL, &public_nonce[0], ncs, 2) == 0);
        CHECK(ecount == 2);
        CHECK(secp256k1_musig_session_get_public_nonce(none, &session[0], signer0, NULL, ncs, 2) == 0);
        CHECK(ecount == 3);
        CHECK(secp256k1_musig_session_get_public_nonce(none, &session[0], signer0, &public_nonce[0], NULL, 2) == 0);
        CHECK(ecount == 4);
        /* Number of commitments and number of signers are different */
        CHECK(secp256k1_musig_session_get_public_nonce(none, &session[0], signer0, &public_nonce[0], ncs, 1) == 0);
        CHECK(ecount == 4);

        CHECK(secp256k1_musig_session_get_public_nonce(none, &session[0], signer0, &public_nonce[0], ncs, 2) == 1);
        CHECK(secp256k1_musig_session_get_public_nonce(none, &session[1], signer1, &public_nonce[1], ncs, 2) == 1);

        CHECK(secp256k1_musig_set_nonce(none, &signer0[0], &public_nonce[0]) == 1);
        CHECK(secp256k1_musig_set_nonce(none, &signer0[1], &public_nonce[0]) == 0);
        CHECK(secp256k1_musig_set_nonce(none, &signer0[1], &public_nonce[1]) == 1);
        CHECK(secp256k1_musig_set_nonce(none, &signer0[1], &public_nonce[1]) == 1);
        CHECK(ecount == 4);

        CHECK(secp256k1_musig_set_nonce(none, NULL, &public_nonce[0]) == 0);
        CHECK(ecount == 5);
        CHECK(secp256k1_musig_set_nonce(none, &signer1[0], NULL) == 0);
        CHECK(ecount == 6);

        CHECK(secp256k1_musig_set_nonce(none, &signer1[0], &public_nonce[0]) == 1);
        CHECK(secp256k1_musig_set_nonce(none, &signer1[1], &public_nonce[1]) == 1);
        CHECK(secp256k1_musig_set_nonce(none, &verifier_signer_data[0], &public_nonce[0]) == 1);
        CHECK(secp256k1_musig_set_nonce(none, &verifier_signer_data[1], &public_nonce[1]) == 1);

        ecount = 0;
        CHECK(secp256k1_musig_session_combine_nonces(none, &session[0], signer0, 2, &nonce_is_negated, &adaptor) == 1);
        CHECK(secp256k1_musig_session_combine_nonces(none, NULL, signer0, 2, &nonce_is_negated, &adaptor) == 0);
        CHECK(ecount == 1);
        CHECK(secp256k1_musig_session_combine_nonces(none, &session[0], NULL, 2, &nonce_is_negated, &adaptor) == 0);
        CHECK(ecount == 2);
        /* Number of signers differs from number during intialization */
        CHECK(secp256k1_musig_session_combine_nonces(none, &session[0], signer0, 1, &nonce_is_negated, &adaptor) == 0);
        CHECK(ecount == 2);
        CHECK(secp256k1_musig_session_combine_nonces(none, &session[0], signer0, 2, NULL, &adaptor) == 1);
        CHECK(ecount == 2);
        CHECK(secp256k1_musig_session_combine_nonces(none, &session[0], signer0, 2, &nonce_is_negated, NULL) == 1);

        CHECK(secp256k1_musig_session_combine_nonces(none, &session[0], signer0, 2, &nonce_is_negated, &adaptor) == 1);
        CHECK(secp256k1_musig_session_combine_nonces(none, &session[1], signer0, 2, &nonce_is_negated, &adaptor) == 1);
        CHECK(secp256k1_musig_session_combine_nonces(none, &verifier_session, verifier_signer_data, 2, &nonce_is_negated, &adaptor) == 1);
    }

    /** Signing step 2 -- partial signatures */
    ecount = 0;
    CHECK(secp256k1_musig_partial_sign(none, &session[0], &partial_sig[0]) == 1);
    CHECK(ecount == 0);
    CHECK(secp256k1_musig_partial_sign(none, NULL, &partial_sig[0]) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_partial_sign(none, &session[0], NULL) == 0);
    CHECK(ecount == 2);

    CHECK(secp256k1_musig_partial_sign(none, &session[0], &partial_sig[0]) == 1);
    CHECK(secp256k1_musig_partial_sign(none, &session[1], &partial_sig[1]) == 1);
    /* observer can't sign */
    CHECK(secp256k1_musig_partial_sign(none, &verifier_session, &partial_sig[2]) == 0);
    CHECK(ecount == 2);

    ecount = 0;
    CHECK(secp256k1_musig_partial_signature_serialize(none, buf, &partial_sig[0]) == 1);
    CHECK(secp256k1_musig_partial_signature_serialize(none, NULL, &partial_sig[0]) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_partial_signature_serialize(none, buf, NULL) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_partial_signature_parse(none, &partial_sig[0], buf) == 1);
    CHECK(secp256k1_musig_partial_signature_parse(none, NULL, buf) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_partial_signature_parse(none, &partial_sig[0], NULL) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_musig_partial_signature_parse(none, &partial_sig_overflow, ones) == 1);

    /** Partial signature verification */
    ecount = 0;
    CHECK(secp256k1_musig_partial_sig_verify(none, &session[0], &signer0[0], &partial_sig[0], &pk[0]) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_partial_sig_verify(sign, &session[0], &signer0[0], &partial_sig[0], &pk[0]) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &session[0], &signer0[0], &partial_sig[0], &pk[0]) == 1);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &session[0], &signer0[0], &partial_sig[1], &pk[0]) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, NULL, &signer0[0], &partial_sig[0], &pk[0]) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &session[0], NULL, &partial_sig[0], &pk[0]) == 0);
    CHECK(ecount == 4);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &session[0], &signer0[0], NULL, &pk[0]) == 0);
    CHECK(ecount == 5);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &session[0], &signer0[0], &partial_sig_overflow, &pk[0]) == 0);
    CHECK(ecount == 5);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &session[0], &signer0[0], &partial_sig[0], NULL) == 0);
    CHECK(ecount == 6);

    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &session[0], &signer0[0], &partial_sig[0], &pk[0]) == 1);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &session[1], &signer1[0], &partial_sig[0], &pk[0]) == 1);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &session[0], &signer0[1], &partial_sig[1], &pk[1]) == 1);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &session[1], &signer1[1], &partial_sig[1], &pk[1]) == 1);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &verifier_session, &verifier_signer_data[0], &partial_sig[0], &pk[0]) == 1);
    CHECK(secp256k1_musig_partial_sig_verify(vrfy, &verifier_session, &verifier_signer_data[1], &partial_sig[1], &pk[1]) == 1);
    CHECK(ecount == 6);

    /** Adaptor signature verification */
    memcpy(&partial_sig_adapted[1], &partial_sig[1], sizeof(partial_sig_adapted[1]));
    ecount = 0;
    CHECK(secp256k1_musig_partial_sig_adapt(none, &partial_sig_adapted[0], &partial_sig[0], sec_adaptor, nonce_is_negated) == 1);
    CHECK(secp256k1_musig_partial_sig_adapt(none, NULL, &partial_sig[0], sec_adaptor, 0) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_partial_sig_adapt(none, &partial_sig_adapted[0], NULL, sec_adaptor, 0) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_partial_sig_adapt(none, &partial_sig_adapted[0], &partial_sig_overflow, sec_adaptor, nonce_is_negated) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_partial_sig_adapt(none, &partial_sig_adapted[0], &partial_sig[0], NULL, 0) == 0);
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_partial_sig_adapt(none, &partial_sig_adapted[0], &partial_sig[0], ones, nonce_is_negated) == 0);
    CHECK(ecount == 3);

    /** Signing combining and verification */
    ecount = 0;
    CHECK(secp256k1_musig_partial_sig_combine(none, &session[0], &final_sig, partial_sig_adapted, 2) == 1);
    CHECK(secp256k1_musig_partial_sig_combine(none, &session[0], &final_sig_cmp, partial_sig_adapted, 2) == 1);
    CHECK(memcmp(&final_sig, &final_sig_cmp, sizeof(final_sig)) == 0);
    CHECK(secp256k1_musig_partial_sig_combine(none, &session[0], &final_sig_cmp, partial_sig_adapted, 2) == 1);
    CHECK(memcmp(&final_sig, &final_sig_cmp, sizeof(final_sig)) == 0);

    CHECK(secp256k1_musig_partial_sig_combine(none, NULL, &final_sig, partial_sig_adapted, 2) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_partial_sig_combine(none, &session[0], NULL, partial_sig_adapted, 2) == 0);
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_partial_sig_combine(none, &session[0], &final_sig, NULL, 2) == 0);
    CHECK(ecount == 3);
    {
        secp256k1_musig_partial_signature partial_sig_tmp[2];
        partial_sig_tmp[0] = partial_sig_adapted[0];
        partial_sig_tmp[1] = partial_sig_overflow;
        CHECK(secp256k1_musig_partial_sig_combine(none, &session[0], &final_sig, partial_sig_tmp, 2) == 0);
    }
    CHECK(ecount == 3);
    /* Wrong number of partial sigs */
    CHECK(secp256k1_musig_partial_sig_combine(none, &session[0], &final_sig, partial_sig_adapted, 1) == 0);
    CHECK(ecount == 3);

    CHECK(secp256k1_schnorrsig_verify(vrfy, &final_sig, msg, &combined_pk) == 1);

    /** Secret adaptor can be extracted from signature */
    ecount = 0;
    CHECK(secp256k1_musig_extract_secret_adaptor(none, sec_adaptor1, &final_sig, partial_sig, 2, nonce_is_negated) == 1);
    CHECK(memcmp(sec_adaptor, sec_adaptor1, 32) == 0);
    CHECK(secp256k1_musig_extract_secret_adaptor(none, NULL, &final_sig, partial_sig, 2, 0) == 0);
    CHECK(ecount == 1);
    CHECK(secp256k1_musig_extract_secret_adaptor(none, sec_adaptor1, NULL, partial_sig, 2, 0) == 0);
    CHECK(ecount == 2);
    {
        secp256k1_schnorrsig final_sig_tmp = final_sig;
        memcpy(&final_sig_tmp.data[32], ones, 32);
        CHECK(secp256k1_musig_extract_secret_adaptor(none, sec_adaptor1, &final_sig_tmp, partial_sig, 2, nonce_is_negated) == 0);
    }
    CHECK(ecount == 2);
    CHECK(secp256k1_musig_extract_secret_adaptor(none, sec_adaptor1, &final_sig, NULL, 2, 0) == 0);
    CHECK(ecount == 3);
    {
        secp256k1_musig_partial_signature partial_sig_tmp[2];
        partial_sig_tmp[0] = partial_sig[0];
        partial_sig_tmp[1] = partial_sig_overflow;
        CHECK(secp256k1_musig_extract_secret_adaptor(none, sec_adaptor1, &final_sig, partial_sig_tmp, 2, nonce_is_negated) == 0);
    }
    CHECK(ecount == 3);
    CHECK(secp256k1_musig_extract_secret_adaptor(none, sec_adaptor1, &final_sig, partial_sig, 0, 0) == 1);
    CHECK(secp256k1_musig_extract_secret_adaptor(none, sec_adaptor1, &final_sig, partial_sig, 2, 1) == 1);

    /** cleanup **/
    memset(&session, 0, sizeof(session));
    secp256k1_context_destroy(none);
    secp256k1_context_destroy(sign);
    secp256k1_context_destroy(vrfy);
}

/* Initializes two sessions, one use the given parameters (session_id,
 * nonce_commitments, etc.) except that `session_tmp` uses new signers with different
 * public keys. The point of this test is to call `musig_session_get_public_nonce`
 * with signers from `session_tmp` who have different public keys than the correct
 * ones and return the resulting messagehash. This should not result in a different
 * messagehash because the public keys of the signers are only used during session
 * initialization. */
int musig_state_machine_diff_signer_msghash_test(unsigned char *msghash, secp256k1_pubkey *pks, secp256k1_pubkey *combined_pk, unsigned char *pk_hash, const unsigned char * const *nonce_commitments, unsigned char *msg, secp256k1_pubkey *nonce_other, unsigned char *sk, unsigned char *session_id) {
    secp256k1_musig_session session;
    secp256k1_musig_session session_tmp;
    unsigned char nonce_commitment[32];
    secp256k1_musig_session_signer_data signers[2];
    secp256k1_musig_session_signer_data signers_tmp[2];
    unsigned char sk_dummy[32];
    secp256k1_pubkey pks_tmp[2];
    secp256k1_pubkey combined_pk_tmp;
    unsigned char pk_hash_tmp[32];
    secp256k1_pubkey nonce;

    /* Set up signers with different public keys */
    secp256k1_rand256(sk_dummy);
    pks_tmp[0] = pks[0];
    CHECK(secp256k1_ec_pubkey_create(ctx, &pks_tmp[1], sk_dummy) == 1);
    CHECK(secp256k1_musig_pubkey_combine(ctx, NULL, &combined_pk_tmp, pk_hash_tmp, pks_tmp, 2) == 1);
    CHECK(secp256k1_musig_session_initialize(ctx, &session_tmp, signers_tmp, nonce_commitment, session_id, msg, &combined_pk_tmp, pk_hash_tmp, 2, 0, sk_dummy) == 1);

    CHECK(secp256k1_musig_session_initialize(ctx, &session, signers, nonce_commitment, session_id, msg, combined_pk, pk_hash, 2, 0, sk) == 1);
    CHECK(memcmp(nonce_commitment, nonce_commitments[1], 32) == 0);
    /* Call get_public_nonce with different signers than the signers the session was
     * initialized with. */
    CHECK(secp256k1_musig_session_get_public_nonce(ctx, &session_tmp, signers, &nonce, nonce_commitments, 2) == 1);
    CHECK(secp256k1_musig_session_get_public_nonce(ctx, &session, signers_tmp, &nonce, nonce_commitments, 2) == 1);
    CHECK(secp256k1_musig_set_nonce(ctx, &signers[0], nonce_other) == 1);
    CHECK(secp256k1_musig_set_nonce(ctx, &signers[1], &nonce) == 1);
    CHECK(secp256k1_musig_session_combine_nonces(ctx, &session, signers, 2, NULL, NULL) == 1);

    return secp256k1_musig_compute_messagehash(ctx, msghash, &session);
}

/* Creates a new session (with a different session id) and tries to use that session
 * to combine nonces with given signers_other. This should fail, because the nonce
 * commitments of signers_other do not match the nonce commitments the new session
 * was initialized with. If do_test is 0, the correct signers are being used and
 * therefore the function should return 1. */
int musig_state_machine_diff_signers_combine_nonce_test(secp256k1_pubkey *combined_pk, unsigned char *pk_hash, unsigned char *nonce_commitment_other, secp256k1_pubkey *nonce_other, unsigned char *msg, unsigned char *sk, secp256k1_musig_session_signer_data *signers_other, int do_test) {
    secp256k1_musig_session session;
    secp256k1_musig_session_signer_data signers[2];
    secp256k1_musig_session_signer_data *signers_to_use;
    unsigned char nonce_commitment[32];
    unsigned char session_id[32];
    secp256k1_pubkey nonce;
    const unsigned char *ncs[2];

    /* Initialize new signers */
    secp256k1_rand256(session_id);
    CHECK(secp256k1_musig_session_initialize(ctx, &session, signers, nonce_commitment, session_id, msg, combined_pk, pk_hash, 2, 1, sk) == 1);
    ncs[0] = nonce_commitment_other;
    ncs[1] = nonce_commitment;
    CHECK(secp256k1_musig_session_get_public_nonce(ctx, &session, signers, &nonce, ncs, 2) == 1);
    CHECK(secp256k1_musig_set_nonce(ctx, &signers[0], nonce_other) == 1);
    CHECK(secp256k1_musig_set_nonce(ctx, &signers[1], &nonce) == 1);
    CHECK(secp256k1_musig_set_nonce(ctx, &signers[1], &nonce) == 1);
    secp256k1_musig_session_combine_nonces(ctx, &session, signers_other, 2, NULL, NULL);
    if (do_test) {
        signers_to_use = signers_other;
    } else {
        signers_to_use = signers;
    }
    return secp256k1_musig_session_combine_nonces(ctx, &session, signers_to_use, 2, NULL, NULL);
}

/* Recreates a session with the given session_id, signers, pk, msg etc. parameters
 * and tries to sign and verify the other signers partial signature. Both should fail
 * if msg is NULL. */
int musig_state_machine_missing_msg_test(secp256k1_pubkey *pks, secp256k1_pubkey *combined_pk, unsigned char *pk_hash, unsigned char *nonce_commitment_other, secp256k1_pubkey *nonce_other, secp256k1_musig_partial_signature *partial_sig_other, unsigned char *sk, unsigned char *session_id, unsigned char *msg) {
    secp256k1_musig_session session;
    secp256k1_musig_session_signer_data signers[2];
    unsigned char nonce_commitment[32];
    const unsigned char *ncs[2];
    secp256k1_pubkey nonce;
    secp256k1_musig_partial_signature partial_sig;
    int partial_sign, partial_verify;

    CHECK(secp256k1_musig_session_initialize(ctx, &session, signers, nonce_commitment, session_id, msg, combined_pk, pk_hash, 2, 0, sk) == 1);
    ncs[0] = nonce_commitment_other;
    ncs[1] = nonce_commitment;
    CHECK(secp256k1_musig_session_get_public_nonce(ctx, &session, signers, &nonce, ncs, 2) == 1);
    CHECK(secp256k1_musig_set_nonce(ctx, &signers[0], nonce_other) == 1);
    CHECK(secp256k1_musig_set_nonce(ctx, &signers[1], &nonce) == 1);

    CHECK(secp256k1_musig_session_combine_nonces(ctx, &session, signers, 2, NULL, NULL) == 1);
    partial_sign = secp256k1_musig_partial_sign(ctx, &session, &partial_sig);
    partial_verify = secp256k1_musig_partial_sig_verify(ctx, &session, &signers[0], partial_sig_other, &pks[0]);
    if (msg != NULL) {
        /* Return 1 if both succeeded */
        return partial_sign && partial_verify;
    }
    /* Return 0 if both failed */
    return partial_sign || partial_verify;
}

/* Recreates a session with the given session_id, signers, pk, msg etc. parameters
 * and tries to verify and combine partial sigs. If do_combine is 0, the
 * combine_nonces step is left out. In that case verify and combine should fail and
 * this function should return 0. */
int musig_state_machine_missing_combine_test(secp256k1_pubkey *pks, secp256k1_pubkey *combined_pk, unsigned char *pk_hash, unsigned char *nonce_commitment_other, secp256k1_pubkey *nonce_other, secp256k1_musig_partial_signature *partial_sig_other, unsigned char *msg, unsigned char *sk, unsigned char *session_id, secp256k1_musig_partial_signature *partial_sig, int do_combine) {
    secp256k1_musig_session session;
    secp256k1_musig_session_signer_data signers[2];
    unsigned char nonce_commitment[32];
    const unsigned char *ncs[2];
    secp256k1_pubkey nonce;
    secp256k1_musig_partial_signature partial_sigs[2];
    secp256k1_schnorrsig sig;
    int partial_verify, sig_combine;

    CHECK(secp256k1_musig_session_initialize(ctx, &session, signers, nonce_commitment, session_id, msg, combined_pk, pk_hash, 2, 0, sk) == 1);
    ncs[0] = nonce_commitment_other;
    ncs[1] = nonce_commitment;
    CHECK(secp256k1_musig_session_get_public_nonce(ctx, &session, signers, &nonce, ncs, 2) == 1);
    CHECK(secp256k1_musig_set_nonce(ctx, &signers[0], nonce_other) == 1);
    CHECK(secp256k1_musig_set_nonce(ctx, &signers[1], &nonce) == 1);

    partial_sigs[0] = *partial_sig_other;
    partial_sigs[1] = *partial_sig;
    if (do_combine != 0) {
        CHECK(secp256k1_musig_session_combine_nonces(ctx, &session, signers, 2, NULL, NULL) == 1);
    }
    partial_verify = secp256k1_musig_partial_sig_verify(ctx, &session, signers, partial_sig_other, &pks[0]);
    sig_combine = secp256k1_musig_partial_sig_combine(ctx, &session, &sig, partial_sigs, 2);
    if (do_combine != 0) {
        /* Return 1 if both succeeded */
        return partial_verify && sig_combine;
    }
    /* Return 0 if both failed */
    return partial_verify || sig_combine;
}

void musig_state_machine_tests(secp256k1_scratch_space *scratch) {
    size_t i;
    secp256k1_musig_session session[2];
    secp256k1_musig_session_signer_data signers0[2];
    secp256k1_musig_session_signer_data signers1[2];
    unsigned char nonce_commitment[2][32];
    unsigned char session_id[2][32];
    unsigned char msg[32];
    unsigned char sk[2][32];
    secp256k1_pubkey pk[2];
    secp256k1_pubkey combined_pk;
    unsigned char pk_hash[32];
    secp256k1_pubkey nonce[2];
    const unsigned char *ncs[2];
    secp256k1_musig_partial_signature partial_sig[2];
    unsigned char msghash1[32];
    unsigned char msghash2[32];

    /* Run state machine with the same objects twice to test that it's allowed to
     * reinitialize session and session_signer_data. */
    for (i = 0; i < 2; i++) {
        /* Setup */
        secp256k1_rand256(session_id[0]);
        secp256k1_rand256(session_id[1]);
        secp256k1_rand256(sk[0]);
        secp256k1_rand256(sk[1]);
        secp256k1_rand256(msg);
        CHECK(secp256k1_ec_pubkey_create(ctx, &pk[0], sk[0]) == 1);
        CHECK(secp256k1_ec_pubkey_create(ctx, &pk[1], sk[1]) == 1);
        CHECK(secp256k1_musig_pubkey_combine(ctx, scratch, &combined_pk, pk_hash, pk, 2) == 1);
        CHECK(secp256k1_musig_session_initialize(ctx, &session[0], signers0, nonce_commitment[0], session_id[0], msg, &combined_pk, pk_hash, 2, 0, sk[0]) == 1);
        CHECK(secp256k1_musig_session_initialize(ctx, &session[1], signers1, nonce_commitment[1], session_id[1], msg, &combined_pk, pk_hash, 2, 1, sk[1]) == 1);

        /* Set nonce commitments */
        ncs[0] = nonce_commitment[0];
        ncs[1] = nonce_commitment[1];
        CHECK(secp256k1_musig_session_get_public_nonce(ctx, &session[0], signers0, &nonce[0], ncs, 2) == 1);
        /* Changing a nonce commitment is not okay */
        ncs[1] = (unsigned char*) "this isn't a nonce commitment...";
        CHECK(secp256k1_musig_session_get_public_nonce(ctx, &session[0], signers0, &nonce[0], ncs, 2) == 0);
        /* Repeating with the same nonce commitments is okay */
        ncs[1] = nonce_commitment[1];
        CHECK(secp256k1_musig_session_get_public_nonce(ctx, &session[0], signers0, &nonce[0], ncs, 2) == 1);

        /* Get nonce for signer 1 */
        CHECK(secp256k1_musig_session_get_public_nonce(ctx, &session[1], signers1, &nonce[1], ncs, 2) == 1);

        /* Set nonces */
        CHECK(secp256k1_musig_set_nonce(ctx, &signers0[0], &nonce[0]) == 1);
        /* Can't set nonce that doesn't match nonce commitment */
        CHECK(secp256k1_musig_set_nonce(ctx, &signers0[1], &nonce[0]) == 0);
        /* Set correct nonce */
        CHECK(secp256k1_musig_set_nonce(ctx, &signers0[1], &nonce[1]) == 1);

        /* Combine nonces */
        CHECK(secp256k1_musig_session_combine_nonces(ctx, &session[0], signers0, 2, NULL, NULL) == 1);
        /* Not everyone is present from signer 1's view */
        CHECK(secp256k1_musig_session_combine_nonces(ctx, &session[1], signers1, 2, NULL, NULL) == 0);
        /* Make everyone present */
        CHECK(secp256k1_musig_set_nonce(ctx, &signers1[0], &nonce[0]) == 1);
        CHECK(secp256k1_musig_set_nonce(ctx, &signers1[1], &nonce[1]) == 1);

        /* Can't combine nonces from signers of a different session */
        CHECK(musig_state_machine_diff_signers_combine_nonce_test(&combined_pk, pk_hash, nonce_commitment[0], &nonce[0], msg, sk[1], signers1, 1) == 0);
        CHECK(musig_state_machine_diff_signers_combine_nonce_test(&combined_pk, pk_hash, nonce_commitment[0], &nonce[0], msg, sk[1], signers1, 0) == 1);

        /* Partially sign */
        CHECK(secp256k1_musig_partial_sign(ctx, &session[0], &partial_sig[0]) == 1);
        /* Can't verify or sign until nonce is combined */
        CHECK(secp256k1_musig_partial_sig_verify(ctx, &session[1], &signers1[0], &partial_sig[0], &pk[0]) == 0);
        CHECK(secp256k1_musig_partial_sign(ctx, &session[1], &partial_sig[1]) == 0);
        CHECK(secp256k1_musig_session_combine_nonces(ctx, &session[1], signers1, 2, NULL, NULL) == 1);
        CHECK(secp256k1_musig_partial_sig_verify(ctx, &session[1], &signers1[0], &partial_sig[0], &pk[0]) == 1);
        /* messagehash should be the same as a session whose get_public_nonce was called
         * with different signers (i.e. they diff in public keys). This is because the
         * public keys of the signers is set in stone when initializing the session. */
        CHECK(secp256k1_musig_compute_messagehash(ctx, msghash1, &session[1]) == 1);
        CHECK(musig_state_machine_diff_signer_msghash_test(msghash2, pk, &combined_pk, pk_hash, ncs, msg, &nonce[0], sk[1], session_id[1]) == 1);
        CHECK(memcmp(msghash1, msghash2, 32) == 0);
        CHECK(secp256k1_musig_partial_sign(ctx, &session[1], &partial_sig[1]) == 1);
        CHECK(secp256k1_musig_partial_sig_verify(ctx, &session[1], &signers1[1], &partial_sig[1], &pk[1]) == 1);
        /* Wrong signature */
        CHECK(secp256k1_musig_partial_sig_verify(ctx, &session[1], &signers1[1], &partial_sig[0], &pk[1]) == 0);
        /* Can't sign or verify until msg is set */
        CHECK(musig_state_machine_missing_msg_test(pk, &combined_pk, pk_hash, nonce_commitment[0], &nonce[0], &partial_sig[0], sk[1], session_id[1], NULL) == 0);
        CHECK(musig_state_machine_missing_msg_test(pk, &combined_pk, pk_hash, nonce_commitment[0], &nonce[0], &partial_sig[0], sk[1], session_id[1], msg) == 1);

        /* Can't verify and combine partial sigs until nonces are combined */
        CHECK(musig_state_machine_missing_combine_test(pk, &combined_pk, pk_hash, nonce_commitment[0], &nonce[0], &partial_sig[0], msg, sk[1], session_id[1], &partial_sig[1], 0) == 0);
        CHECK(musig_state_machine_missing_combine_test(pk, &combined_pk, pk_hash, nonce_commitment[0], &nonce[0], &partial_sig[0], msg, sk[1], session_id[1], &partial_sig[1], 1) == 1);
    }
}

void scriptless_atomic_swap(secp256k1_scratch_space *scratch) {
    /* Throughout this test "a" and "b" refer to two hypothetical blockchains,
     * while the indices 0 and 1 refer to the two signers. Here signer 0 is
     * sending a-coins to signer 1, while signer 1 is sending b-coins to signer
     * 0. Signer 0 produces the adaptor signatures. */
    secp256k1_schnorrsig final_sig_a;
    secp256k1_schnorrsig final_sig_b;
    secp256k1_musig_partial_signature partial_sig_a[2];
    secp256k1_musig_partial_signature partial_sig_b_adapted[2];
    secp256k1_musig_partial_signature partial_sig_b[2];
    unsigned char sec_adaptor[32];
    unsigned char sec_adaptor_extracted[32];
    secp256k1_pubkey pub_adaptor;

    unsigned char seckey_a[2][32];
    unsigned char seckey_b[2][32];
    secp256k1_pubkey pk_a[2];
    secp256k1_pubkey pk_b[2];
    unsigned char pk_hash_a[32];
    unsigned char pk_hash_b[32];
    secp256k1_pubkey combined_pk_a;
    secp256k1_pubkey combined_pk_b;
    secp256k1_musig_session musig_session_a[2];
    secp256k1_musig_session musig_session_b[2];
    unsigned char noncommit_a[2][32];
    unsigned char noncommit_b[2][32];
    const unsigned char *noncommit_a_ptr[2];
    const unsigned char *noncommit_b_ptr[2];
    secp256k1_pubkey pubnon_a[2];
    secp256k1_pubkey pubnon_b[2];
    int nonce_is_negated_a;
    int nonce_is_negated_b;
    secp256k1_musig_session_signer_data data_a[2];
    secp256k1_musig_session_signer_data data_b[2];

    const unsigned char seed[32] = "still tired of choosing seeds...";
    const unsigned char msg32_a[32] = "this is the message blockchain a";
    const unsigned char msg32_b[32] = "this is the message blockchain b";

    /* Step 1: key setup */
    secp256k1_rand256(seckey_a[0]);
    secp256k1_rand256(seckey_a[1]);
    secp256k1_rand256(seckey_b[0]);
    secp256k1_rand256(seckey_b[1]);
    secp256k1_rand256(sec_adaptor);

    CHECK(secp256k1_ec_pubkey_create(ctx, &pk_a[0], seckey_a[0]));
    CHECK(secp256k1_ec_pubkey_create(ctx, &pk_a[1], seckey_a[1]));
    CHECK(secp256k1_ec_pubkey_create(ctx, &pk_b[0], seckey_b[0]));
    CHECK(secp256k1_ec_pubkey_create(ctx, &pk_b[1], seckey_b[1]));
    CHECK(secp256k1_ec_pubkey_create(ctx, &pub_adaptor, sec_adaptor));

    CHECK(secp256k1_musig_pubkey_combine(ctx, scratch, &combined_pk_a, pk_hash_a, pk_a, 2));
    CHECK(secp256k1_musig_pubkey_combine(ctx, scratch, &combined_pk_b, pk_hash_b, pk_b, 2));

    CHECK(secp256k1_musig_session_initialize(ctx, &musig_session_a[0], data_a, noncommit_a[0], seed, msg32_a, &combined_pk_a, pk_hash_a, 2, 0, seckey_a[0]));
    CHECK(secp256k1_musig_session_initialize(ctx, &musig_session_a[1], data_a, noncommit_a[1], seed, msg32_a, &combined_pk_a, pk_hash_a, 2, 1, seckey_a[1]));
    noncommit_a_ptr[0] = noncommit_a[0];
    noncommit_a_ptr[1] = noncommit_a[1];

    CHECK(secp256k1_musig_session_initialize(ctx, &musig_session_b[0], data_b, noncommit_b[0], seed, msg32_b, &combined_pk_b, pk_hash_b, 2, 0, seckey_b[0]));
    CHECK(secp256k1_musig_session_initialize(ctx, &musig_session_b[1], data_b, noncommit_b[1], seed, msg32_b, &combined_pk_b, pk_hash_b, 2, 1, seckey_b[1]));
    noncommit_b_ptr[0] = noncommit_b[0];
    noncommit_b_ptr[1] = noncommit_b[1];

    /* Step 2: Exchange nonces */
    CHECK(secp256k1_musig_session_get_public_nonce(ctx, &musig_session_a[0], data_a, &pubnon_a[0], noncommit_a_ptr, 2));
    CHECK(secp256k1_musig_session_get_public_nonce(ctx, &musig_session_a[1], data_a, &pubnon_a[1], noncommit_a_ptr, 2));
    CHECK(secp256k1_musig_session_get_public_nonce(ctx, &musig_session_b[0], data_b, &pubnon_b[0], noncommit_b_ptr, 2));
    CHECK(secp256k1_musig_session_get_public_nonce(ctx, &musig_session_b[1], data_b, &pubnon_b[1], noncommit_b_ptr, 2));
    CHECK(secp256k1_musig_set_nonce(ctx, &data_a[0], &pubnon_a[0]));
    CHECK(secp256k1_musig_set_nonce(ctx, &data_a[1], &pubnon_a[1]));
    CHECK(secp256k1_musig_set_nonce(ctx, &data_b[0], &pubnon_b[0]));
    CHECK(secp256k1_musig_set_nonce(ctx, &data_b[1], &pubnon_b[1]));
    CHECK(secp256k1_musig_session_combine_nonces(ctx, &musig_session_a[0], data_a, 2, &nonce_is_negated_a, &pub_adaptor));
    CHECK(secp256k1_musig_session_combine_nonces(ctx, &musig_session_a[1], data_a, 2, NULL, &pub_adaptor));
    CHECK(secp256k1_musig_session_combine_nonces(ctx, &musig_session_b[0], data_b, 2, &nonce_is_negated_b, &pub_adaptor));
    CHECK(secp256k1_musig_session_combine_nonces(ctx, &musig_session_b[1], data_b, 2, NULL, &pub_adaptor));

    /* Step 3: Signer 0 produces partial signatures for both chains. */
    CHECK(secp256k1_musig_partial_sign(ctx, &musig_session_a[0], &partial_sig_a[0]));
    CHECK(secp256k1_musig_partial_sign(ctx, &musig_session_b[0], &partial_sig_b[0]));

    /* Step 4: Signer 1 receives partial signatures, verifies them and creates a
     * partial signature to send B-coins to signer 0. */
    CHECK(secp256k1_musig_partial_sig_verify(ctx, &musig_session_a[1], data_a, &partial_sig_a[0], &pk_a[0]) == 1);
    CHECK(secp256k1_musig_partial_sig_verify(ctx, &musig_session_b[1], data_b, &partial_sig_b[0], &pk_b[0]) == 1);
    CHECK(secp256k1_musig_partial_sign(ctx, &musig_session_b[1], &partial_sig_b[1]));

    /* Step 5: Signer 0 adapts its own partial signature and combines it with the
     * partial signature from signer 1. This results in a complete signature which
     * is broadcasted by signer 0 to take B-coins. */
    CHECK(secp256k1_musig_partial_sig_adapt(ctx, &partial_sig_b_adapted[0], &partial_sig_b[0], sec_adaptor, nonce_is_negated_b));
    memcpy(&partial_sig_b_adapted[1], &partial_sig_b[1], sizeof(partial_sig_b_adapted[1]));
    CHECK(secp256k1_musig_partial_sig_combine(ctx, &musig_session_b[0], &final_sig_b, partial_sig_b_adapted, 2) == 1);
    CHECK(secp256k1_schnorrsig_verify(ctx, &final_sig_b, msg32_b, &combined_pk_b) == 1);

    /* Step 6: Signer 1 extracts adaptor from the published signature, applies it to
     * other partial signature, and takes A-coins. */
    CHECK(secp256k1_musig_extract_secret_adaptor(ctx, sec_adaptor_extracted, &final_sig_b, partial_sig_b, 2, nonce_is_negated_b) == 1);
    CHECK(memcmp(sec_adaptor_extracted, sec_adaptor, sizeof(sec_adaptor)) == 0); /* in real life we couldn't check this, of course */
    CHECK(secp256k1_musig_partial_sig_adapt(ctx, &partial_sig_a[0], &partial_sig_a[0], sec_adaptor_extracted, nonce_is_negated_a));
    CHECK(secp256k1_musig_partial_sign(ctx, &musig_session_a[1], &partial_sig_a[1]));
    CHECK(secp256k1_musig_partial_sig_combine(ctx, &musig_session_a[1], &final_sig_a, partial_sig_a, 2) == 1);
    CHECK(secp256k1_schnorrsig_verify(ctx, &final_sig_a, msg32_a, &combined_pk_a) == 1);
}

/* Checks that hash initialized by secp256k1_musig_sha256_init_tagged has the
 * expected state. */
void sha256_tag_test(void) {
    char tag[17] = "MuSig coefficient";
    secp256k1_sha256 sha;
    secp256k1_sha256 sha_tagged;
    unsigned char buf[32];
    unsigned char buf2[32];
    size_t i;

    secp256k1_sha256_initialize(&sha);
    secp256k1_sha256_write(&sha, (unsigned char *) tag, 17);
    secp256k1_sha256_finalize(&sha, buf);
    /* buf = SHA256("MuSig coefficient") */

    secp256k1_sha256_initialize(&sha);
    secp256k1_sha256_write(&sha, buf, 32);
    secp256k1_sha256_write(&sha, buf, 32);
    /* Is buffer fully consumed? */
    CHECK((sha.bytes & 0x3F) == 0);

    /* Compare with tagged SHA */
    secp256k1_musig_sha256_init_tagged(&sha_tagged);
    for (i = 0; i < 8; i++) {
        CHECK(sha_tagged.s[i] == sha.s[i]);
    }
    secp256k1_sha256_write(&sha, buf, 32);
    secp256k1_sha256_write(&sha_tagged, buf, 32);
    secp256k1_sha256_finalize(&sha, buf);
    secp256k1_sha256_finalize(&sha_tagged, buf2);
    CHECK(memcmp(buf, buf2, 32) == 0);
}

void run_musig_tests(void) {
    int i;
    secp256k1_scratch_space *scratch = secp256k1_scratch_space_create(ctx, 1024 * 1024);

    musig_api_tests(scratch);
    musig_state_machine_tests(scratch);
    for (i = 0; i < count; i++) {
        /* Run multiple times to ensure that the nonce is negated in some tests */
        scriptless_atomic_swap(scratch);
    }
    sha256_tag_test();

    secp256k1_scratch_space_destroy(scratch);
}

#endif

