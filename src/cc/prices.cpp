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

#include "CCassets.h"
#include "CCPrices.h"

/*
CBOPRET creates trustless oracles, which can be used for making a synthetic cash settlement system based on real world prices;
 
 0.5% fee based on betamount, NOT leveraged betamount!!
 0.1% collected by price basis determinant
 0.2% collected by rekt tx

 At the simplest, prices CC allows to make leveraged cash settled bets on long and short BTCUSD:
 BTCUSD, 1 with positive leverage parameter up to 777
 BTCUSD, 1 with negative leverage parameter up to -777
 
 These specific limits of 0.5%, 0.1%, 0.2%, 777 should be able to be changed based on #define
 
 Another configuration is to send the 0.4% (or 0.2%) fees to a fee collection address (this is not currently implemented, but would be needed for systems that dont use -ac_perc to collect a global override)
 
 The definition of the synthetic instrument that is being tracked is actually defined with a forth type of syntax and is quite flexible to allow unlimited combinations and weights, but that is an independent aspect and will be covered later.
 
 Let us discuss the simplest synthetic case of a long or short of BTCUSD (or any other direct pricepoint from the trustless oracle). If you look at the charts, you will see the blue line that is direct from the miners (and therefore cant be directly used). The orange line is the 51% correlated price that is deterministically random selected from the prior 24 hours. And finally the green line which is simply the average value from the priot 24 hours.
 
 We will assume that the orange and green prices are able to be deterministically calculated from the raw coinbase data (blue). The prices rpc is currently working reasonably well and appears to return deterministic prices, however for use in the prices CC, it is often needed to find just a single data point. To that effect there is the temporary function prices_syntheticprice, which uses a bruteforce and totally inefficient way to calculate the correlated and smoothed prices. This inefficient process will need to be changed to a permanent storage of correlated and smoothed prices for each trustless oracle that is updated each block. This will then allow to directly lookup each value for any of the trustless prices. Such speed will indeed be needed to scale up usage of a REKT chain.
 
 Since the mined prices can be manipulated by any miner, combined with the +/-1% tolerance, it would be possible for an external miner to obtain a large hashrate and skew the price +1%, then to -1% to create a 2% price difference, which under high leverage might be able to generate a large profit.
 
 To avoid this precisely controllable biasing, the 51% correlation of past day is used (orange), which makes the price jump around a lot. The green line that sums the prior day is needed to remove this jitter back to the average value. However, this creates a 1.5 day delay to the price movement of the smoothed price compared to the current price. What this means is that if we want to use the green line to settle prices automatically, a trivial way to make money is to bet in the direct that the mined prices are relative to the smoothed prices. Given 1.5 day head start, it wont be any mystery which direction the smoothed line will move in the next 24 hours.
 
 So this makes the smoothed price unusable for settling the bets with. However, we cant use the correlated price either, as it would also allow to make bets when the correlated price picked a significantly lower/higher price than the raw price. The solution is to have a floating basis for the costbasis of the bet. In order to allow finding the right costbasis, for long bets the maximum price between the correlated and smoothed price is needed over the first day after the bet. For short bets, the minimum price is needed.
 
 What this means is that the actual starting price for a bet wont be known for sure when the bet is made, but if the price is relatively stable, there wont be much of a change. If there is a lot of recent volatility, then the starting price will have a high variability. In order to calculate the costbasis, it currently requires many calculations to find the MAX (or MIN) for the first day. Even when this is made to be a lookup, it still requires to issue a transaction to change the state of a bet from a "starting" state to a "in effect" state and this state change is indicated by whether the bettx vout that contains the costbasis utxo is spent or not.
 
 Once a bet goes into effect, then block by block, it is checked by the decentralized set of rekt scanners if it is rekt or not, in this case for double the reward for calculating the cost basis. This fully decentralized mechanism ensures that some node will decide to collect the free money and ensures that the bankroll is growing. To miss a rekt bet and not close it out when it can be is to allow it to survive for another block, which can change it profitability from negative to positive.
 
 Which comes to how profits are calculated. Once the costbasis is locked, then the profit calculation can be made based on the ratio between the costbasis and the current price, multiplied by the leverage. So, if a long position gains 10% at a 10x leverage, then approximately the entire bet amount will be made, ie. a double. Similarily with a short position, a 10% drop in price at 10x leverage will double the bet amount. Since it takes a full day to establish the costbasis, it is not possible to allow changing the costbasis for an existing bet, however it is possible to add funds so that it is less likely to be rekt. The sum of the initial bet and all added funds must be greater than the loss at every block to prevent a position from being rekt. To make it simple to calculate the amount of funds added, another bettx vout is used as a baton. Techniques similar to rogue CC to find where the bettx vout was spent and looking at each such transaction to find the total funds added can be used.
 
 The once that makes the bet is able to add funds or cashout at any block, as long as it isnt rekt. If it is rekt, the bettor is able to collect the rekt fee, so at least that is some consolation, but it is advised to increase the total funding to avoid being rekt. On a cashout all the funds that were bet adjusted by the profits can be collected.
 
 Hopefully the above description makes it clear what the following rpc calls should be doing:
 
 UniValue PricesBet(uint64_t txfee,int64_t amount,int16_t leverage,std::vector<std::string> synthetic)
     funds are locked into 1of2 global CC address
     for first day, long basis is MAX(correlated,smoothed), short is MIN()
     reference price is the smoothed of the previous block
     if synthetic value + amount goes negative, then anybody can rekt it to collect a rektfee, proof of rekt must be included to show cost basis, rekt price
     original creator can liquidate at anytime and collect (synthetic value + amount) from globalfund
     0.5% of bet -> globalfund

 UniValue PricesAddFunding(uint64_t txfee,uint256 bettxid,int64_t amount)
    add funding to an existing bet, doesnt change the profit calcs but does make the bet less likely to be rekt
 
 UniValue PricesSetcostbasis(uint64_t txfee,uint256 bettxid)
    in the first day from the bet, the costbasis can (and usually does) change based on the MAX(correlated,smoothed) for long and MIN() for shorts
    to calculate this requires a bit of work, so whatever node is first to get the proper calculation confirmed, gets a 0.1% costbasis fee
 
 UniValue PricesRekt(uint64_t txfee,uint256 bettxid,int32_t rektheight)
    similarily, any node can submit a rekt tx and cash in on 0.2% fee if their claim is confirmed
 
 UniValue PricesCashout(uint64_t txfee,uint256 bettxid)
    only the actually creator of bet is able to cashout and only if it isnt rekt at that moment
 
 UniValue PricesInfo(uint256 bettxid,int32_t refheight)
    all relevant info about a bet
 
 UniValue PricesList()
    a list of all pending and completed bets in different lists
 
 
 Appendix Synthetic position definition language:
 
 Let us start from the familiar BTCUSD nomenclature. this is similar (identical) to forex EURUSD and the equivalent. Notice the price needs two things as it is actually a ratio, ie. BTCUSD is how many USD does 1 BTC get converted to. It can be thought of as the value of BTC/USD, in other words a ratio.
 
 The value of BTC alone, or USD alone, it is actually quite an abstract issue and it can only be approximated. Specific ways of how to do this can be discussed later, for now it is only important to understand that all prices are actually ratios.
 
 And these ratios work as normal algebra does. So a/b * b/c == a/c! You can try this out with BTCUSD and USDJPY -> BTC/USD * USD/JPY -> BTC/JPY or the BTCJPY price
 
 division is the reciprocal, so BTCUSD reciprocated is USDBTC
 
 Without getting into all the possible combinations of things, let us first understand what the language allows. It uses a stack based language, where individual tokens update the state. The final state needs to have an empty stack and it will have a calculated price.
 
 The most important is pushing a specific price onto the stack. All the correlated and smoothed prices are normalized so it has each integer unit equal to 0.00000001, amounts above 100 million are above one, amounts less are below one. 100 million is the unit constant.
 
 In the prices CC synthetic definition language, the symbol that is returned pushes the adjusted price to the stack. The adjustment is selecting between the correlated and smoothed if within a day from the bet creation, or just the smoothed if after a day. You can have a maximum depth of 3, any more than that and it should return an error.
 
 This means there are operations that are possible on 1, 2 and 3 symbols. For 1 symbol, it is easy, the only direct operation is the inverse, which is represented by "!". All items in the language are separated by ","
 
 "BTCUSD, !"
 
 The above is the inverse or USD/BTC, which is another way to short BTCUSD. It is also possible to short the entire synthetic with a negative leverage. The exact results of a -1 leverage and inverse, might not be exact due to the math involved with calculating the profit, but it should generally be similar.
 
 For two symbols, there is a bit more we can do, namely multiply and divide, combined with inverting, however to simplify the language any inverting is required to be done by the ordering of the symbols and usage of multiply or divide. multiply is "*" and divide is "/" the top of the stack (last to be pushed) is the divisor, the one right before the divisor is the numerator.
 
 "BTCUSD, USDJPY, *" <- That will create a BTCJPY synthetic
 
 "BTCEUR, BTCUSD, /" <- That will create a USDEUR synthetic
 
 If you experiment around with this, you will see that given two symbols and the proper order and * or /, you can always create the synthetic that you want, assuming what you want is the cancelling out of the term in common with the two symbols and the resulting ratio is between the two unique terms.
 */

// Now we get to the three symbol operations, which there are 4 of *//, **/, *** and ///
 
/*
 these four operators work on the top of the stack in left to right order as the syntax of the definition lists it, though it is even possible to have the value from an earlier computation on the top of the stack. Ultimately all three will be multiplied together, so a * in a spot means it us used without changing. A / means its inverse will be used.
 
 "KMDBTC, BTCUSD, USDJPY, ***" <- this would create a KMDJPY synthetic. The various location of the / to make an inverse is to orient the raw symbol into the right orientation as the pricefeed is only getting one orientation of the ratio.

 So now we have covered all ways to map 1, 2 and 3 values on the top of the stack. A value can be on the top of the stack directly from a symbol, or as the result of some 1, 2 or 3 symbol operation. With a maximum stack depth of 3, it might require some experimentation to get a very complex synthetic to compile. Alternately, a stack deeper than 3 might be the acceptable solution  if there are a family of synthetics that need it.
 
 At this point, it is time to describe the weights. It turns out that all above examples are incomplete and the synthetic descriptions are all insufficient and should generate an error. The reason is that the actual synthetic price is the value of the accumulator, which adds up all the weighted prices. In order to assign a weight to a price value on the stack, you simply use a number that is less than 2048. 
 
 What such a weight number does, is consume the top of the stack, which also must be at depth of 1 and adds it to the accumulator. This allows combining multiple different synthetics into a meta synthetic, like for an aggregated index that is weighted by marketcap, or whatever other type of ratio trade that is desired.
 
 "BTCUSD, 1000, ETHBTC, BTCUSD, *, 300" -> that creates a dual index of BTCUSD and ETHUSD with a 30% ETH weight
 
 all weight operations consumes the one and only stack element and adds its weight to the accumulator, using this a very large number of terms can be all added together. Using inverses, allows to get the short direction into the equation mixed with longs, but all terms must be positive. If you want to create a spread between two synthetics, you need to create two different synthetics and go long with one and short with another.
 
 "BTCUSD, 1" with leverage -2 and "KMDBTC, BTCUSD, *, 1" with leverage 1 this will setup a +KMDUSD -2 BTCUSD spread when the two are combined, and would be at breakeven when KMDUSD gains 2x more percentage wise than BTC does. anytime KMD gains more, will be positive, if the gains are less, would be negative.
 
 Believe it or not, the string to binary compiler for synthetic description and interpretation of it is less than 200 lines of code.
 
 todo: complete all the above, bet tokens, cross chain prices within cluster These specific limits of 0.5%, 0.1%, 0.2%, 777 should be able to be changed based on #define
 
 Another configuration is to send the 0.4% (or 0.2%) fees to a fee collection scriptPubKey (this is not currently implemented, but would be needed for systems that dont use -ac_perc to collect a global override) this requires adding new vouts
 
 Modification: in the event there is one price in the accumulator and one price on the stack at the end, then it is a (A - B) spread
 
 Monetizations should be sent to: RGsWqwFviaNJSbmgWi6338NL2tKkY7ZqKL
 
 */


// start of consensus code

CScript prices_betopret(CPubKey mypk,int32_t height,int64_t amount,int16_t leverage,int64_t firstprice,std::vector<uint16_t> vec,uint256 tokenid)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'B' << mypk << height << amount << leverage << firstprice << vec << tokenid);
    return(opret);
}

uint8_t prices_betopretdecode(CScript scriptPubKey,CPubKey &pk,int32_t &height,int64_t &amount,int16_t &leverage,int64_t &firstprice,std::vector<uint16_t> &vec,uint256 &tokenid)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> pk; ss >> height; ss >> amount; ss >> leverage; ss >> firstprice; ss >> vec; ss >> tokenid) != 0 && e == EVAL_PRICES && f == 'B' )
    {
        return(f);
    }
    return(0);
}

CScript prices_addopret(uint256 bettxid,CPubKey mypk,int64_t amount)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'A' << bettxid << mypk << amount);
    return(opret);
}

uint8_t prices_addopretdecode(CScript scriptPubKey,uint256 &bettxid,CPubKey &pk,int64_t &amount)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> bettxid; ss >> pk; ss >> amount) != 0 && e == EVAL_PRICES && f == 'A' )
    {
        return(f);
    }
    return(0);
}

CScript prices_costbasisopret(uint256 bettxid,CPubKey mypk,int32_t height,int64_t costbasis)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'C' << bettxid << mypk << height << costbasis);
    return(opret);
}

uint8_t prices_costbasisopretdecode(CScript scriptPubKey,uint256 &bettxid,CPubKey &pk,int32_t &height,int64_t &costbasis)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> bettxid; ss >> pk; ss >> height; ss >> costbasis) != 0 && e == EVAL_PRICES && f == 'C' )
    {
        return(f);
    }
    return(0);
}

CScript prices_finalopret(uint256 bettxid,int64_t profits,int32_t height,CPubKey mypk,int64_t firstprice,int64_t costbasis,int64_t addedbets,int64_t positionsize,int16_t leverage)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'F' << bettxid << profits << height << mypk << firstprice << costbasis << addedbets << positionsize << leverage);
    return(opret);
}

uint8_t prices_finalopretdecode(CScript scriptPubKey,uint256 &bettxid,int64_t &profits,int32_t &height,CPubKey &pk,int64_t &firstprice,int64_t &costbasis,int64_t &addedbets,int64_t &positionsize,int16_t &leverage)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> bettxid; ss >> profits; ss >> height; ss >> pk; ss >> firstprice; ss >> costbasis; ss >> addedbets; ss >> positionsize; ss >> leverage) != 0 && e == EVAL_PRICES && f == 'F' )
    {
        return(f);
    }
    return(0);
}

bool PricesValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    return true;
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddPricesInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,char *destaddr,int64_t total,int32_t maxinputs,uint256 vintxid,int32_t vinvout)
{
    int64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs,destaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( vout == vinvout && txid == vintxid )
            continue;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 && vout < vintx.vout.size() )
        {
            if ( (nValue= vintx.vout[vout].nValue) >= total/maxinputs && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
            {
                if ( total != 0 && maxinputs != 0 )
                    mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                nValue = it->second.satoshis;
                totalinputs += nValue;
                n++;
                if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                    break;
            }
        }
    }
    return(totalinputs);
}

UniValue prices_rawtxresult(UniValue &result,std::string rawtx,int32_t broadcastflag)
{
    CTransaction tx;
    if ( rawtx.size() > 0 )
    {
        result.push_back(Pair("hex",rawtx));
        if ( DecodeHexTx(tx,rawtx) != 0 )
        {
            if ( broadcastflag != 0 && myAddtomempool(tx) != 0 )
                RelayTransaction(tx);
            result.push_back(Pair("txid",tx.GetHash().ToString()));
            result.push_back(Pair("result","success"));
        } else result.push_back(Pair("error","decode hex"));
    } else result.push_back(Pair("error","couldnt finalize CCtx"));
    return(result);
}

int32_t prices_syntheticvec(std::vector<uint16_t> &vec,std::vector<std::string> synthetic)
{
    int32_t i,need,ind,depth = 0; std::string opstr; uint16_t opcode,weight;
    if ( synthetic.size() == 0 )
        return(-1);
    for (i=0; i<synthetic.size(); i++)
    {
        need = 0;
        opstr = synthetic[i];
        if ( opstr == "*" )
            opcode = PRICES_MULT, need = 2;
        else if ( opstr == "/" )
            opcode = PRICES_DIV, need = 2;
        else if ( opstr == "!" )
            opcode = PRICES_INV, need = 1;
        else if ( opstr == "**/" )
            opcode = PRICES_MMD, need = 3;
        else if ( opstr == "*//" )
            opcode = PRICES_MDD, need = 3;
        else if ( opstr == "***" )
            opcode = PRICES_MMM, need = 3;
        else if ( opstr == "///" )
            opcode = PRICES_DDD, need = 3;
        else if ( (ind= komodo_priceind((char *)opstr.c_str())) >= 0 )
            opcode = ind, need = 0;
        else if ( (weight= atoi(opstr.c_str())) > 0 && weight < KOMODO_MAXPRICES )
        {
            opcode = PRICES_WEIGHT | weight;
            need = 1;
        } else return(-2);
        if ( depth < need )
            return(-3);
        depth -= need;
        if ( (opcode & KOMODO_PRICEMASK) != PRICES_WEIGHT ) // weight
            depth++;
        if ( depth > 3 )
            return(-4);
        vec.push_back(opcode);
    }
    if ( depth != 0 )
    {
        fprintf(stderr,"depth.%d not empty\n",depth);
        return(-5);
    }
    return(0);
}

int64_t prices_syntheticprice(std::vector<uint16_t> vec,int32_t height,int32_t minmax,int16_t leverage)
{
    int32_t i,ind,errcode,depth,retval = -1; uint16_t opcode; int64_t *pricedata,pricestack[4],price,den,a,b,c;
    pricedata = (int64_t *)calloc(sizeof(*pricedata)*3,1 + PRICES_DAYWINDOW*2 + PRICES_SMOOTHWIDTH);
    price = den = depth = errcode = 0;
    for (i=0; i<vec.size(); i++)
    {
        opcode = vec[i];
        ind = (opcode & (KOMODO_MAXPRICES-1));
        switch ( opcode & KOMODO_PRICEMASK )
        {
            case 0:
                pricestack[depth] = 0;
                if ( prices_extract(pricedata,height,1,ind) == 0 )
                {
                    if ( minmax == 0 )
                        pricestack[depth] = pricedata[2];
                    else
                    {
                        if ( leverage > 0 )
                            pricestack[depth] = (pricedata[1] > pricedata[2]) ? pricedata[1] : pricedata[2]; // MAX
                        else pricestack[depth] = (pricedata[1] < pricedata[2]) ? pricedata[1] : pricedata[2]; // MIN
                    }
                }
                if ( pricestack[depth] == 0 )
                    errcode = -1;
                depth++;
                break;
            case PRICES_WEIGHT: // multiply by weight and consume top of stack by updating price
                if ( depth == 1 )
                {
                    depth--;
                    price += pricestack[0] * ind;
                    den += ind;
                } else errcode = -2;
                break;
            case PRICES_MULT:
                if ( depth >= 2 )
                {
                    b = pricestack[--depth];
                    a = pricestack[--depth];
                    pricestack[depth++] = (a * b) / SATOSHIDEN;
                } else errcode = -3;
                break;
            case PRICES_DIV:
                if ( depth >= 2 )
                {
                    b = pricestack[--depth];
                    a = pricestack[--depth];
                    pricestack[depth++] = (a * SATOSHIDEN) / b;
                } else errcode = -4;
                break;
            case PRICES_INV:
                if ( depth >= 1 )
                {
                    a = pricestack[--depth];
                    pricestack[depth++] = (SATOSHIDEN * SATOSHIDEN) / a;
                } else errcode = -5;
                break;
            case PRICES_MDD:
                if ( depth >= 3 )
                {
                    c = pricestack[--depth];
                    b = pricestack[--depth];
                    a = pricestack[--depth];
                    pricestack[depth++] = (((a * SATOSHIDEN) / b) * SATOSHIDEN) / c;
                } else errcode = -6;
                break;
            case PRICES_MMD:
                if ( depth >= 3 )
                {
                    c = pricestack[--depth];
                    b = pricestack[--depth];
                    a = pricestack[--depth];
                    pricestack[depth++] = (a * b) / c;
                } else errcode = -7;
                break;
            case PRICES_MMM:
                if ( depth >= 3 )
                {
                    c = pricestack[--depth];
                    b = pricestack[--depth];
                    a = pricestack[--depth];
                    pricestack[depth++] = ((a * b) / SATOSHIDEN) * c;
               } else errcode = -8;
                break;
            case PRICES_DDD:
                if ( depth >= 3 )
                {
                    c = pricestack[--depth];
                    b = pricestack[--depth];
                    a = pricestack[--depth];
                    pricestack[depth++] = (((((SATOSHIDEN * SATOSHIDEN) / a) * SATOSHIDEN) / b) * SATOSHIDEN) / c;
                } else errcode = -9;
                break;
            default:
                errcode = -10;
                break;
        }
        if ( errcode != 0 )
            break;
    }
    free(pricedata);
    if ( den == 0 )
        return(-11);
    else if ( depth != 0 )
        return(-12);
    else if ( errcode != 0 )
        return(errcode);
    return(price / den);
}

int64_t prices_syntheticprofits(int64_t &costbasis,int32_t firstheight,int32_t height,int16_t leverage,std::vector<uint16_t> vec,int64_t positionsize,int64_t addedbets)
{
    int64_t price,profits = 0; int32_t minmax;
    minmax = (height > firstheight+PRICES_DAYWINDOW);
    if ( (price= prices_syntheticprice(vec,height,minmax,leverage)) < 0 )
    {
        fprintf(stderr,"unexpected zero synthetic price at height.%d\n",height);
        return(0);
    }
    if ( minmax != 0 )
    {
        if ( leverage > 0 && price > costbasis )
            costbasis = price;
        else if ( leverage < 0 && (costbasis == 0 || price < costbasis) )
            costbasis = price;
    }
    profits = ((price * SATOSHIDEN) / costbasis) - SATOSHIDEN;
    profits *= leverage * positionsize;
    return(positionsize + addedbets + profits);
}

void prices_betjson(UniValue &result,int64_t profits,int64_t costbasis,int64_t positionsize,int64_t addedbets,int16_t leverage,int32_t firstheight,int64_t firstprice)
{
    result.push_back(Pair("profits",ValueFromAmount(profits)));
    result.push_back(Pair("costbasis",ValueFromAmount(costbasis)));
    result.push_back(Pair("positionsize",ValueFromAmount(positionsize)));
    result.push_back(Pair("addedbets",ValueFromAmount(addedbets)));
    result.push_back(Pair("leverage",(int64_t)leverage));
    result.push_back(Pair("firstheight",(int64_t)firstheight));
    result.push_back(Pair("firstprice",ValueFromAmount(firstprice)));
}

int64_t prices_costbasis(CTransaction bettx)
{
    int64_t costbasis = 0;
    // if vout1 is spent, follow and extract costbasis from opreturn
    //uint8_t prices_costbasisopretdecode(CScript scriptPubKey,uint256 &bettxid,CPubKey &pk,int32_t &height,int64_t &costbasis)

    return(costbasis);
}

int64_t prices_batontxid(uint256 &batontxid,CTransaction bettx,uint256 bettxid)
{
    int64_t addedbets = 0;
    // iterate through batons, adding up vout1 -> addedbets
    return(addedbets);
}

UniValue PricesBet(uint64_t txfee,int64_t amount,int16_t leverage,std::vector<std::string> synthetic)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(),nextheight); UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp,C; CPubKey pricespk,mypk; int64_t betamount,firstprice; std::vector<uint16_t> vec; char myaddr[64]; std::string rawtx;
    if ( leverage > PRICES_MAXLEVERAGE || leverage < -PRICES_MAXLEVERAGE )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","leverage too big"));
        return(result);
    }
    cp = CCinit(&C,EVAL_PRICES);
    if ( txfee == 0 )
        txfee = PRICES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    GetCCaddress(cp,myaddr,mypk);
    if ( prices_syntheticvec(vec,synthetic) < 0 || (firstprice= prices_syntheticprice(vec,nextheight-1,1,leverage)) < 0 || vec.size() == 0 || vec.size() > 4096 )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","invalid synthetic"));
        return(result);
    }
    if ( AddNormalinputs(mtx,mypk,amount+4*txfee,64) >= amount+4*txfee )
    {
        betamount = (amount * 199) / 200;
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,mypk)); // baton for total funding
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,(amount-betamount)+2*txfee,pricespk));
        mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,betamount,pricespk,mypk));
        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,prices_betopret(mypk,nextheight-1,amount,leverage,firstprice,vec,zeroid));
        return(prices_rawtxresult(result,rawtx,0));
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","not enough funds"));
    return(result);
}

UniValue PricesAddFunding(uint64_t txfee,uint256 bettxid,int64_t amount)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(),nextheight); UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp,C; CTransaction bettx; CPubKey pricespk,mypk; int64_t addedbets=0,betamount,firstprice; std::vector<uint16_t> vec; uint256 batontxid; std::string rawtx; char myaddr[64];
    cp = CCinit(&C,EVAL_PRICES);
    if ( txfee == 0 )
        txfee = PRICES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    GetCCaddress(cp,myaddr,mypk);
    if ( AddNormalinputs(mtx,mypk,amount+txfee,64) >= amount+txfee )
    {
        if ( prices_batontxid(batontxid,bettx,bettxid) >= 0 )
        {
            mtx.vin.push_back(CTxIn(batontxid,0,CScript()));
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,mypk)); // baton for total funding
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,pricespk));
            rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,prices_addopret(bettxid,mypk,amount));
            return(prices_rawtxresult(result,rawtx,0));
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","couldnt find batonttxid"));
            return(result);
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","not enough funds"));
    return(result);
}

UniValue PricesSetcostbasis(uint64_t txfee,uint256 bettxid)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(),nextheight); UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp,C; CTransaction bettx; uint256 hashBlock,batontxid,tokenid; int64_t myfee,positionsize=0,addedbets,firstprice=0,profits=0,costbasis=0; int32_t i,firstheight=0,height,numvouts; int16_t leverage=0; std::vector<uint16_t> vec; CPubKey pk,mypk,pricespk; std::string rawtx;
    cp = CCinit(&C,EVAL_PRICES);
    if ( txfee == 0 )
        txfee = PRICES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    if ( myGetTransaction(bettxid,bettx,hashBlock) != 0 && (numvouts= bettx.vout.size()) > 3 )
    {
        if ( prices_betopretdecode(bettx.vout[numvouts-1].scriptPubKey,pk,firstheight,positionsize,leverage,firstprice,vec,tokenid) == 'B' )
        {
            addedbets = prices_batontxid(batontxid,bettx,bettxid);
            mtx.vin.push_back(CTxIn(bettxid,1,CScript()));
            for (i=0; i<PRICES_DAYWINDOW; i++)
            {
                if ( (profits= prices_syntheticprofits(costbasis,firstheight,firstheight+i,leverage,vec,positionsize,addedbets)) < 0 )
                {
                    result.push_back(Pair("rekt",(int64_t)1));
                    result.push_back(Pair("rektheight",(int64_t)firstheight+i));
                    break;
                }
            }
            if ( i == PRICES_DAYWINDOW )
                result.push_back(Pair("rekt",0));
            prices_betjson(result,profits,costbasis,positionsize,addedbets,leverage,firstheight,firstprice);
            myfee = bettx.vout[1].nValue / 10;
            result.push_back(Pair("myfee",myfee));
            mtx.vout.push_back(CTxOut(myfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,bettx.vout[1].nValue-myfee-txfee,pricespk));
            rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,prices_costbasisopret(bettxid,mypk,firstheight+PRICES_DAYWINDOW-1,costbasis));
            return(prices_rawtxresult(result,rawtx,0));
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","cant find bettxid"));
    return(result);
}

UniValue PricesRekt(uint64_t txfee,uint256 bettxid,int32_t rektheight)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(),nextheight); UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp,C; CTransaction bettx; uint256 hashBlock,tokenid,batontxid; int64_t myfee=0,positionsize,addedbets,firstprice,profits,ignore,costbasis=0; int32_t firstheight,numvouts; int16_t leverage; std::vector<uint16_t> vec; CPubKey pk,mypk,pricespk; std::string rawtx;
    cp = CCinit(&C,EVAL_PRICES);
    if ( txfee == 0 )
        txfee = PRICES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    if ( myGetTransaction(bettxid,bettx,hashBlock) != 0 && (numvouts= bettx.vout.size()) > 3 )
    {
        if ( prices_betopretdecode(bettx.vout[numvouts-1].scriptPubKey,pk,firstheight,positionsize,leverage,firstprice,vec,tokenid) == 'B' )
        {
            costbasis = prices_costbasis(bettx);
            addedbets = prices_batontxid(batontxid,bettx,bettxid);
            if ( (profits= prices_syntheticprofits(ignore,firstheight,rektheight,leverage,vec,positionsize,addedbets)) < 0 )
            {
                myfee = (positionsize + addedbets) / 500;
            }
            prices_betjson(result,profits,costbasis,positionsize,addedbets,leverage,firstheight,firstprice);
            if ( myfee != 0 )
            {
                mtx.vin.push_back(CTxIn(bettxid,2,CScript()));
                mtx.vout.push_back(CTxOut(myfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,bettx.vout[2].nValue-myfee-txfee,pricespk));
                rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,prices_finalopret(bettxid,profits,rektheight,mypk,firstprice,costbasis,addedbets,positionsize,leverage));
                return(prices_rawtxresult(result,rawtx,0));
            }
            else
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","position not rekt"));
                return(result);
            }
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","cant decode opret"));
            return(result);
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","cant find bettxid"));
    return(result);
}

UniValue PricesCashout(uint64_t txfee,uint256 bettxid)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(),nextheight); UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp,C; char destaddr[64]; CTransaction bettx; uint256 hashBlock,batontxid,tokenid; int64_t CCchange=0,positionsize,inputsum,ignore,addedbets,firstprice,profits,costbasis=0; int32_t i,firstheight,height,numvouts; int16_t leverage; std::vector<uint16_t> vec; CPubKey pk,mypk,pricespk; std::string rawtx;
    cp = CCinit(&C,EVAL_PRICES);
    if ( txfee == 0 )
        txfee = PRICES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    GetCCaddress(cp,destaddr,pricespk);
    if ( myGetTransaction(bettxid,bettx,hashBlock) != 0 && (numvouts= bettx.vout.size()) > 3 )
    {
        if ( prices_betopretdecode(bettx.vout[numvouts-1].scriptPubKey,pk,firstheight,positionsize,leverage,firstprice,vec,tokenid) == 'B' )
        {
            costbasis = prices_costbasis(bettx);
            addedbets = prices_batontxid(batontxid,bettx,bettxid);
            if ( (profits= prices_syntheticprofits(ignore,firstheight,nextheight-1,leverage,vec,positionsize,addedbets)) < 0 )
            {
                prices_betjson(result,profits,costbasis,positionsize,addedbets,leverage,firstheight,firstprice);
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","position rekt"));
                return(result);
            }
            prices_betjson(result,profits,costbasis,positionsize,addedbets,leverage,firstheight,firstprice);
            mtx.vin.push_back(CTxIn(bettxid,2,CScript()));
            if ( (inputsum= AddPricesInputs(cp,mtx,destaddr,profits+txfee,64,bettxid,2)) > profits+txfee )
                CCchange = (inputsum - profits);
            mtx.vout.push_back(CTxOut(bettx.vout[2].nValue + profits,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            if ( CCchange >= txfee )
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange,pricespk));
            rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,prices_finalopret(bettxid,profits,nextheight-1,mypk,firstprice,costbasis,addedbets,positionsize,leverage));
            return(prices_rawtxresult(result,rawtx,0));
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","cant decode opret"));
            return(result);
        }
    }
    return(result);
}

UniValue PricesInfo(uint256 bettxid,int32_t refheight)
{
    UniValue result(UniValue::VOBJ); CTransaction bettx; uint256 hashBlock,batontxid,tokenid; int64_t myfee,ignore,positionsize=0,addedbets=0,firstprice=0,profits=0,costbasis=0; int32_t i,firstheight=0,height,numvouts; int16_t leverage=0; std::vector<uint16_t> vec; CPubKey pk,mypk,pricespk; std::string rawtx;
    if ( myGetTransaction(bettxid,bettx,hashBlock) != 0 && (numvouts= bettx.vout.size()) > 3 )
    {
        if ( prices_betopretdecode(bettx.vout[numvouts-1].scriptPubKey,pk,firstheight,positionsize,leverage,firstprice,vec,tokenid) == 'B' )
        {
            costbasis = prices_costbasis(bettx);
            addedbets = prices_batontxid(batontxid,bettx,bettxid);
            if ( (profits= prices_syntheticprofits(ignore,firstheight,refheight,leverage,vec,positionsize,addedbets)) < 0 )
            {
                result.push_back(Pair("rekt",1));
                result.push_back(Pair("rektfee",(positionsize + addedbets) / 500));
            } else result.push_back(Pair("rekt",0));
            result.push_back(Pair("batontxid",batontxid.GetHex()));
            prices_betjson(result,profits,costbasis,positionsize,addedbets,leverage,firstheight,firstprice);
            result.push_back(Pair("height",(int64_t)refheight));
            return(result);
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","cant find bettxid"));
    return(result);
}

UniValue PricesList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; struct CCcontract_info *cp,C; int64_t amount,firstprice; int32_t height; int16_t leverage; uint256 txid,hashBlock,tokenid; CPubKey pk,pricespk; std::vector<uint16_t> vec; CTransaction vintx; char str[65];
    cp = CCinit(&C,EVAL_PRICES);
    pricespk = GetUnspendable(cp,0);
    SetCCtxids(addressIndex,cp->normaladdr,false);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( vintx.vout.size() > 0 && prices_betopretdecode(vintx.vout[vintx.vout.size()-1].scriptPubKey,pk,height,amount,leverage,firstprice,vec,tokenid) == 'B' )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}


