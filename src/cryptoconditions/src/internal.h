#include "include/cJSON.h"
#include "asn/asn_application.h"
#include "cryptoconditions.h"

#ifndef INTERNAL_H
#define INTERNAL_H


#ifdef __cplusplus
extern "C" {
#endif


#define BUF_SIZE 1024 * 1024


/*
 * Condition Type */
typedef struct CCType {
    uint8_t typeId;
    unsigned char name[100];
    Condition_PR asnType;
    int hasSubtypes;
    int (*visitChildren)(CC *cond, CCVisitor visitor);
    unsigned char *(*fingerprint)(const CC *cond);
    unsigned long (*getCost)(const CC *cond);
    uint32_t (*getSubtypes)(const  CC *cond);
    CC *(*fromJSON)(const cJSON *params, unsigned char *err);
    void (*toJSON)(const CC *cond, cJSON *params);
    CC *(*fromFulfillment)(const Fulfillment_t *ffill);
    Fulfillment_t *(*toFulfillment)(const CC *cond);
    int (*isFulfilled)(const CC *cond);
    void (*free)(struct CC *cond);
} CCType;


/*
 * Globals
 */
static struct CCType *typeRegistry[];
static int typeRegistryLength;


/*
 * Internal API
 */
static uint32_t fromAsnSubtypes(ConditionTypes_t types);
static CC *mkAnon(const Condition_t *asnCond);
static void asnCondition(const CC *cond, Condition_t *asn);
static Condition_t *asnConditionNew(const CC *cond);
static Fulfillment_t *asnFulfillmentNew(const CC *cond);
static cJSON *jsonEncodeCondition(cJSON *params, unsigned char *err);
static struct CC *fulfillmentToCC(Fulfillment_t *ffill);
static struct CCType *getTypeByAsnEnum(Condition_PR present);


/*
 * Utility functions
 */
unsigned char *base64_encode(const unsigned char *data, size_t input_length);
unsigned char *base64_decode(const unsigned char *data_, size_t *output_length);
unsigned char *hashFingerprintContents(asn_TYPE_descriptor_t *asnType, void *fp);
void dumpStr(unsigned char *str, size_t len);
int checkString(const cJSON *value, unsigned char *key, unsigned char *err);
int checkDecodeBase64(const cJSON *value, unsigned char *key, unsigned char *err, unsigned char **data, size_t *size);
int jsonGetBase64(const cJSON *params, unsigned char *key, unsigned char *err, unsigned char **data, size_t *size);
int jsonGetBase64Optional(const cJSON *params, unsigned char *key, unsigned char *err, unsigned char **data, size_t *size);
void jsonAddBase64(cJSON *params, unsigned char *key, unsigned char *bin, size_t size);


#ifdef __cplusplus
}
#endif

#endif  /* INTERNAL_H */
