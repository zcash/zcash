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
#ifndef WIN32
#include <dlfcn.h>
#endif

#include "CCPrices.h"
// #include "CCinclude.h"
#include "pricesfeed.h"
#include "priceslibs/priceslibs.h"

#ifdef LOGSTREAM
#undef LOGSTREAM
#endif

#ifdef LOGSTREAMFN
#undef LOGSTREAMFN
#endif

template <typename T>
static void logJsonPath(const char *fname, T errToStream) {
    std::ostringstream stream;
    errToStream(stream);
    if (fname)
        std::cerr << fname << " ";
    std::cerr << stream.str();
}
// use redefined log functions because some code in this source is called in komodo init when 'debug is not parsed yet,
// so the output of some log functions in CCinclude may be lost
#define LOGSTREAM(name, level, streamexp) logJsonPath(NULL, [=](std::ostringstream &stream){ streamexp; })
#define LOGSTREAMFN(name, level, streamexp) logJsonPath(__func__, [=](std::ostringstream &stream){ streamexp; })

// external defs:
cJSON *get_urljson(char *url);

typedef struct _PriceStatus {
    std::string symbol;
    uint32_t averageValue; // polled value, average value if the symbol is polled several urls
    uint32_t multiplier;
    std::set<uint32_t> feedConfigIds;  // if the same symbol is polled from several web sources, this is the indexes of feedConfig which did the update
} PriceStatus;

static std::vector<PriceStatus> pricesStatuses;

static std::vector<CFeedConfigItem> feedconfig({ 
    {
        // default feed:
        "prices",   // name
        "",         // custom lib.so
        "http://api.coindesk.com/v1/bpi/currentprice.json",  // url
        {},     // substitutes
        "",     // base
        { },    // substituteResult 
        { // manyResults:
            { "USD_BTC", "/bpi/USD/rate_float" },    // symbol and valuepath
            { "EUR_BTC", "/bpi/EUR/rate_float" },
            { "GBP_BTC", "/bpi/GBP/rate_float" }
        },
        60, // interval
        10000  // multiplier
    } 
});

struct CPollStatus
{
    time_t lasttime;
    void *customlibHandle;
    CustomJsonParser customJsonParser;

    CPollStatus() {
        lasttime = 0L;
        customlibHandle = NULL;
        customJsonParser = NULL;
    }
    ~CPollStatus() {
#ifndef WIN32
        if (customlibHandle)
            dlclose(customlibHandle);
#endif
    }

};

static std::vector<CPollStatus> pollStatuses;  

bool init_prices_statuses()
{
    int32_t nsymbols = 0, nduplicates = 0;
    pricesStatuses.reserve(128);

    pricesStatuses.push_back({ "", 0, 0 });  // first item is timestamp

    for (const auto &citem : feedconfig)
    {
        if (!citem.substitutes.empty()) {
            for (const auto &s : citem.substitutes) {
                std::string name = s;
                if (!citem.base.empty())
                    name += "_" + citem.base;

                std::vector<PriceStatus>::iterator iter = std::find_if(pricesStatuses.begin(), pricesStatuses.end(), [&](const PriceStatus &p) { return p.symbol == name;});
                if (iter == pricesStatuses.end()) {
                    pricesStatuses.push_back({ name, 0, citem.multiplier });
                    nsymbols++;
                }
                else
                {
                    if (iter->multiplier != citem.multiplier) {
                        LOGSTREAMFN("prices", CCLOG_INFO, stream << "cannot initialize prices, for a symbol different multipliers set" << std::endl);
                        return false;
                    }
                    nduplicates++;
                }
            }
        }
        else {
            for (const auto &r : citem.manyResults) {
                std::string name = r.symbol;
                std::vector<PriceStatus>::iterator iter = std::find_if(pricesStatuses.begin(), pricesStatuses.end(), [&](const PriceStatus &p) { return p.symbol == name;});
                if (iter == pricesStatuses.end()) {
                    pricesStatuses.push_back({ name, (uint32_t)0, citem.multiplier });
                    nsymbols++;
                }
                else
                {
                    if (iter->multiplier != citem.multiplier) {
                        LOGSTREAMFN("prices", CCLOG_INFO, stream << "cannot initialize prices, for a symbol different multipliers set" << std::endl);
                        return false;
                    }
                    nduplicates++;
                }
            }
        }
    }
    LOGSTREAMFN("prices", CCLOG_INFO, stream << "initialized symbols=" << nsymbols << " duplicates=" << nduplicates << std::endl);
    return true;
}

bool init_poll_statuses()
{
    int itemcount = feedconfig.size();
    while (itemcount--)
        pollStatuses.push_back(CPollStatus());

    for (int i = 0; i < feedconfig.size(); i ++)
    {
        if (!feedconfig[i].customlib.empty()) {
            std::string libpath = "./cc/priceslibs/" + feedconfig[i].customlib;
#ifndef WIN32
            pollStatuses[i].customlibHandle = dlopen(libpath.c_str(), RTLD_LAZY);
            if (pollStatuses[i].customlibHandle == NULL) {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "can't load prices custom lib=" << libpath << std::endl);
                return false;
            }
            pollStatuses[i].customJsonParser = (CustomJsonParser)dlsym(pollStatuses[i].customlibHandle, PF_CUSTOMJSONPARSERFUNCNAME);
            if (pollStatuses[i].customlibHandle == NULL) {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "can't load parser function=" << PF_CUSTOMJSONPARSERFUNCNAME << " from custom lib=" << feedconfig[i].customlib << std::endl);
                return false;
            }
#endif
        }
    }
    return true;
}
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

        if (cJSON_HasObjectItem(jitem, "customlib"))
            citem.customlib = cJSON_GetObjectItem(jitem, "customlib")->valuestring;
        
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
            auto parseResults = [](const cJSON *jres) -> CFeedConfigItem::ResultProcessor {
                std::string symbol, valuepath, customdata;
                std::vector<std::string> averagepaths;

                //const cJSON *jsymbolpath = cJSON_GetObjectItem(jres, "symbolpath"); // not supported for substitutes
                //if (jsymbolpath)
                //    citem.substituteResult.symbolpath = jsymbolpath->valuestring;

                const cJSON *jsymbol = cJSON_GetObjectItem(jres, "symbol");
                if (jsymbol)
                    symbol = jsymbol->valuestring;

                const cJSON *jvaluepath = cJSON_GetObjectItem(jres, "valuepath");
                if (jvaluepath)
                    valuepath = jvaluepath->valuestring;

                const cJSON *javeragepaths = cJSON_GetObjectItem(jres, "averagevaluepaths");
                if (javeragepaths) {
                    if (cJSON_IsArray(javeragepaths)) {
                        for (int i = 0; i < cJSON_GetArraySize(javeragepaths); i++) {
                            const cJSON *jitem = cJSON_GetArrayItem(javeragepaths, i);
                            if (cJSON_IsString(jitem)) {
                                std::string sitem = jitem->valuestring;
                                averagepaths.push_back(sitem);
                            }
                        }
                    }
                }
                const cJSON *jcustomdata = cJSON_GetObjectItem(jres, "customdata");
                if (jcustomdata)
                    customdata = jcustomdata->valuestring;

                return { symbol, valuepath, averagepaths, customdata };
            };

            const cJSON *jres = cJSON_GetObjectItem(jitem, "results");
            if (cJSON_IsObject(jres))
            {
                // for substitutes single results object is used
                citem.substituteResult = parseResults(jres);

                if (citem.customlib.empty() && citem.substituteResult.valuepath.empty() && citem.substituteResult.averagepaths.empty()) {
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' object: no 'valuepath' or 'averagevaluepaths' elements" << std::endl);
                    return false;
                }
                if (!citem.substituteResult.valuepath.empty() && !citem.substituteResult.averagepaths.empty()) {  
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' object: can't specify both 'valuepath' and 'averagevaluepaths'" << std::endl);
                    return false;
                }
            }
            else if (cJSON_IsArray(jres))
            {
                // if no substitutes, it should be an array of results
                for (int j = 0; j < cJSON_GetArraySize(jres); j++)
                {
                    cJSON *jresitem = cJSON_GetArrayItem(jres, j);
                    if (cJSON_IsObject(jresitem))
                    {
                        CFeedConfigItem::ResultProcessor res = parseResults(jresitem);

                        if (citem.customlib.empty() && res.valuepath.empty() && res.averagepaths.empty()) {
                            LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' array: no either 'valuepath' or 'averagevaluepaths' elements" << std::endl);
                            return false;
                        }
                        if (!res.valuepath.empty() && !res.averagepaths.empty()) {
                            LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' object: can't specify both 'valuepath' and 'averagevaluepaths'" << std::endl);
                            return false;
                        }
                        citem.manyResults.push_back(res);
                    }
                    else
                    {
                        LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'results' array element has incorrect type" << std::endl);
                        return false;
                    }
                }
            }
            else {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' element, should be either object or array" << std::endl);
                return false;
            }
        }

        // check config rules for results
        if (!citem.customlib.empty())   {
            if (cJSON_HasObjectItem(jitem, "results")) {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no 'results' element" << std::endl);
                return false;
            }
            if (citem.substitutes.size() == 0 && citem.manyResults.empty()) {   
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item has no correct 'results' element: not an array or empty" << std::endl);
                return false;
            }
        }

        // base which is added to symbol to complete it like: for "base":"BTC" and "symbol":"USD" extened symbol is "USD_BTC"
        if (cJSON_HasObjectItem(jitem, "base")) 
        {
            citem.base = cJSON_GetObjectItem(jitem, "base")->valuestring;
            if (citem.base.empty()) {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'base' is incorrect" << std::endl);
                return false;
            }
            if (!citem.manyResults.empty()) {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "use of 'base' is allowed only for 'substitutes' configuration" << std::endl);
                return false;
            }
        }

        // multiplier to normalize price value to uint32
        citem.multiplier = 10000; // default value
        cJSON *jmultiplier = cJSON_GetObjectItem(jitem, "multiplier");
        if (jmultiplier) {
            if (cJSON_IsNumber(jmultiplier) && jmultiplier->valuedouble >= 1)
                citem.multiplier = jmultiplier->valuedouble;
            else   {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'multiplier' value is incorrect, should be number >= 1" << std::endl);
                return false;
            }
        }
        LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'multiplier' used value=" << citem.multiplier << std::endl);

        citem.interval = 60; //default value
        cJSON *jinterval = cJSON_GetObjectItem(jitem, "interval");
        if (jinterval) {
            if (cJSON_IsNumber(jinterval) && jinterval->valuedouble >= 60) 
                citem.interval = jinterval->valuedouble;
            else {
                LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'interval' value is incorrect, should be number >= 60" << std::endl);
                return false;
            }
        }
        LOGSTREAMFN("prices", CCLOG_INFO, stream << "config item 'interval' used value=" << citem.interval << std::endl);

        feedconfig.push_back(citem);  // add new feed config item
    }

    // get symbols and init prices status for each symbol:
    if (!init_prices_statuses())
        return false;

    // init statuses for the configured feeds:
    if (!init_poll_statuses())
        return false;

    if (PricesFeedSymbolsCount() > KOMODO_MAXPRICES-1) {
        LOGSTREAMFN("prices", CCLOG_ERROR, stream << "prices values too big: " << PricesFeedSymbolsCount() << std::endl);
        return false;
    }

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

// check if string is a int  
static bool is_string_int(const std::string &s)
{
    const char *p = s.c_str();
    int count = 0;
    while (*p && (*p =='+' || *p== '-' || isdigit(*p))) p++, count++;
    return (count > 0 && count == s.length());
}

// check if string is a float
static bool is_string_float(const std::string &s)
{
    const char *p = s.c_str();
    int count = 0;
    while (*p && (*p == '+' || *p == '-' || *p == '.' || isdigit(*p))) p++, count++;
    return (count > 0 && count == s.length());
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
			if (!is_string_int(tokens.front()))
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
static uint32_t feed_config_size(const CFeedConfigItem &citem)
{
    if (!citem.substitutes.empty())
        return citem.substitutes.size();
    else
        return citem.manyResults.size();
}

// return price name for index
char *PricesFeedSymbolName(char *name, int32_t ind) 
{
    if (ind >= 1 && ind < pricesStatuses.size()) {
        if (strlen(pricesStatuses[ind].symbol.c_str()) < PRICES_MAXNAMELENGTH-1)
            strcpy(name, pricesStatuses[ind].symbol.c_str());
        else
        {
            strncpy(name, pricesStatuses[ind].symbol.c_str(), PRICES_MAXNAMELENGTH-1);
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
        return 10000;  //dummy value for 'timestamp' element, not used really

    int32_t offset = 1;
    for (const auto & citem : feedconfig) {
        uint32_t size1 = feed_config_size(citem);
        if (ind - offset < size1)
            return citem.multiplier;
        offset += size1;
    }
    return 0;
}

// returns how many names added to pricesNames (names could be added in random order)
int32_t PricesFeedSymbolsCount()
{
    return pricesStatuses.size();
}

// returns string with all price names parameters (for including into the chain magic)
void PricesFeedAllSymbolsParameters(std::string &names)
{
    names.reserve(PricesFeedSymbolsCount() * 4); // reserve space considering that mean value is somewhere between BTS_USD, AAMTS, XAU,...

    for (const auto &ci : feedconfig)
    {
        if (!ci.substitutes.empty()) {
            // make names from substitutes:
            for (const auto &s : ci.substitutes) {
                std::string name = s;
                // TODO: removed for compat with prev version:
                //if (!ci.base.empty())
                //    name += "_" + ci.base;
                names += name;
            }
        }
        else {
            // make names from manyResults symbols :
            for (const auto &r : ci.manyResults) {
                names += r.symbol;
            }
        }
    }
    //std::cerr << __func__ << " names=" << names << std::endl;
}


// extract price value (and symbol name if required)
// note: extracting symbol names from json is disabled, probably we won't ever need this
// to enable this we should switch to delayed initialization of pricesStatuses (until all symbols will be extracted from all the feeds)
static bool parse_result_json_value(const cJSON *json, /*const std::string &symbolpath,*/ const std::string &valuepath, uint32_t multiplier, /*std::string &symbol,*/ uint32_t *pricevalue)
{

    const cJSON *jvalue = SimpleJsonPointer(json, valuepath.c_str());
    if (jvalue)
    {
        // reliable processing of value: allow either number or string
        if (cJSON_IsNumber(jvalue)) {
            *pricevalue = jvalue->valuedouble * multiplier;
        }
        else if (cJSON_IsString(jvalue) && is_string_float(jvalue->valuestring)) {
            *pricevalue = atof(jvalue->valuestring) * multiplier;
        }
        else
        {
            *pricevalue = 0;
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "feed json value not a number, path=" << valuepath << " json=" << cJSON_Print(json) << std::endl);
            return false;
        }
    }
    else
    {
        *pricevalue = 0;
        LOGSTREAMFN("prices", CCLOG_INFO, stream << "feed json value not found" << std::endl);
        return false;
    }

/*  it is not supported getting symbols from the json as we need to know them on the config stage
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
*/
    return true;
}


// calc average value by enumerating value arrays by json paths with "*" as array indexes 
static bool parse_result_json_average(const cJSON *json, const std::vector<std::string> &valuepaths, uint32_t multiplier, uint32_t *pricevalue)
{
    double total = 0.0;
    int32_t count = 0;

    for (const auto &origpath : valuepaths)
    {
        std::function<void(const cJSON*, const std::string &)> enumJsonOnLevel = [&](const cJSON *json, const std::string &path)->void
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

                    // note that names are added on komodo init when -debug has not been parsed yet and LOGSTREAM output won't be shown at this stage
                    // so we use redefined LOGSTREAM (not that one in CCinclude.h) which sends always to std::cerr
                    //LOGSTREAM("prices", CCLOG_DEBUG2, stream << "enumJsonOnLevel searching index subpath=" << toppathind << " " << (jfound ? "found" : "null") << std::endl);
                    if (!jfound)
                        break;
                    if (restpath.empty()) 
                    {
                        if (cJSON_IsNumber(jfound)) {
                            total += jfound->valuedouble;
                            count++;
                        }
                        else     
                            LOGSTREAM("prices", CCLOG_DEBUG2, stream << "enumJsonOnLevel array leaf value not a number" << std::endl);
                        
                    }
                    else 
                        enumJsonOnLevel(jfound, restpath);  // object or array
                }
            }
            else
            {
                // should be leaf value
                const cJSON *jfound = SimpleJsonPointer(json, path.c_str());
                //LOGSTREAM("prices", CCLOG_DEBUG2, stream << "enumJsonOnLevel checking last subpath=" << path << " " << (jfound ? "found" : "null") << std::endl);
                if (jfound) {
                    if (cJSON_IsNumber(jfound)) {
                        total += jfound->valuedouble;
                        count++;
                    }
                    else
                        LOGSTREAM("prices", CCLOG_DEBUG2, stream << "enumJsonOnLevel object leaf value not a number" << std::endl);
                }
                else 
                    LOGSTREAM("prices", CCLOG_DEBUG2, stream << "enumJsonOnLevel leaf not found" << std::endl);
            }
        };
        enumJsonOnLevel(json, origpath);
    }

    if (count > 0)   {
        *pricevalue = (uint32_t) (total / count * multiplier);
        return true;
    }
    else   {
        *pricevalue = 0;
        return false;
    }
}

// pool single url using substitutes or without them
static uint32_t poll_one_feed(const CFeedConfigItem &citem, const CPollStatus &pollStat, uint32_t *pricevalues, std::vector<std::string> &symbols)
{
    uint32_t numadded = 0;

    LOGSTREAMFN("prices", CCLOG_INFO, stream << "polling..." << std::endl);
    if (citem.substitutes.size() > 0)
    {
        // substitute %s in url, call url and get the symbol from json result
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
                    std::string symbol;
                    //std::string jsymbol;
                    bool parsed;
                    //if (citem.substituteResult.symbolpath.empty()) {
                    symbol = subst;
                    if (!citem.base.empty())
                        symbol += "_" + citem.base;
                    //}
                    //else
                    //    symbol = jsymbol;

                    if (!citem.customlib.empty()) {
                        if (pollStat.customJsonParser) {
                            char *sjson = cJSON_Print(json);
                            parsed = pollStat.customJsonParser(sjson, symbol.c_str(), citem.substituteResult.customdata.c_str(), citem.multiplier, &pricevalues[numadded++]);
                            if (sjson)
                                cJSON_free(sjson);
                        }
                    }
                    else if (!citem.substituteResult.averagepaths.empty())
                        parsed = parse_result_json_average(json, citem.substituteResult.averagepaths, citem.multiplier, &pricevalues[numadded++]);
                    else
                        parsed = parse_result_json_value(json, /*citem.substituteResult.symbolpath,*/ citem.substituteResult.valuepath, citem.multiplier, /*jsymbol,*/ &pricevalues[numadded++]);
                    if (parsed) {
                       
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
    {   // many values in one json result
        cJSON *json = get_urljson((char*)citem.url.c_str()); // poll
        if (json != NULL)
        {
            bool parsed = false;
            const std::string empty;
            std::string dummy;

            for (const auto &r : citem.manyResults) 
            {
                if (!citem.customlib.empty()) {
                    if (pollStat.customJsonParser) {
                        char *sjson = cJSON_Print(json);
                        parsed = pollStat.customJsonParser(sjson, r.symbol.c_str(), r.customdata.c_str(), citem.multiplier, &pricevalues[numadded++]);
                        if (sjson)
                            cJSON_free(sjson);
                    }
                }
                else if (!r.averagepaths.empty())
                    parsed = parse_result_json_average(json, r.averagepaths, citem.multiplier, &pricevalues[numadded++]);
                else
                    parsed = parse_result_json_value(json, /*empty,*/ r.valuepath, citem.multiplier, /*dummy,*/ &pricevalues[numadded++]);
                if (parsed) 
                    symbols.push_back(r.symbol);
                else
                    break;

                LOGSTREAM("prices", CCLOG_INFO, stream << r.symbol << " " << pricevalues[numadded-1] << " ");
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

void store_price_value(const std::string &symbol, int32_t configid, uint32_t value)
{
    std::vector<PriceStatus>::iterator iter = std::find_if(pricesStatuses.begin(), pricesStatuses.end(), [&](const PriceStatus &p) { return p.symbol == symbol;});
    if (iter == pricesStatuses.end()) {
        LOGSTREAMFN("prices", CCLOG_INFO, stream << "internal error: can't store value, no such symbol: " << symbol << std::endl);
        return;
    }

    std::cerr << __func__ << "\t" << "before feedConfigIds.size()=" << iter->feedConfigIds.size() << " averagevalue=" << iter->averageValue << " configid=" << configid << std::endl;
    if (iter->feedConfigIds.find(configid) != iter->feedConfigIds.end())
    {
        // configid is repeated, this means a new poll cycle is beginning, reset average value and clear configids:
        iter->averageValue = value;
        iter->feedConfigIds.clear();
        iter->feedConfigIds.insert(configid);
    }
    else
    {
        // recalculate average value and store additional polling configid
        iter->averageValue = (uint32_t)((uint64_t)iter->averageValue * iter->feedConfigIds.size() + value) / (iter->feedConfigIds.size() + 1);
        iter->feedConfigIds.insert(configid);
    }
    std::cerr << __func__ << "\t" << "after feedConfigIds.size()=" << iter->feedConfigIds.size() << " averagevalue=" << iter->averageValue << std::endl;
}

uint32_t PricesFeedPoll(uint32_t *pricevalues, const uint32_t maxsize, time_t *now)
{
    uint32_t offset;
    uint32_t currentValNumber = 0;
    uint32_t nsymbols = 0;
    *now = time(NULL);
    bool updated = false;

    // dont do this, should be old values!
    // memset(pricevalues, '\0', maxsize); // reset to 0 as some feeds maybe updated, some not in this poll

    pricevalues[0] = *now;              //set timestamp
    offset = 1; // one off

    for (int32_t iconfig = 0; iconfig < feedconfig.size(); iconfig++)
    {
        uint32_t size1 = feed_config_size(feedconfig[iconfig]);
        std::shared_ptr<uint32_t> pricesbuf1((uint32_t*)calloc(size1, sizeof(uint32_t)));

        if (!pollStatuses[iconfig].lasttime || *now > pollStatuses[iconfig].lasttime + feedconfig[iconfig].interval)  // first time poll
        {
            LOGSTREAMFN("prices", CCLOG_INFO, stream << "entering poll, !pollStatuses[iconfig].lasttime=" << !pollStatuses[iconfig].lasttime << std::endl);
            std::vector<std::string> symbols;
            // poll url and get values and symbols
            if (poll_one_feed(feedconfig[iconfig], pollStatuses[iconfig], pricesbuf1.get(), symbols) > 0)
            {
                if (size1 != symbols.size()) {
                    LOGSTREAMFN("prices", CCLOG_INFO, stream << "internal error: incorrect returned symbol size" << std::endl);
                    return 0;
                }
                for (int32_t ibuf = 0; ibuf < size1; ibuf++)
                {
                    store_price_value(symbols[ibuf], iconfig, pricesbuf1.get()[ibuf]);
                }
                updated = true;
                pollStatuses[iconfig].lasttime = *now;
            }
        }

        // free(pricesbuf1); use shared_ptr

        if (offset + size1 >= maxsize) 
            return PF_BUFOVERFLOW;  // buffer overflow

        offset += size1;
    }
    if (updated) {
        // unload price values to the output buffer
        for (int i = 1; i < pricesStatuses.size(); i++)
            pricevalues[i] = pricesStatuses[i].averageValue;  // one off
        return pricesStatuses.size();
    }
    else
        return 0;  // no update in this poll
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
        bool substituteResult;
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
    	    std::cerr << "for ptr: " << cases[i][j].ptr << " substituteResult: " << (res ? cJSON_Print(res) : "NULL") << ", test=" << ((cases[i][j].substituteResult == (res != NULL)) ? "ok" : "failed") << std::endl;
    	}                                                                                                     
    }
} 
--- */