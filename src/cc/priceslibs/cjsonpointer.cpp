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

// cjsonpointer.cpp
// CPP-language RFC 6901 JSON pointer implementation for cJSON parser

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <list>
#include <vector>
#include <functional>
#include <ctype.h>
#include "cjsonpointer.h"

template <typename T>
static cJSON *reportJsonPointerErr(T errToStream) {
    std::ostringstream stream;
    errToStream(stream);
    std::cerr << stream.str() << std::endl;
    return NULL;
}

// #define ERR_JSONPOINTER(streamexp) reportJsonPointerErr([=](std::ostringstream &stream){ streamexp; })
#define ERR_JSONPOINTER(msg) errorstr = msg, (const cJSON*)NULL;


// unescape json pointer as RFC 6901 requires
static void junescape(std::string &s)
{
    size_t mpos;
    mpos = s.find("~1");
    while (mpos != std::string::npos) {
        s.replace(mpos, 2, "/");
        mpos = s.find("~1");
    }

	// must be in second order to prevent accidental escaping effect like "~01" --> "~1":
    mpos = s.find("~0");
    while (mpos != std::string::npos) {
        s.replace(mpos, 2, "~");
        mpos = s.find("~0");
    }
}

// check if string is a int  
static bool is_string_int(const std::string &s)
{
    const char *p = s.c_str();
    int count = 0;
    while (*p && (*p =='+' || *p== '-' || isdigit(*p))) p++, count++;
    return (count > 0 && count == s.length());
}

// simple json pointer parser as RFC 6901 defines it
// returns json object or property specified by the pointer or NULL
// pointer format examples:
// /object1/object2/property
// /array/index/property   (index is zero-based)
// /array/index 
// supports escaping of "~" with "~0" and "/" with "~1"
const cJSON *SimpleJsonPointer(const cJSON *json, const char *pointer, std::string &errorstr)
{
    std::list<std::string> tokens;

    // parse 'path':
    const char *b = pointer;
	if (*b != '/')
		return ERR_JSONPOINTER("json pointer should be prefixed by /");				
	b++;
    const char *e = b;
    while (1) {
        //const char *e0 = e;
        if (!*e || *e == '/') {
            // if (b < e) { -- allow empty "" properties
            std::string token(b, e);
			junescape(token); 
            tokens.push_back(token);
            if (!*e)
                break;
            //}
			b = e + 1;
        }
        e++;
    }

    //std::cerr << "tokens:"; 
    //for(auto l:tokens) std::cerr << l << " ";
    //std::cerr << std::endl;

    // lambda to browse json recursively
    std::function<const cJSON*(const cJSON*)> browseOnLevel = [&](const cJSON *json)->const cJSON*
    {
		if (cJSON_IsNull(json))	
			return ERR_JSONPOINTER("json pointer: json is null");			

        if (tokens.empty())                                                                       
            return json;                                                                          

        //char *p=cJSON_Print(json);
		//std::cerr << "json on level:"<< (p?*p:"NULL") << std::endl;
        //if (p) cJSON_free(p); 
        if (cJSON_IsArray(json))
        {
			if (!is_string_int(tokens.front()))
				return ERR_JSONPOINTER("json pointer: should be numeric array index");				
			                                                                                
            int32_t index = atoi( tokens.front().c_str() );                                     
            tokens.pop_front();                                                                 
                                                                                            
            if (index >= 0 && index < cJSON_GetArraySize(json))     {                           
                const cJSON *item = cJSON_GetArrayItem(json, index);                                  
                if (tokens.empty())                                                             
                    return item;                                                                
				else	
               		return browseOnLevel(item);
            }                                                                                   
            else                                                                                
                return ERR_JSONPOINTER("json pointer: array index out of range");                   
        }
        else if (cJSON_IsObject(json))  // object 
        {
            const cJSON *item = cJSON_GetObjectItem(json, tokens.front().c_str());                          
            if (item) {                                                                               
                tokens.pop_front();
				if (tokens.empty())
					return item;
				else    
               		return browseOnLevel(item);                                                       
			}                                                                                             
			else
                return ERR_JSONPOINTER("json pointer not found (no such item in object)");
        }
		else {  // property
			return ERR_JSONPOINTER("json pointer not found (json branch end reached)");         									
		}
		return ERR_JSONPOINTER("json pointer: unexpected code reached");
    };

    return browseOnLevel(json);
}