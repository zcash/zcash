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
 
 PricesBet -> +/-leverage, amount, synthetic -> opreturn includes current price
    funds are locked into 1of2 global CC address
    for first day, long basis is MAX(correlated,smoothed), short is MIN()
    reference price is the smoothed of the previous block
    if synthetic value + amount goes negative, then anybody can rekt it to collect a rektfee, proof of rekt must be included to show cost basis, rekt price
    original creator can liquidate at anytime and collect (synthetic value + amount) from globalfund
    0.5% of bet -> globalfund
  
 PricesStatus -> bettxid maxsamples returns initial params, cost basis, amount left, rekt:true/false, rektheight, initial synthetic price, current synthetic price, net gain
 
 PricesRekt -> bettxid height -> 0.1% to miner, rest to global CC
 
 PricesClose -> bettxid returns (synthetic value + amount)
 
 PricesList -> all bettxid -> list [bettxid, netgain]
 
 
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
    SetCCunspents(unspentOutputs,destaddr);
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

UniValue PricesList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; struct CCcontract_info *cp,C; int64_t amount,firstprice; int32_t height; int16_t leverage; uint256 txid,hashBlock,tokenid; CPubKey pk,pricespk; std::vector<uint16_t> vec; CTransaction vintx;
    cp = CCinit(&C,EVAL_PRICES);
    pricespk = GetUnspendable(cp,0);
    SetCCtxids(addressIndex,cp->normaladdr);
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
    int32_t i,need,depth = 0; std::string opstr; uint16_t opcode,weight;
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
        else if ( (ind= prices_ind(opstr.c_str())) >= 0 )
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
    int32_t i,errcode,depth,retval = -1; uint16_t opcode; int64_t *pricedata,stack[4],price,den,a,b,c;
    pricedata = (int64_t *)calloc(sizeof(*pricedata)*3,numblocks + PRICES_DAYWINDOW*2 + PRICES_SMOOTHWIDTH);
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
                            pricestack[depth] = MAX(pricedata[1],pricedata[2]);
                        else pricestack[depth] = MIN(pricedata[1],pricedata[2]);
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
            case PRICES_MUL:
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
    if ( (price= prices_syntheticprice(vec,height,minmax)) < 0 )
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
    return(positionsize + addedbets + profits)
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

int64_t prices_batontxid(uint256 &batontxid,CTransaction bettx,uint256 bettxid);
{
    int64_t addedbets = 0;
    // iterate through batons, adding up vout1 -> addedbets
    return(addedbets);
}

UniValue PricesBet(uint64_t txfee,int64_t amount,int16_t leverage,std::vector<std::string> synthetic)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(),nextheight); UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp,C; CPubKey pricespk,mypk; int64_t betamount,firstprice; std::vector<uint16_t> vec;
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
    if ( prices_syntheticvec(vec,synthetic) < 0 || (firstprice= prices_syntheticprice(vec,nextheight-1,1)) < 0 || vec.size() == 0 || vec.size() > 4096 )
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
    struct CCcontract_info *cp,C; CPubKey pricespk,mypk; int64_t addedbets=0,betamount,firstprice; std::vector<uint16_t> vec; uint256 batonttxid;
    cp = CCinit(&C,EVAL_PRICES);
    if ( txfee == 0 )
        txfee = PRICES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    GetCCaddress(cp,myaddr,mypk);
    if ( AddNormalinputs(mtx,mypk,amount+txfee,64) >= amount+txfee )
    {
        if ( prices_batontxid(addedbets,batontxid,bettxid) >= 0 )
        {
            mtx.vin.push_back(CTxVin(batontxid,0,CScript()));
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
    struct CCcontract_info *cp,C; CTransaction bettx; uint256 hashBlock,tokenid; int64_t myfee,positionsize=0,addedbets,firstprice=0,profits=0,costbasis=0; int32_t i,firstheight=0,height,numvouts; int16_t leverage=0; std::vector<uint16_t> vec; CPubKey pk,mypk,pricespk; std::string rawtx;
    cp = CCinit(&C,EVAL_PRICES);
    if ( txfee == 0 )
        txfee = PRICES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp,0);
    if ( myGetTransaction(bettxid,bettx,hashBlock) != 0 && (numvouts= bettx.vout.size()) > 3 )
    {
        if ( prices_betopretdecode(bettx.vout[numvouts-1].scriptPubKey,pk,firstheight,positionsize,leverage,firstprice,vec,tokenid) == 'B' )
        {
            addedbets = prices_addedbets(bettx);
            mtx.vin.push_back(CTxVin(bettx,1,CScript()));
            for (i=0; i<PRICES_DAYWINDOW; i++)
            {
                if ( (profits= prices_syntheticprofits(&costbasis,firstheight,firstheight+i,leverage,vec,positionsize,addedbets)) < 0 )
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
    struct CCcontract_info *cp,C; CTransaction bettx; uint256 hashBlock,tokenid; int64_t myfee=0,positionsize,addedbets,firstprice,profits,costbasis=0; int32_t firstheight,numvouts; int16_t leverage; std::vector<uint16_t> vec; CPubKey pk,mypk,pricespk; std::string rawtx;
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
            if ( (profits= prices_syntheticprofits(&ignore,firstheight,rektheight,leverage,vec,positionsize,addedbets)) < 0 )
            {
                myfee = (positionsize + addedbets) / 500;
            }
            prices_betjson(result,profits,costbasis,positionsize,addedbets,leverage,firstheight,firstprice);
            if ( myfee != 0 )
            {
                mtx.vin.push_back(CTxVin(bettxid,2,CScript()));
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
    struct CCcontract_info *cp,C; char destaddr[64]; CTransaction bettx; uint256 hashBlock,tokenid; int64_t CCchange=0,positionsize,addedbets,firstprice,profits,costbasis=0; int32_t i,firstheight,height,numvouts; int16_t leverage; std::vector<uint16_t> vec; CPubKey pk,mypk,pricespk; std::string rawtx;
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
            if ( (profits= prices_syntheticprofits(&ignore,firstheight,nextheight-1,leverage,vec,positionsize,addedbets)) < 0 )
            {
                prices_betjson(result,profits,costbasis,positionsize,addedbets,leverage,firstheight,firstprice);
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","position rekt"));
                return(result);
            }
            prices_betjson(result,profits,costbasis,positionsize,addedbets,leverage,firstheight,firstprice);
            mtx.vin.push_back(CTxVin(bettxid,2,CScript()));
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

UniValue PricesInfo(uint256 bettxid,int32_t height)
{
    UniValue result(UniValue::VOBJ); uint256 hashBlock,tokenid; int64_t myfee,ignore,positionsize=0,addedbets=0,firstprice=0,profits=0,costbasis=0; int32_t i,firstheight=0,height,numvouts; int16_t leverage=0; std::vector<uint16_t> vec; CPubKey pk,mypk,pricespk; std::string rawtx;
    if ( myGetTransaction(bettxid,bettx,hashBlock) != 0 && (numvouts= bettx.vout.size()) > 3 )
    {
        if ( prices_betopretdecode(bettx.vout[numvouts-1].scriptPubKey,pk,firstheight,positionsize,leverage,firstprice,vec,tokenid) == 'B' )
        {
            costbasis = prices_costbasis(bettx);
            addedbets = prices_batontxid(batontxid,bettx,bettxid);
            if ( (profits= prices_syntheticprofits(&ignore,firstheight,firstheight+i,leverage,vec,positionsize,addedbets)) < 0 )
            {
                result.push_back(Pair("rekt",1));
                result.push_back(Pair("rektfee",(positionsize + addedbets) / 500));
            } else result.push_back(Pair("rekt",0));
            result.push_back(Pair("batontxid",batontxid.GetHex()));
            prices_betjson(result,profits,costbasis,positionsize,addedbets,leverage,firstheight,firstprice);
            return(result);
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","cant find bettxid"));
    return(result);
}


