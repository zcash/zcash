
#include "asn/Condition.h"
#include "asn/Fulfillment.h"
#include "asn/ThresholdFingerprintContents.h"
#include "asn/OCTET_STRING.h"
#include "include/cJSON.h"
#include "cryptoconditions.h"
#include "internal.h"


struct CCType CC_ThresholdType;


static uint32_t thresholdSubtypes(const CC *cond) {
    uint32_t mask = 0;
    for (int i=0; i<cond->size; i++) {
        mask |= cc_typeMask(cond->subconditions[i]);
    }
    mask &= ~(1 << CC_Threshold);
    return mask;
}


static int cmpCostDesc(const void *a, const void *b) {
    return (int) ( *(unsigned long*)b - *(unsigned long*)a );
}


static unsigned long thresholdCost(const CC *cond) {
    CC *sub;
    unsigned long *costs = calloc(1, cond->size * sizeof(unsigned long));
    for (int i=0; i<cond->size; i++) {
        sub = cond->subconditions[i];
        costs[i] = cc_getCost(sub);
    }
    qsort(costs, cond->size, sizeof(unsigned long), cmpCostDesc);
    unsigned long cost = 0;
    for (int i=0; i<cond->threshold; i++) {
        cost += costs[i];
    }
    free(costs);
    return cost + 1024 * cond->size;
}


static int thresholdVisitChildren(CC *cond, CCVisitor visitor) {
    for (int i=0; i<cond->size; i++) {
        if (!cc_visit(cond->subconditions[i], visitor)) {
            return 0;
        }
    }
    return 1;
}


static int cmpConditionBin(const void *a, const void *b) {
    /* Compare conditions by their ASN binary representation */
    unsigned char bufa[BUF_SIZE], bufb[BUF_SIZE];
    asn_enc_rval_t r0 = der_encode_to_buffer(&asn_DEF_Condition, *(Condition_t**)a, bufa, BUF_SIZE);
    asn_enc_rval_t r1 = der_encode_to_buffer(&asn_DEF_Condition, *(Condition_t**)b, bufb, BUF_SIZE);

    // below copied from ASN lib
    size_t commonLen = r0.encoded < r1.encoded ? r0.encoded : r1.encoded;
    int ret = memcmp(bufa, bufb, commonLen);

    if (ret == 0)
        return r0.encoded < r1.encoded ? -1 : 1;
    return 0;
}


static unsigned char *thresholdFingerprint(const CC *cond) {
    /* Create fingerprint */
    ThresholdFingerprintContents_t *fp = calloc(1, sizeof(ThresholdFingerprintContents_t));
    fp->threshold = cond->threshold;
    for (int i=0; i<cond->size; i++) {
        Condition_t *asnCond = asnConditionNew(cond->subconditions[i]);
        asn_set_add(&fp->subconditions2, asnCond);
    }
    qsort(fp->subconditions2.list.array, cond->size, sizeof(Condition_t*), cmpConditionBin);
    return hashFingerprintContents(&asn_DEF_ThresholdFingerprintContents, fp);
}


static int cmpConditionCost(const void *a, const void *b) {
    CC *ca = *((CC**)a);
    CC *cb = *((CC**)b);

    int out = cc_getCost(ca) - cc_getCost(cb);
    if (out != 0) return out;

    // Do an additional sort to establish consistent order
    // between conditions with the same cost.
    Condition_t *asna = asnConditionNew(ca);
    Condition_t *asnb = asnConditionNew(cb);
    out = cmpConditionBin(&asna, &asnb);
    ASN_STRUCT_FREE(asn_DEF_Condition, asna);
    ASN_STRUCT_FREE(asn_DEF_Condition, asnb);
    return out;
}


static CC *thresholdFromFulfillment(const Fulfillment_t *ffill) {
    ThresholdFulfillment_t *t = ffill->choice.thresholdSha256;
    int threshold = t->subfulfillments.list.count;
    int size = threshold + t->subconditions.list.count;

    CC **subconditions = calloc(size, sizeof(CC*));

    for (int i=0; i<size; i++) {
        subconditions[i] = (i < threshold) ?
            fulfillmentToCC(t->subfulfillments.list.array[i]) :
            mkAnon(t->subconditions.list.array[i-threshold]);

        if (!subconditions[i]) {
            for (int j=0; j<i; j++) free(subconditions[j]);
            free(subconditions);
            return 0;
        }
    }

    CC *cond = cc_new(CC_Threshold);
    cond->threshold = threshold;
    cond->size = size;
    cond->subconditions = subconditions;
    return cond;
}


static Fulfillment_t *thresholdToFulfillment(const CC *cond) {
    CC *sub;
    Fulfillment_t *fulfillment;

    // Make a copy of subconditions so we can leave original order alone
    CC** subconditions = malloc(cond->size*sizeof(CC*));
    memcpy(subconditions, cond->subconditions, cond->size*sizeof(CC*));
    
    qsort(subconditions, cond->size, sizeof(CC*), cmpConditionCost);

    ThresholdFulfillment_t *tf = calloc(1, sizeof(ThresholdFulfillment_t));

    int needed = cond->threshold;

    for (int i=0; i<cond->size; i++) {
        sub = subconditions[i];
        if (needed && (fulfillment = asnFulfillmentNew(sub))) {
            asn_set_add(&tf->subfulfillments, fulfillment);
            needed--;
        } else {
            asn_set_add(&tf->subconditions, asnConditionNew(sub));
        }
    }

    free(subconditions);

    if (needed) {
        ASN_STRUCT_FREE(asn_DEF_ThresholdFulfillment, tf);
        return NULL;
    }

    fulfillment = calloc(1, sizeof(Fulfillment_t));
    fulfillment->present = Fulfillment_PR_thresholdSha256;
    fulfillment->choice.thresholdSha256 = tf;
    return fulfillment;
}


static CC *thresholdFromJSON(const cJSON *params, char *err) {
    cJSON *threshold_item = cJSON_GetObjectItem(params, "threshold");
    if (!cJSON_IsNumber(threshold_item)) {
        strcpy(err, "threshold must be a number");
        return NULL;
    }

    cJSON *subfulfillments_item = cJSON_GetObjectItem(params, "subfulfillments");
    if (!cJSON_IsArray(subfulfillments_item)) {
        strcpy(err, "subfulfullments must be an array");
        return NULL;
    }

    CC *cond = cc_new(CC_Threshold);
    cond->threshold = (long) threshold_item->valuedouble;
    cond->size = cJSON_GetArraySize(subfulfillments_item);
    cond->subconditions = calloc(cond->size, sizeof(CC*));
    
    cJSON *sub;
    for (int i=0; i<cond->size; i++) {
        sub = cJSON_GetArrayItem(subfulfillments_item, i);
        cond->subconditions[i] = cc_conditionFromJSON(sub, err);
        if (err[0]) return NULL;
    }

    return cond;
}


static void thresholdToJSON(const CC *cond, cJSON *params) {
    cJSON *subs = cJSON_CreateArray();
    cJSON_AddNumberToObject(params, "threshold", cond->threshold);
    for (int i=0; i<cond->size; i++) {
        cJSON_AddItemToArray(subs, cc_conditionToJSON(cond->subconditions[i]));
    }
    cJSON_AddItemToObject(params, "subfulfillments", subs);
}


static int thresholdIsFulfilled(const CC *cond) {
    int nFulfilled = 0;
    for (int i=0; i<cond->size; i++) {
        if (cc_isFulfilled(cond->subconditions[i])) {
            nFulfilled++;
        }
        if (nFulfilled == cond->threshold) {
            return 1;
        }
    }
    return 0;
}


static void thresholdFree(CC *cond) {
    for (int i=0; i<cond->size; i++) {
        cc_free(cond->subconditions[i]);
    }
    free(cond->subconditions);
}


struct CCType CC_ThresholdType = { 2, "threshold-sha-256", Condition_PR_thresholdSha256, &thresholdVisitChildren, &thresholdFingerprint, &thresholdCost, &thresholdSubtypes, &thresholdFromJSON, &thresholdToJSON, &thresholdFromFulfillment, &thresholdToFulfillment, &thresholdIsFulfilled, &thresholdFree };
