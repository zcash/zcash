
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

/* cJSON */
/* JSON parser in C. */
#include <math.h>

#include "cJSON.h"
#include "komodo_cJSON.h"
#include "cJSON.c"

#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131E-16
#endif

static const char *ep;

long stripquotes(char *str)
{
    long len,offset;
    if ( str == 0 )
        return(0);
    len = strlen(str);
    if ( str[0] == '"' && str[len-1] == '"' )
        str[len-1] = 0, offset = 1;
    else offset = 0;
    return(offset);
}

static int32_t cJSON_strcasecmp(const char *s1,const char *s2)
{
	if (!s1) return (s1==s2)?0:1;if (!s2) return 1;
	for(; tolower((int32_t)(*s1)) == tolower((int32_t)(*s2)); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return tolower((int32_t)(*(const unsigned char *)s1)) - tolower((int32_t)(*(const unsigned char *)s2));
}

// the following written by jl777
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

void copy_cJSON(struct destbuf *dest,cJSON *obj)
{
    char *str;
    int i;
    long offset;
    dest->buf[0] = 0;
    if ( obj != 0 )
    {
        str = cJSON_Print(obj);
        if ( str != 0 )
        {
            offset = stripquotes(str);
            //strcpy(dest,str+offset);
            for (i=0; i<MAX_JSON_FIELD-1; i++)
                if ( (dest->buf[i]= str[offset+i]) == 0 )
                    break;
            dest->buf[i] = 0;
            free(str);
        }
    }
}

void copy_cJSON2(char *dest,int32_t maxlen,cJSON *obj)
{
    struct destbuf tmp;
    maxlen--;
    dest[0] = 0;
    if ( maxlen > sizeof(tmp.buf) )
        maxlen = sizeof(tmp.buf);
    copy_cJSON(&tmp,obj);
    if ( strlen(tmp.buf) < maxlen )
        strcpy(dest,tmp.buf);
    else dest[0] = 0;
}

int64_t _get_cJSON_int(cJSON *json)
{
    struct destbuf tmp;
    if ( json != 0 )
    {
        copy_cJSON(&tmp,json);
        if ( tmp.buf[0] != 0 )
            return(calc_nxt64bits(tmp.buf));
    }
    return(0);
}

int64_t get_cJSON_int(cJSON *json,char *field)
{
    cJSON *numjson;
    if ( json != 0 )
    {
        numjson = cJSON_GetObjectItem(json,field);
        if ( numjson != 0 )
            return(_get_cJSON_int(numjson));
    }
    return(0);
}

int64_t conv_floatstr(char *numstr)
{
    double val,corr;
    val = atof(numstr);
    corr = (val < 0.) ? -0.50000000001 : 0.50000000001;
    return((int64_t)(val * SATOSHIDEN + corr));
}

int64_t _conv_cJSON_float(cJSON *json)
{
    int64_t conv_floatstr(char *);
    struct destbuf tmp;
    if ( json != 0 )
    {
        copy_cJSON(&tmp,json);
        return(conv_floatstr(tmp.buf));
    }
    return(0);
}

int64_t conv_cJSON_float(cJSON *json,char *field)
{
    if ( json != 0 )
        return(_conv_cJSON_float(cJSON_GetObjectItem(json,field)));
    return(0);
}

int32_t extract_cJSON_str(char *dest,int32_t max,cJSON *json,char *field)
{
    int32_t safecopy(char *dest,char *src,long len);
    char *str;
    cJSON *obj;
    int32_t len;
    long offset;
    dest[0] = 0;
    obj = cJSON_GetObjectItem(json,field);
    if ( obj != 0 )
    {
        str = cJSON_Print(obj);
        offset = stripquotes(str);
        len = safecopy(dest,str+offset,max);
        free(str);
        return(len);
    }
    return(0);
}

cJSON *gen_list_json(char **list)
{
    cJSON *array,*item;
    array = cJSON_CreateArray();
    while ( list != 0 && *list != 0 && *list[0] != 0 )
    {
        item = cJSON_CreateString(*list++);
        cJSON_AddItemToArray(array,item);
    }
    return(array);
}

uint64_t get_API_nxt64bits(cJSON *obj)
{
    uint64_t nxt64bits = 0;
    struct destbuf tmp;
    if ( obj != 0 )
    {
        if ( cJSON_IsNumber(obj) != 0 )
            return((uint64_t)obj->valuedouble);
        copy_cJSON(&tmp,obj);
        nxt64bits = calc_nxt64bits(tmp.buf);
    }
    return(nxt64bits);
}
uint64_t j64bits(cJSON *json,char *field) { if ( field == 0 ) return(get_API_nxt64bits(json)); return(get_API_nxt64bits(cJSON_GetObjectItem(json,field))); }
uint64_t j64bitsi(cJSON *json,int32_t i) { return(get_API_nxt64bits(cJSON_GetArrayItem(json,i))); }

uint64_t get_satoshi_obj(cJSON *json,char *field)
{
    int32_t i,n;
    uint64_t prev,satoshis,mult = 1;
    struct destbuf numstr,checkstr;
    cJSON *numjson;
    numjson = cJSON_GetObjectItem(json,field);
    copy_cJSON(&numstr,numjson);
    satoshis = prev = 0; mult = 1; n = (int32_t)strlen(numstr.buf);
    for (i=n-1; i>=0; i--,mult*=10)
    {
        satoshis += (mult * (numstr.buf[i] - '0'));
        if ( satoshis < prev )
            printf("get_satoshi_obj numstr.(%s) i.%d prev.%llu vs satoshis.%llu\n",numstr.buf,i,(unsigned long long)prev,(unsigned long long)satoshis);
        prev = satoshis;
    }
    sprintf(checkstr.buf,"%llu",(long long)satoshis);
    if ( strcmp(checkstr.buf,numstr.buf) != 0 )
    {
        printf("SATOSHI GREMLIN?? numstr.(%s) -> %.8f -> (%s)\n",numstr.buf,dstr(satoshis),checkstr.buf);
    }
    return(satoshis);
}

void add_satoshis_json(cJSON *json,char *field,uint64_t satoshis)
{
    cJSON *obj;
    char numstr[64];
    sprintf(numstr,"%lld",(long long)satoshis);
    obj = cJSON_CreateString(numstr);
    cJSON_AddItemToObject(json,field,obj);
    if ( satoshis != get_satoshi_obj(json,field) )
        printf("error adding satoshi obj %ld -> %ld\n",(unsigned long)satoshis,(unsigned long)get_satoshi_obj(json,field));
}

char *cJSON_str(cJSON *json)
{
    if ( json != 0 && cJSON_IsString(json) != 0 )
        return(json->valuestring);
    return(0);
}

void jadd(cJSON *json,char *field,cJSON *item) { if ( json != 0 )cJSON_AddItemToObject(json,field,item); }
void jaddstr(cJSON *json,char *field,char *str) { if ( json != 0 && str != 0 ) cJSON_AddItemToObject(json,field,cJSON_CreateString(str)); }
void jaddnum(cJSON *json,char *field,double num) { if ( json != 0 )cJSON_AddItemToObject(json,field,cJSON_CreateNumber(num)); }
void jadd64bits(cJSON *json,char *field,uint64_t nxt64bits) { char numstr[64]; sprintf(numstr,"%llu",(long long)nxt64bits), jaddstr(json,field,numstr); }
void jaddi(cJSON *json,cJSON *item) { if ( json != 0 ) cJSON_AddItemToArray(json,item); }
void jaddistr(cJSON *json,char *str) { if ( json != 0 ) cJSON_AddItemToArray(json,cJSON_CreateString(str)); }
void jaddinum(cJSON *json,double num) { if ( json != 0 ) cJSON_AddItemToArray(json,cJSON_CreateNumber(num)); }
void jaddi64bits(cJSON *json,uint64_t nxt64bits) { char numstr[64]; sprintf(numstr,"%llu",(long long)nxt64bits), jaddistr(json,numstr); }
char *jstr(cJSON *json,char *field) { if ( json == 0 ) return(0); if ( field == 0 ) return(cJSON_str(json)); return(cJSON_str(cJSON_GetObjectItem(json,field))); }

char *jstri(cJSON *json,int32_t i) { return(cJSON_str(cJSON_GetArrayItem(json,i))); }
char *jprint(cJSON *json,int32_t freeflag)
{
    char *str;
    /*static portable_mutex_t mutex; static int32_t initflag;
    if ( initflag == 0 )
    {
        portable_mutex_init(&mutex);
        initflag = 1;
    }*/
    if ( json == 0 )
        return(clonestr((char *)"{}"));
    //portable_mutex_lock(&mutex);
    //usleep(5000);
    str = cJSON_Print(json), _stripwhite(str,' ');
    if ( freeflag != 0 )
        free_json(json);
    //portable_mutex_unlock(&mutex);
    return(str);
}

bits256 get_API_bits256(cJSON *obj)
{
    bits256 hash; char *str;
    memset(hash.bytes,0,sizeof(hash));
    if ( obj != 0 )
    {
        if ( cJSON_IsString(obj) != 0 && (str= obj->valuestring) != 0 && strlen(str) == 64 )
            decode_hex(hash.bytes,sizeof(hash),str);
    }
    return(hash);
}
bits256 jbits256(cJSON *json,char *field) { if ( field == 0 ) return(get_API_bits256(json)); return(get_API_bits256(cJSON_GetObjectItem(json,field))); }
bits256 jbits256i(cJSON *json,int32_t i) { return(get_API_bits256(cJSON_GetArrayItem(json,i))); }
void jaddbits256(cJSON *json,char *field,bits256 hash) { char str[65]; bits256_str(str,hash), jaddstr(json,field,str); }
void jaddibits256(cJSON *json,bits256 hash) { char str[65]; bits256_str(str,hash), jaddistr(json,str); }

char *get_cJSON_fieldname(cJSON *obj)
{
    if ( obj != 0 )
    {
        if ( obj->child != 0 && obj->child->string != 0 )
            return(obj->child->string);
        else if ( obj->string != 0 )
            return(obj->string);
    }
    return((char *)"<no cJSON string field>");
}

int32_t jnum(cJSON *obj,char *field)
{
    char *str; int32_t polarity = 1;
    if ( field != 0 )
        obj = jobj(obj,field);
    if ( obj != 0 )
    {
        if ( cJSON_IsNumber(obj) != 0 )
            return(obj->valuedouble);
        else if ( cJSON_IsString(obj) != 0 && (str= jstr(obj,0)) != 0 )
        {
            if ( str[0] == '-' )
                polarity = -1, str++;
            return(polarity * (int32_t)calc_nxt64bits(str));
        }
    }
    return(0);
}

void ensure_jsonitem(cJSON *json,char *field,char *value)
{
    cJSON *obj = cJSON_GetObjectItem(json,field);
    if ( obj == 0 )
        cJSON_AddItemToObject(json,field,cJSON_CreateString(value));
    else cJSON_ReplaceItemInObject(json,field,cJSON_CreateString(value));
}

int32_t in_jsonarray(cJSON *array,char *value)
{
    int32_t i,n;
    struct destbuf remote;
    if ( array != 0 && cJSON_IsArray(array) != 0 )
    {
        n = cJSON_GetArraySize(array);
        for (i=0; i<n; i++)
        {
            if ( array == 0 || n == 0 )
                break;
            copy_cJSON(&remote,cJSON_GetArrayItem(array,i));
            if ( strcmp(remote.buf,value) == 0 )
                return(1);
        }
    }
    return(0);
}

int32_t myatoi(char *str,int32_t range)
{
    long x; char *ptr;
    x = strtol(str,&ptr,10);
    if ( range != 0 && x >= range )
        x = (range - 1);
    return((int32_t)x);
}

int32_t get_API_int(cJSON *obj,int32_t val)
{
    struct destbuf buf;
    if ( obj != 0 )
    {
        if ( cJSON_IsNumber(obj) != 0 )
            return((int32_t)obj->valuedouble);
        copy_cJSON(&buf,obj);
        val = myatoi(buf.buf,0);
        if ( val < 0 )
            val = 0;
    }
    return(val);
}

int32_t jint(cJSON *json,char *field) { if ( json == 0 ) return(0); if ( field == 0 ) return(get_API_int(json,0)); return(get_API_int(cJSON_GetObjectItem(json,field),0)); }
int32_t jinti(cJSON *json,int32_t i) { if ( json == 0 ) return(0); return(get_API_int(cJSON_GetArrayItem(json,i),0)); }

uint32_t get_API_uint(cJSON *obj,uint32_t val)
{
    struct destbuf buf;
    if ( obj != 0 )
    {
        if ( cJSON_IsNumber(obj) != 0 )
            return((uint32_t)obj->valuedouble);
        copy_cJSON(&buf,obj);
        val = myatoi(buf.buf,0);
    }
    return(val);
}
uint32_t juint(cJSON *json,char *field) { if ( json == 0 ) return(0); if ( field == 0 ) return(get_API_uint(json,0)); return(get_API_uint(cJSON_GetObjectItem(json,field),0)); }
uint32_t juinti(cJSON *json,int32_t i) { if ( json == 0 ) return(0); return(get_API_uint(cJSON_GetArrayItem(json,i),0)); }

double get_API_float(cJSON *obj)
{
    double val = 0.;
    struct destbuf buf;
    if ( obj != 0 )
    {
        if ( cJSON_IsNumber(obj) != 0 )
            return(obj->valuedouble);
        copy_cJSON(&buf,obj);
        val = atof(buf.buf);
    }
    return(val);
}

double jdouble(cJSON *json,char *field)
{
    if ( json != 0 )
    {
        if ( field == 0 )
            return(get_API_float(json));
        else return(get_API_float(cJSON_GetObjectItem(json,field)));
    } else return(0.);
}

double jdoublei(cJSON *json,int32_t i)
{
    if ( json != 0 )
        return(get_API_float(cJSON_GetArrayItem(json,i)));
    else return(0.);
}

cJSON *jobj(cJSON *json,char *field) { if ( json != 0 ) return(cJSON_GetObjectItem(json,field)); return(0); }

void jdelete(cJSON *json,char *field)
{
    if ( jobj(json,field) != 0 )
        cJSON_DeleteItemFromObject(json,field);
}

cJSON *jduplicate(cJSON *json) { return(cJSON_Duplicate(json,1)); }

cJSON *jitem(cJSON *array,int32_t i) { if ( array != 0 && cJSON_IsArray(array) != 0 && cJSON_GetArraySize(array) > i ) return(cJSON_GetArrayItem(array,i)); return(0); }
cJSON *jarray(int32_t *nump,cJSON *json,char *field)
{
    cJSON *array;
    if ( json != 0 )
    {
        if ( field == 0 )
            array = json;
        else array = cJSON_GetObjectItem(json,field);
        if ( array != 0 && cJSON_IsArray(array) != 0 && (*nump= cJSON_GetArraySize(array)) > 0 )
            return(array);
    }
    *nump = 0;
    return(0);
}

int32_t expand_nxt64bits(char *NXTaddr,uint64_t nxt64bits)
{
    int32_t i,n;
    uint64_t modval;
    char rev[64];
    for (i=0; nxt64bits!=0; i++)
    {
        modval = nxt64bits % 10;
        rev[i] = (char)(modval + '0');
        nxt64bits /= 10;
    }
    n = i;
    for (i=0; i<n; i++)
        NXTaddr[i] = rev[n-1-i];
    NXTaddr[i] = 0;
    return(n);
}

char *nxt64str(uint64_t nxt64bits)
{
    static char NXTaddr[64];
    expand_nxt64bits(NXTaddr,nxt64bits);
    return(NXTaddr);
}

char *nxt64str2(uint64_t nxt64bits)
{
    static char NXTaddr[64];
    expand_nxt64bits(NXTaddr,nxt64bits);
    return(NXTaddr);
}

int32_t cmp_nxt64bits(const char *str,uint64_t nxt64bits)
{
    char expanded[64];
    if ( str == 0 )//|| str[0] == 0 || nxt64bits == 0 )
        return(-1);
    if ( nxt64bits == 0 && str[0] == 0 )
        return(0);
    expand_nxt64bits(expanded,nxt64bits);
    return(strcmp(str,expanded));
}

uint64_t calc_nxt64bits(const char *NXTaddr)
{
    int32_t c;
    int64_t n,i,polarity = 1;
    uint64_t lastval,mult,nxt64bits = 0;
    if ( NXTaddr == 0 )
    {
        printf("calling calc_nxt64bits with null ptr!\n");
        return(0);
    }
    n = strlen(NXTaddr);
    if ( n >= 22 )
    {
        printf("calc_nxt64bits: illegal NXTaddr.(%s) too long\n",NXTaddr);
        return(0);
    }
    else if ( strcmp(NXTaddr,"0") == 0 || strcmp(NXTaddr,"false") == 0 )
    {
        // printf("zero address?\n"); getchar();
        return(0);
    }
    if ( NXTaddr[0] == '-' )
        polarity = -1, NXTaddr++, n--;
    mult = 1;
    lastval = 0;
    for (i=n-1; i>=0; i--,mult*=10)
    {
        c = NXTaddr[i];
        if ( c < '0' || c > '9' )
        {
            printf("calc_nxt64bits: illegal char.(%c %d) in (%s).%d\n",c,c,NXTaddr,(int32_t)i);
#ifdef __APPLE__
            //while ( 1 )
            {
                //sleep(60);
                printf("calc_nxt64bits: illegal char.(%c %d) in (%s).%d\n",c,c,NXTaddr,(int32_t)i);
            }
#endif
            return(0);
        }
        nxt64bits += mult * (c - '0');
        if ( nxt64bits < lastval )
            printf("calc_nxt64bits: warning: 64bit overflow %llx < %llx\n",(long long)nxt64bits,(long long)lastval);
        lastval = nxt64bits;
    }
    while ( *NXTaddr == '0' && *NXTaddr != 0 )
        NXTaddr++;
    if ( cmp_nxt64bits(NXTaddr,nxt64bits) != 0 )
        printf("error calculating nxt64bits: %s -> %llx -> %s\n",NXTaddr,(long long)nxt64bits,nxt64str(nxt64bits));
    if ( polarity < 0 )
        return(-(int64_t)nxt64bits);
    return(nxt64bits);
}

cJSON *addrs_jsonarray(uint64_t *addrs,int32_t num)
{
    int32_t j; cJSON *array;
    array = cJSON_CreateArray();
    for (j=0; j<num; j++)
        jaddi64bits(array,addrs[j]);
    return(array);
}

void free_json(cJSON *json) { if ( json != 0 ) cJSON_Delete(json); }
