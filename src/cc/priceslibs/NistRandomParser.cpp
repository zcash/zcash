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

// PricesResultParserSample.cpp sample lib for prices cc that parses the returned json

#include <stdint.h>
#include <string>
#include <iostream>
#include "cJSON.h"
#include "priceslibs.h"
#include "cjsonpointer.h"

extern "C" int pricesJsonParser(const char *sjson /*in*/, const char *symbol /*in*/, const char *customdata, uint32_t multiplier /*in*/, uint32_t *value /*out*/)
{
    std::string errorstr;
    cJSON *json = cJSON_Parse(sjson);
    if (json == NULL) {
        std::cerr << __func__ << "\t" << "error: can't parse json" << std::endl;
        return 0;
    }

    if (symbol == NULL) {
        std::cerr << __func__ << "\t" << "error: null symbol" << std::endl;
        return 0;
    }

    if (customdata == NULL) {
        std::cerr << __func__ << "\t" << "error: null custom data" << std::endl;
        return 0;
    }

    if (strcmp(symbol, "pulseIndex") == 0)
    {
        const cJSON *jfound = SimpleJsonPointer(json, customdata, errorstr);
        if (jfound == NULL) {
            std::cerr << __func__ << "\t" << "can't found pulseIndex json pointer:" << customdata << " :" << errorstr << std::endl;
            return 0;
        }
        if (cJSON_IsNumber(jfound)) {
            *value = (uint32_t)(jfound->valuedouble);
            return 1;
        }
        else {
            std::cerr << __func__ << "\t" << "pulseIndex value is not a number" << std::endl;
            return 0;
        }
    }
    // check pulseData0...pulseData15 format
    else if (strlen(symbol) >= 10 && strlen(symbol) <= 11 && strncmp(symbol, "pulseData", 9) == 0 && atoi(&symbol[9]) >= 0 && atoi(&symbol[9]) <= 15)
    {
        const cJSON *jfound = SimpleJsonPointer(json, customdata, errorstr);
        if (jfound == NULL) {
            std::cerr << __func__ << "\t" << "can't found pulseData json pointer:" << customdata << " :" << errorstr << std::endl;
            return 0;
        }
        if (cJSON_IsString(jfound) && strlen(jfound->string) == 256/8*2) // 256-bit number in hex
        {
            std::string str256 = std::string(jfound->string);
            *value = (uint32_t) std::stoi(str256.substr(atoi(&symbol[9]), 4), NULL, 16);  // parse 4-byte part
            return 1;
        }
        else {
            std::cerr << __func__ << "\t" << "pulseData value is not a number" << std::endl;
            return 0;
        }
    }
    else
    {
        std::cerr << __func__ << "\t" << "error: unsupported symbol, should be 'pulseIndex' or 'pulseData0'..'pulseData15'" << std::endl;
        return 0;
    }
}

