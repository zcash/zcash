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

#ifndef __PRICES_FEED__
#define __PRICES_FEED__

#include <string>
#include <vector>
#include <stdint.h>
#include "priceslibs/priceslibs.h"

#include <cJSON.h>

// support for creating data feeds for prices cc

#define PF_BUFOVERFLOW 0xFFFFFFFF
#define PF_DEFAULTINTERVAL 120

struct CFeedConfigItem {

    std::string name;       // config name
    std::string customlib;  // custom shared lib
    std::string url;        // url, can contain '%s' where substitute strings ill be places
    std::vector<std::string> substitutes;   // array of strings that will be substituted in the 'url' to form the request
    std::string quote;

    struct ResultProcessor  
    {
    //  std::string symbolpath;                     // not supported by now
        std::string symbol;                            // symbol names 
        std::string valuepath;                         // json pointers to values
        std::vector<std::string> averagepaths;         // path to many result to calc average
        std::string customdata;                              // message to custom lib
    };

    ResultProcessor substituteResult;           // descriptor how to process the result returned by url with substitutes
    std::vector <ResultProcessor> manyResults;  // descriptor how to process results returned by url with no substitutes (could be many values for many symbols in the result json in this case)

    uint32_t interval;      // poll interval
    uint32_t multiplier;    // multiplier to convert price value from float to integer
};


struct CCustomProcessor {
    CustomJsonParser parser;
    CustomClamper clamper;          // custom prices clamper
    CustomValidator validator;      // custom prices validator
    CustomConverter converter;      // custom prices validator

    int32_t b, e;                   // price index range begin (inclusive) and end (exclusive)
};


bool PricesFeedParseConfig(const cJSON *json);
bool PricesInitStatuses();
uint32_t PricesFeedPoll(uint32_t *pricevalues, const uint32_t maxsize, uint32_t *timestamp);
char *PricesFeedSymbolName(char *name, int32_t ind);
int64_t PricesFeedMultiplier(int32_t ind);
int32_t PricesFeedSymbolsCount();
void PricesFeedSymbolsForMagic(std::string &names, bool compatible);
void PricesAddOldForexConfig(const std::vector<std::string> &ac_forex);
void PricesAddOldPricesConfig(const std::vector<std::string> &ac_prices);
void PricesAddOldStocksConfig(const std::vector<std::string> &ac_stocks);

void PricesFeedGetCustomProcessors(std::vector<CCustomProcessor> &priceProcessors);


#endif // #ifndef __PRICES_FEED__