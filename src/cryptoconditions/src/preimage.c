/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
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

#include "asn/Condition.h"
#include "asn/Fulfillment.h"
#include "asn/OCTET_STRING.h"
#include "include/cJSON.h"
#include "include/sha256.h"
#include "cryptoconditions.h"


struct CCType CC_PreimageType;


static CC *preimageFromJSON(const cJSON *params, char *err) {
    CC *cond = cc_new(CC_Preimage);
    if (!jsonGetBase64(params, "preimage", err, &cond->preimage, &cond->preimageLength)) {
        free(cond);
        return NULL;
    }
    return cond;
}


static void preimageToJSON(const CC *cond, cJSON *params) {
    jsonAddBase64(params, "preimage", cond->preimage, cond->preimageLength);
}


static unsigned long preimageCost(const CC *cond) {
    return (unsigned long) cond->preimageLength;
}


static unsigned char *preimageFingerprint(const CC *cond) {
    unsigned char *hash = calloc(1, 32);
    //fprintf(stderr,"preimage %p %p\n",hash,cond->preimage);
    sha256(cond->preimage, cond->preimageLength, hash);
    return hash;
}


static CC *preimageFromFulfillment(const Fulfillment_t *ffill) {
    CC *cond = cc_new(CC_Preimage);
    PreimageFulfillment_t p = ffill->choice.preimageSha256;
    cond->preimage = calloc(1, p.preimage.size);
    memcpy(cond->preimage, p.preimage.buf, p.preimage.size);
    cond->preimageLength = p.preimage.size;
    return cond;
}


static Fulfillment_t *preimageToFulfillment(const CC *cond) {
    Fulfillment_t *ffill = calloc(1, sizeof(Fulfillment_t));
    ffill->present = Fulfillment_PR_preimageSha256;
    PreimageFulfillment_t *pf = &ffill->choice.preimageSha256;
    OCTET_STRING_fromBuf(&pf->preimage, cond->preimage, cond->preimageLength);
    return ffill;
}


int preimageIsFulfilled(const CC *cond) {
    return 1;
}


static void preimageFree(CC *cond) {
    free(cond->preimage);
}


static uint32_t preimageSubtypes(const CC *cond) {
    return 0;
}


struct CCType CC_PreimageType = { 0, "preimage-sha-256", Condition_PR_preimageSha256, 0, &preimageFingerprint, &preimageCost, &preimageSubtypes, &preimageFromJSON, &preimageToJSON, &preimageFromFulfillment, &preimageToFulfillment, &preimageIsFulfilled, &preimageFree };
