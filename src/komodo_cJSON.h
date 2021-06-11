/*
 Copyright (c) 2009 Dave Gamble
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

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

#ifndef komodo_cJSON__h
#define komodo_cJSON__h

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <float.h>
#include <memory.h>

#include "bits256.h"
#include "cJSON.h"

//#include "../crypto777/OS_portable.h"

#define MAX_JSON_FIELD 4096 // on the big side

#ifdef __cplusplus
extern "C"
{
#endif

    /* Macros for creating things quickly. */
#define cJSON_AddNullToObject(object,name)		cJSON_AddItemToObject(object, name, cJSON_CreateNull())
#define cJSON_AddTrueToObject(object,name)		cJSON_AddItemToObject(object, name, cJSON_CreateTrue())
#define cJSON_AddFalseToObject(object,name)		cJSON_AddItemToObject(object, name, cJSON_CreateFalse())
#define cJSON_AddBoolToObject(object,name,b)	cJSON_AddItemToObject(object, name, cJSON_CreateBool(b))
#define cJSON_AddNumberToObject(object,name,n)	cJSON_AddItemToObject(object, name, cJSON_CreateNumber(n))
#define cJSON_AddStringToObject(object,name,s)	cJSON_AddItemToObject(object, name, cJSON_CreateString(s))
    
    struct destbuf { char buf[MAX_JSON_FIELD]; };
    
#define jfieldstr get_cJSON_fieldname
    
    char *cJSON_str(cJSON *json);
    char *jstr(cJSON *json,char *field);
    char *jprint(cJSON *json,int32_t freeflag);
    int32_t jint(cJSON *json,char *field);
    uint32_t juint(cJSON *json,char *field);
    char *jstri(cJSON *json,int32_t i);
    int32_t jinti(cJSON *json,int32_t i);
    uint32_t juinti(cJSON *json,int32_t i);
    uint64_t j64bitsi(cJSON *json,int32_t i);
    double jdoublei(cJSON *json,int32_t i);
    double jdouble(cJSON *json,char *field);
    cJSON *jobj(cJSON *json,char *field);
    cJSON *jarray(int32_t *nump,cJSON *json,char *field);
    cJSON *jitem(cJSON *array,int32_t i);
    uint64_t j64bits(cJSON *json,char *field);
    void jadd(cJSON *json,char *field,cJSON *item);
    void jaddstr(cJSON *json,char *field,char *str);
    void jaddnum(cJSON *json,char *field,double num);
    void jadd64bits(cJSON *json,char *field,uint64_t nxt64bits);
    void jaddi(cJSON *json,cJSON *item);
    void jaddistr(cJSON *json,char *str);
    void jaddinum(cJSON *json,double num);
    void jaddi64bits(cJSON *json,uint64_t nxt64bits);
    void jdelete(cJSON *object,char *string);
    cJSON *jduplicate(cJSON *json);
    int32_t jnum(cJSON *obj,char *field);
 
    bits256 jbits256(cJSON *json,char *field);
    bits256 jbits256i(cJSON *json,int32_t i);
    void jaddbits256(cJSON *json,char *field,bits256 hash);
    void jaddibits256(cJSON *json,bits256 hash);
    void copy_cJSON(struct destbuf *dest,cJSON *obj);
    void copy_cJSON2(char *dest,int32_t maxlen,cJSON *obj);
    cJSON *gen_list_json(char **list);
    int32_t extract_cJSON_str(char *dest,int32_t max,cJSON *json,char *field);

    void free_json(cJSON *json);
    int64_t _conv_cJSON_float(cJSON *json);
    int64_t conv_cJSON_float(cJSON *json,char *field);
    int64_t get_cJSON_int(cJSON *json,char *field);
    void add_satoshis_json(cJSON *json,char *field,uint64_t satoshis);
    uint64_t get_satoshi_obj(cJSON *json,char *field);
    
    int32_t get_API_int(cJSON *obj,int32_t val);
    uint32_t get_API_uint(cJSON *obj,uint32_t val);
    uint64_t get_API_nxt64bits(cJSON *obj);
    double get_API_float(cJSON *obj);
    char *get_cJSON_fieldname(cJSON *obj);
    void ensure_jsonitem(cJSON *json,char *field,char *value);
    int32_t in_jsonarray(cJSON *array,char *value);
    char *bitcoind_RPC(char **retstrp,char *debugstr,char *url,char *userpass,char *command,char *params);
    uint64_t calc_nxt64bits(const char *str);
    int32_t expand_nxt64bits(char *str,uint64_t nxt64bits);
    char *nxt64str(uint64_t nxt64bits);
    char *nxt64str2(uint64_t nxt64bits);
    cJSON *addrs_jsonarray(uint64_t *addrs,int32_t num);
    int32_t myatoi(char *str,int32_t range);

    char *stringifyM(char *str);
#define replace_backslashquotes unstringify
    char *unstringify(char *str);
#define jtrue cJSON_CreateTrue
#define jfalse cJSON_CreateFalse

#define jfieldname get_cJSON_fieldname

#ifdef __cplusplus
}
#endif

#endif
