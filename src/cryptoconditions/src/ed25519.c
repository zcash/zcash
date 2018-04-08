#include "asn/Condition.h"
#include "asn/Fulfillment.h"
#include "asn/Ed25519FingerprintContents.h"
#include "asn/OCTET_STRING.h"
#include "include/cJSON.h"
#include "include/ed25519/src/ed25519.h"
#include "cryptoconditions.h"


struct CCType CC_Ed25519Type;


static unsigned char *ed25519Fingerprint(const CC *cond) {
    Ed25519FingerprintContents_t *fp = calloc(1, sizeof(Ed25519FingerprintContents_t));
    OCTET_STRING_fromBuf(&fp->publicKey, cond->publicKey, 32);
    return hashFingerprintContents(&asn_DEF_Ed25519FingerprintContents, fp);
}


int ed25519Verify(CC *cond, CCVisitor visitor) {
    if (cond->type->typeId != CC_Ed25519Type.typeId) return 1;
    // TODO: test failure mode: empty sig / null pointer
    return ed25519_verify(cond->signature, visitor.msg, visitor.msgLength, cond->publicKey);
}


static int cc_ed25519VerifyTree(const CC *cond, const unsigned char *msg, size_t msgLength) {
    CCVisitor visitor = {&ed25519Verify, msg, msgLength, NULL};
    return cc_visit((CC*) cond, visitor);
}


/*
 * Signing data
 */
typedef struct CCEd25519SigningData {
    unsigned char *pk;
    unsigned char *skpk;
    int nSigned;
} CCEd25519SigningData;


/*
 * Visitor that signs an ed25519 condition if it has a matching public key
 */
static int ed25519Sign(CC *cond, CCVisitor visitor) {
    if (cond->type->typeId != CC_Ed25519Type.typeId) return 1;
    CCEd25519SigningData *signing = (CCEd25519SigningData*) visitor.context;
    if (0 != memcmp(cond->publicKey, signing->pk, 32)) return 1;
    if (!cond->signature) cond->signature = malloc(64);
    ed25519_sign(cond->signature, visitor.msg, visitor.msgLength,
            signing->pk, signing->skpk);
    signing->nSigned++;
    return 1;
}


/*
 * Sign ed25519 conditions in a tree
 */
int cc_signTreeEd25519(CC *cond, const unsigned char *privateKey, const unsigned char *msg,
        const size_t msgLength) {
    unsigned char pk[32], skpk[64];
    ed25519_create_keypair(pk, skpk, privateKey);

    CCEd25519SigningData signing = {pk, skpk, 0};
    CCVisitor visitor = {&ed25519Sign, (unsigned char*)msg, msgLength, &signing};
    cc_visit(cond, visitor);
    return signing.nSigned;
}


static unsigned long ed25519Cost(const CC *cond) {
    return 131072;
}


static CC *ed25519FromJSON(const cJSON *params, char *err) {
    size_t binsz;

    cJSON *pk_item = cJSON_GetObjectItem(params, "publicKey");
    if (!cJSON_IsString(pk_item)) {
        strcpy(err, "publicKey must be a string");
        return NULL;
    }
    unsigned char *pk = base64_decode(pk_item->valuestring, &binsz);
    if (32 != binsz) {
        strcpy(err, "publicKey has incorrect length");
        free(pk);
        return NULL;
    }

    cJSON *signature_item = cJSON_GetObjectItem(params, "signature");
    unsigned char *sig = NULL;
    if (signature_item && !cJSON_IsNull(signature_item)) {
        if (!cJSON_IsString(signature_item)) {
            strcpy(err, "signature must be null or a string");
            return NULL;
        }
        sig = base64_decode(signature_item->valuestring, &binsz);
        if (64 != binsz) {
            strcpy(err, "signature has incorrect length");
            free(sig);
            return NULL;
        }
    }

    CC *cond = cc_new(CC_Ed25519);
    cond->publicKey = pk;
    cond->signature = sig;
    return cond;
}


static void ed25519ToJSON(const CC *cond, cJSON *params) {
    unsigned char *b64 = base64_encode(cond->publicKey, 32);
    cJSON_AddItemToObject(params, "publicKey", cJSON_CreateString(b64));
    free(b64);
    if (cond->signature) {
        b64 = base64_encode(cond->signature, 64);
        cJSON_AddItemToObject(params, "signature", cJSON_CreateString(b64));
        free(b64);
    }
}


static CC *ed25519FromFulfillment(const Fulfillment_t *ffill) {
    CC *cond = cc_new(CC_Ed25519);
    cond->publicKey = malloc(32);
    memcpy(cond->publicKey, ffill->choice.ed25519Sha256.publicKey.buf, 32);
    cond->signature = malloc(64);
    memcpy(cond->signature, ffill->choice.ed25519Sha256.signature.buf, 64);
    return cond;
}


static Fulfillment_t *ed25519ToFulfillment(const CC *cond) {
    if (!cond->signature) {
        return NULL;
    }
    Fulfillment_t *ffill = calloc(1, sizeof(Fulfillment_t));
    ffill->present = Fulfillment_PR_ed25519Sha256;
    Ed25519Sha512Fulfillment_t *ed2 = &ffill->choice.ed25519Sha256;
    OCTET_STRING_fromBuf(&ed2->publicKey, cond->publicKey, 32);
    OCTET_STRING_fromBuf(&ed2->signature, cond->signature, 64);
    return ffill;
}


int ed25519IsFulfilled(const CC *cond) {
    return cond->signature > 0;
}


static void ed25519Free(CC *cond) {
    free(cond->publicKey);
    if (cond->signature) {
        free(cond->signature);
    }
}


static uint32_t ed25519Subtypes(const CC *cond) {
    return 0;
}


struct CCType CC_Ed25519Type = { 4, "ed25519-sha-256", Condition_PR_ed25519Sha256, 0, &ed25519Fingerprint, &ed25519Cost, &ed25519Subtypes, &ed25519FromJSON, &ed25519ToJSON, &ed25519FromFulfillment, &ed25519ToFulfillment, &ed25519IsFulfilled, &ed25519Free };
