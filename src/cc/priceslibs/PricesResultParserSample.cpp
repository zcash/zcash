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

#include <iostream>
#include "cjson.h"
#include "priceslibs.h"
#include "cjsonpointer.h"

extern "C" int CustomJsonParserSample(const char *sjson /*in*/, const char *symbol /*in*/, const char *customdata, uint32_t multiplier /*in*/, uint32_t *value /*out*/)
{

    cJSON *json = cJSON_Parse(sjson);
    if (json == NULL) {
        std::cerr << __func__ << "\t" << "can't parse json" << std::endl;
        return 0;
    }

    // ignore symbol and use custom data as json pointer:
    const cJSON *jfound = SimpleJsonPointer(json, customdata);
    if (jfound == NULL) {
        std::cerr << __func__ << "\t" << "can't found json pointer" << std::endl;
        return 0;
    }
    if (cJSON_IsNumber(jfound)) {
        *value = (uint32_t)(jfound->valuedouble * (double)multiplier);
        return 1;
    }
    else {
        std::cerr << __func__ << "\t" << "value is not a number" << std::endl;
        return 0;
    }
}

