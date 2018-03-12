
#include "asn/Condition.h"
#include "asn/Fulfillment.h"
#include "asn/PrefixFingerprintContents.h"
#include "asn/OCTET_STRING.h"
#include "include/cJSON.h"
#include "cryptoconditions.h"


struct CCType cc_anonType;


static CC *mkAnon(const Condition_t *asnCond) {
    CCType *realType = getTypeByAsnEnum(asnCond->present);
    if (!realType) {
        printf("Unknown ASN type: %i", asnCond->present);
        return 0;
    }
    CC *cond = calloc(1, sizeof(CC));
    cond->type = (CCType*) calloc(1, sizeof(CCType));
    *cond->type = cc_anonType;
    strcpy(cond->type->name, realType->name);
    cond->type->hasSubtypes = realType->hasSubtypes;
    cond->type->typeId = realType->typeId;
    cond->type->asnType = realType->asnType;
    const CompoundSha256Condition_t *deets = &asnCond->choice.thresholdSha256;
    memcpy(cond->fingerprint, deets->fingerprint.buf, 32);
    cond->cost = deets->cost;
    if (realType->hasSubtypes) {
        cond->subtypes = fromAsnSubtypes(deets->subtypes);
    }
    return cond;
}



static void anonToJSON(const CC *cond, cJSON *params) {
    unsigned char *b64 = base64_encode(cond->fingerprint, 32);
    cJSON_AddItemToObject(params, "fingerprint", cJSON_CreateString(b64));
    free(b64);
    cJSON_AddItemToObject(params, "cost", cJSON_CreateNumber(cond->cost));
    cJSON_AddItemToObject(params, "subtypes", cJSON_CreateNumber(cond->subtypes));
}


static unsigned char *anonFingerprint(const CC *cond) {
    unsigned char *out = calloc(1, 32);
    memcpy(out, cond->fingerprint, 32);
    return out;
}


static unsigned long anonCost(const CC *cond) {
    return cond->cost;
}


static uint32_t anonSubtypes(const CC *cond) {
    return cond->subtypes;
}


static Fulfillment_t *anonFulfillment(const CC *cond) {
    return NULL;
}


static void anonFree(CC *cond) {
    free(cond->type);
    free(cond);
}


static int anonIsFulfilled(const CC *cond) {
    return 0;
}


struct CCType cc_anonType = { -1, "anon  (a buffer large enough to accomodate any type name)", Condition_PR_NOTHING, 0, NULL, &anonFingerprint, &anonCost, &anonSubtypes, NULL, &anonToJSON, NULL, &anonFulfillment, &anonIsFulfilled, &anonFree };
