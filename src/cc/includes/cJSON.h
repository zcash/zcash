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

#ifndef cJSON__ccih
#define cJSON__ccih

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <float.h>
#include <memory.h>

#include "../crypto777/OS_portable.h"

#define SATOSHIDEN ((uint64_t)100000000L)
#define dstr(x) ((double)(x) / SATOSHIDEN)
#define MAX_JSON_FIELD 4096 // on the big side

#ifdef __cplusplus
extern "C"
{
#endif
    
    /* cJSON Types: */
#define cJSON_False 0
#define cJSON_True 1
#define cJSON_NULL 2
#define cJSON_Number 3
#define cJSON_String 4
#define cJSON_Array 5
#define cJSON_Object 6
	
#define is_cJSON_Null(json) ((json) != 0 && ((json)->type & 0xff) == cJSON_NULL)
#define is_cJSON_Array(json) ((json) != 0 && ((json)->type & 0xff) == cJSON_Array)
#define is_cJSON_String(json) ((json) != 0 && ((json)->type & 0xff) == cJSON_String)
#define is_cJSON_Number(json) ((json) != 0 && ((json)->type & 0xff) == cJSON_Number)
#define is_cJSON_Object(json) ((json) != 0 && ((json)->type & 0xff) == cJSON_Object)
#define is_cJSON_True(json) ((json) != 0 && ((json)->type & 0xff) == cJSON_True)
#define is_cJSON_False(json) ((json) != 0 && ((json)->type & 0xff) == cJSON_False)
    
#define cJSON_IsReference 256
    
    /* The cJSON structure: */
    typedef struct cJSON {
        struct cJSON *next,*prev;	/* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
        struct cJSON *child;		/* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
        
        int32_t type;					/* The type of the item, as above. */
        
        char *valuestring;			/* The item's string, if type==cJSON_String */
        int64_t valueint;				/* The item's number, if type==cJSON_Number */
        double valuedouble;			/* The item's number, if type==cJSON_Number */
        
        char *string;				/* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
        uint32_t cjsonid;
    } cJSON;
    
    typedef struct cJSON_Hooks {
        void *(*malloc_fn)(size_t sz);
        void (*free_fn)(void *ptr);
    } cJSON_Hooks;
    
    /* Supply malloc, realloc and free functions to cJSON */
    extern void cJSON_InitHooks(cJSON_Hooks* hooks);
    
    
    /* Supply a block of JSON, and this returns a cJSON object you can interrogate. Call cJSON_Delete when finished. */
    extern cJSON *cJSON_Parse(const char *value);
    /* Render a cJSON entity to text for transfer/storage. Free the char* when finished. */
    extern char  *cJSON_Print(cJSON *item);
    /* Render a cJSON entity to text for transfer/storage without any formatting. Free the char* when finished. */
    extern char  *cJSON_PrintUnformatted(cJSON *item);
    /* Delete a cJSON entity and all subentities. */
    extern void   cJSON_Delete(cJSON *c);
    
    /* Returns the number of items in an array (or object). */
    extern int	  cJSON_GetArraySize(cJSON *array);
    /* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
    extern cJSON *cJSON_GetArrayItem(cJSON *array,int32_t item);
    /* Get item "string" from object. Case insensitive. */
    extern cJSON *cJSON_GetObjectItem(cJSON *object,const char *string);
    
    /* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when cJSON_Parse() returns 0. 0 when cJSON_Parse() succeeds. */
    extern const char *cJSON_GetErrorPtr(void);
	
    /* These calls create a cJSON item of the appropriate type. */
    extern cJSON *cJSON_CreateNull(void);
    extern cJSON *cJSON_CreateTrue(void);
    extern cJSON *cJSON_CreateFalse(void);
    extern cJSON *cJSON_CreateBool(int32_t b);
    extern cJSON *cJSON_CreateNumber(double num);
    extern cJSON *cJSON_CreateString(const char *string);
    extern cJSON *cJSON_CreateArray(void);
    extern cJSON *cJSON_CreateObject(void);
    
    /* These utilities create an Array of count items. */
    extern cJSON *cJSON_CreateIntArray(int64_t *numbers,int32_t count);
    extern cJSON *cJSON_CreateFloatArray(float *numbers,int32_t count);
    extern cJSON *cJSON_CreateDoubleArray(double *numbers,int32_t count);
    extern cJSON *cJSON_CreateStringArray(char **strings,int32_t count);
    
    /* Append item to the specified array/object. */
    extern void cJSON_AddItemToArray(cJSON *array, cJSON *item);
    extern void	cJSON_AddItemToObject(cJSON *object,const char *string,cJSON *item);
    /* Append reference to item to the specified array/object. Use this when you want to add an existing cJSON to a new cJSON, but don't want to corrupt your existing cJSON. */
    extern void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item);
    extern void	cJSON_AddItemReferenceToObject(cJSON *object,const char *string,cJSON *item);
    
    /* Remove/Detatch items from Arrays/Objects. */
    extern cJSON *cJSON_DetachItemFromArray(cJSON *array,int32_t which);
    extern void   cJSON_DeleteItemFromArray(cJSON *array,int32_t which);
    extern cJSON *cJSON_DetachItemFromObject(cJSON *object,const char *string);
    extern void   cJSON_DeleteItemFromObject(cJSON *object,const char *string);
	
    /* Update array items. */
    extern void cJSON_ReplaceItemInArray(cJSON *array,int32_t which,cJSON *newitem);
    extern void cJSON_ReplaceItemInObject(cJSON *object,const char *string,cJSON *newitem);
    
    /* Duplicate a cJSON item */
    extern cJSON *cJSON_Duplicate(cJSON *item,int32_t recurse);
    /* Duplicate will create a new, identical cJSON item to the one you pass, in new memory that will
     need to be released. With recurse!=0, it will duplicate any children connected to the item.
     The item->next and ->prev pointers are always zero on return from Duplicate. */
    
    /* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
    extern cJSON *cJSON_ParseWithOpts(const char *value,const char **return_parse_end,int32_t require_null_terminated);
    
    extern void cJSON_Minify(char *json);
    
    /* Macros for creating things quickly. */
#define cJSON_AddNullToObject(object,name)		cJSON_AddItemToObject(object, name, cJSON_CreateNull())
#define cJSON_AddTrueToObject(object,name)		cJSON_AddItemToObject(object, name, cJSON_CreateTrue())
#define cJSON_AddFalseToObject(object,name)		cJSON_AddItemToObject(object, name, cJSON_CreateFalse())
#define cJSON_AddBoolToObject(object,name,b)	cJSON_AddItemToObject(object, name, cJSON_CreateBool(b))
#define cJSON_AddNumberToObject(object,name,n)	cJSON_AddItemToObject(object, name, cJSON_CreateNumber(n))
#define cJSON_AddStringToObject(object,name,s)	cJSON_AddItemToObject(object, name, cJSON_CreateString(s))
    
    struct destbuf { char buf[MAX_JSON_FIELD]; };
    
    /* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define cJSON_SetIntValue(object,val)			((object)?(object)->valueint=(object)->valuedouble=(val):(val))
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
    char *bitcoind_RPC(char **retstrp,char *debugstr,char *url,char *userpass,char *command,char *params,int32_t timeout);
    uint64_t calc_nxt64bits(const char *str);
    int32_t expand_nxt64bits(char *str,uint64_t nxt64bits);
    char *nxt64str(uint64_t nxt64bits);
    char *nxt64str2(uint64_t nxt64bits);
    cJSON *addrs_jsonarray(uint64_t *addrs,int32_t num);
    int32_t myatoi(char *str,int32_t range);
    void cJSON_register(cJSON *item);
    void cJSON_unregister(cJSON *item);

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
