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
#include <map>
#include <iostream>
#include "cJSON.h"
#include "priceslibs.h"
#include "cjsonpointer.h"


const uint32_t timestampIndex1 = 1532373960; // 2018-07-23T19:26:00 for pulseIndex=1 chain=1

// custom parse nist random value and divide it into prices 32-bit
extern "C" int PF_EXPORT_SYMBOL pricesJsonParser(const char *sjson /*in*/, const char *symbol /*in*/, const char *customdata, uint32_t multiplier /*in*/, uint32_t *value /*out*/)
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
    // check pulseData0...pulseData15 format
    else if (strlen(symbol) > 9 && strlen(symbol) <= 11 && strncmp(symbol, "pulseData", 9) == 0 && atoi(&symbol[9]) >= 0 && atoi(&symbol[9]) <= 15)
    {
        const cJSON *jfound = SimpleJsonPointer(json, customdata, errorstr);
        if (jfound && cJSON_IsString(jfound) && strlen(jfound->valuestring) == 512 / 8 * 2) // 512-bit number in hex
        {
            std::string str256 = std::string(jfound->valuestring);
            *value = (uint32_t) std::stoul(str256.substr(atoi(&symbol[9])*8, 8), NULL, 16);  // parse 4-byte part
            r = true;
        }
        else 
            std::cerr << __func__ << "\t" << "error: pulseData value is not a valid 512-bit value as hex string" << std::endl;
    }
    else
        std::cerr << __func__ << "\t" << "error: unsupported symbol, should be 'pulseIndex' or 'pulseData0'..'pulseData15'" << std::endl;

    cJSON_free(json);
    return r ? 1 : 0;
}

// validate nist random value
extern "C" int PF_EXPORT_SYMBOL pricesValidator(int32_t height, uint32_t blocktime, uint32_t timestampBlock, uint32_t timestampPrevBlock, uint32_t prices[], uint32_t prevprices[], int32_t beginpos, int32_t endpos)
{
    static std::map<uint32_t, uint32_t[16]> randomCache;
    
    if (prices == NULL)
    {
        std::cerr << __func__ << " prices array null" << std::endl;
        return -1;
    }
    if (endpos - beginpos != 1+16)
    {
        std::cerr << __func__ << " invalid NIST random values count" << std::endl;
        return -1;
    }

    uint32_t pulseIndex = prices[beginpos];
    uint32_t pulseIndexPrev = prevprices[beginpos];

    uint32_t parts[16];
    for (int i = 0; i < sizeof(parts)/sizeof(parts[0]); i++)
        parts[i] = prices[beginpos + 1 + i];

    if (randomCache.find(pulseIndex) == randomCache.end())
    {
        if (randomCache.size() >= 10) // max num of tracked pulses is 10
        {
            // remove lru element:
            uint32_t minIndex = (*randomCache.begin()).first;
            for (const auto &r : randomCache)
                if (r.first < minIndex)
                    minIndex = r.first;
            randomCache.erase(minIndex);
        }

        // store the current value
        memcpy(randomCache[prices[beginpos]], parts, sizeof(parts));
    }
    else
    {
        // check the current value is not changed
        if (memcmp(randomCache[pulseIndex], parts, sizeof(parts)) != 0)
        {
            std::cerr << __func__ << " invalid NIST random value for pulseIndex=" <<  pulseIndex << std::endl;
            return -1;
        }
    }

    // validate current value against blocktime:
    //uint32_t timestampRandomPrev = timestampIndex1 + (pulseIndexPrev - 1) * 60;
    uint32_t timestampRandom = timestampIndex1 + (pulseIndex - 1) * 60;
    uint32_t timestampRandomExpected = timestampPrevBlock + blocktime;


    if (timestampRandom < timestampRandomExpected - 60)
    {
        std::cerr << __func__ << " invalid NIST random value timestamp is too close for pulseIndex=" << pulseIndex << std::endl;
        return -1;
    }

    if (timestampRandom > timestampRandomExpected + 60)
    {
        std::cerr << __func__ << " invalid NIST random value timestamp is too far for pulseIndex=" << pulseIndex << std::endl;
        return -1;
    }

    return 0;
}

// empty clamper
extern "C" void PF_EXPORT_SYMBOL pricesClamper(int32_t height, uint32_t prices[], uint32_t prevprices[], int32_t beginpos, int32_t endpos, int64_t tolerance)
{
    return; // no clamping for NIST random data
}

// trivial converter
extern "C" void PF_EXPORT_SYMBOL pricesConverter(int32_t index, uint32_t storedvalue, int64_t *converted)
{
    if (converted)
        *converted = storedvalue;
}