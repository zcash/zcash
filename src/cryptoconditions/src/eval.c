#include "asn/Condition.h"
#include "asn/Fulfillment.h"
#include "asn/EvalFulfillment.h"
#include "asn/EvalFingerprintContents.h"
#include "asn/OCTET_STRING.h"
#include "cryptoconditions.h"
#include "internal.h"
#include "include/cJSON.h"


struct CCType cc_evalType;


static unsigned char *evalFingerprint(const CC *cond) {
    EvalFingerprintContents_t *fp = calloc(1, sizeof(EvalFingerprintContents_t));
    OCTET_STRING_fromBuf(&fp->method, cond->method, strlen(cond->method));
    OCTET_STRING_fromBuf(&fp->paramsBin, cond->paramsBin, cond->paramsBinLength);
    return hashFingerprintContents(&asn_DEF_EvalFingerprintContents, fp);
}


static unsigned long evalCost(const CC *cond) {
    return 1048576;  // Pretty high
}


static CC *evalFromJSON(const cJSON *params, unsigned char *err) {
    size_t paramsBinLength;
    unsigned char *paramsBin = 0;

    cJSON *method_item = cJSON_GetObjectItem(params, "method");
    if (!checkString(method_item, "method", err)) {
        return NULL;
    }

    if (strlen(method_item->valuestring) > 64) {
        strcpy(err, "method must be less than or equal to 64 bytes");
        return NULL;
    }

    if (!jsonGetBase64(params, "params", err, &paramsBin, &paramsBinLength)) {
        return NULL;
    }

    CC *cond = calloc(1, sizeof(CC));
    strcpy(cond->method, method_item->valuestring);
    cond->paramsBin = paramsBin;
    cond->paramsBinLength = paramsBinLength;
    cond->type = &cc_evalType;
    return cond;
}


static void evalToJSON(const CC *cond, cJSON *params) {

    // add method
    cJSON_AddItemToObject(params, "method", cJSON_CreateString(cond->method));

    // add params
    unsigned char *b64 = base64_encode(cond->paramsBin, cond->paramsBinLength);
    cJSON_AddItemToObject(params, "params", cJSON_CreateString(b64));
    free(b64);
}


static CC *evalFromFulfillment(const Fulfillment_t *ffill) {
    CC *cond = calloc(1, sizeof(CC));
    cond->type = &cc_evalType;

    EvalFulfillment_t *eval = &ffill->choice.evalSha256;

    memcpy(cond->method, eval->method.buf, eval->method.size);
    cond->method[eval->method.size] = 0;

    OCTET_STRING_t octets = eval->paramsBin;
    cond->paramsBinLength = octets.size;
    cond->paramsBin = malloc(octets.size);
    memcpy(cond->paramsBin, octets.buf, octets.size);

    return cond;
}


static Fulfillment_t *evalToFulfillment(const CC *cond) {
    Fulfillment_t *ffill = calloc(1, sizeof(Fulfillment_t));
    ffill->present = Fulfillment_PR_evalSha256;
    EvalFulfillment_t *eval = &ffill->choice.evalSha256;
    OCTET_STRING_fromBuf(&eval->method, cond->method, strlen(cond->method));
    OCTET_STRING_fromBuf(&eval->paramsBin, cond->paramsBin, cond->paramsBinLength);
    return ffill;
}


int evalIsFulfilled(const CC *cond) {
    return 1;
}


static void evalFree(CC *cond) {
    free(cond->paramsBin);
    free(cond);
}


static uint32_t evalSubtypes(const CC *cond) {
    return 0;
}


/*
 * The JSON api doesn't contain custom verifiers, so a stub method is provided suitable for testing
 */
int jsonVerifyEval(CC *cond, void *context) {
    if (strcmp(cond->method, "testEval") == 0) {
        return memcmp(cond->paramsBin, "testEval", cond->paramsBinLength) == 0;
    }
    fprintf(stderr, "Cannot verify eval; user function unknown\n");
    return 0;
}


typedef struct CCEvalVerifyData {
    VerifyEval verify;
    void *context;
} CCEvalVerifyData;


int evalVisit(CC *cond, CCVisitor visitor) {
    if (cond->type->typeId != cc_evalType.typeId) return 1;
    CCEvalVerifyData *evalData = visitor.context;
    return evalData->verify(cond, evalData->context);
}


int cc_verifyEval(const CC *cond, VerifyEval verify, void *context) {
    CCEvalVerifyData evalData = {verify, context};
    CCVisitor visitor = {&evalVisit, "", 0, &evalData};
    return cc_visit(cond, visitor);
}


struct CCType cc_evalType = { 15, "eval-sha-256", Condition_PR_evalSha256, 0, 0, &evalFingerprint, &evalCost, &evalSubtypes, &evalFromJSON, &evalToJSON, &evalFromFulfillment, &evalToFulfillment, &evalIsFulfilled, &evalFree };
