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

#include <stdlib.h>
#include <functional>
#include <list>
#include <sstream>
#include <iostream>

#include <chrono>
#include <thread>
#include <time.h>

#include "CCPrices.h"
#include "CCinclude.h"
#include "pricesfeed.h"

/* not used:
template <typename T>
static void logJsonPath(T errToStream) {
    std::ostringstream stream;
    errToStream(stream);
    std::cerr << stream.str();
}

#define LOGSTREAMFN(name, level, streamexp) logJsonPath([=](std::ostringstream &stream){ streamexp; })
*/

cJSON *get_urljson(char *url);

static std::vector<CFeedConfigItem> feedconfig({ 
    {
        // default feed:
        "prices",   // name
        "http://api.coindesk.com/v1/bpi/currentprice.json",  // url
        {},     // substitutes
        "BTC",  // base
        { },    // result
        // results:
        { 
            { { "/bpi/USD/code", "/bpi/USD/rate_float" }, { "/bpi/EUR/code", "/bpi/EUR/rate_float" }, { "/bpi/GBP/code", "/bpi/GBP/rate_float" } },    // paths
            true     // symbolIsPath
        },
        60, // interval
        10000  // multiplier
    } 
});
static std::vector<std::string> priceNames;
static int priceNamesCount = 0;

typedef struct 
{
    time_t lasttime;
} PollStatus;


static const PollStatus nullPollStatus = { 0 };
static std::vector<PollStatus> pollStatuses({ nullPollStatus });  // init for default feed

// parse poll configuration in json from cmd line 
bool PricesFeedParseConfig(const cJSON *json)
{
    if (!cJSON_IsArray(json)) {
        LOGSTREAMFN("prices", CCLOG_INFO, stream << "config not a json array" << std::endl);
        return false;
    }

    for (int i = 0; i < cJSON_GetArraySize(json); i++)
    {
        cJSON *jitem = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(jitem)) {
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "config element not a object" << std::endl);
            return false;
        }
        CFeedConfigItem citem;
        if (cJSON_HasObjectItem(jitem, "name"))
            citem.name = cJSON_GetObjectItem(jitem, "name")->valuestring;
        if (citem.name.empty()) {
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no 'name'" << std::endl);
            return false;
        }
        if (cJSON_HasObjectItem(jitem, "url"))
            citem.url = cJSON_GetObjectItem(jitem, "url")->valuestring;
        if (citem.url.empty()) {
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "prices feed config item has no 'url'" << std::endl);
            return false;
        }
        if (cJSON_HasObjectItem(jitem, "substitutes")) {
            cJSON *jsubst = cJSON_GetObjectItem(jitem, "substitutes");
            if (!cJSON_IsArray(jsubst)) {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'substitutes' is not an array" << std::endl);
                return false;
            }
            for (int j = 0; j < cJSON_GetArraySize(jsubst); j++)
            {
                cJSON *jsubstitem = cJSON_GetArrayItem(jsubst, j);
                std::string subst = jsubstitem->valuestring;
                if (subst.empty()) {
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'substitutes' is empty" << std::endl);
                    return false;
                }
                citem.substitutes.push_back(subst);
            }
        }

        // check we have substitute macro in url:
        if (citem.substitutes.size() > 0 && citem.url.find("%s") == std::string::npos) {
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'url' has no '%s' macro required for 'substitutes'" << std::endl);
            return false;
        }

        if (cJSON_HasObjectItem(jitem, "results"))
        {
            const cJSON *jres = cJSON_GetObjectItem(jitem, "results");
            if (cJSON_IsObject(jres))
            {
                const cJSON *jsymbolpath = cJSON_GetObjectItem(jres, "symbolpath");
                const cJSON *jvaluepath = cJSON_GetObjectItem(jres, "valuepath");
                const cJSON *javeragepaths = cJSON_GetObjectItem(jres, "averagevaluepaths");
                if (jsymbolpath)
                    citem.result.symbolpath = jsymbolpath->valuestring;
                if (jvaluepath)
                    citem.result.valuepath = jvaluepath->valuestring;
                if (javeragepaths) {
                    if (cJSON_IsArray(javeragepaths)) {
                        for(int i = 0; i < cJSON_GetArraySize(javeragepaths); i ++)     {
                            const cJSON *jitem = cJSON_GetArrayItem(javeragepaths, i);
                            if (cJSON_IsString(jitem)) {
                                std::string sitem = jitem->valuestring;
                                citem.result.averagepaths.push_back(sitem);
                            }
                        }
                    }
                }

                if (citem.result.valuepath.empty() && citem.result.averagepaths.empty()) {  // empty symbolpath means to use 'substitute' as symbol
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' description: no valuepath property or averagevaluepaths array" << std::endl);
                    return false;
                }
                if (!citem.result.valuepath.empty() && !citem.result.averagepaths.empty()) {  
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' description: can't specify both valuepath property and averagevaluepaths array" << std::endl);
                    return false;
                }
            }
            else if (cJSON_IsArray(jres))
            {
                int nsymbolname = 0, nsymbolpath = 0;
                for (int j = 0; j < cJSON_GetArraySize(jres); j++)
                {
                    cJSON *jresitem = cJSON_GetArrayItem(jres, j);
                    if (cJSON_IsObject(jresitem))
                    {
                        std::string symbolpath, valuepath;

                        if (cJSON_HasObjectItem(jitem, "symbolname")) {
                            citem.results.symbolIsPath = false;
                            symbolpath = cJSON_GetObjectItem(jresitem, "symbolname")->valuestring;
                            nsymbolname++;
                        }
                        else {
                            citem.results.symbolIsPath = true;
                            symbolpath = cJSON_GetObjectItem(jresitem, "symbolpath")->valuestring;
                            nsymbolpath++;
                        }
                        if (cJSON_HasObjectItem(jitem, "valuepath"))
                            valuepath = cJSON_GetObjectItem(jresitem, "valuepath")->valuestring;

                        if (symbolpath.empty() || valuepath.empty()) {
                            LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' description: no either 'symbolname' or 'symbolpath' or no 'valuepath' items" << std::endl);
                            return false;
                        }
                        citem.results.paths.push_back(std::make_pair(symbolpath, valuepath));
                    }
                    else
                    {
                        LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'results' element has incorrect type" << std::endl);
                        return false;
                    }
                }
                // check either symbolname or symbolpath is used:
                if (nsymbolname > 0 && nsymbolpath > 0) {
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'results' can't have both symbolname and symbolpath" << std::endl);
                    return false;
                }
            }
            else {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' description: neither object nor array" << std::endl);
                return false;
            }
        }
        else {
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no 'results' element" << std::endl);
            return false;
        }

        if (citem.substitutes.size() > 0)   {
            if (citem.result.valuepath.empty() && citem.result.averagepaths.empty()) {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' description: not an object with 'valuepath' or 'averagepath' element" << std::endl);
                return false;
            }
        }
        else {
            if (citem.results.paths.empty()) {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' description: not an array with symbol and valuepath, or empty" << std::endl);
                return false;
            }
        }

        if (cJSON_HasObjectItem(jitem, "base")) 
        {
            citem.base = cJSON_GetObjectItem(jitem, "base")->valuestring;
            if (citem.base.empty()) {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'base' is incorrect" << std::endl);
                return false;
            }
        }

        citem.multiplier = 10000; // default value
        cJSON *jmultiplier = cJSON_GetObjectItem(jitem, "multiplier");
        if (jmultiplier) {
            std::string smultiplier = jmultiplier->valuestring;
            if (!smultiplier.empty()) {
                citem.multiplier = atoi(smultiplier.c_str());
                if (citem.multiplier < 1) {
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'multiplier' value is incorrect, should be >= 1" << std::endl);
                    return false;
                }
            }
        }
        LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'multiplier' value=" << citem.multiplier << std::endl);

        citem.interval = 60; //default value
        cJSON *jinterval = cJSON_GetObjectItem(jitem, "interval");
        if (jinterval) {
            std::string sinterval = jinterval->valuestring;
            if (!sinterval.empty()) {
                citem.interval = atoi(sinterval.c_str());
                if (citem.interval < 60) {
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'interval' value is incorrect, should be >= 60" << std::endl);
                    return false;
                }
            }
        }
        LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'interval' value=" << citem.interval << std::endl);

        feedconfig.push_back(citem);  // add new feed config item
    }

    // init statuses for the configured feeds:
    int count = feedconfig.size();
    while (count--)
        pollStatuses.push_back(nullPollStatus);

    return true;
}

template <typename T>
static cJSON *reportJsonPointerErr(T errToStream) {
    std::ostringstream stream;
    errToStream(stream);
    std::cerr << stream.str() << std::endl;
    return NULL;
}

#define ERR_JSONPOINTER(streamexp) reportJsonPointerErr([=](std::ostringstream &stream){ streamexp; })

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

// check if string is a int number  
static bool is_string_number(const std::string &s)
{
    const char *p = s.c_str();
    int count = 0;
    while (*p && isdigit(*p++)) count++;
    return (count == s.length());
}

// simple json pointer parser as RFC 6901 defines it
// returns json object or property specified by the pointer or NULL
// pointer format examples:
// /object1/object2/property
// /array/index/property   (index is zero-based)
// /array/index 
// supports escaping of "~" with "~0" and "/" with "~1"
const cJSON *SimpleJsonPointer(const cJSON *json, const char *pointer)
{
    std::list<std::string> tokens;

    // parse 'path':
    const char *b = pointer;
	if (*b != '/')
		return ERR_JSONPOINTER(stream << "json pointer should be prefixed by /");				
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
			return ERR_JSONPOINTER(stream << "json pointer: json is null");			

        if (tokens.empty())                                                                       
            return json;                                                                          

        //char *p=cJSON_Print(json);
		//std::cerr << "json on level:"<< (p?*p:"NULL") << std::endl;
        //if (p) cJSON_free(p); 
        if (cJSON_IsArray(json))
        {
			if (!is_string_number(tokens.front()))
				return ERR_JSONPOINTER(stream << "json pointer: should be numeric array index");				
			                                                                                
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
                return ERR_JSONPOINTER(stream << "json pointer: array index out of range");                   
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
                return ERR_JSONPOINTER(stream << "json pointer not found (no such item in object)");
        }
		else {  // property
			return ERR_JSONPOINTER(stream << "json pointer not found (json branch end reached)");         									
		}
		return ERR_JSONPOINTER(stream << "json pointer: unexpected code reached");
    };

    return browseOnLevel(json);
}

// return number of a configured feed's symbols to get
uint32_t PricesFeedGetItemSize(const CFeedConfigItem &citem)
{
    if (!citem.substitutes.empty())
        return citem.substitutes.size();
    else
        return citem.results.paths.size();
}

// return total number of all configured price symbols to get
uint32_t PricesFeedTotalSize(void)
{
    uint32_t totalsize = 0;
    for (const auto & fc : feedconfig)
        totalsize += PricesFeedGetItemSize(fc);
    return totalsize;
}

// return price name for index
char *PricesFeedName(char *name, int32_t ind) {
    if (ind > 0 && ind < priceNames.size()) {
        if (strlen(priceNames[ind].c_str()) < PRICES_MAXNAMELENGTH-1)
            strcpy(name, priceNames[ind].c_str());
        else
        {
            strncpy(name, priceNames[ind].c_str(), PRICES_MAXNAMELENGTH-1);
            name[PRICES_MAXNAMELENGTH-1] = '\0';
        }
        return name;
    }
    return NULL;
}

// get config item multiplier
// ind begins from 1
int64_t PricesFeedMultiplier(int32_t ind)
{
    if (ind == 0)
        return 10000;

    int32_t offset = 1;
    for (const auto & citem : feedconfig) {
        uint32_t size1 = PricesFeedGetItemSize(citem);
        if (ind - offset < size1)
            return citem.multiplier;
        offset += size1;
    }
    return 0;
}

int32_t PricesFeedNamesCount()
{
    return priceNamesCount;
}


// extract price value (and symbol name if required)
static bool parse_result_json_value(const cJSON *json, const std::string &symbolpath, const std::string &valuepath, uint32_t multiplier, std::string &symbol, uint32_t *pricevalue)
{
    
    const cJSON *jvalue = SimpleJsonPointer(json, valuepath.c_str());
    if (jvalue)
    {
        if (cJSON_IsNumber(jvalue)) {
            *pricevalue = jvalue->valuedouble * multiplier;
        }
        else
        {
            *pricevalue = 0;
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "feed json value not a number" << std::endl);
            return false;
        }
    }
    else
    {
        *pricevalue = 0;
        LOGSTREAMFN("prices", CCLOG_INFO, stream << "feed json value not found" << std::endl);
        return false;
    }

    if (!symbolpath.empty())
    {
        const cJSON *jsymbol = SimpleJsonPointer(json, symbolpath.c_str());
        if (jsymbol)
        {
            if (cJSON_IsString(jsymbol)) {
                symbol = jsymbol->valuestring;
            }
            else
            {
                symbol = "";
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "feed json symbol not a string" << std::endl);
                return false;
            }
        }
        else
        {
            symbol = "";
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "feed json symbol not found" << std::endl);
            return false;
        }
    }      
    return true;
}


// calc average value by enumerating value arrays by json paths with "*" as array indexes 
static bool parse_result_json_average(const cJSON *json, const std::vector<std::string> &valuepaths, uint32_t multiplier, uint32_t *pricevalue)
{
    double total = 0.0;
    int32_t count = 0;

    for (const auto &origpath : valuepaths)
    {
        std::function<void(const cJSON*, const std::string &)> enumOnLevel = [&](const cJSON *json, const std::string &path)->void
        {
            if (!json) return;

            size_t starpos = path.find('*');
            if (starpos != std::string::npos)
            {
                // found "*" wildcard symbol
                std::string toppath = path.substr(0, starpos);
                std::string restpath = path.substr(starpos+1);
                
                int ind = 0;
                while (1) {
                    std::string toppathind = toppath + std::to_string(ind++);
                    const cJSON *jfound = SimpleJsonPointer(json, toppathind.c_str());
                    if (!jfound)
                        break;
                    if (restpath.empty()) 
                    {
                        if (cJSON_IsNumber(jfound)) {
                            total += jfound->valuedouble;
                            count++;
                        }
                        else
                            LOGSTREAMFN("prices", CCLOG_DEBUG1, stream << "array leaf value not a number" << std::endl);
                    }
                    else 
                        enumOnLevel(jfound, restpath);  // object or array
                }
            }
            else
            {
                // should be leaf value
                const cJSON *jfound = cJSON_GetObjectItem(json, path.c_str());
                if (jfound) {
                    if (cJSON_IsNumber(jfound)) {
                        total += jfound->valuedouble;
                        count++;
                    }
                    else
                        LOGSTREAMFN("prices", CCLOG_DEBUG1, stream << "object leaf value not a number" << std::endl);
                }
                else {
                    LOGSTREAMFN("prices", CCLOG_DEBUG1, stream << "leaf not found" << std::endl);

                }

            }
        };

        enumOnLevel(json, origpath);
    }

    if (count > 0)   {
        *pricevalue = total / count;
        return true;
    }
    else   {
        *pricevalue = 0;
        return false;
    }
}

// pool single url using substitutes or without them
static uint32_t poll_one_feed(const CFeedConfigItem &citem, uint32_t *pricevalues, std::vector<std::string> &symbols)
{
    uint32_t numadded = 0;

    LOGSTREAMFN("prices", CCLOG_INFO, stream << "polling..." << std::endl);
    if (citem.substitutes.size() > 0)
    {
        for (const auto subst : citem.substitutes)
        {
            size_t mpos = citem.url.find("%s");
            if (mpos != std::string::npos)
            {
                std::string url = citem.url;
                url.replace(mpos, 2, subst);
                cJSON *json = get_urljson((char*)url.c_str()); //poll
                if (json != NULL)
                {
                    std::string symbol, jsymbol;
                    bool parsed;
                    if (!citem.result.averagepaths.empty())
                        parsed = parse_result_json_average(json, citem.result.averagepaths, citem.multiplier, &pricevalues[numadded++]);
                    else
                        parsed = parse_result_json_value(json, citem.result.symbolpath, citem.result.valuepath, citem.multiplier, jsymbol, &pricevalues[numadded++]);
                    if (parsed) {
                        if (citem.result.symbolpath.empty())
                            symbol = subst;
                        else
                            symbol = jsymbol;
                        if (!citem.base.empty())
                            symbol += "_" + citem.base;
                        symbols.push_back(symbol);
                        LOGSTREAM("prices", CCLOG_INFO, stream << symbol << " " << pricevalues[numadded - 1] << " ");
                    }
                    cJSON_Delete(json);
                    if (!parsed)
                        return 0;
                }
                else 
                {
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "feed service not available: " << url << std::endl);
                    return 0;
                }
                // pause to prevent ban by web resource
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    else
    {
        cJSON *json = get_urljson((char*)citem.url.c_str()); // poll
        if (json != NULL)
        {
            const std::string empty;
            std::string symbol, jsymbol;
            bool parsed = false;

            for (const auto &r : citem.results.paths) {
                bool parsed = parse_result_json_value(json, (citem.results.symbolIsPath ? r.first : empty), r.second, citem.multiplier, jsymbol, &pricevalues[numadded++]);
                if (parsed) {
                    if (citem.results.symbolIsPath)
                        symbol = jsymbol; // from json
                    else
                        symbol = r.first; // from config
                    if (!citem.base.empty())
                        symbol += "_" + citem.base;
                    symbols.push_back(symbol);
                }
                else
                    break;

                LOGSTREAM("prices", CCLOG_INFO, stream << symbol << " " << pricevalues[numadded-1] << " ");
            }
            cJSON_Delete(json);
            if (!parsed)
                return 0;
        }
        else
        {
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "feed service not available: " << citem.url << std::endl);
            return 0;
        }
        // pause to prevent ban by the web resource
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOGSTREAM("prices", CCLOG_INFO, stream << std::endl);
    return numadded;
}

uint32_t PricesFeedPoll(uint32_t *pricevalues, uint32_t maxsize, time_t *now)
{
    uint32_t offset = 0;
    uint32_t totalsize = 0;
    uint32_t nsymbols = 0;
    *now = time(NULL);
    bool updated = false;

    for(const auto &fc : feedconfig)
        nsymbols += PricesFeedGetItemSize(fc);
    priceNames.resize(nsymbols+1);

    memset(pricevalues, '\0', maxsize); // reset to 0 as some feeds maybe updated, some not in this poll
    pricevalues[offset++] = (uint32_t)0;
    totalsize++; // 1 off  !!

    for (int32_t i = 0; i < feedconfig.size(); i ++)
    {
        uint32_t size1 = PricesFeedGetItemSize(feedconfig[i]);    

        if (!pollStatuses[i].lasttime || *now > pollStatuses[i].lasttime + feedconfig[i].interval)  // first time poll
        {
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "entering poll, !pollStatuses[i].lasttime=" << !pollStatuses[i].lasttime << std::endl);
            std::vector<std::string> symbols;
            // poll url and get values and symbols
            if (poll_one_feed(feedconfig[i], &pricevalues[offset], symbols))
            {
                updated = true;
                if (!pollStatuses[i].lasttime) {
                    // add symbols, first item is timestamp:
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "adding symbols to pricename" << std::endl);
                    for (int32_t j = 0; j < symbols.size(); j++) {
                        priceNames[totalsize + j] = symbols[j];
                        LOGSTREAMFN("prices", CCLOG_INFO, stream << "added to pricename index=" << totalsize + j << " symbol=" << symbols[j] << std::endl);
                        priceNamesCount++;
                    }
                }
                pollStatuses[i].lasttime = *now;
            }
        }

        if (maxsize < size1) 
            return PF_BUFOVERFLOW;  // buffer overflow

        maxsize -= size1;
        totalsize += size1;
        offset += size1;
    }
    if (updated) {
        pricevalues[0] = (uint32_t)*now;
        return totalsize;
    }
    else
        return 0;
}

/* tests for SimpleJsonPointer:
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
        bool result;
    }  testcase;
    
    std::vector<testcase> t0 = { { "", false },{ "/foo", false } };
    std::vector<testcase> t1 = { { "", false },{ "/foo", true },{ "/foo/0", true },{ "/foo/0/0", false },{ "/foo/1", true },{ "/foo/2", false },{ "/foo/xx", false },{ "/foo/1/xx", false },{ "/xx/yy", true },{ "/xx/yy/0", false } };
    std::vector<testcase> t2 = { { "", false },{ "/foo", true },{ "/foo/0", true },{ "/foo/0/0", true },{ "/foo/1", false },{ "/boo/xx", false },{ "/boo/0", true },{ "/boo/1", true },{ "/boo/1/yy", true },{ "/boo/yy/0", false } };
    std::vector<testcase> t3 = { };
    std::vector<testcase> t4 = {
        { "/", true },    // RFC 6901 requires to return "0" for 'foo' json sample. We do!
        { "/a~1b", true },
        { "/c%d", true },
        { "/e^f", true },
        { "/g|h", true },
        { "/i\\j", true },
        { "/k\"l", true },
        { "/ ", true },
        { "/m~n", true }
    };

    std::vector< std::vector<testcase> > cases;
    cases.push_back(t0);
    cases.push_back(t1);
    cases.push_back(t2);
    cases.push_back(t3);
    cases.push_back(t4);

	for(int i = 0; i < sizeof(examples)/sizeof(examples[0]); i++)                                               
    {	                                 
		std::cerr << "\nparse object i=" << i << ": ";				                                                                         
	    cJSON *json = cJSON_Parse(examples[i]);                                                                	                                                                                                          
		if (!json) {                                                                                          
			std::cerr << "json is NULL" << std::endl;                                                         
			continue;                                                                                         
		}                                                                                                     
		std::cerr << cJSON_Print(json) << std::endl << std::endl;             	         		                                                                                          
		                                                                                                      
		for(int j = 0; j < cases[i].size(); j ++) {                                              
			const cJSON *res = SimpleJsonPointer( json, cases[i][j].ptr);                                                        
    	    std::cerr << "for ptr: " << cases[i][j].ptr << " result: " << (res ? cJSON_Print(res) : "NULL") << ", test=" << ((cases[i][j].result == (res != NULL)) ? "ok" : "failed") << std::endl;
    	}                                                                                                     
    }
} 
--- */