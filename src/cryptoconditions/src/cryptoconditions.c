/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "strings.h"
#include "asn/Condition.h"
#include "asn/Fulfillment.h"
#include "asn/OCTET_STRING.h"
#include "cryptoconditions.h"
#include "src/internal.h"
#include "src/threshold.c"
#include "src/prefix.c"
#include "src/preimage.c"
#include "src/ed25519.c"
#include "src/secp256k1.c"
#include "src/anon.c"
#include "src/eval.c"
#include "src/json_rpc.c"
#include <cJSON.h>

#include <stdlib.h>


struct CCType *CCTypeRegistry[] = {
    &CC_PreimageType,
    &CC_PrefixType,
    &CC_ThresholdType,
    NULL, /* &CC_rsaType */
    &CC_Ed25519Type,
    &CC_Secp256k1Type,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 6-14 unused */
    &CC_EvalType
};


int CCTypeRegistryLength = sizeof(CCTypeRegistry) / sizeof(CCTypeRegistry[0]);


void appendUriSubtypes(uint32_t mask, unsigned char *buf) {
    int append = 0;
    for (int i=0; i<32; i++) {
        if (mask & 1 << i) {
            if (append) {
                strcat(buf, ",");
                strcat(buf, CCTypeRegistry[i]->name);
            } else {
                strcat(buf, "&subtypes=");
                strcat(buf, CCTypeRegistry[i]->name);
                append = 1;
            }
        }
    }
}


char *cc_conditionUri(const CC *cond) {
    unsigned char *fp = cond->type->fingerprint(cond);
    if (!fp) return NULL;

    unsigned char *encoded = base64_encode(fp, 32);

    unsigned char *out = calloc(1, 1000);
    sprintf(out, "ni:///sha-256;%s?fpt=%s&cost=%lu",encoded, cc_typeName(cond), cc_getCost(cond));
    
    if (cond->type->getSubtypes) {
        appendUriSubtypes(cond->type->getSubtypes(cond), out);
    }

    free(fp);
    free(encoded);

    return out;
}


ConditionTypes_t asnSubtypes(uint32_t mask) {
    ConditionTypes_t types;
    uint8_t buf[4] = {0,0,0,0};
    int maxId = 0;

    for (int i=0; i<32; i++) {
        if (mask & (1<<i)) {
            maxId = i;
            buf[i >> 3] |= 1 << (7 - i % 8);
        }
    }
    
    types.size = 1 + (maxId >> 3);
    types.buf = calloc(1, types.size);
    memcpy(types.buf, &buf, types.size);
    types.bits_unused = 7 - maxId % 8;
    return types;
}


uint32_t fromAsnSubtypes(const ConditionTypes_t types) {
    uint32_t mask = 0;
    for (int i=0; i<types.size*8; i++) {
        if (types.buf[i >> 3] & (1 << (7 - i % 8))) {
            mask |= 1 << i;
        }
    }
    return mask;
}


size_t cc_conditionBinary(const CC *cond, unsigned char *buf) {
    Condition_t *asn = calloc(1, sizeof(Condition_t));
    asnCondition(cond, asn);
    asn_enc_rval_t rc = der_encode_to_buffer(&asn_DEF_Condition, asn, buf, 1000);
    if (rc.encoded == -1) {
        fprintf(stderr, "CONDITION NOT ENCODED\n");
        return 0;
    }
    ASN_STRUCT_FREE(asn_DEF_Condition, asn);
    return rc.encoded;
}


size_t cc_fulfillmentBinary(const CC *cond, unsigned char *buf, size_t length) {
    Fulfillment_t *ffill = asnFulfillmentNew(cond);
    asn_enc_rval_t rc = der_encode_to_buffer(&asn_DEF_Fulfillment, ffill, buf, length);
    if (rc.encoded == -1) {
        fprintf(stderr, "FULFILLMENT NOT ENCODED\n");
        return 0;
    }
    ASN_STRUCT_FREE(asn_DEF_Fulfillment, ffill);
    return rc.encoded;
}


void asnCondition(const CC *cond, Condition_t *asn) {
    asn->present = cc_isAnon(cond) ? cond->conditionType->asnType : cond->type->asnType;
    
    // This may look a little weird - we dont have a reference here to the correct
    // union choice for the condition type, so we just assign everything to the threshold
    // type. This works out nicely since the union choices have the same binary interface.
    CompoundSha256Condition_t *choice = &asn->choice.thresholdSha256;
    choice->cost = cc_getCost(cond);
    choice->fingerprint.buf = cond->type->fingerprint(cond);
    choice->fingerprint.size = 32;
    choice->subtypes = asnSubtypes(cond->type->getSubtypes(cond));
}


Condition_t *asnConditionNew(const CC *cond) {
    Condition_t *asn = calloc(1, sizeof(Condition_t));
    asnCondition(cond, asn);
    return asn;
}


Fulfillment_t *asnFulfillmentNew(const CC *cond) {
    return cond->type->toFulfillment(cond);
}


unsigned long cc_getCost(const CC *cond) {
    return cond->type->getCost(cond);
}


CCType *getTypeByAsnEnum(Condition_PR present) {
    for (int i=0; i<CCTypeRegistryLength; i++) {
        if (CCTypeRegistry[i] != NULL && CCTypeRegistry[i]->asnType == present) {
            return CCTypeRegistry[i];
        }
    }
    return NULL;
}


CC *fulfillmentToCC(Fulfillment_t *ffill) {
    CCType *type = getTypeByAsnEnum(ffill->present);
    if (!type) {
        fprintf(stderr, "Unknown fulfillment type: %i\n", ffill->present);
        return 0;
    }
    return type->fromFulfillment(ffill);
}


CC *cc_readFulfillmentBinary(const unsigned char *ffill_bin, size_t ffill_bin_len) {
    CC *cond = 0;
    unsigned char *buf = calloc(1,ffill_bin_len);
    Fulfillment_t *ffill = 0;
    asn_dec_rval_t rval = ber_decode(0, &asn_DEF_Fulfillment, (void **)&ffill, ffill_bin, ffill_bin_len);
    if (rval.code != RC_OK) {
        goto end;
    }
    // Do malleability check
    asn_enc_rval_t rc = der_encode_to_buffer(&asn_DEF_Fulfillment, ffill, buf, ffill_bin_len);
    if (rc.encoded == -1) {
        fprintf(stderr, "FULFILLMENT NOT ENCODED\n");
        goto end;
    }
    if (rc.encoded != ffill_bin_len || 0 != memcmp(ffill_bin, buf, rc.encoded)) {
        goto end;
    }
    
    cond = fulfillmentToCC(ffill);
end:
    free(buf);
    if (ffill) ASN_STRUCT_FREE(asn_DEF_Fulfillment, ffill);
    return cond;
}

int cc_readFulfillmentBinaryExt(const unsigned char *ffill_bin, size_t ffill_bin_len, CC **ppcc) {

    int error = 0;
    unsigned char *buf = calloc(1,ffill_bin_len);
    Fulfillment_t *ffill = 0;
    asn_dec_rval_t rval = ber_decode(0, &asn_DEF_Fulfillment, (void **)&ffill, ffill_bin, ffill_bin_len);
    if (rval.code != RC_OK) {
        error = rval.code;
        goto end;
    }
    // Do malleability check
    asn_enc_rval_t rc = der_encode_to_buffer(&asn_DEF_Fulfillment, ffill, buf, ffill_bin_len);
    if (rc.encoded == -1) {
        fprintf(stderr, "FULFILLMENT NOT ENCODED\n");
        error = -1;
        goto end;
    }
    if (rc.encoded != ffill_bin_len || 0 != memcmp(ffill_bin, buf, rc.encoded)) {
        error = (rc.encoded == ffill_bin_len) ? -3 : -2;
        goto end;
    }
    
    *ppcc = fulfillmentToCC(ffill);
end:
    free(buf);
    if (ffill) ASN_STRUCT_FREE(asn_DEF_Fulfillment, ffill);
    return error;
}


int cc_visit(CC *cond, CCVisitor visitor) {
    int out = visitor.visit(cond, visitor);
    if (out && cond->type->visitChildren) {
        out = cond->type->visitChildren(cond, visitor);
    }
    return out;
}

int cc_verify(const struct CC *cond, const unsigned char *msg, size_t msgLength, int doHashMsg,
              const unsigned char *condBin, size_t condBinLength,
              VerifyEval verifyEval, void *evalContext) {
    unsigned char targetBinary[1000];
    //fprintf(stderr,"in cc_verify cond.%p msg.%p[%d] dohash.%d condbin.%p[%d]\n",cond,msg,(int32_t)msgLength,doHashMsg,condBin,(int32_t)condBinLength);
    const size_t binLength = cc_conditionBinary(cond, targetBinary);
    if (0 != memcmp(condBin, targetBinary, binLength)) {
        fprintf(stderr,"cc_verify error A\n");
        return 0;
    }
    if (!cc_ed25519VerifyTree(cond, msg, msgLength)) {
        fprintf(stderr,"cc_verify error B\n");
        return 0;
    }

    unsigned char msgHash[32];
    if (doHashMsg) sha256(msg, msgLength, msgHash);
    else memcpy(msgHash, msg, 32);

    if (!cc_secp256k1VerifyTreeMsg32(cond, msgHash)) {
        fprintf(stderr,"cc_verify error C\n");
        return 0;
    }

    if (!cc_verifyEval(cond, verifyEval, evalContext)) {
        //fprintf(stderr,"cc_verify error D\n");
        return 0;
    }
    return 1;
}


CC *cc_readConditionBinary(const unsigned char *cond_bin, size_t length) {
    Condition_t *asnCond = 0;
    asn_dec_rval_t rval;
    rval = ber_decode(0, &asn_DEF_Condition, (void **)&asnCond, cond_bin, length);
    if (rval.code != RC_OK) {
        fprintf(stderr, "Failed reading condition binary\n");
        return NULL;
    }
    CC *cond = mkAnon(asnCond);
    ASN_STRUCT_FREE(asn_DEF_Condition, asnCond);
    return cond;
}


int cc_isAnon(const CC *cond) {
    return cond->type->typeId == CC_Anon;
}


enum CCTypeId cc_typeId(const CC *cond) {
    return cc_isAnon(cond) ? cond->conditionType->typeId : cond->type->typeId;
}


uint32_t cc_typeMask(const CC *cond) {
    uint32_t mask = 1 << cc_typeId(cond);
    if (cond->type->getSubtypes)
        mask |= cond->type->getSubtypes(cond);
    return mask;
}


int cc_isFulfilled(const CC *cond) {
    return cond->type->isFulfilled(cond);
}


char *cc_typeName(const CC *cond) {
    return cc_isAnon(cond) ? cond->conditionType->name : cond->type->name;
}


CC *cc_new(int typeId) {
     CC *cond = calloc(1, sizeof(CC));
     cond->type = typeId == CC_Anon ? &CC_AnonType : CCTypeRegistry[typeId];
     return cond;
}


void cc_free(CC *cond) {
    if (cond)
        cond->type->free(cond);
    free(cond);
}


