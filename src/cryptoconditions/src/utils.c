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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "include/cJSON.h"
#include "include/sha256.h"
#include "asn/asn_application.h"
#include "cryptoconditions.h"
#include "internal.h"


static unsigned char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static unsigned char *decoding_table = NULL;
static int mod_table[] = {0, 2, 1};


void build_decoding_table() {
    decoding_table = calloc(1,256);
    for (int i = 0; i < 64; i++)
        decoding_table[(unsigned char) encoding_table[i]] = i;
}


unsigned char *base64_encode(const unsigned char *data, size_t input_length) {

    size_t output_length = 4 * ((input_length + 2) / 3);

    unsigned char *encoded_data = calloc(1,output_length + 1);
    if (encoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {

        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    int strip = mod_table[input_length % 3];
    for (int i = 0; i < strip; i++)
        encoded_data[output_length - 1 - i] = '\0';
    // make sure there's a null termination for string protocol
    encoded_data[output_length] = '\0';


    // url safe
    for (int i=0; i<output_length; i++) {
        if (encoded_data[i] == '/') encoded_data[i] = '_';
        if (encoded_data[i] == '+') encoded_data[i] = '-';
    }

    return encoded_data;
}


unsigned char *base64_decode(const unsigned char *data_,
                             size_t *output_length) {

    if (decoding_table == NULL) build_decoding_table();

    size_t input_length = strlen(data_);
    int rem = input_length % 4;
    unsigned char *data = calloc(1,input_length + (4-rem));
    strcpy(data, data_);

    // for unpadded b64
    if (0 != rem) {
        for (int i=0; i<4-rem; i++) {
            data[input_length + i] = '=';
        }
        input_length += 4-rem;
    }

    // for URL safe
    for (int i=0; i<input_length; i++) {
        if (data[i] == '-') data[i] = '+';
        if (data[i] == '_') data[i] = '/';
    }

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    unsigned char *decoded_data = calloc(1,*output_length);
    if (decoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {

        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }

    return decoded_data;
}


void base64_cleanup() {
    free(decoding_table);
    decoding_table = 0;
}


void dumpStr(unsigned char *str, size_t len) {
    if (-1 == len) len = strlen(str);
    fprintf(stderr, "len:%i ", (int)len);
    for (int i=0; i<len; i++) {
        if (isprint(str[i])) {
            fprintf(stderr, "%c", str[i]);
        } else {
            fprintf(stderr, "\\x%02x", str[i] & 0xff);
            //fprintf(stderr, "\\%u", str[i] & 0xff);
        }
    }
    fprintf(stderr, "\n");
}



int checkString(const cJSON *value, char *key, char *err) {
    if (value == NULL) {
        sprintf(err, "%s is required", key);
        return 0;
    }
    if (!cJSON_IsString(value)) {
        sprintf(err, "%s must be a string", key);
        return 0;
    }
    return 1;
}

int checkDecodeBase64(const cJSON *value, char *key, char *err, unsigned char **data, size_t *size) {
    if (!checkString(value, key, err))
        return 0;
    

    *data = base64_decode(value->valuestring, size);
    if (!*data) {
        sprintf(err, "%s must be valid base64 string", key);
        return 0;
    }
    return 1;
}


int jsonGetBase64(const cJSON *params, char *key, char *err, unsigned char **data, size_t *size)
{
    cJSON *item = cJSON_GetObjectItem(params, key);
    if (!item) {
        sprintf(err, "%s is required", key);
        return 0;
    }
    return checkDecodeBase64(item, key, err, data, size);
}


int jsonGetBase64Optional(const cJSON *params, char *key, char *err, unsigned char **data, size_t *size) {
    cJSON *item = cJSON_GetObjectItem(params, key);
    if (!item) {
        return 1;
    }
    return checkDecodeBase64(item, key, err, data, size);
}


void jsonAddBase64(cJSON *params, char *key, unsigned char *bin, size_t size) {
    unsigned char *b64 = base64_encode(bin, size);
    cJSON_AddItemToObject(params, key, cJSON_CreateString(b64));
    free(b64);
}


unsigned char *hashFingerprintContents(asn_TYPE_descriptor_t *asnType, void *fp) {
    unsigned char buf[BUF_SIZE];
    asn_enc_rval_t rc = der_encode_to_buffer(asnType, fp, buf, BUF_SIZE);
    ASN_STRUCT_FREE(*asnType, fp);
    if (rc.encoded < 1) {
        fprintf(stderr, "Encoding fingerprint failed\n");
        return 0;
    }
    unsigned char *hash = calloc(1,32);
    sha256(buf, rc.encoded, hash);
    return hash;
}


char* cc_hex_encode(const uint8_t *bin, size_t len)
{
    char* hex = calloc(1,len*2+1);
    if (bin == NULL) return hex;
    char map[16] = "0123456789ABCDEF";
    for (int i=0; i<len; i++) {
        hex[i*2] = map[bin[i] >> 4];
        hex[i*2+1] = map[bin[i] & 0x0F];
    }
    hex[len*2] = '\0';
    return hex;
}


uint8_t* cc_hex_decode(const char* hex)
{
    size_t len = strlen(hex);

    if (len % 2 == 1) return NULL;

    uint8_t* bin = calloc(1, len/2);
    
    for (int i=0; i<len; i++) {
        char c = hex[i];
        if      (c <= 57) c -= 48;
        else if (c <= 70) c -= 55;
        else if (c <= 102) c -= 87;
        if (c < 0 || c > 15) goto ERR;
        
        bin[i/2] += c << (i%2 ? 0 : 4);
    }
    return bin;
ERR:
    free(bin);
    return NULL;
}


bool checkDecodeHex(const cJSON *value, char *key, char *err, unsigned char **data, size_t *size) {
    if (!checkString(value, key, err))
        return 0;

    *data = cc_hex_decode(value->valuestring);
    if (!*data) {
        sprintf(err, "%s must be valid hex string", key);
        return 0;
    }
    *size = strlen(value->valuestring) / 2;
    return 1;
}


bool jsonGetHex(const cJSON *params, char *key, char *err, unsigned char **data, size_t *size)
{
    cJSON *item = cJSON_GetObjectItem(params, key);
    if (!item) {
        sprintf(err, "%s is required", key);
        return 0;
    }
    return checkDecodeHex(item, key, err, data, size);
}


void jsonAddHex(cJSON *params, char *key, unsigned char *bin, size_t size) {
    unsigned char *hex = cc_hex_encode(bin, size);
    cJSON_AddItemToObject(params, key, cJSON_CreateString(hex));
    free(hex);
}


int jsonGetHexOptional(const cJSON *params, char *key, char *err, unsigned char **data, size_t *size) {
    cJSON *item = cJSON_GetObjectItem(params, key);
    if (!item) {
        return 1;
    }
    return checkDecodeHex(item, key, err, data, size);
}


