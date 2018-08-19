#define _GNU_SOURCE 1

#if __linux
#include <sys/syscall.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h> 
#endif

#include <unistd.h>
#include <pthread.h>

#include "asn/Condition.h"
#include "asn/Fulfillment.h"
#include "asn/Secp256k1Fulfillment.h"
#include "asn/Secp256k1FingerprintContents.h"
#include "asn/OCTET_STRING.h"
#include "include/cJSON.h"
#include "include/secp256k1/include/secp256k1.h"
#include "cryptoconditions.h"
#include "internal.h"


struct CCType CC_Secp256k1Type;


static const size_t SECP256K1_PK_SIZE = 33;
static const size_t SECP256K1_SK_SIZE = 32;
static const size_t SECP256K1_SIG_SIZE = 64;


secp256k1_context *ec_ctx_sign = 0, *ec_ctx_verify = 0;
pthread_mutex_t cc_secp256k1ContextLock = PTHREAD_MUTEX_INITIALIZER;


void lockSign() {
    pthread_mutex_lock(&cc_secp256k1ContextLock);
    if (!ec_ctx_sign) {
        ec_ctx_sign = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    }
    unsigned char ent[32];
#ifdef SYS_getrandom
    int read = syscall(SYS_getrandom, ent, 32, 0);
#else
    FILE *fp = fopen("/dev/urandom", "r");
    int read = (int) fread(&ent, 1, 32, fp);
    fclose(fp);
#endif
    if (read != 32)
    {
        int32_t i;
        for (i=0; i<32; i++)
            ((uint8_t *)ent)[i] = rand();
    }
    if (!secp256k1_context_randomize(ec_ctx_sign, ent)) {
        fprintf(stderr, "Could not randomize secp256k1 context\n");
        exit(1);
    }
}


void unlockSign() {
    pthread_mutex_unlock(&cc_secp256k1ContextLock);
}


void initVerify() {
    if (!ec_ctx_verify) {
        pthread_mutex_lock(&cc_secp256k1ContextLock);
        if (!ec_ctx_verify)
            ec_ctx_verify = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
        pthread_mutex_unlock(&cc_secp256k1ContextLock);
    }
}


static unsigned char *secp256k1Fingerprint(const CC *cond) {
    Secp256k1FingerprintContents_t *fp = calloc(1, sizeof(Secp256k1FingerprintContents_t));
    //fprintf(stderr,"secpfinger %p %p size %d vs %d\n",fp,cond->publicKey,(int32_t)sizeof(Secp256k1FingerprintContents_t),(int32_t)SECP256K1_PK_SIZE);
    OCTET_STRING_fromBuf(&fp->publicKey, cond->publicKey, SECP256K1_PK_SIZE);
    return hashFingerprintContents(&asn_DEF_Secp256k1FingerprintContents, fp);
}


int secp256k1Verify(CC *cond, CCVisitor visitor) {
    if (cond->type->typeId != CC_Secp256k1Type.typeId) return 1;
    initVerify();

    int rc;

    // parse pubkey
    secp256k1_pubkey pk;
    rc = secp256k1_ec_pubkey_parse(ec_ctx_verify, &pk, cond->publicKey, SECP256K1_PK_SIZE);
    if (rc != 1) return 0;

    // parse siganature
    secp256k1_ecdsa_signature sig;
    rc = secp256k1_ecdsa_signature_parse_compact(ec_ctx_verify, &sig, cond->signature);
    if (rc != 1) return 0;

    // Only accepts lower S signatures
    rc = secp256k1_ecdsa_verify(ec_ctx_verify, &sig, visitor.msg, &pk);
    if (rc != 1) return 0;

    return 1;
}


int cc_secp256k1VerifyTreeMsg32(const CC *cond, const unsigned char *msg32) {
    int subtypes = cc_typeMask(cond);
    if (subtypes & (1 << CC_PrefixType.typeId) &&
        subtypes & (1 << CC_Secp256k1Type.typeId)) {
        // No support for prefix currently, due to pending protocol decision on
        // how to combine message and prefix into 32 byte hash
        return 0;
    }
    CCVisitor visitor = {&secp256k1Verify, msg32, 0, NULL};
    int out = cc_visit(cond, visitor);
    return out;
}


/*
 * Signing data
 */
typedef struct CCSecp256k1SigningData {
    const unsigned char *pk;
    const unsigned char *sk;
    int nSigned;
} CCSecp256k1SigningData;


/*
 * Visitor that signs an secp256k1 condition if it has a matching public key
 */
static int secp256k1Sign(CC *cond, CCVisitor visitor) {
    if (cond->type->typeId != CC_Secp256k1) return 1;
    CCSecp256k1SigningData *signing = (CCSecp256k1SigningData*) visitor.context;
    if (0 != memcmp(cond->publicKey, signing->pk, SECP256K1_PK_SIZE)) return 1;

    secp256k1_ecdsa_signature sig;
    lockSign();
    int rc = secp256k1_ecdsa_sign(ec_ctx_sign, &sig, visitor.msg, signing->sk, NULL, NULL);
    unlockSign();

    if (rc != 1) return 0;

    if (!cond->signature) cond->signature = calloc(1, SECP256K1_SIG_SIZE);
    secp256k1_ecdsa_signature_serialize_compact(ec_ctx_verify, cond->signature, &sig);

    signing->nSigned++;
    return 1;
}


/*
 * Sign secp256k1 conditions in a tree
 */
int cc_signTreeSecp256k1Msg32(CC *cond, const unsigned char *privateKey, const unsigned char *msg32) {
    if (cc_typeMask(cond) & (1 << CC_Prefix)) {
        // No support for prefix currently, due to pending protocol decision on
        // how to combine message and prefix into 32 byte hash
        return 0;
    }

    // derive the pubkey
    secp256k1_pubkey spk;
    lockSign();
    int rc = secp256k1_ec_pubkey_create(ec_ctx_sign, &spk, privateKey);
    unlockSign();
    if (rc != 1) {
        fprintf(stderr, "Cryptoconditions couldn't derive secp256k1 pubkey\n");
        return 0;
    }

    // serialize pubkey
    unsigned char *publicKey = calloc(1, SECP256K1_PK_SIZE);
    size_t ol = SECP256K1_PK_SIZE;
    secp256k1_ec_pubkey_serialize(ec_ctx_verify, publicKey, &ol, &spk, SECP256K1_EC_COMPRESSED);

    // sign
    CCSecp256k1SigningData signing = {publicKey, privateKey, 0};
    CCVisitor visitor = {&secp256k1Sign, msg32, 32, &signing};
    cc_visit(cond, visitor);

    free(publicKey);
    return signing.nSigned;
}


static unsigned long secp256k1Cost(const CC *cond) {
    return 131072;
}


static CC *cc_secp256k1Condition(const unsigned char *publicKey, const unsigned char *signature) {
    // Check that pk parses
    initVerify();
    secp256k1_pubkey spk;
    int rc = secp256k1_ec_pubkey_parse(ec_ctx_verify, &spk, publicKey, SECP256K1_PK_SIZE);
    if (!rc) {
        return NULL;
    }

    unsigned char *pk = 0, *sig = 0;

    pk = calloc(1, SECP256K1_PK_SIZE);
    memcpy(pk, publicKey, SECP256K1_PK_SIZE);
    if (signature) {
        sig = calloc(1, SECP256K1_SIG_SIZE);
        memcpy(sig, signature, SECP256K1_SIG_SIZE);
    }

    CC *cond = cc_new(CC_Secp256k1);
    cond->publicKey = pk;
    cond->signature = sig;
    return cond;
}


static CC *secp256k1FromJSON(const cJSON *params, char *err) {
    CC *cond = 0;
    unsigned char *pk = 0, *sig = 0;
    size_t pkSize, sigSize;

    if (!jsonGetHex(params, "publicKey", err, &pk, &pkSize)) goto END;

    if (!jsonGetHexOptional(params, "signature", err, &sig, &sigSize)) goto END;
    if (sig && SECP256K1_SIG_SIZE != sigSize) {
        strcpy(err, "signature has incorrect length");
        goto END;
    }

    cond = cc_secp256k1Condition(pk, sig);
    if (!cond) {
        strcpy(err, "invalid public key");
    }
END:
    free(pk);
    free(sig);
    return cond;
}


static void secp256k1ToJSON(const CC *cond, cJSON *params) {
    jsonAddHex(params, "publicKey", cond->publicKey, SECP256K1_PK_SIZE);
    if (cond->signature) {
        jsonAddHex(params, "signature", cond->signature, SECP256K1_SIG_SIZE);
    }
}


static CC *secp256k1FromFulfillment(const Fulfillment_t *ffill) {
    return cc_secp256k1Condition(ffill->choice.secp256k1Sha256.publicKey.buf,
                                 ffill->choice.secp256k1Sha256.signature.buf);
}


static Fulfillment_t *secp256k1ToFulfillment(const CC *cond) {
    if (!cond->signature) {
        return NULL;
    }

    Fulfillment_t *ffill = calloc(1, sizeof(Fulfillment_t));
    ffill->present = Fulfillment_PR_secp256k1Sha256;
    Secp256k1Fulfillment_t *sec = &ffill->choice.secp256k1Sha256;

    OCTET_STRING_fromBuf(&sec->publicKey, cond->publicKey, SECP256K1_PK_SIZE);
    OCTET_STRING_fromBuf(&sec->signature, cond->signature, SECP256K1_SIG_SIZE);
    return ffill;
}


int secp256k1IsFulfilled(const CC *cond) {
    return cond->signature > 0;
}


static void secp256k1Free(CC *cond) {
    free(cond->publicKey);
    if (cond->signature) {
        free(cond->signature);
    }
}


static uint32_t secp256k1Subtypes(const CC *cond) {
    return 0;
}


struct CCType CC_Secp256k1Type = { 5, "secp256k1-sha-256", Condition_PR_secp256k1Sha256, 0, &secp256k1Fingerprint, &secp256k1Cost, &secp256k1Subtypes, &secp256k1FromJSON, &secp256k1ToJSON, &secp256k1FromFulfillment, &secp256k1ToFulfillment, &secp256k1IsFulfilled, &secp256k1Free };
