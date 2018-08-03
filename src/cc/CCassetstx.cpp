/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
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

// need allassets
// find asset

int64_t AddAssetInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint256 assetid,int64_t total,int32_t maxinputs)
{
    char coinaddr[64]; int64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t j,vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        for (j=0; j<mtx.vin.size(); j++)
            if ( txid == mtx.vin[j].prevout.hash && vout == mtx.vin[j].prevout.n )
                break;
        if ( j != mtx.vin.size() )
            continue;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( (nValue= IsAssetvout(price,origpubkey,vintx,vout,assetid)) > 0 )
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

int64_t GetAssetBalance(CPubKey pk,uint256 tokenid)
{
    CMutableTransaction mtx; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_ASSETS);
    return(AddAssetInputs(cp,mtx,pk,tokenid,0,0));
}

UniValue AssetInfo(uint256 assetid)
{
    UniValue result(UniValue::VOBJ); uint256 hashBlock; CTransaction vintx; std::vector<uint8_t> origpubkey; std::string name,description; char str[67],numstr[65];
    if ( GetTransaction(assetid,vintx,hashBlock,false) == 0 )
    {
        fprintf(stderr,"cant find assetid\n");
        result.push_back(Pair("error","cant find assetid"));
        return(0);
    }
    if ( vintx.vout.size() > 0 && DecodeAssetCreateOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,origpubkey,name,description) == 0 )
    {
        fprintf(stderr,"assetid isnt assetcreation txid\n");
        result.push_back(Pair("error","assetid isnt assetcreation txid"));
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("tokenid",uint256_str(str,assetid)));
    result.push_back(Pair("owner",pubkey33_str(str,origpubkey.data())));
    result.push_back(Pair("name",name));
    sprintf(numstr,"%.8f",(double)vintx.vout[0].nValue/COIN);
    result.push_back(Pair("supply",numstr));
    result.push_back(Pair("description",description));
    return(result);
}

UniValue AssetList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; struct CCcontract_info *cp,C; uint256 txid,hashBlock; CTransaction vintx; std::vector<uint8_t> origpubkey; std::string name,description; char str[65];
    cp = CCinit(&C,EVAL_ASSETS);
    SetCCtxids(addressIndex,cp->normaladdr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( vintx.vout.size() > 0 && DecodeAssetCreateOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,origpubkey,name,description) != 0 )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}

UniValue AssetOrders(uint256 refassetid)
{
    static uint256 zero;
    int64_t price; uint256 txid,hashBlock,assetid,assetid2; std::vector<uint8_t> origpubkey; CTransaction vintx; UniValue result(UniValue::VARR);  std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; uint8_t funcid; char numstr[32],funcidstr[16],origaddr[64],assetidstr[65]; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_ASSETS);
    SetCCunspents(unspentOutputs,(char *)cp->unspendableCCaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( vintx.vout.size() > 0 && (funcid= DecodeAssetOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,assetid,assetid2,price,origpubkey)) != 0 )
            {
                if ( refassetid != zero && assetid != refassetid )
                {
                    //int32_t z;
                    //for (z=31; z>=0; z--) fprintf(stderr,"%02x",((uint8_t *)&txid)[z]);
                    //fprintf(stderr," txid\n");
                    //for (z=31; z>=0; z--) fprintf(stderr,"%02x",((uint8_t *)&assetid)[z]);
                    //fprintf(stderr," assetid\n");
                    //for (z=31; z>=0; z--) fprintf(stderr,"%02x",((uint8_t *)&refassetid)[z]);
                    //fprintf(stderr," refassetid\n");
                    continue;
                }
                if ( vintx.vout[it->first.index].nValue == 0 )
                    continue;
                UniValue item(UniValue::VOBJ);
                funcidstr[0] = funcid;
                funcidstr[1] = 0;
                item.push_back(Pair("funcid", funcidstr));
                item.push_back(Pair("txid", uint256_str(assetidstr,txid)));
                item.push_back(Pair("vout", (int64_t)it->first.index));
                if ( funcid == 'b' || funcid == 'B' )
                {
                    sprintf(numstr,"%.8f",(double)vintx.vout[it->first.index].nValue/COIN);
                    item.push_back(Pair("amount",numstr));
                    sprintf(numstr,"%.8f",(double)vintx.vout[0].nValue/COIN);
                    item.push_back(Pair("bidamount",numstr));
                }
                else
                {
                    sprintf(numstr,"%llu",(long long)vintx.vout[it->first.index].nValue);
                    item.push_back(Pair("amount",numstr));
                    sprintf(numstr,"%llu",(long long)vintx.vout[0].nValue);
                    item.push_back(Pair("askamount",numstr));
                }
                if ( origpubkey.size() == 33 )
                {
                    GetCCaddress(cp,origaddr,pubkey2pk(origpubkey));
                    item.push_back(Pair("origaddress",origaddr));
                }
                if ( assetid != zeroid )
                    item.push_back(Pair("tokenid",uint256_str(assetidstr,assetid)));
                if ( assetid2 != zeroid )
                    item.push_back(Pair("otherid",uint256_str(assetidstr,assetid2)));
                if ( price > 0 )
                {
                    if ( funcid == 's' || funcid == 'S' || funcid == 'e' || funcid == 'e' )
                    {
                        sprintf(numstr,"%.8f",(double)price / COIN);
                        item.push_back(Pair("totalrequired", numstr));
                        sprintf(numstr,"%.8f",(double)price / (COIN * vintx.vout[0].nValue));
                        item.push_back(Pair("price", numstr));
                    }
                    else
                    {
                        item.push_back(Pair("totalrequired", (int64_t)price));
                        sprintf(numstr,"%.8f",(double)vintx.vout[0].nValue / (price * COIN));
                        item.push_back(Pair("price",numstr));
                    }
                }
                result.push_back(item);
                //fprintf(stderr,"func.(%c) %s/v%d %.8f\n",funcid,uint256_str(assetidstr,txid),(int32_t)it->first.index,(double)vintx.vout[it->first.index].nValue/COIN);
            }
        }
    }
    return(result);
}

std::string CreateAsset(int64_t txfee,int64_t assetsupply,std::string name,std::string description)
{
    CMutableTransaction mtx; CPubKey mypk; struct CCcontract_info *cp,C;
    if ( assetsupply < 0 )
    {
        fprintf(stderr,"negative assetsupply %lld\n",(long long)assetsupply);
        return(0);
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( name.size() > 32 || description.size() > 4096 )
    {
        fprintf(stderr,"name.%d or description.%d is too big\n",(int32_t)name.size(),(int32_t)description.size());
        return(0);
    }
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,assetsupply+2*txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,assetsupply,mypk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(cp->CChexstr) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeAssetCreateOpRet('c',Mypubkey(),name,description)));
    }
    return(0);
}
               
std::string AssetTransfer(int64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,int64_t total)
{
    CMutableTransaction mtx; CPubKey mypk; uint64_t mask; int64_t CCchange=0,inputs=0;  struct CCcontract_info *cp,C;
    if ( total < 0 )
    {
        fprintf(stderr,"negative total %lld\n",(long long)total);
        return(0);
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        /*n = outputs.size();
        if ( n == amounts.size() )
        {
            for (i=0; i<n; i++)
                total += amounts[i];*/
        mask = ~((1LL << mtx.vin.size()) - 1);
            if ( (inputs= AddAssetInputs(cp,mtx,mypk,assetid,total,60)) > 0 )
            {
                if ( inputs > total )
                    CCchange = (inputs - total);
                //for (i=0; i<n; i++)
                    mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,total,pubkey2pk(destpubkey)));
                if ( CCchange != 0 )
                    mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,mypk));
                return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,EncodeAssetOpRet('t',assetid,zeroid,0,Mypubkey())));
            } else fprintf(stderr,"not enough CC asset inputs for %.8f\n",(double)total/COIN);
        //} else fprintf(stderr,"numoutputs.%d != numamounts.%d\n",n,(int32_t)amounts.size());
    }
    return(0);
}

std::string CreateBuyOffer(int64_t txfee,int64_t bidamount,uint256 assetid,int64_t pricetotal)
{
    CMutableTransaction mtx; CPubKey mypk; struct CCcontract_info *cp,C; uint256 hashBlock; CTransaction vintx; std::vector<uint8_t> origpubkey; std::string name,description;
    if ( bidamount < 0 || pricetotal < 0 )
    {
        fprintf(stderr,"negative bidamount %lld, pricetotal %lld\n",(long long)bidamount,(long long)pricetotal);
        return(0);
    }
    if ( GetTransaction(assetid,vintx,hashBlock,false) == 0 )
    {
        fprintf(stderr,"cant find assetid\n");
        return(0);
    }
    if ( vintx.vout.size() > 0 && DecodeAssetCreateOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,origpubkey,name,description) == 0 )
    {
        fprintf(stderr,"assetid isnt assetcreation txid\n");
        return(0);
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,bidamount+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,bidamount,GetUnspendable(cp,0)));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeAssetOpRet('b',assetid,zeroid,pricetotal,Mypubkey())));
    }
    return(0);
}

std::string CreateSell(int64_t txfee,int64_t askamount,uint256 assetid,int64_t pricetotal)
{
    CMutableTransaction mtx; CPubKey mypk; uint64_t mask; int64_t inputs,CCchange; CScript opret; struct CCcontract_info *cp,C;
    if ( askamount < 0 || pricetotal < 0 )
    {
        fprintf(stderr,"negative askamount %lld, askamount %lld\n",(long long)pricetotal,(long long)askamount);
        return(0);
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if ( (inputs= AddAssetInputs(cp,mtx,mypk,assetid,askamount,60)) > 0 )
        {
            if ( inputs < askamount )
                askamount = inputs;
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,askamount,GetUnspendable(cp,0)));
            if ( inputs > askamount )
                CCchange = (inputs - askamount);
            if ( CCchange != 0 )
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,mypk));
            opret = EncodeAssetOpRet('s',assetid,zeroid,pricetotal,Mypubkey());
            return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,opret));
        } else fprintf(stderr,"need some assets to place ask\n");
    }
    fprintf(stderr,"need some native coins to place ask\n");
    return(0);
}

std::string CreateSwap(int64_t txfee,int64_t askamount,uint256 assetid,uint256 assetid2,int64_t pricetotal)
{
    CMutableTransaction mtx; CPubKey mypk; uint64_t mask; int64_t inputs,CCchange; CScript opret; struct CCcontract_info *cp,C;
    fprintf(stderr,"asset swaps disabled\n");
    return(0);
    if ( askamount < 0 || pricetotal < 0 )
    {
        fprintf(stderr,"negative askamount %lld, askamount %lld\n",(long long)pricetotal,(long long)askamount);
        return(0);
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if ( (inputs= AddAssetInputs(cp,mtx,mypk,assetid,askamount,60)) > 0 )
        {
            if ( inputs < askamount )
                askamount = inputs;
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,askamount,GetUnspendable(cp,0)));
            if ( inputs > askamount )
                CCchange = (inputs - askamount);
            if ( CCchange != 0 )
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,mypk));
            if ( assetid2 == zeroid )
                opret = EncodeAssetOpRet('s',assetid,zeroid,pricetotal,Mypubkey());
            else
            {
                opret = EncodeAssetOpRet('e',assetid,assetid2,pricetotal,Mypubkey());
            }
            return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,opret));
        } else fprintf(stderr,"need some assets to place ask\n");
    }
    fprintf(stderr,"need some native coins to place ask\n");
    return(0);
}

std::string CancelBuyOffer(int64_t txfee,uint256 assetid,uint256 bidtxid)
{
    CMutableTransaction mtx; CTransaction vintx; uint64_t mask; uint256 hashBlock; int64_t bidamount; CPubKey mypk; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if ( GetTransaction(bidtxid,vintx,hashBlock,false) != 0 )
        {
            bidamount = vintx.vout[0].nValue;
            mtx.vin.push_back(CTxIn(bidtxid,0,CScript()));
            mtx.vout.push_back(CTxOut(bidamount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,EncodeAssetOpRet('o',assetid,zeroid,0,Mypubkey())));
        }
    }
    return(0);
}

std::string CancelSell(int64_t txfee,uint256 assetid,uint256 asktxid)
{
    CMutableTransaction mtx; CTransaction vintx; uint64_t mask; uint256 hashBlock; int64_t askamount; CPubKey mypk; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if ( GetTransaction(asktxid,vintx,hashBlock,false) != 0 )
        {
            askamount = vintx.vout[0].nValue;
            mtx.vin.push_back(CTxIn(asktxid,0,CScript()));
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,askamount,mypk));
            return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,EncodeAssetOpRet('x',assetid,zeroid,0,Mypubkey())));
        }
    }
    return(0);
}

std::string FillBuyOffer(int64_t txfee,uint256 assetid,uint256 bidtxid,int64_t fillamount)
{
    CTransaction vintx; uint256 hashBlock; CMutableTransaction mtx; CPubKey mypk; std::vector<uint8_t> origpubkey; int32_t bidvout=0; uint64_t mask; int64_t origprice,bidamount,paid_amount,remaining_required,inputs,CCchange=0; struct CCcontract_info *cp,C;
    if ( fillamount < 0 )
    {
        fprintf(stderr,"negative fillamount %lld\n",(long long)fillamount);
        return(0);
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if ( GetTransaction(bidtxid,vintx,hashBlock,false) != 0 )
        {
            bidamount = vintx.vout[bidvout].nValue;
            SetAssetOrigpubkey(origpubkey,origprice,vintx);
            mtx.vin.push_back(CTxIn(bidtxid,bidvout,CScript()));
            if ( (inputs= AddAssetInputs(cp,mtx,mypk,assetid,fillamount,60)) > 0 )
            {
                if ( inputs < fillamount )
                    fillamount = inputs;
                SetBidFillamounts(paid_amount,remaining_required,bidamount,fillamount,origprice);
                if ( inputs > fillamount )
                    CCchange = (inputs - fillamount);
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,bidamount - paid_amount,GetUnspendable(cp,0)));
                mtx.vout.push_back(CTxOut(paid_amount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,fillamount,pubkey2pk(origpubkey)));
                if ( CCchange != 0 )
                    mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,mypk));
                fprintf(stderr,"remaining %llu -> origpubkey\n",(long long)remaining_required);
                return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,EncodeAssetOpRet('B',assetid,zeroid,remaining_required,origpubkey)));
            } else return("dont have any assets to fill bid\n");
        }
    }
    return("no normal coins left");
}

std::string FillSell(int64_t txfee,uint256 assetid,uint256 assetid2,uint256 asktxid,int64_t fillunits)
{
    CTransaction vintx,filltx; uint256 hashBlock; CMutableTransaction mtx; CPubKey mypk; std::vector<uint8_t> origpubkey; double dprice; uint64_t mask; int32_t askvout=0; int64_t received_assetoshis,total_nValue,orig_assetoshis,paid_nValue,remaining_nValue,inputs,CCchange=0; struct CCcontract_info *cp,C;
    if ( fillunits < 0 )
    {
        fprintf(stderr,"negative fillunits %lld\n",(long long)fillunits);
        return(0);
    }
    if ( assetid2 != zeroid )
    {
        fprintf(stderr,"asset swaps disabled\n");
        return(0);
    }

    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if ( GetTransaction(asktxid,vintx,hashBlock,false) != 0 )
        {
            orig_assetoshis = vintx.vout[askvout].nValue;
            SetAssetOrigpubkey(origpubkey,total_nValue,vintx);
            dprice = (double)total_nValue / orig_assetoshis;
            paid_nValue = dprice * fillunits;
            mtx.vin.push_back(CTxIn(asktxid,askvout,CScript()));
            if ( assetid2 != zeroid )
                inputs = AddAssetInputs(cp,mtx,mypk,assetid2,paid_nValue,60);
            else
            {
                inputs = AddNormalinputs(mtx,mypk,paid_nValue,60);
                mask = ~((1LL << mtx.vin.size()) - 1);
            }
            if ( inputs > 0 )
            {
                if ( inputs < paid_nValue )
                    paid_nValue = inputs;
                if ( assetid2 != zeroid )
                    SetSwapFillamounts(received_assetoshis,remaining_nValue,orig_assetoshis,paid_nValue,total_nValue);
                else SetAskFillamounts(received_assetoshis,remaining_nValue,orig_assetoshis,paid_nValue,total_nValue);
                if ( assetid2 != zeroid && inputs > paid_nValue )
                    CCchange = (inputs - paid_nValue);
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,orig_assetoshis - received_assetoshis,GetUnspendable(cp,0)));
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,received_assetoshis,mypk));
                if ( assetid2 != zeroid )
                    mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,paid_nValue,origpubkey));
                else mtx.vout.push_back(CTxOut(paid_nValue,CScript() << origpubkey << OP_CHECKSIG));
                if ( CCchange != 0 )
                    mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,mypk));
                return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,EncodeAssetOpRet(assetid2!=zeroid?'E':'S',assetid,assetid2,remaining_nValue,origpubkey)));
            } else fprintf(stderr,"filltx not enough utxos\n");
        }
    }
    return(0);
}
