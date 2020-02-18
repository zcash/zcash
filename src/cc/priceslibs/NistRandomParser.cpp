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

// NistRandomParser.cpp lib for prices DTO module that parses NIST service (https://beacon.nist.gov/) random values into 8 x 32-bit numbers

#include <stdint.h>
#include <string.h>
#include <string>
#include <iostream>
#include "cJSON.h"
#include "priceslibs.h"
#include "cjsonpointer.h"

extern "C" int pricesJsonParser(const char *sjson /*in*/, const char *symbol /*in*/, const char *customdata, uint32_t multiplier /*in*/, uint32_t *value /*out*/)
{
    std::string errorstr;
    if (symbol == NULL) {
        std::cerr << __func__ << "\t" << "error: null symbol" << std::endl;
        return 0;
    }

    if (customdata == NULL) {
        std::cerr << __func__ << "\t" << "error: null custom data" << std::endl;
        return 0;
    }

    cJSON *json = cJSON_Parse(sjson);
    if (json == NULL) {
        std::cerr << __func__ << "\t" << "error: can't parse json" << std::endl;
        return 0;
    }

    bool r = false;
    if (strcmp(symbol, "pulseIndex") == 0)
    {
        const cJSON *jfound = SimpleJsonPointer(json, customdata, errorstr);
        if (jfound && cJSON_IsNumber(jfound))
        {
            *value = (uint32_t)(jfound->valuedouble);
            r = true;
        }
        else 
            std::cerr << __func__ << "\t" << "error: can't found pulseIndex json pointer as number:" << customdata << " :" << errorstr << std::endl;
        
    }
    // check pulseData0...pulseData7 format
    else if (strlen(symbol) == 10 && strncmp(symbol, "pulseData", 9) == 0 && atoi(&symbol[9]) >= 0 && atoi(&symbol[9]) <= 7)
    {
        const cJSON *jfound = SimpleJsonPointer(json, customdata, errorstr);
        if (jfound && cJSON_IsString(jfound) && strlen(jfound->valuestring) == 256 / 8 * 2) // 256-bit number in hex
        {
            std::string str256 = std::string(jfound->valuestring);
            *value = (uint32_t) std::stoul(str256.substr(atoi(&symbol[9])*16, 16), NULL, 16);  // parse 4-byte part
            r = true;
        }
        else 
            std::cerr << __func__ << "\t" << "error: pulseData value is not a valid 256-bit value as hex string" << std::endl;
    }
    else
        std::cerr << __func__ << "\t" << "error: unsupported symbol, should be 'pulseIndex' or 'pulseData0'..'pulseData7'" << std::endl;

    cJSON_free(json);
    return r ? 1 : 0;
}

