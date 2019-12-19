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

//#include "../include/cryptoconditions.h"
#include "internal.h"
//#include <cJSON.h>

#include <stdlib.h>


static cJSON *jsonCondition(CC *cond) {
    cJSON *root = cJSON_CreateObject();

    char *uri = cc_conditionUri(cond);
    cJSON_AddItemToObject(root, "uri", cJSON_CreateString(uri));
    free(uri);

    unsigned char buf[1000];
    size_t conditionBinLength = cc_conditionBinary(cond, buf);
    jsonAddHex(root, "bin", buf, conditionBinLength);

    return root;
}


static cJSON *jsonFulfillment(CC *cond) {
    uint8_t buf[1000000];
    size_t fulfillmentBinLength = cc_fulfillmentBinary(cond, buf, 1000000);

    cJSON *root = cJSON_CreateObject();
    jsonAddHex(root, "fulfillment", buf, fulfillmentBinLength);
    return root;
}


CC *cc_conditionFromJSON(cJSON *params, char *err) {
    if (!params || !cJSON_IsObject(params)) {
        strcpy(err, "Condition params must be an object");
        return NULL;
    }
    cJSON *typeName = cJSON_GetObjectItem(params, "type");
    if (!typeName || !cJSON_IsString(typeName)) {
        strcpy(err, "\"type\" must be a string");
        return NULL;
    }
    for (int i=0; i<CCTypeRegistryLength; i++) {
        if (CCTypeRegistry[i] != NULL) {
            if (0 == strcmp(typeName->valuestring, CCTypeRegistry[i]->name)) {
                return CCTypeRegistry[i]->fromJSON(params, err);
            }
        }
    }
    strcpy(err, "cannot detect type of condition");
    return NULL;
}


CC *cc_conditionFromJSONString(const char *data, char *err) {
    cJSON *params = cJSON_Parse(data);
    CC *out = cc_conditionFromJSON(params, err);
    cJSON_Delete(params);
    return out;
}


static cJSON *jsonEncodeCondition(cJSON *params, char *err) {
    CC *cond = cc_conditionFromJSON(params, err);
    cJSON *out = NULL;
    if (cond != NULL) {
        out = jsonCondition(cond);
        cc_free(cond);
    }
    return out;
}


static cJSON *jsonEncodeFulfillment(cJSON *params, char *err) {
    CC *cond = cc_conditionFromJSON(params, err);
    cJSON *out = NULL;
    if (cond != NULL) {
        out = jsonFulfillment(cond);
        cc_free(cond);
    }
    return out;
}


static cJSON *jsonErr(char *err) {
    cJSON *out = cJSON_CreateObject();
    cJSON_AddItemToObject(out, "error", cJSON_CreateString(err));
    return out;
}


static cJSON *jsonVerifyFulfillment(cJSON *params, char *err) {
    unsigned char *ffill_bin = 0, *msg = 0, *cond_bin = 0;
    size_t ffill_bin_len, msg_len, cond_bin_len;
    cJSON *out = 0;

    if (!(jsonGetHex(params, "fulfillment", err, &ffill_bin, &ffill_bin_len) &&
          jsonGetHex(params, "message", err, &msg, &msg_len) &&
          jsonGetHex(params, "condition", err, &cond_bin, &cond_bin_len)))
        goto END;

    CC *cond = cc_readFulfillmentBinary(ffill_bin, ffill_bin_len);

    if (!cond) {
        strcpy(err, "Invalid fulfillment payload");
        goto END;
    }

    int valid = cc_verify(cond, msg, msg_len, 1, cond_bin, cond_bin_len, &jsonVerifyEval, NULL);
    cc_free(cond);
    out = cJSON_CreateObject();
    cJSON_AddItemToObject(out, "valid", cJSON_CreateBool(valid));

END:
    free(ffill_bin); free(msg); free(cond_bin);
    return out;
}


static cJSON *jsonDecodeFulfillment(cJSON *params, char *err) {
    size_t ffill_bin_len;
    unsigned char *ffill_bin;
    if (!jsonGetHex(params, "fulfillment", err, &ffill_bin, &ffill_bin_len))
        return NULL;

    CC *cond = cc_readFulfillmentBinary(ffill_bin, ffill_bin_len);
    free(ffill_bin);
    if (!cond) {
        strcpy(err, "Invalid fulfillment payload");
        return NULL;
    }
    cJSON *out = jsonCondition(cond);
    cc_free(cond);
    return out;
}


static cJSON *jsonDecodeCondition(cJSON *params, char *err) {
    size_t cond_bin_len;
    unsigned char *cond_bin;
    if (!jsonGetHex(params, "bin", err, &cond_bin, &cond_bin_len))
        return NULL;

    CC *cond = cc_readConditionBinary(cond_bin, cond_bin_len);
    free(cond_bin);

    if (!cond) {
        strcpy(err, "Invalid condition payload");
        return NULL;
    }

    cJSON *out = jsonCondition(cond);
    cJSON_AddItemToObject(out, "condition", cc_conditionToJSON(cond));
    cc_free(cond);
    return out;
}


static cJSON *jsonSignTreeEd25519(cJSON *params, char *err) {
    cJSON *out = 0;
    unsigned char *msg = 0, *sk = 0;

    cJSON *condition_item = cJSON_GetObjectItem(params, "condition");
    CC *cond = cc_conditionFromJSON(condition_item, err);
    if (cond == NULL) {
        goto END;
    }

    size_t skLength;
    if (!jsonGetHex(params, "privateKey", err, &sk, &skLength)) {
        goto END;
    }

    if (skLength != 32) {
        strcpy(err, "privateKey wrong length");
    }
    
    size_t msgLength;
    if (!jsonGetHex(params, "message", err, &msg, &msgLength)) {
        goto END;
    }

    int nSigned = cc_signTreeEd25519(cond, sk, msg, msgLength);
    out = cJSON_CreateObject();
    cJSON_AddItemToObject(out, "num_signed", cJSON_CreateNumber(nSigned));
    cJSON_AddItemToObject(out, "condition", cc_conditionToJSON(cond));

END:
    cc_free(cond);
    free(msg);
    free(sk);
    return out;
}


static cJSON *jsonSignTreeSecp256k1(cJSON *params, char *err) {
    cJSON *out = 0;
    unsigned char *msg = 0, *sk = 0;

    cJSON *condition_item = cJSON_GetObjectItem(params, "condition");
    CC *cond = cc_conditionFromJSON(condition_item, err);
    if (cond == NULL) {
        goto END;
    }

    size_t skLength;
    if (!jsonGetHex(params, "privateKey", err, &sk, &skLength)) {
        goto END;
    }

    if (skLength != SECP256K1_SK_SIZE) {
        strcpy(err, "privateKey wrong length");
    }
    
    size_t msgLength;
    if (!jsonGetHex(params, "message", err, &msg, &msgLength)) {
        goto END;
    }

    char msgHash[32];
    sha256(msg, msgLength, msgHash);
    int nSigned = cc_signTreeSecp256k1Msg32(cond, sk, msgHash);
    out = cJSON_CreateObject();
    cJSON_AddItemToObject(out, "num_signed", cJSON_CreateNumber(nSigned));
    cJSON_AddItemToObject(out, "condition", cc_conditionToJSON(cond));

END:
    cc_free(cond);
    free(msg);
    free(sk);
    return out;
}


cJSON *cc_conditionToJSON(const CC *cond) {
    cJSON *params = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "type", cJSON_CreateString(cond->type->name));
    cond->type->toJSON(cond, params);
    return params;
}


char *cc_conditionToJSONString(const CC *cond) {
    assert(cond != NULL);
    cJSON *params = cc_conditionToJSON(cond);
    char *out = cJSON_Print(params);
    cJSON_Delete(params);
    return out;
}


static cJSON *jsonListMethods(cJSON *params, char *err);


typedef struct JsonMethod {
    char *name;
    cJSON* (*method)(cJSON *params, char *err);
    char *description;
} JsonMethod;


static JsonMethod cc_jsonMethods[] = {
    {"encodeCondition", &jsonEncodeCondition, "Encode a JSON condition to binary"},
    {"decodeCondition", &jsonDecodeCondition, "Decode a binary condition"},
    {"encodeFulfillment", &jsonEncodeFulfillment, "Encode a JSON condition to a fulfillment"},
    {"decodeFulfillment", &jsonDecodeFulfillment, "Decode a binary fulfillment"},
    {"verifyFulfillment", &jsonVerifyFulfillment, "Verify a fulfillment"},
    {"signTreeEd25519", &jsonSignTreeEd25519, "Sign ed25519 condition nodes"},
    {"signTreeSecp256k1", &jsonSignTreeSecp256k1, "Sign secp256k1 condition nodes"},
    {"listMethods", &jsonListMethods, "List available methods"}
};


static int nJsonMethods = sizeof(cc_jsonMethods) / sizeof(*cc_jsonMethods);


static cJSON *jsonListMethods(cJSON *params, char *err) {
    cJSON *list = cJSON_CreateArray();
    for (int i=0; i<nJsonMethods; i++) {
        JsonMethod method = cc_jsonMethods[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddItemToObject(item, "name", cJSON_CreateString(method.name));
        cJSON_AddItemToObject(item, "description", cJSON_CreateString(method.description));
        cJSON_AddItemToArray(list, item);
    }
    cJSON *out = cJSON_CreateObject();
    cJSON_AddItemToObject(out, "methods", list);
    return out;
}


static cJSON* execJsonRPC(cJSON *root, char *err) {
    cJSON *method_item = cJSON_GetObjectItem(root, "method");

    if (!cJSON_IsString(method_item)) {
        return jsonErr("malformed method");
    }

    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (!cJSON_IsObject(params)) {
        return jsonErr("params is not an object");
    }

    for (int i=0; i<nJsonMethods; i++) {
        JsonMethod method = cc_jsonMethods[i];
        if (0 == strcmp(method.name, method_item->valuestring)) {
            return method.method(params, err);
        }
    }

    return jsonErr("invalid method");
}


char *cc_jsonRPC(char* input) {
    char err[1000] = "\0";
    cJSON *out;
    cJSON *root = cJSON_Parse(input);
    if (!root) out = jsonErr("Error parsing JSON request");
    else {
        out = execJsonRPC(root, err);
        if (NULL == out) out = jsonErr(err);
    }
    char *res = cJSON_Print(out);
    cJSON_Delete(out);
    cJSON_Delete(root);
    return res;
}
