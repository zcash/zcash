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

// cjsonpointer.c  
// C-language RFC 6901 JSON pointer implementation for cJSON parser

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "cjsonpointer.h"

#define JP_MAXDEPTH 64
#define TRUE 1
#define FALSE 0


#define ERR_JSONPOINTER(msg) strncpy(errorstr, msg, CJP_ERRMSGLENGTH-1), errorstr[CJP_ERRMSGLENGTH-1]='\0', (const cJSON*)NULL;

// unescape json pointer as RFC 6901 requires
static void junescape(char *s)
{
    char *pos;
    pos = strstr(s, "~1");
    while (pos) {
        *pos = '/';
        strcpy(pos+1, pos + 2);
        pos = strstr(pos+2, "~1");
    }

    // must be in second order to prevent accidental escaping effect like "~01" --> "~1":
    pos = strstr(s, "~0");
    while (pos) {
        *pos = '~';
        strcpy(pos+1, pos + 2);
        pos = strstr(pos + 2, "~0");
    }
}

// check if string is a int number  
static int isStringInt(const char *s)
{
    int count = 0;
    size_t len = strlen(s);
    while (*s && (*s == '+' || *s == '-' || isdigit(*s))) s++,count++;
    return (count == len);
}

// browse json recursively
const cJSON* browseOnLevel(const cJSON *json, char *tokens[], int curtoken, int numtokens, char errorstr[])
{
    if (cJSON_IsNull(json))
        return ERR_JSONPOINTER("json is null");

    if (curtoken == numtokens)
        return json;

    // char *p = cJSON_Print(json);
    // fprintf(stderr, "json=%s\n", (p ? p : "NULL") );
    // if (p) cJSON_free(p);

    if (cJSON_IsArray(json))
    {
        if (!isStringInt(tokens[curtoken]))
            return ERR_JSONPOINTER("should be numeric array index");

        int index = atoi(tokens[curtoken++]);

        if (index >= 0 && index < cJSON_GetArraySize(json)) {
            const cJSON *item = cJSON_GetArrayItem(json, index);
            if (curtoken == numtokens)
                return item;
            else
                return browseOnLevel(item, tokens, curtoken, numtokens, errorstr);
        }
        else
            return ERR_JSONPOINTER("array index out of range");
    }
    else if (cJSON_IsObject(json))  // object 
    {
        const cJSON *item = cJSON_GetObjectItem(json, tokens[curtoken]);
        if (item) {
            curtoken++;
            if (curtoken == numtokens)
                return item;
            else
                return browseOnLevel(item, tokens, curtoken, numtokens, errorstr);
        }
        else
            return ERR_JSONPOINTER("json pointer not found (no such item in object)");
    }
    else {  // property
        return ERR_JSONPOINTER("json pointer not found (json branch end reached)");
    }
    return ERR_JSONPOINTER("unexpected code reached");
}


// simple json pointer parser as RFC 6901 defines it
// returns json object or property specified by the pointer or NULL
// pointer format examples:
// /object1/object2/property
// /array/index/property   (index is zero-based)
// /array/index 
// supports escaping of "~" with "~0" and "/" with "~1"
const cJSON *SimpleJsonPointer(const cJSON *json, const char *pointer, char errorstr[])
{
    char * tokens[JP_MAXDEPTH];
    int numtokens = 0;

    // parse 'path':
    const char *b = pointer;
    if (*b != '/')
        return ERR_JSONPOINTER("json pointer should be prefixed by /");
    b++;
    const char *e = b;
    while (1) 
    {
        if (!*e || *e == '/') 
        {
            char *token = (char*)malloc(e-b+1);
            strncpy(token, b, e - b);
            token[e - b] = '\0';
            junescape(token);
            tokens[numtokens++] = token;
            
            if (numtokens == JP_MAXDEPTH)
                return ERR_JSONPOINTER("json pointer too deep");

            if (!*e)
                break;

            b = e + 1;
        }
        e++;
    }
                              
    // fprintf(stderr, "tokens:"); 
    // for (int i = 0; i < numtokens; i++) fprintf(stderr, "%s ", tokens[i]);
    // fprintf(stderr, "\n");

    const cJSON *foundjson = browseOnLevel(json, tokens, 0, numtokens, errorstr);
    for (int i = 0; i < numtokens; i++) 
		free(tokens[i]);

	return foundjson;
}

/*
// tests for SimpleJsonPointer:
int main()
{
    const char *examples[] = {
        "{}",
        "{ \"foo\": [\"bar\", \"baz\"], \"xx\": {\"yy\": 111 }  }",
        "{ \"foo\": [ [\"bar\", \"AAA\", \"BBB\"] ], \"boo\": [\"xx\", {\"yy\": 111 } ] }",
        "{[\"ppp\":{}]}",  // bad json
        "{"
        "\"foo\": [\"bar\", \"baz\" ],"
        "\"\" : 0,"
        "\"a/b\" : 1,"
        "\"c%d\" : 2,"
        "\"e^f\" : 3,"
        "\"g|h\" : 4,"
        "\"i\\\\j\" : 5,"
        "\"k\\\"l\" : 6,"
        "\" \" : 7,"
        "\"m~n\" : 8"
        "}"
    };

    typedef struct {
        const char *ptr;
        int result;
    }  testcase;

    testcase t0[] = { { "", FALSE },{ "/foo", FALSE }, {NULL, 0} };
    testcase t1[] = { { "", FALSE },{ "/foo", TRUE },{ "/foo/0", TRUE },{ "/foo/0/0", FALSE },{ "/foo/1", TRUE },{ "/foo/2", FALSE },{ "/foo/xx", FALSE },{ "/foo/1/xx", FALSE },{ "/xx/yy", TRUE },{ "/xx/yy/0", FALSE },{ NULL, 0 } };
    testcase t2[] = { { "", FALSE },{ "/foo", TRUE },{ "/foo/0", TRUE },{ "/foo/0/0", TRUE },{ "/foo/1", FALSE },{ "/boo/xx", FALSE },{ "/boo/0", TRUE },{ "/boo/1", TRUE },{ "/boo/1/yy", TRUE },{ "/boo/yy/0", FALSE },{ NULL, 0 } };
    testcase t3[] = { {NULL, FALSE} };
    testcase t4[] = {
        { "/", TRUE },    // RFC 6901 requires to return "0" for 'foo' json sample. We do!
        { "/a~1b", TRUE },
        { "/c%d", TRUE },
        { "/e^f", TRUE },
        { "/g|h", TRUE },
        { "/i\\j", TRUE },
        { "/k\"l", TRUE },
        { "/ ", TRUE },
        { "/m~n", TRUE },
        { NULL, 0 }
    };

    testcase *cases[] = { t0,t1,t2,t3,t4 };
    
    for (int i = 0; i < sizeof(examples) / sizeof(examples[0]); i++)
    {
        fprintf(stderr, "\nparse object i=%d: ", i);
        cJSON *json = cJSON_Parse(examples[i]);
        if (!json) {
            fprintf(stderr, "json is NULL\n");
            continue;
        }
        char *p = cJSON_Print(json);
        fprintf(stderr, "%s\n", p);
	if (p) cJSON_free(p);


        for (int j = 0; cases[i][j].ptr; j++) {
            const cJSON *res = SimpleJsonPointer(json, cases[i][j].ptr);
            char *p = (res ? cJSON_Print(res) : NULL);
            fprintf(stderr, "for ptr: \"%s\" json: %s, test: %s\n", cases[i][j].ptr, (p ? p : "NULL"), ((!!cases[i][j].result == !!(res != NULL)) ? "ok" : "failed"));
            if (p) cJSON_free(p);
        }
	cJSON_Delete(json);
    }
}
*/