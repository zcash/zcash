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

#include "CCinclude.h"
#include "pricesfeed.h"

/* not used:
template <typename T>
static void logJsonPath(T errToStream) {
    std::ostringstream stream;
    errToStream(stream);
    std::cerr << stream.str();
}

#define LOGSTREAM(name, level, streamexp) logJsonPath([=](std::ostringstream &stream){ streamexp; })
*/

cJSON *get_urljson(char *url);

static std::vector<CFeedConfigItem> feedconfig({ 
    {
        // default feed:
        "prices",   // name
        "http://api.coindesk.com/v1/bpi/currentprice.json",  // url
        {},     // substitutes
        "BTC",  // base
        { },    // resultDesc
        // resultsDesc:
        { 
            { { "/bpi/USD/code", "/bpi/USD/rate_float" }, { "/bpi/EUR/code", "/bpi/EUR/rate_float" }, { "/bpi/GBP/code", "/bpi/GBP/rate_float" } },    // paths
            true     // symbolIsPath
        },
        60, // interval
        10000  // multiplier
    } 
});
static std::vector<std::string> priceNames;

typedef struct 
{
    time_t lasttime;
} PollStatus;


const PollStatus nullPollStatus = { 0 };
static std::vector<PollStatus> pollStatuses({ nullPollStatus });  // init for default feed

// parse poll configuration in json from cmd line 
bool PricesFeedParseConfig(const cJSON *json)
{
    if (!cJSON_IsArray(json)) {
        LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config not a json array" << std::endl);
        return false;
    }

    for (int i = 0; i < cJSON_GetArraySize(json); i++)
    {
        cJSON *jitem = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(jitem)) {
            LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config element not a object" << std::endl);
            return false;
        }
        CFeedConfigItem citem;
        if (cJSON_HasObjectItem(jitem, "name"))
            citem.name = cJSON_GetObjectItem(jitem, "name")->valuestring;
        if (citem.name.empty()) {
            LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item has no 'name'" << std::endl);
            return false;
        }
        if (cJSON_HasObjectItem(jitem, "url"))
            citem.url = cJSON_GetObjectItem(jitem, "url")->valuestring;
        if (citem.url.empty()) {
            LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item has no 'url'" << std::endl);
            return false;
        }
        if (cJSON_HasObjectItem(jitem, "substitutes")) {
            cJSON *jsubst = cJSON_GetObjectItem(jitem, "substitutes");
            if (!cJSON_IsArray(jsubst)) {
                LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item 'substitutes' is not an array" << std::endl);
                return false;
            }
            for (int j = 0; j < cJSON_GetArraySize(jsubst); j++)
            {
                cJSON *jsubstitem = cJSON_GetArrayItem(jsubst, j);
                std::string subst = jsubstitem->valuestring;
                if (subst.empty()) {
                    LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item 'substitutes' is empty" << std::endl);
                    return false;
                }
                citem.substitutes.push_back(subst);
            }
        }

        // check we have substitute macro in url:
        if (citem.substitutes.size() > 0 && citem.url.find("%s") == std::string::npos) {
            LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item 'url' has no '%s' macro required for 'substitutes'" << std::endl);
            return false;
        }

        if (cJSON_HasObjectItem(jitem, "results"))
        {
            cJSON *jres = cJSON_GetObjectItem(jitem, "results");
            if (cJSON_IsObject(jres))
            {
                cJSON *jsymbolpath = cJSON_GetObjectItem(jres, "symbolpath");
                cJSON *jvaluepath = cJSON_GetObjectItem(jres, "valuepath");
                if (jsymbolpath)
                    citem.result.symbolpath = jsymbolpath->valuestring;
                if (jvaluepath)
                    citem.result.valuepath = jvaluepath->valuestring;

                if (/*citem.result.symbolpath.empty() || */ citem.result.valuepath.empty()) {  // empty symbolpath means use substitute as symbol
                    LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item has no correct 'results' description: no valuepath in results" << std::endl);
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
                            LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item has no correct 'results' description: no either 'symbolname' or 'symbolpath' or no 'valuepath' items" << std::endl);
                            return false;
                        }
                        citem.results.paths.push_back(std::make_pair(symbolpath, valuepath));
                    }
                    else
                    {
                        LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item 'results' element has incorrect type" << std::endl);
                        return false;
                    }
                }
                // check either symbolname or symbolpath is used:
                if (nsymbolname > 0 && nsymbolpath > 0) {
                    LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item 'results' can't have both symbolname and symbolpath" << std::endl);
                    return false;
                }
            }
            else {
                LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item has no correct 'results' description: neither object nor array" << std::endl);
                return false;
            }
        }
        else {
            LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item has no 'results' element" << std::endl);
            return false;
        }

        if (citem.substitutes.size() > 0)   {
            if (citem.result.valuepath.empty() ) {
                LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item has no correct 'results' description: not an object with 'valuepath' element" << std::endl);
                return false;
            }
        }
        else {
            if (citem.results.paths.empty()) {
                LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item has no correct 'results' description: not an array with symbol and valuepath, or empty" << std::endl);
                return false;
            }
        }

        if (cJSON_HasObjectItem(jitem, "base")) 
        {
            citem.base = cJSON_GetObjectItem(jitem, "base")->valuestring;
            if (citem.base.empty()) {
                LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item 'base' is incorrect" << std::endl);
                return false;
            }
        }

        citem.multiplier = 10000; // default value
        std::string smultiplier = cJSON_GetObjectItem(jitem, "multiplier")->valuestring;
        if (!smultiplier.empty()) {
            citem.multiplier = atoi(smultiplier.c_str());
            if (citem.multiplier < 1) {
                LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item 'multiplier' value is incorrect, should be >= 1" << std::endl);
                return false;
            }
        }

        citem.interval = 60; //default value
        std::string sinterval = cJSON_GetObjectItem(jitem, "interval")->valuestring;
        if (!sinterval.empty()) {
            citem.interval = atoi(sinterval.c_str());
            if (citem.interval < 60) {
                LOGSTREAM("prices", CCLOG_INFO, stream << "prices feed config item 'interval' value is incorrect, should be >= 60" << std::endl);
                return false;
            }
        }

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
static bool isNumberString(const std::string &s)
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
    while (*e) {
        //const char *e0 = e;
        if (*e == '/') {
            // if (b < e) { -- allow empty "" properties
            std::string token(b, e);
			junescape(token); 
            tokens.push_back(token);
            //}
			b = e + 1;
        }
        e++;
    }

	// if (b <= e) {  -- allow empty "" properties                       
    std::string token(b, e);        
	junescape (token);
    tokens.push_back(token);        
    // }                                   


    //std::cerr << "tokens:"; 
    //for(auto l:tokens) std::cerr << l << " ";
    //std::cerr << std::endl;

    // lambda to browse json recursively
    std::function<const cJSON*(const cJSON*)> browseOnLevel = [&](const cJSON *json)->const cJSON*
    {
		if (cJSON_IsNull(json))	
			return ERR_JSONPOINTER(stream << "json is null");			

        if (tokens.empty())                                                                       
            return json;                                                                          

        //char *p=cJSON_Print(json);
		//std::cerr << "json on level:"<< (p?*p:"NULL") << std::endl;
        //if (p) cJSON_free(p); 
        if (cJSON_IsArray(json))
        {
			if (!isNumberString(tokens.front()))
				return ERR_JSONPOINTER(stream << "should be numeric array index");				
			                                                                                
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
                return ERR_JSONPOINTER(stream << "array index out of range");                   
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
		return ERR_JSONPOINTER(stream << "unexpected code reached");
    };

    return browseOnLevel(json);
}

// return number of a configured feed's symbols to get
uint32_t GetFeedSize(const CFeedConfigItem &citem)
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
        totalsize += GetFeedSize(fc);
    return totalsize;
}

// extract price value (and symbol name if required)
static void PricesFeedParseJson(const cJSON *json, const std::string &symbolpath, const std::string &valuepath, uint32_t multiplier, std::string &symbol, uint32_t *pricevalue)
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
            LOGSTREAM("prices", CCLOG_INFO, stream << "PricesFeedParseJson" << " " << "feed json value not a number" << std::endl);
        }
    }
    else
    {
        *pricevalue = 0;
        LOGSTREAM("prices", CCLOG_INFO, stream << "PricesFeedParseJson" << " " << "feed json value not found" << std::endl);
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
                LOGSTREAM("prices", CCLOG_INFO, stream << "PricesFeedParseJson" << " " << "feed json symbol not a string" << std::endl);
            }
        }
        else
        {
            symbol = "";
            LOGSTREAM("prices", CCLOG_INFO, stream << "ParseFeedJson" << " " << "feed json symbol not found" << std::endl);
        }
    }      
}

// 
static uint32_t PollOneFeed(const CFeedConfigItem &citem, uint32_t *pricevalues, std::vector<std::string> &symbols)
{
    uint32_t numadded = 0;

    LOGSTREAM("prices", CCLOG_INFO, stream << "PollOneFeed" << " " << "polling...");
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
                    PricesFeedParseJson(json, citem.result.symbolpath, citem.result.valuepath, citem.multiplier, jsymbol, &pricevalues[numadded++]);
                    if (citem.result.symbolpath.empty())
                        symbol = subst;
                    else
                        symbol = jsymbol;
                    if (!citem.base.empty())
                        symbol += "_" + citem.base;
                    symbols.push_back(jsymbol);
                    LOGSTREAM("prices", CCLOG_INFO, stream << jsymbol << " " << pricevalues[numadded - 1] << " ");
                    cJSON_Delete(json);
                }
                else 
                {
                    LOGSTREAM("prices", CCLOG_INFO, stream << "PollOneFeed" << " " << "feed service not available: " << url << std::endl);
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

            for (const auto &r : citem.results.paths) {
                PricesFeedParseJson(json, (citem.results.symbolIsPath ? r.first : empty), r.second, citem.multiplier, jsymbol, &pricevalues[numadded++]);
                if (citem.results.symbolIsPath)
                    symbol = jsymbol; // from json
                else
                    symbol = r.first; // from config
                if (!citem.base.empty())
                    symbol += "_" + citem.base;
                symbols.push_back(symbol);

                LOGSTREAM("prices", CCLOG_INFO, stream << symbol << " " << pricevalues[numadded-1] << " ");
            }
            cJSON_Delete(json);
        }
        else
        {
            LOGSTREAM("prices", CCLOG_INFO, stream << "PollOneFeed" << " " << "feed service not available: " << citem.url << std::endl);
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
        nsymbols += GetFeedSize(fc);
    priceNames.resize(nsymbols+1);

    pricevalues[offset++] = (uint32_t)0;
    totalsize++;

    for (int32_t i = 0; i < feedconfig.size(); i ++)
    {
        uint32_t size1 = GetFeedSize(feedconfig[i]);    

        if (!pollStatuses[i].lasttime || *now > pollStatuses[i].lasttime + feedconfig[i].interval)  // first time poll
        {
            LOGSTREAM("prices", CCLOG_INFO, stream << "PricesFeedPoll" << " " << "entering poll, !pollStatuses[i].lasttime=" << !pollStatuses[i].lasttime << std::endl);
            std::vector<std::string> symbols;
            // poll url and get values and symbols
            if (PollOneFeed(feedconfig[i], &pricevalues[offset], symbols))
            {
                updated = true;
                if (!pollStatuses[i].lasttime) {
                    // add symbols, first item is timestamp:
                    LOGSTREAM("prices", CCLOG_INFO, stream << "PricesFeedPoll" << " " << "adding symbols to pricename" << std::endl);
                    for (int32_t j = 0; j < symbols.size(); j++) {
                        priceNames[totalsize + j] = symbols[j];
                        LOGSTREAM("prices", CCLOG_INFO, stream << "PricesFeedPoll" << " " << "added to pricename index=" << totalsize + j << " symbol=" << symbols[j] << std::endl);
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