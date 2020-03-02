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
#include <stdint.h>
#include <stdexcept>
#include <univalue.h>

#include "server.h"
#include "cc/CCPrices.h"
#include "cc/pricesfeed.h"

using namespace std;


// fills pricedata with raw price, correlated and smoothed values for numblock
/* int32_t prices_extract(int64_t *pricedata, int32_t firstheight, int32_t numblocks, int32_t ind)
{
    int32_t height, i, n, width, numpricefeeds = -1; uint64_t seed, ignore, rngval; uint32_t rawprices[1440 * 6], *ptr; int64_t *tmpbuf;
    width = numblocks + PRICES_DAYWINDOW * 2 + PRICES_SMOOTHWIDTH;    // need 2*PRICES_DAYWINDOW previous raw price points to calc PRICES_DAYWINDOW correlated points to calc, in turn, smoothed point
    komodo_heightpricebits(&seed, rawprices, firstheight + numblocks - 1);
    if (firstheight < width)
        return(-1);
    for (i = 0; i < width; i++)
    {
        if ((n = komodo_heightpricebits(&ignore, rawprices, firstheight + numblocks - 1 - i)) < 0)  // stores raw prices in backward order
            return(-1);
        if (numpricefeeds < 0)
            numpricefeeds = n;
        if (n != numpricefeeds)
            return(-2);
        ptr = (uint32_t *)&pricedata[i * 3];
        ptr[0] = rawprices[ind];
        ptr[1] = rawprices[0]; // timestamp
    }
    rngval = seed;
    for (i = 0; i < numblocks + PRICES_DAYWINDOW + PRICES_SMOOTHWIDTH; i++) // calculates +PRICES_DAYWINDOW more correlated values
    {
        rngval = (rngval * 11109 + 13849);
        ptr = (uint32_t *)&pricedata[i * 3];
        // takes previous PRICES_DAYWINDOW raw prices and calculates correlated price value
        if ((pricedata[i * 3 + 1] = komodo_pricecorrelated(rngval, ind, (uint32_t *)&pricedata[i * 3], 6, 0, PRICES_SMOOTHWIDTH)) < 0) // skip is 6 == sizeof(int64_t)/sizeof(int32_t)*3
            return(-3);
    }
    tmpbuf = (int64_t *)calloc(sizeof(int64_t), 2 * PRICES_DAYWINDOW);
    for (i = 0; i < numblocks; i++)
        // takes previous PRICES_DAYWINDOW correlated price values and calculates smoothed value
        pricedata[i * 3 + 2] = komodo_priceave(tmpbuf, &pricedata[i * 3 + 1], 3);
    free(tmpbuf);
    return(0);
}  */

UniValue prices(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("prices maxsamples\n");
    LOCK(cs_main);
    UniValue ret(UniValue::VOBJ); 
    uint64_t seed, rngval; 
    int64_t *tmpbuf, smoothed, *correlated, checkprices[PRICES_MAXDATAPOINTS]; 
    char name[64], *str; 
    uint32_t rawprices[1440 * 6], *prices = NULL; 
    int32_t ival, width, numpricefeeds = -1, n, numsamples, nextheight, ht;

    if (ASSETCHAINS_CBOPRET == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "only -ac_cbopret chains have prices");

    int32_t maxsamples = atoi(params[0].get_str().c_str());
    if (maxsamples < 1)
        maxsamples = 1;
    nextheight = komodo_nextheight();
    UniValue a(UniValue::VARR);
    if (PRICES_DAYWINDOW < 7)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "daywindow is too small");
    width = maxsamples + 2 * PRICES_DAYWINDOW + PRICES_SMOOTHWIDTH;
    numpricefeeds = komodo_heightpricebits(&seed, rawprices, nextheight - 1);
    if (numpricefeeds <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "illegal numpricefeeds");
    prices = (uint32_t *)calloc(sizeof(*prices), width*numpricefeeds);
    correlated = (int64_t *)calloc(sizeof(*correlated), width);
    ival = 0;
    for (ht = nextheight - 1, ival = 0; ival<width && ht>2; ival++, ht--)
    {
        if (ht < 0 || ht > chainActive.Height())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
        else
        {
            if ((n = komodo_heightpricebits(0, rawprices, ht)) > 0)
            {
                if (n != numpricefeeds)
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "numprices at some height != tip numprices");
                else
                {
                    for (int32_t index = 0; index < numpricefeeds; index++)
                        prices[index*width + ival] = rawprices[index];
                }
            }
            else 
                throw JSONRPCError(RPC_INVALID_PARAMETER, "no komodo_rawprices found");
        }
    }
    numsamples = ival;
    ret.push_back(Pair("firstheight", (int64_t)nextheight - 1 - ival));
    UniValue timestamps(UniValue::VARR);
    for (int32_t i = 0; i<maxsamples; i++)
        timestamps.push_back((int64_t)prices[i]);
    ret.push_back(Pair("timestamps", timestamps));
    rngval = seed;
    //for (i=0; i<PRICES_DAYWINDOW; i++)
    //    fprintf(stderr,"%.4f ",(double)prices[width+i]/10000);
    //fprintf(stderr," maxsamples.%d\n",maxsamples);


    std::vector<CCustomProcessor> processors;
    PricesFeedGetCustomProcessors(processors);

    for (int32_t index = 1; index < numpricefeeds; index++)
    {
        CustomConverter priceConverter = NULL;
        for (auto const &p : processors)
            if (index >= p.b && index < p.b)
                priceConverter = p.converter;

        UniValue item(UniValue::VOBJ), p(UniValue::VARR);
        if ((str = komodo_pricename(name, index)) != 0)
        {
            item.push_back(Pair("name", str));
            if (priceConverter != NULL)
            {
                for (int32_t i = 0; i < maxsamples + PRICES_DAYWINDOW + PRICES_SMOOTHWIDTH && i < numsamples; i++)
                {
                    UniValue parr(UniValue::VARR);
                    int64_t converted;
                    int32_t offset = index * width + i;
                    priceConverter(index, prices[offset], &converted);
                    parr.push_back(ValueFromAmount(converted));
                }
            }
            else
            {
                if (numsamples >= width)
                {
                    for (int32_t i = 0; i < maxsamples + PRICES_DAYWINDOW + PRICES_SMOOTHWIDTH && i < numsamples; i++)
                    {
                        int32_t offset = index * width + i;
                        rngval = (rngval * 11109 + 13849);
                        if ((correlated[i] = komodo_pricecorrelated(rngval, index, &prices[offset], 1, 0, PRICES_SMOOTHWIDTH)) < 0)
                            throw JSONRPCError(RPC_INVALID_PARAMETER, "null correlated price");
                        {
                            if (komodo_priceget(checkprices, index, nextheight - 1 - i, 1) >= 0)
                            {
                                if (checkprices[1] != correlated[i])
                                {
                                    //fprintf(stderr,"ind.%d ht.%d %.8f != %.8f\n",j,nextheight-1-i,(double)checkprices[1]/COIN,(double)correlated[i]/COIN);
                                    correlated[i] = checkprices[1];
                                }
                            }
                        }
                    }
                    tmpbuf = (int64_t *)calloc(sizeof(int64_t), 2 * PRICES_DAYWINDOW);
                    for (int32_t i = 0; i < maxsamples && i < numsamples; i++)
                    {
                        int32_t offset = index * width + i;
                        smoothed = komodo_priceave(tmpbuf, &correlated[i], 1);
                        if (komodo_priceget(checkprices, index, nextheight - 1 - i, 1) >= 0)
                        {
                            if (checkprices[2] != smoothed)
                            {
                                fprintf(stderr, "ind.%d ht.%d %.8f != %.8f\n", index, nextheight - 1 - i, (double)checkprices[2] / COIN, (double)smoothed / COIN);
                                smoothed = checkprices[2];
                            }
                        }
                        UniValue parr(UniValue::VARR);
                        parr.push_back(ValueFromAmount((int64_t)prices[offset] * komodo_pricemult_to10e8(index)));
                        parr.push_back(ValueFromAmount(correlated[i]));  // correlated values returned normalized to 100,000,000 order for creating synthetic indexes
                        parr.push_back(ValueFromAmount(smoothed));       // smoothed values returned normalized to 100,000,000 order for creating synthetic indexes
                        // compare to alternate method
                        p.push_back(parr);
                    }
                    free(tmpbuf);
                }
                else
                {
                    for (int32_t i = 0; i < maxsamples && i < numsamples; i++)
                    {
                        int32_t offset = index * width + i;
                        UniValue parr(UniValue::VARR);
                        parr.push_back(ValueFromAmount((int64_t)prices[offset] * komodo_pricemult_to10e8(index)));
                        p.push_back(parr);
                    }
                }
            }
            item.push_back(Pair("prices", p));
        }
        else 
            item.push_back(Pair("name", "error"));
        a.push_back(item);
    }
    ret.push_back(Pair("pricefeeds", a));
    ret.push_back(Pair("result", "success"));
    ret.push_back(Pair("seed", (int64_t)seed));
    ret.push_back(Pair("height", (int64_t)nextheight - 1));
    ret.push_back(Pair("maxsamples", (int64_t)maxsamples));
    ret.push_back(Pair("width", (int64_t)width));
    ret.push_back(Pair("daywindow", (int64_t)PRICES_DAYWINDOW));
    ret.push_back(Pair("numpricefeeds", (int64_t)numpricefeeds));
    free(prices);
    free(correlated);
    return ret;
}

// pricesbet rpc implementation
UniValue pricesbet(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 3)
        throw runtime_error("pricesbet amount leverage \"synthetic-expression\"\n"
            "amount is in coins\n"
            "leverage is integer non-zero value, positive for long, negative for short position\n"
            "synthetic-expression examples:\n"
            "    \"BTC_USD, 1\" - BTC to USD index with mult 1\n"
            "    'BTC_USD,!,1' - USD to BTC index with mult 1 (use apostrophes to escape '!' in linux bash)\n"
            "    \"BTC_USD, KMD_BTC, INR_USD, **/ , 1\" is actually KMD to INR index with mult 1\n" "\n");
    LOCK(cs_main);
    UniValue ret(UniValue::VOBJ);

    if (ASSETCHAINS_CBOPRET == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "only -ac_cbopret chains have prices");

    CAmount txfee = 10000;
    CAmount amount = atof(params[0].get_str().c_str()) * COIN;
    int16_t leverage = (int16_t)atoi(params[1].get_str().c_str());
    if (leverage == 0)
        throw runtime_error("invalid leverage\n");

    std::string sexpr = params[2].get_str();
    std::vector<std::string> vexpr;
    SplitStr(sexpr, vexpr);

    // debug print parsed strings:
    std::cerr << "parsed synthetic: ";
    for (auto s : vexpr)
        std::cerr << s << " ";
    std::cerr << std::endl;

    return PricesBet(txfee, amount, leverage, vexpr);
}

// pricesaddfunding rpc implementation
UniValue pricesaddfunding(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("pricesaddfunding bettxid amount\n"
            "where amount is in coins\n");
    LOCK(cs_main);
    UniValue ret(UniValue::VOBJ);

    if (ASSETCHAINS_CBOPRET == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "only -ac_cbopret chains have prices");

    CAmount txfee = 10000;
    uint256 bettxid = Parseuint256(params[0].get_str().c_str());
    if (bettxid.IsNull())
        throw runtime_error("invalid bettxid\n");

    CAmount amount = atof(params[1].get_str().c_str()) * COIN;
    if (amount <= 0)
        throw runtime_error("invalid amount\n");

    return PricesAddFunding(txfee, bettxid, amount);
}

// rpc pricessetcostbasis implementation
UniValue pricessetcostbasis(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("pricessetcostbasis bettxid\n");
    LOCK(cs_main);
    UniValue ret(UniValue::VOBJ);

    if (ASSETCHAINS_CBOPRET == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "only -ac_cbopret chains have prices");

    uint256 bettxid = Parseuint256(params[0].get_str().c_str());
    if (bettxid.IsNull())
        throw runtime_error("invalid bettxid\n");

    int64_t txfee = 10000;

    return PricesSetcostbasis(txfee, bettxid);
}

// pricescashout rpc implementation
UniValue pricescashout(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("pricescashout bettxid\n");
    LOCK(cs_main);
    UniValue ret(UniValue::VOBJ);

    if (ASSETCHAINS_CBOPRET == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "only -ac_cbopret chains have prices");

    uint256 bettxid = Parseuint256(params[0].get_str().c_str());
    if (bettxid.IsNull())
        throw runtime_error("invalid bettxid\n");

    int64_t txfee = 10000;

    return PricesCashout(txfee, bettxid);
}

// pricesrekt rpc implementation
UniValue pricesrekt(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("pricesrekt bettxid height\n");
    LOCK(cs_main);
    UniValue ret(UniValue::VOBJ);

    if (ASSETCHAINS_CBOPRET == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "only -ac_cbopret chains have prices");

    uint256 bettxid = Parseuint256(params[0].get_str().c_str());
    if (bettxid.IsNull())
        throw runtime_error("invalid bettxid\n");

    int32_t height = atoi(params[0].get_str().c_str());

    int64_t txfee = 10000;

    return PricesRekt(txfee, bettxid, height);
}

// pricesrekt rpc implementation
UniValue pricesgetorderbook(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("pricesgetorderbook\n");
    LOCK(cs_main);
    UniValue ret(UniValue::VOBJ);

    if (ASSETCHAINS_CBOPRET == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "only -ac_cbopret chains have prices");

    return PricesGetOrderbook();
}

// pricesrekt rpc implementation
UniValue pricesrefillfund(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("pricesrefillfund amount\n");
    LOCK(cs_main);
    UniValue ret(UniValue::VOBJ);

    if (ASSETCHAINS_CBOPRET == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "only -ac_cbopret chains have prices");

    CAmount amount = atof(params[0].get_str().c_str()) * COIN;

    return PricesRefillFund(amount);
}

