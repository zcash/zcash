#ifndef SECP256K1_MUSIG_H
#define SECP256K1_MUSIG_H

#include <stdint.h>

/** This module implements a Schnorr-based multi-signature scheme called MuSig
 * (https://eprint.iacr.org/2018/068.pdf). There's an example C source file in the
 * module's directory (src/modules/musig/example.c) that demonstrates how it can be
 * used.
 */

/** Data structure containing data related to a signing session resulting in a single
 * signature.
 *
 * This structure is not opaque, but it MUST NOT be copied or read or written to it
 * directly. A signer who is online throughout the whole process and can keep this
 * structure in memory can use the provided API functions for a safe standard
 * workflow.
 *
 * A signer who goes offline and needs to import/export or save/load this structure
 * **must** take measures prevent replay attacks wherein an old state is loaded and
 * the signing protocol forked from that point. One straightforward way to accomplish
 * this is to attach the output of a monotonic non-resettable counter (hardware
 * support is needed for this). Increment the counter before each output and
 * encrypt+sign the entire package. If a package is deserialized with an old counter
 * state or bad signature it should be rejected.
 *
 * Observe that an independent counter is needed for each concurrent signing session
 * such a device is involved in. To avoid fragility, it is therefore recommended that
 * any offline signer be usable for only a single session at once.
 *
 * Given access to such a counter, its output should be used as (or mixed into) the
 * session ID to ensure uniqueness.
 *
 * Fields:
 *      combined_pk: MuSig-computed combined public key
 *        n_signers: Number of signers
 *          pk_hash: The 32-byte hash of the original public keys
 *   combined_nonce: Summed combined public nonce (undefined if `nonce_is_set` is false)
 *     nonce_is_set: Whether the above nonce has been set
 * nonce_is_negated: If `nonce_is_set`, whether the above nonce was negated after
 *                   summing the participants' nonces. Needed to ensure the nonce's y
 *                   coordinate has a quadratic-residue y coordinate
 *              msg: The 32-byte message (hash) to be signed
 *       msg_is_set: Whether the above message has been set
 *  has_secret_data: Whether this session object has a signers' secret data; if this
 *                   is `false`, it may still be used for verification purposes.
 *           seckey: If `has_secret_data`, the signer's secret key
 *         secnonce: If `has_secret_data`, the signer's secret nonce
 *            nonce: If `has_secret_data`, the signer's public nonce
 * nonce_commitments_hash: If `has_secret_data` and `nonce_commitments_hash_is_set`,
 *                   the hash of all signers' commitments
 * nonce_commitments_hash_is_set: If `has_secret_data`, whether the
 *                   nonce_commitments_hash has been set
 */
typedef struct {
    secp256k1_pubkey combined_pk;
    uint32_t n_signers;
    unsigned char pk_hash[32];
    secp256k1_pubkey combined_nonce;
    int nonce_is_set;
    int nonce_is_negated;
    unsigned char msg[32];
    int msg_is_set;
    int has_secret_data;
    unsigned char seckey[32];
    unsigned char secnonce[32];
    secp256k1_pubkey nonce;
    unsigned char nonce_commitments_hash[32];
    int nonce_commitments_hash_is_set;
} secp256k1_musig_session;

/** Data structure containing data on all signers in a single session.
 *
 * The workflow for this structure is as follows:
 *
 * 1. This structure is initialized with `musig_session_initialize` or
 *    `musig_session_initialize_verifier`, which set the `index` field, and zero out
 *    all other fields. The public session is initialized with the signers'
 *    nonce_commitments.
 *
 * 2. In a non-public session the nonce_commitments are set with the function
 *    `musig_get_public_nonce`, which also returns the signer's public nonce. This
 *    ensures that the public nonce is not exposed until all commitments have been
 *    received.
 *
 * 3. Each individual data struct should be updated with `musig_set_nonce` once a
 *    nonce is available. This function takes a single signer data struct rather than
 *    an array because it may fail in the case that the provided nonce does not match
 *    the commitment. In this case, it is desirable to identify the exact party whose
 *    nonce was inconsistent.
 *
 * Fields:
 *   present: indicates whether the signer's nonce is set
 *     index: index of the signer in the MuSig key aggregation
 *     nonce: public nonce, must be a valid curvepoint if the signer is `present`
 * nonce_commitment: commitment to the nonce, or all-bits zero if a commitment
 *                   has not yet been set
 */
typedef struct {
    int present;
    uint32_t index;
    secp256k1_pubkey nonce;
    unsigned char nonce_commitment[32];
} secp256k1_musig_session_signer_data;

/** Opaque data structure that holds a MuSig partial signature.
 *
 *  The exact representation of data inside is implementation defined and not
 *  guaranteed to be portable between different platforms or versions. It is however
 *  guaranteed to be 32 bytes in size, and can be safely copied/moved. If you need
 *  to convert to a format suitable for storage, transmission, or comparison, use the
 *  `musig_partial_signature_serialize` and `musig_partial_signature_parse`
 *  functions.
 */
typedef struct {
    unsigned char data[32];
} secp256k1_musig_partial_signature;

/** Computes a combined public key and the hash of the given public keys
 *
 *  Returns: 1 if the public keys were successfully combined, 0 otherwise
 *  Args:        ctx: pointer to a context object initialized for verification
 *                    (cannot be NULL)
 *           scratch: scratch space used to compute the combined pubkey by
 *                    multiexponentiation. If NULL, an inefficient algorithm is used.
 *  Out: combined_pk: the MuSig-combined public key (cannot be NULL)
 *         pk_hash32: if non-NULL, filled with the 32-byte hash of all input public
 *                    keys in order to be used in `musig_session_initialize`.
 *   In:     pubkeys: input array of public keys to combine. The order is important;
 *                    a different order will result in a different combined public
 *                    key (cannot be NULL)
 *         n_pubkeys: length of pubkeys array
 */
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 int secp256k1_musig_pubkey_combine(
    const secp256k1_context* ctx,
    secp256k1_scratch_space *scratch,
    secp256k1_pubkey *combined_pk,
    unsigned char *pk_hash32,
    const secp256k1_pubkey *pubkeys,
    size_t n_pubkeys
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(5);

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
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 int secp256k1_musig_session_initialize(
    const secp256k1_context* ctx,
    secp256k1_musig_session *session,
    secp256k1_musig_session_signer_data *signers,
    unsigned char *nonce_commitment32,
    const unsigned char *session_id32,
    const unsigned char *msg32,
    const secp256k1_pubkey *combined_pk,
    const unsigned char *pk_hash32,
    size_t n_signers,
    size_t my_index,
    const unsigned char *seckey
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4) SECP256K1_ARG_NONNULL(5) SECP256K1_ARG_NONNULL(7) SECP256K1_ARG_NONNULL(8) SECP256K1_ARG_NONNULL(11);

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
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 SECP256K1_WARN_UNUSED_RESULT int secp256k1_musig_session_get_public_nonce(
    const secp256k1_context* ctx,
    secp256k1_musig_session *session,
    secp256k1_musig_session_signer_data *signers,
    secp256k1_pubkey *nonce,
    const unsigned char *const *commitments,
    size_t n_commitments
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4) SECP256K1_ARG_NONNULL(5);

/** Initializes a verifier session that can be used for verifying nonce commitments
 *  and partial signatures. It does not have secret key material and therefore can not
 *  be used to create signatures.
 *
 *  Returns: 1 when session is successfully initialized, 0 otherwise
 *  Args:        ctx: pointer to a context object (cannot be NULL)
 *  Out:     session: the session structure to initialize (cannot be NULL)
 *           signers: an array of signers' data to be initialized. Array length must
 *                    equal to `n_signers`(cannot be NULL)
 *  In:        msg32: the 32-byte message to be signed If NULL, must be set with
 *                    `musig_session_set_msg` before using the session for verifying
 *                    partial signatures.
 *       combined_pk: the combined public key of all signers (cannot be NULL)
 *         pk_hash32: the 32-byte hash of the signers' individual keys (cannot be NULL)
 *       commitments: array of 32-byte nonce commitments. Array length must equal to
 *                    `n_signers` (cannot be NULL)
 *         n_signers: length of signers and commitments array. Number of signers
 *                    participating in the MuSig. Must be greater than 0 and at most
 *                    2^32 - 1.
 */
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 int secp256k1_musig_session_initialize_verifier(
    const secp256k1_context* ctx,
    secp256k1_musig_session *session,
    secp256k1_musig_session_signer_data *signers,
    const unsigned char *msg32,
    const secp256k1_pubkey *combined_pk,
    const unsigned char *pk_hash32,
    const unsigned char *const *commitments,
    size_t n_signers
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(5) SECP256K1_ARG_NONNULL(6) SECP256K1_ARG_NONNULL(7);

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
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 SECP256K1_WARN_UNUSED_RESULT int secp256k1_musig_set_nonce(
    const secp256k1_context* ctx,
    secp256k1_musig_session_signer_data *signer,
    const secp256k1_pubkey *nonce
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

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
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 int secp256k1_musig_session_combine_nonces(
    const secp256k1_context* ctx,
    secp256k1_musig_session *session,
    const secp256k1_musig_session_signer_data *signers,
    size_t n_signers,
    int *nonce_is_negated,
    const secp256k1_pubkey *adaptor
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/** Sets the message of a session if previously unset
 *
 *  Returns 1 if the message was not set yet and is now successfully set
 *          0 otherwise
 *  Args:    ctx: pointer to a context object (cannot be NULL)
 *       session: the session structure to update with the message (cannot be NULL)
 *  In:    msg32: the 32-byte message to be signed (cannot be NULL)
 */
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 SECP256K1_WARN_UNUSED_RESULT int secp256k1_musig_session_set_msg(
    const secp256k1_context* ctx,
    secp256k1_musig_session *session,
    const unsigned char *msg32
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/** Serialize a MuSig partial signature or adaptor signature
 *
 *  Returns: 1 when the signature could be serialized, 0 otherwise
 *  Args:    ctx: a secp256k1 context object
 *  Out:   out32: pointer to a 32-byte array to store the serialized signature
 *  In:      sig: pointer to the signature
 */
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 int secp256k1_musig_partial_signature_serialize(
    const secp256k1_context* ctx,
    unsigned char *out32,
    const secp256k1_musig_partial_signature* sig
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

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
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 int secp256k1_musig_partial_signature_parse(
    const secp256k1_context* ctx,
    secp256k1_musig_partial_signature* sig,
    const unsigned char *in32
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

/** Produces a partial signature
 *
 *  Returns: 1: partial signature constructed
 *           0: session in incorrect or inconsistent state
 *  Args:         ctx: pointer to a context object (cannot be NULL)
 *            session: active signing session for which the combined nonce has been
 *                     computed (cannot be NULL)
 *  Out:  partial_sig: partial signature (cannot be NULL)
 */
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 int secp256k1_musig_partial_sign(
    const secp256k1_context* ctx,
    const secp256k1_musig_session *session,
    secp256k1_musig_partial_signature *partial_sig
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3);

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
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 SECP256K1_WARN_UNUSED_RESULT int secp256k1_musig_partial_sig_verify(
    const secp256k1_context* ctx,
    const secp256k1_musig_session *session,
    const secp256k1_musig_session_signer_data *signer,
    const secp256k1_musig_partial_signature *partial_sig,
    const secp256k1_pubkey *pubkey
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4) SECP256K1_ARG_NONNULL(5);

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
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 SECP256K1_WARN_UNUSED_RESULT int secp256k1_musig_partial_sig_combine(
    const secp256k1_context* ctx,
    const secp256k1_musig_session *session,
    secp256k1_schnorrsig *sig,
    const secp256k1_musig_partial_signature *partial_sigs,
    size_t n_sigs
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

/** Converts a partial signature to an adaptor signature by adding a given secret
 *  adaptor.
 *
 *  Returns: 1: signature and secret adaptor contained valid values
 *           0: otherwise
 *  Args:         ctx: pointer to a context object (cannot be NULL)
 *  Out:  adaptor_sig: adaptor signature to produce (cannot be NULL)
 *  In:   partial_sig: partial signature to tweak with secret adaptor (cannot be NULL)
 *      sec_adaptor32: 32-byte secret adaptor to add to the partial signature (cannot
 *                     be NULL)
 *   nonce_is_negated: the `nonce_is_negated` output of `musig_session_combine_nonces`
 */
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 int secp256k1_musig_partial_sig_adapt(
    const secp256k1_context* ctx,
    secp256k1_musig_partial_signature *adaptor_sig,
    const secp256k1_musig_partial_signature *partial_sig,
    const unsigned char *sec_adaptor32,
    int nonce_is_negated
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

/** Extracts a secret adaptor from a MuSig, given all parties' partial
 *  signatures. This function will not fail unless given grossly invalid data; if it
 *  is merely given signatures that do not verify, the returned value will be
 *  nonsense. It is therefore important that all data be verified at earlier steps of
 *  any protocol that uses this function.
 *
 *  Returns: 1: signatures contained valid data such that an adaptor could be extracted
 *           0: otherwise
 *  Args:         ctx: pointer to a context object (cannot be NULL)
 *  Out:sec_adaptor32: 32-byte secret adaptor (cannot be NULL)
 *  In:           sig: complete 2-of-2 signature (cannot be NULL)
 *       partial_sigs: array of partial signatures (cannot be NULL)
 *     n_partial_sigs: number of elements in partial_sigs array
 *   nonce_is_negated: the `nonce_is_negated` output of `musig_session_combine_nonces`
 */
#ifdef __cplusplus
extern "C"
#else
SECP256K1_API
#endif
 SECP256K1_WARN_UNUSED_RESULT int secp256k1_musig_extract_secret_adaptor(
    const secp256k1_context* ctx,
    unsigned char *sec_adaptor32,
    const secp256k1_schnorrsig *sig,
    const secp256k1_musig_partial_signature *partial_sigs,
    size_t n_partial_sigs,
    int nonce_is_negated
) SECP256K1_ARG_NONNULL(1) SECP256K1_ARG_NONNULL(2) SECP256K1_ARG_NONNULL(3) SECP256K1_ARG_NONNULL(4);

#endif

