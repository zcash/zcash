
/**********************************************************************
 * Copyright (c) 2018 Andrew Poelstra, Jonas Nick                     *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _SECP256K1_MODULE_MUSIG_MAIN_
#define _SECP256K1_MODULE_MUSIG_MAIN_

#include "../../../include/secp256k1.h"
#include "../../../include/secp256k1_musig.h"
#include "hash.h"

/* Computes ell = SHA256(pk[0], ..., pk[np-1]) */
static int secp256k1_musig_compute_ell(const secp256k1_context *ctx, unsigned char *ell, const secp256k1_pubkey *pk, size_t np) {
    secp256k1_sha256 sha;
    size_t i;

    secp256k1_sha256_initialize(&sha);
    for (i = 0; i < np; i++) {
        unsigned char ser[33];
        size_t serlen = sizeof(ser);
        if (!secp256k1_ec_pubkey_serialize(ctx, ser, &serlen, &pk[i], SECP256K1_EC_COMPRESSED)) {
            return 0;
        }
        secp256k1_sha256_write(&sha, ser, serlen);
    }
    secp256k1_sha256_finalize(&sha, ell);
    return 1;
}

/* Initializes SHA256 with fixed midstate. This midstate was computed by applying
 * SHA256 to SHA256("MuSig coefficient")||SHA256("MuSig coefficient"). */
static void secp256k1_musig_sha256_init_tagged(secp256k1_sha256 *sha) {
    secp256k1_sha256_initialize(sha);

    sha->s[0] = 0x0fd0690cul;
    sha->s[1] = 0xfefeae97ul;
    sha->s[2] = 0x996eac7ful;
    sha->s[3] = 0x5c30d864ul;
    sha->s[4] = 0x8c4a0573ul;
    sha->s[5] = 0xaca1a22ful;
    sha->s[6] = 0x6f43b801ul;
    sha->s[7] = 0x85ce27cdul;
    sha->bytes = 64;
}

/* Compute r = SHA256(ell, idx). The four bytes of idx are serialized least significant byte first. */
static void secp256k1_musig_coefficient(secp256k1_scalar *r, const unsigned char *ell, uint32_t idx) {
    secp256k1_sha256 sha;
    unsigned char buf[32];
    size_t i;

    secp256k1_musig_sha256_init_tagged(&sha);
    secp256k1_sha256_write(&sha, ell, 32);
    /* We're hashing the index of the signer instead of its public key as specified
     * in the MuSig paper. This reduces the total amount of data that needs to be
     * hashed.
     * Additionally, it prevents creating identical musig_coefficients for identical
     * public keys. A participant Bob could choose his public key to be the same as
     * Alice's, then replay Alice's messages (nonce and partial signature) to create
     * a valid partial signature. This is not a problem for MuSig per se, but could
     * result in subtle issues with protocols building on threshold signatures.
     * With the assumption that public keys are unique, hashing the index is
     * equivalent to hashing the public key. Because the public key can be
     * identified by the index given the ordered list of public keys (included in
     * ell), the index is just a different encoding of the public key.*/
    for (i = 0; i < sizeof(uint32_t); i++) {
        unsigned char c = idx;
        secp256k1_sha256_write(&sha, &c, 1);
        idx >>= 8;
    }
    secp256k1_sha256_finalize(&sha, buf);
    secp256k1_scalar_set_b32(r, buf, NULL);
}

typedef struct {
    const secp256k1_context *ctx;
    unsigned char ell[32];
    const secp256k1_pubkey *pks;
} secp256k1_musig_pubkey_combine_ecmult_data;

/* Callback for batch EC multiplication to compute ell_0*P0 + ell_1*P1 + ...  */
static int secp256k1_musig_pubkey_combine_callback(secp256k1_scalar *sc, secp256k1_ge *pt, size_t idx, void *data) {
    secp256k1_musig_pubkey_combine_ecmult_data *ctx = (secp256k1_musig_pubkey_combine_ecmult_data *) data;
    secp256k1_musig_coefficient(sc, ctx->ell, idx);
    return secp256k1_pubkey_load(ctx->ctx, pt, &ctx->pks[idx]);
}


static void secp256k1_musig_signers_init(secp256k1_musig_session_signer_data *signers, uint32_t n_signers) {
    uint32_t i;
    for (i = 0; i < n_signers; i++) {
        memset(&signers[i], 0, sizeof(signers[i]));
        signers[i].index = i;
        signers[i].present = 0;
    }
}

int secp256k1_musig_pubkey_combine(const secp256k1_context* ctx, secp256k1_scratch_space *scratch, secp256k1_pubkey *combined_pk, unsigned char *pk_hash32, const secp256k1_pubkey *pubkeys, size_t n_pubkeys) {
    secp256k1_musig_pubkey_combine_ecmult_data ecmult_data;
    secp256k1_gej pkj;
    secp256k1_ge pkp;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(combined_pk != NULL);
    ARG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    ARG_CHECK(pubkeys != NULL);
    ARG_CHECK(n_pubkeys > 0);

    ecmult_data.ctx = ctx;
    ecmult_data.pks = pubkeys;
    if (!secp256k1_musig_compute_ell(ctx, ecmult_data.ell, pubkeys, n_pubkeys)) {
        return 0;
    }
    if (!secp256k1_ecmult_multi_var(&ctx->ecmult_ctx, scratch, &pkj, NULL, secp256k1_musig_pubkey_combine_callback, (void *) &ecmult_data, n_pubkeys)) {
        return 0;
    }
    secp256k1_ge_set_gej(&pkp, &pkj);
    secp256k1_pubkey_save(combined_pk, &pkp);

    if (pk_hash32 != NULL) {
        memcpy(pk_hash32, ecmult_data.ell, 32);
    }
    return 1;
}

int secp256k1_musig_session_initialize(const secp256k1_context* ctx, secp256k1_musig_session *session, secp256k1_musig_session_signer_data *signers, unsigned char *nonce_commitment32, const unsigned char *session_id32, const unsigned char *msg32, const secp256k1_pubkey *combined_pk, const unsigned char *pk_hash32, size_t n_signers, size_t my_index, const unsigned char *seckey) {
    unsigned char combined_ser[33];
    size_t combined_ser_size = sizeof(combined_ser);
    int overflow;
    secp256k1_scalar secret;
    secp256k1_scalar mu;
    secp256k1_sha256 sha;
    secp256k1_gej rj;
    secp256k1_ge rp;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));
    ARG_CHECK(session != NULL);
    ARG_CHECK(signers != NULL);
    ARG_CHECK(nonce_commitment32 != NULL);
    ARG_CHECK(session_id32 != NULL);
    ARG_CHECK(combined_pk != NULL);
    ARG_CHECK(pk_hash32 != NULL);
    ARG_CHECK(seckey != NULL);

    memset(session, 0, sizeof(*session));

    if (msg32 != NULL) {
        memcpy(session->msg, msg32, 32);
        session->msg_is_set = 1;
    } else {
        session->msg_is_set = 0;
    }
    memcpy(&session->combined_pk, combined_pk, sizeof(*combined_pk));
    memcpy(session->pk_hash, pk_hash32, 32);
    session->nonce_is_set = 0;
    session->has_secret_data = 1;
    if (n_signers == 0 || my_index >= n_signers) {
        return 0;
    }
    if (n_signers > UINT32_MAX) {
        return 0;
    }
    session->n_signers = (uint32_t) n_signers;
    secp256k1_musig_signers_init(signers, session->n_signers);
    session->nonce_commitments_hash_is_set = 0;

    /* Compute secret key */
    secp256k1_scalar_set_b32(&secret, seckey, &overflow);
    if (overflow) {
        secp256k1_scalar_clear(&secret);
        return 0;
    }
    secp256k1_musig_coefficient(&mu, pk_hash32, (uint32_t) my_index);
    secp256k1_scalar_mul(&secret, &secret, &mu);
    secp256k1_scalar_get_b32(session->seckey, &secret);

    /* Compute secret nonce */
    secp256k1_sha256_initialize(&sha);
    secp256k1_sha256_write(&sha, session_id32, 32);
    if (session->msg_is_set) {
        secp256k1_sha256_write(&sha, msg32, 32);
    }
    secp256k1_ec_pubkey_serialize(ctx, combined_ser, &combined_ser_size, combined_pk, SECP256K1_EC_COMPRESSED);
    secp256k1_sha256_write(&sha, combined_ser, combined_ser_size);
    secp256k1_sha256_write(&sha, seckey, 32);
    secp256k1_sha256_finalize(&sha, session->secnonce);
    secp256k1_scalar_set_b32(&secret, session->secnonce, &overflow);
    if (overflow) {
        secp256k1_scalar_clear(&secret);
        return 0;
    }

    /* Compute public nonce and commitment */
    secp256k1_ecmult_gen(&ctx->ecmult_gen_ctx, &rj, &secret);
    secp256k1_ge_set_gej(&rp, &rj);
    secp256k1_pubkey_save(&session->nonce, &rp);

    if (nonce_commitment32 != NULL) {
        unsigned char commit[33];
        size_t commit_size = sizeof(commit);
        secp256k1_sha256_initialize(&sha);
        secp256k1_ec_pubkey_serialize(ctx, commit, &commit_size, &session->nonce, SECP256K1_EC_COMPRESSED);
        secp256k1_sha256_write(&sha, commit, commit_size);
        secp256k1_sha256_finalize(&sha, nonce_commitment32);
    }

    secp256k1_scalar_clear(&secret);
    return 1;
}

int secp256k1_musig_session_get_public_nonce(const secp256k1_context* ctx, secp256k1_musig_session *session, secp256k1_musig_session_signer_data *signers, secp256k1_pubkey *nonce, const unsigned char *const *commitments, size_t n_commitments) {
    secp256k1_sha256 sha;
    unsigned char nonce_commitments_hash[32];
    size_t i;
    (void) ctx;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(session != NULL);
    ARG_CHECK(signers != NULL);
    ARG_CHECK(nonce != NULL);
    ARG_CHECK(commitments != NULL);

    if (!session->has_secret_data || n_commitments != session->n_signers) {
        return 0;
    }
    for (i = 0; i < n_commitments; i++) {
        ARG_CHECK(commitments[i] != NULL);
    }

    secp256k1_sha256_initialize(&sha);
    for (i = 0; i < n_commitments; i++) {
        memcpy(signers[i].nonce_commitment, commitments[i], 32);
        secp256k1_sha256_write(&sha, commitments[i], 32);
    }
    secp256k1_sha256_finalize(&sha, nonce_commitments_hash);
    if (session->nonce_commitments_hash_is_set
            && memcmp(session->nonce_commitments_hash, nonce_commitments_hash, 32) != 0) {
        /* Abort if get_public_nonce has been called before with a different array of
         * commitments. */
        return 0;
    }
    memcpy(session->nonce_commitments_hash, nonce_commitments_hash, 32);
    session->nonce_commitments_hash_is_set = 1;
    memcpy(nonce, &session->nonce, sizeof(*nonce));
    return 1;
}

int secp256k1_musig_session_initialize_verifier(const secp256k1_context* ctx, secp256k1_musig_session *session, secp256k1_musig_session_signer_data *signers, const unsigned char *msg32, const secp256k1_pubkey *combined_pk, const unsigned char *pk_hash32, const unsigned char *const *commitments, size_t n_signers) {
    size_t i;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(session != NULL);
    ARG_CHECK(signers != NULL);
    ARG_CHECK(combined_pk != NULL);
    ARG_CHECK(pk_hash32 != NULL);
    ARG_CHECK(commitments != NULL);
    /* Check n_signers before checking commitments to allow testing the case where
     * n_signers is big without allocating the space. */
    if (n_signers > UINT32_MAX) {
        return 0;
    }
    for (i = 0; i < n_signers; i++) {
        ARG_CHECK(commitments[i] != NULL);
    }
    (void) ctx;

    memset(session, 0, sizeof(*session));

    memcpy(&session->combined_pk, combined_pk, sizeof(*combined_pk));
    if (n_signers == 0) {
        return 0;
    }
    session->n_signers = (uint32_t) n_signers;
    secp256k1_musig_signers_init(signers, session->n_signers);

    memcpy(session->pk_hash, pk_hash32, 32);
    session->nonce_is_set = 0;
    session->msg_is_set = 0;
    if (msg32 != NULL) {
        memcpy(session->msg, msg32, 32);
        session->msg_is_set = 1;
    }
    session->has_secret_data = 0;
    session->nonce_commitments_hash_is_set = 0;

    for (i = 0; i < n_signers; i++) {
        memcpy(signers[i].nonce_commitment, commitments[i], 32);
    }
    return 1;
}

int secp256k1_musig_set_nonce(const secp256k1_context* ctx, secp256k1_musig_session_signer_data *signer, const secp256k1_pubkey *nonce) {
    unsigned char commit[33];
    size_t commit_size = sizeof(commit);
    secp256k1_sha256 sha;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(signer != NULL);
    ARG_CHECK(nonce != NULL);

    secp256k1_sha256_initialize(&sha);
    secp256k1_ec_pubkey_serialize(ctx, commit, &commit_size, nonce, SECP256K1_EC_COMPRESSED);
    secp256k1_sha256_write(&sha, commit, commit_size);
    secp256k1_sha256_finalize(&sha, commit);

    if (memcmp(commit, signer->nonce_commitment, 32) != 0) {
        return 0;
    }
    memcpy(&signer->nonce, nonce, sizeof(*nonce));
    signer->present = 1;
    return 1;
}

int secp256k1_musig_session_combine_nonces(const secp256k1_context* ctx, secp256k1_musig_session *session, const secp256k1_musig_session_signer_data *signers, size_t n_signers, int *nonce_is_negated, const secp256k1_pubkey *adaptor) {
    secp256k1_gej combined_noncej;
    secp256k1_ge combined_noncep;
    secp256k1_ge noncep;
    secp256k1_sha256 sha;
    unsigned char nonce_commitments_hash[32];
    size_t i;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(session != NULL);
    ARG_CHECK(signers != NULL);

    if (n_signers != session->n_signers) {
        return 0;
    }
    secp256k1_sha256_initialize(&sha);
    secp256k1_gej_set_infinity(&combined_noncej);
    for (i = 0; i < n_signers; i++) {
        if (!signers[i].present) {
            return 0;
        }
        secp256k1_sha256_write(&sha, signers[i].nonce_commitment, 32);
        secp256k1_pubkey_load(ctx, &noncep, &signers[i].nonce);
        secp256k1_gej_add_ge_var(&combined_noncej, &combined_noncej, &noncep, NULL);
    }
    secp256k1_sha256_finalize(&sha, nonce_commitments_hash);
    /* Either the session is a verifier session or or the nonce_commitments_hash has
     * been set in `musig_session_get_public_nonce`. */
    VERIFY_CHECK(!session->has_secret_data || session->nonce_commitments_hash_is_set);
    if (session->has_secret_data
            && memcmp(session->nonce_commitments_hash, nonce_commitments_hash, 32) != 0) {
        /* If the signers' commitments changed between get_public_nonce and now we
         * have to abort because in that case they may have seen our nonce before
         * creating their commitment. That can happen if the signer_data given to
         * this function is different to the signer_data given to get_public_nonce.
         * */
        return 0;
    }

    /* Add public adaptor to nonce */
    if (adaptor != NULL) {
        secp256k1_pubkey_load(ctx, &noncep, adaptor);
        secp256k1_gej_add_ge_var(&combined_noncej, &combined_noncej, &noncep, NULL);
    }
    secp256k1_ge_set_gej(&combined_noncep, &combined_noncej);
    if (secp256k1_fe_is_quad_var(&combined_noncep.y)) {
        session->nonce_is_negated = 0;
    } else {
        session->nonce_is_negated = 1;
        secp256k1_ge_neg(&combined_noncep, &combined_noncep);
    }
    if (nonce_is_negated != NULL) {
        *nonce_is_negated = session->nonce_is_negated;
    }
    secp256k1_pubkey_save(&session->combined_nonce, &combined_noncep);
    session->nonce_is_set = 1;
    return 1;
}

int secp256k1_musig_session_set_msg(const secp256k1_context* ctx, secp256k1_musig_session *session, const unsigned char *msg32) {
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(session != NULL);
    ARG_CHECK(msg32 != NULL);

    if (session->msg_is_set) {
        return 0;
    }
    memcpy(session->msg, msg32, 32);
    session->msg_is_set = 1;
    return 1;
}

int secp256k1_musig_partial_signature_serialize(const secp256k1_context* ctx, unsigned char *out32, const secp256k1_musig_partial_signature* sig) {
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(out32 != NULL);
    ARG_CHECK(sig != NULL);
    memcpy(out32, sig->data, 32);
    return 1;
}

int secp256k1_musig_partial_signature_parse(const secp256k1_context* ctx, secp256k1_musig_partial_signature* sig, const unsigned char *in32) {
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(sig != NULL);
    ARG_CHECK(in32 != NULL);
    memcpy(sig->data, in32, 32);
    return 1;
}

/* Compute msghash = SHA256(combined_nonce, combined_pk, msg) */
static int secp256k1_musig_compute_messagehash(const secp256k1_context *ctx, unsigned char *msghash, const secp256k1_musig_session *session) {
    unsigned char buf[33];
    size_t bufsize = 33;
    secp256k1_ge rp;
    secp256k1_sha256 sha;

    secp256k1_sha256_initialize(&sha);
    if (!session->nonce_is_set) {
        return 0;
    }
    secp256k1_pubkey_load(ctx, &rp, &session->combined_nonce);
    secp256k1_fe_get_b32(buf, &rp.x);
    secp256k1_sha256_write(&sha, buf, 32);
    secp256k1_ec_pubkey_serialize(ctx, buf, &bufsize, &session->combined_pk, SECP256K1_EC_COMPRESSED);
    VERIFY_CHECK(bufsize == 33);
    secp256k1_sha256_write(&sha, buf, bufsize);
    if (!session->msg_is_set) {
        return 0;
    }
    secp256k1_sha256_write(&sha, session->msg, 32);
    secp256k1_sha256_finalize(&sha, msghash);
    return 1;
}

int secp256k1_musig_partial_sign(const secp256k1_context* ctx, const secp256k1_musig_session *session, secp256k1_musig_partial_signature *partial_sig) {
    unsigned char msghash[32];
    int overflow;
    secp256k1_scalar sk;
    secp256k1_scalar e, k;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(partial_sig != NULL);
    ARG_CHECK(session != NULL);

    if (!session->nonce_is_set || !session->has_secret_data) {
        return 0;
    }

    /* build message hash */
    if (!secp256k1_musig_compute_messagehash(ctx, msghash, session)) {
        return 0;
    }
    secp256k1_scalar_set_b32(&e, msghash, NULL);

    secp256k1_scalar_set_b32(&sk, session->seckey, &overflow);
    if (overflow) {
        secp256k1_scalar_clear(&sk);
        return 0;
    }

    secp256k1_scalar_set_b32(&k, session->secnonce, &overflow);
    if (overflow || secp256k1_scalar_is_zero(&k)) {
        secp256k1_scalar_clear(&sk);
        secp256k1_scalar_clear(&k);
        return 0;
    }
    if (session->nonce_is_negated) {
        secp256k1_scalar_negate(&k, &k);
    }

    /* Sign */
    secp256k1_scalar_mul(&e, &e, &sk);
    secp256k1_scalar_add(&e, &e, &k);
    secp256k1_scalar_get_b32(&partial_sig->data[0], &e);
    secp256k1_scalar_clear(&sk);
    secp256k1_scalar_clear(&k);

    return 1;
}

int secp256k1_musig_partial_sig_combine(const secp256k1_context* ctx, const secp256k1_musig_session *session, secp256k1_schnorrsig *sig, const secp256k1_musig_partial_signature *partial_sigs, size_t n_sigs) {
    size_t i;
    secp256k1_scalar s;
    secp256k1_ge noncep;
    (void) ctx;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(sig != NULL);
    ARG_CHECK(partial_sigs != NULL);
    ARG_CHECK(session != NULL);

    if (!session->nonce_is_set) {
        return 0;
    }
    if (n_sigs != session->n_signers) {
        return 0;
    }
    secp256k1_scalar_clear(&s);
    for (i = 0; i < n_sigs; i++) {
        int overflow;
        secp256k1_scalar term;

        secp256k1_scalar_set_b32(&term, partial_sigs[i].data, &overflow);
        if (overflow) {
            return 0;
        }
        secp256k1_scalar_add(&s, &s, &term);
    }

    secp256k1_pubkey_load(ctx, &noncep, &session->combined_nonce);
    VERIFY_CHECK(secp256k1_fe_is_quad_var(&noncep.y));
    secp256k1_fe_normalize(&noncep.x);
    secp256k1_fe_get_b32(&sig->data[0], &noncep.x);
    secp256k1_scalar_get_b32(&sig->data[32], &s);

    return 1;
}

int secp256k1_musig_partial_sig_verify(const secp256k1_context* ctx, const secp256k1_musig_session *session, const secp256k1_musig_session_signer_data *signer, const secp256k1_musig_partial_signature *partial_sig, const secp256k1_pubkey *pubkey) {
    unsigned char msghash[32];
    secp256k1_scalar s;
    secp256k1_scalar e;
    secp256k1_scalar mu;
    secp256k1_gej rj;
    secp256k1_ge rp;
    int overflow;

    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    ARG_CHECK(session != NULL);
    ARG_CHECK(signer != NULL);
    ARG_CHECK(partial_sig != NULL);
    ARG_CHECK(pubkey != NULL);

    if (!session->nonce_is_set || !signer->present) {
        return 0;
    }
    secp256k1_scalar_set_b32(&s, partial_sig->data, &overflow);
    if (overflow) {
        return 0;
    }
    if (!secp256k1_musig_compute_messagehash(ctx, msghash, session)) {
        return 0;
    }
    secp256k1_scalar_set_b32(&e, msghash, NULL);

    /* Multiplying the messagehash by the musig coefficient is equivalent
     * to multiplying the signer's public key by the coefficient, except
     * much easier to do. */
    secp256k1_musig_coefficient(&mu, session->pk_hash, signer->index);
    secp256k1_scalar_mul(&e, &e, &mu);

    if (!secp256k1_pubkey_load(ctx, &rp, &signer->nonce)) {
        return 0;
    }

    if (!secp256k1_schnorrsig_real_verify(ctx, &rj, &s, &e, pubkey)) {
        return 0;
    }
    if (!session->nonce_is_negated) {
        secp256k1_ge_neg(&rp, &rp);
    }
    secp256k1_gej_add_ge_var(&rj, &rj, &rp, NULL);

    return secp256k1_gej_is_infinity(&rj);
}

int secp256k1_musig_partial_sig_adapt(const secp256k1_context* ctx, secp256k1_musig_partial_signature *adaptor_sig, const secp256k1_musig_partial_signature *partial_sig, const unsigned char *sec_adaptor32, int nonce_is_negated) {
    secp256k1_scalar s;
    secp256k1_scalar t;
    int overflow;

    (void) ctx;
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(adaptor_sig != NULL);
    ARG_CHECK(partial_sig != NULL);
    ARG_CHECK(sec_adaptor32 != NULL);

    secp256k1_scalar_set_b32(&s, partial_sig->data, &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_scalar_set_b32(&t, sec_adaptor32, &overflow);
    if (overflow) {
        secp256k1_scalar_clear(&t);
        return 0;
    }

    if (nonce_is_negated) {
        secp256k1_scalar_negate(&t, &t);
    }

    secp256k1_scalar_add(&s, &s, &t);
    secp256k1_scalar_get_b32(adaptor_sig->data, &s);
    secp256k1_scalar_clear(&t);
    return 1;
}

int secp256k1_musig_extract_secret_adaptor(const secp256k1_context* ctx, unsigned char *sec_adaptor32, const secp256k1_schnorrsig *sig, const secp256k1_musig_partial_signature *partial_sigs, size_t n_partial_sigs, int nonce_is_negated) {
    secp256k1_scalar t;
    secp256k1_scalar s;
    int overflow;
    size_t i;

    (void) ctx;
    VERIFY_CHECK(ctx != NULL);
    ARG_CHECK(sec_adaptor32 != NULL);
    ARG_CHECK(sig != NULL);
    ARG_CHECK(partial_sigs != NULL);

    secp256k1_scalar_set_b32(&t, &sig->data[32], &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_scalar_negate(&t, &t);

    for (i = 0; i < n_partial_sigs; i++) {
        secp256k1_scalar_set_b32(&s, partial_sigs[i].data, &overflow);
        if (overflow) {
            secp256k1_scalar_clear(&t);
            return 0;
        }
        secp256k1_scalar_add(&t, &t, &s);
    }

    if (!nonce_is_negated) {
        secp256k1_scalar_negate(&t, &t);
    }
    secp256k1_scalar_get_b32(sec_adaptor32, &t);
    secp256k1_scalar_clear(&t);
    return 1;
}

#endif

