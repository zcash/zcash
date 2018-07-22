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

uint64_t AddAssetInputs(CMutableTransaction &mtx,CPubKey pk,uint256 assetid,uint64_t total,int32_t maxinputs)
{
    char coinaddr[64]; uint64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(EVAL_ASSETS,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    //std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( (nValue= IsAssetvout(price,origpubkey,vintx,(int32_t)it->first.index,assetid)) > 0 )
            {
                if ( total != 0 && maxinputs != 0 )
                    mtx.vin.push_back(CTxIn(txid,(int32_t)it->first.index,CScript()));
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

uint64_t GetAssetBalance(CPubKey pk,uint256 tokenid)
{
    CMutableTransaction mtx;
    return(AddAssetInputs(mtx,pk,tokenid,0,0));
}

UniValue AssetOrders(uint256 refassetid)
{
    uint64_t price; uint256 txid,hashBlock,assetid,assetid2; std::vector<uint8_t> origpubkey; CTransaction vintx; UniValue result(UniValue::VARR);  std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; uint8_t funcid; char funcidstr[16],origaddr[64],assetidstr[65];
    SetCCunspents(unspentOutputs,(char *)AssetsCCaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( (funcid= DecodeAssetOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,assetid,assetid2,price,origpubkey)) != 0 )
            {
                UniValue item(UniValue::VOBJ);
                funcidstr[0] = funcid;
                funcidstr[1] = 0;
                item.push_back(Pair("funcid", funcidstr));
                item.push_back(Pair("txid", uint256_str(assetidstr,txid)));
                item.push_back(Pair("vout", (int64_t)it->first.index));
                item.push_back(Pair("amount", (double)vintx.vout[it->first.index].nValue/COIN));
                if ( funcid == 'b' || funcid == 'B' )
                    item.push_back(Pair("bidamount",(double)vintx.vout[0].nValue/COIN));
                else item.push_back(Pair("askamount",(double)vintx.vout[0].nValue));
                if ( origpubkey.size() == 33 )
                {
                    GetCCaddress(EVAL_ASSETS,origaddr,pubkey2pk(origpubkey));
                    item.push_back(Pair("origaddress",origaddr));
                }
                if ( assetid != zeroid )
                    item.push_back(Pair("tokenid",uint256_str(assetidstr,assetid)));
                if ( assetid2 != zeroid )
                    item.push_back(Pair("otherid",uint256_str(assetidstr,assetid2)));
                if ( price > 0 )
                {
                    item.push_back(Pair("totalrequired", (int64_t)price));
                    item.push_back(Pair("price", (double)vintx.vout[0].nValue / (price * COIN)));
                }
                result.push_back(item);
                //fprintf(stderr,"func.(%c) %s/v%d %.8f\n",funcid,uint256_str(assetidstr,txid),(int32_t)it->first.index,(double)vintx.vout[it->first.index].nValue/COIN);
            }
        }
    }
    return(result);
}

std::string CreateAsset(uint64_t txfee,uint64_t assetsupply,std::string name,std::string description)
{
    CMutableTransaction mtx; CPubKey mypk;
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
        mtx.vout.push_back(MakeAssetsVout(assetsupply,mypk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(AssetsCChexstr) << OP_CHECKSIG));
        return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeAssetCreateOpRet('c',Mypubkey(),name,description)));
    }
    return(0);
}
               
std::string AssetTransfer(uint64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,uint64_t total)
{
    CMutableTransaction mtx; CPubKey mypk; uint64_t CCchange=0,inputs=0; //int32_t i,n;
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
            if ( (inputs= AddAssetInputs(mtx,mypk,assetid,total,60)) > 0 )
            {
                if ( inputs > total )
                    CCchange = (inputs - total);
                //for (i=0; i<n; i++)
                    mtx.vout.push_back(MakeAssetsVout(total,pubkey2pk(destpubkey)));
                if ( CCchange != 0 )
                    mtx.vout.push_back(MakeAssetsVout(CCchange,mypk));
                return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeAssetOpRet('t',assetid,zeroid,0,Mypubkey())));
            } else fprintf(stderr,"not enough CC asset inputs for %.8f\n",(double)total/COIN);
        //} else fprintf(stderr,"numoutputs.%d != numamounts.%d\n",n,(int32_t)amounts.size());
    }
    return(0);
}

std::string CreateBuyOffer(uint64_t txfee,uint64_t bidamount,uint256 assetid,uint64_t pricetotal)
{
    CMutableTransaction mtx; CPubKey mypk;
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,bidamount+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeAssetsVout(bidamount,GetUnspendable(EVAL_ASSETS,0)));
        return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeAssetOpRet('b',assetid,zeroid,pricetotal,Mypubkey())));
    }
    return(0);
}

std::string CreateSell(uint64_t txfee,uint64_t askamount,uint256 assetid,uint256 assetid2,uint64_t pricetotal)
{
    CMutableTransaction mtx; CPubKey mypk; uint64_t inputs,CCchange; CScript opret;
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        if ( (inputs= AddAssetInputs(mtx,mypk,assetid,askamount,60)) > 0 )
        {
            mtx.vout.push_back(MakeAssetsVout(askamount,GetUnspendable(EVAL_ASSETS,0)));
            if ( inputs > askamount )
                CCchange = (inputs - askamount);
            if ( CCchange != 0 )
                mtx.vout.push_back(MakeAssetsVout(CCchange,mypk));
            if ( assetid2 == zeroid )
                opret = EncodeAssetOpRet('s',assetid,zeroid,pricetotal,Mypubkey());
            else opret = EncodeAssetOpRet('e',assetid,assetid2,pricetotal,Mypubkey());
            return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,opret));
        }
    }
    return(0);
}

std::string CancelBuyOffer(uint64_t txfee,uint256 assetid,uint256 bidtxid)
{
    CMutableTransaction mtx; CTransaction vintx; uint256 hashBlock; uint64_t bidamount; CPubKey mypk;
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        if ( GetTransaction(bidtxid,vintx,hashBlock,false) != 0 )
        {
            bidamount = vintx.vout[0].nValue;
            mtx.vin.push_back(CTxIn(bidtxid,0,CScript()));
            mtx.vout.push_back(CTxOut(bidamount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeAssetOpRet('o',assetid,zeroid,0,Mypubkey())));
        }
    }
    return(0);
}

std::string CancelSell(uint64_t txfee,uint256 assetid,uint256 asktxid)
{
    CMutableTransaction mtx; CTransaction vintx; uint256 hashBlock; uint64_t askamount; CPubKey mypk;
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        if ( GetTransaction(asktxid,vintx,hashBlock,false) != 0 )
        {
            askamount = vintx.vout[0].nValue;
            mtx.vin.push_back(CTxIn(asktxid,0,CScript()));
            mtx.vout.push_back(MakeAssetsVout(askamount,mypk));
            return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeAssetOpRet('x',assetid,zeroid,0,Mypubkey())));
        }
    }
    return(0);
}

std::string FillBuyOffer(uint64_t txfee,uint256 assetid,uint256 bidtxid,uint64_t fillamount)
{
    CTransaction vintx; uint256 hashBlock; CMutableTransaction mtx; CPubKey mypk; std::vector<uint8_t> origpubkey; int32_t bidvout=0; uint64_t origprice,bidamount,paid_amount,remaining_required,inputs,CCchange=0;
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        if ( GetTransaction(bidtxid,vintx,hashBlock,false) != 0 )
        {
            bidamount = vintx.vout[bidvout].nValue;
            SetAssetOrigpubkey(origpubkey,origprice,vintx);
            mtx.vin.push_back(CTxIn(bidtxid,bidvout,CScript()));
            if ( (inputs= AddAssetInputs(mtx,mypk,assetid,fillamount,60)) > 0 )
            {
                if ( inputs > fillamount )
                    CCchange = (inputs - fillamount);
                SetAssetFillamounts(paid_amount,remaining_required,bidamount,fillamount,origprice);
                mtx.vout.push_back(MakeAssetsVout(bidamount - paid_amount,GetUnspendable(EVAL_ASSETS,0)));
                mtx.vout.push_back(CTxOut(paid_amount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                mtx.vout.push_back(MakeAssetsVout(fillamount,pubkey2pk(origpubkey)));
                if ( CCchange != 0 )
                    mtx.vout.push_back(MakeAssetsVout(CCchange,mypk));
                fprintf(stderr,"remaining %llu -> origpubkey\n",(long long)remaining_required);
                return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeAssetOpRet('B',assetid,zeroid,remaining_required,origpubkey)));
            } else fprintf(stderr,"filltx wasnt for assetid\n");
        }
    }
    return(0);
}

std::string FillSell(uint64_t txfee,uint256 assetid,uint256 assetid2,uint256 asktxid,uint64_t fillamount)
{
    CTransaction vintx,filltx; uint256 hashBlock; CMutableTransaction mtx; CPubKey mypk; std::vector<uint8_t> origpubkey; int32_t askvout=0; uint64_t origprice,askamount,paid_amount,remaining_required,inputs,CCchange=0;
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        if ( GetTransaction(asktxid,vintx,hashBlock,false) != 0 )
        {
            askamount = vintx.vout[askvout].nValue;
            SetAssetOrigpubkey(origpubkey,origprice,vintx);
            mtx.vin.push_back(CTxIn(asktxid,askvout,CScript()));
            if ( assetid2 == zeroid )
                inputs = AddAssetInputs(mtx,mypk,assetid2,fillamount,60);
            else inputs = AddNormalinputs(mtx,mypk,fillamount,60);
            if ( inputs > 0 )
            {
                if ( assetid2 == zeroid && inputs > fillamount )
                    CCchange = (inputs - fillamount);
                SetAssetFillamounts(paid_amount,remaining_required,askamount,fillamount,origprice);
                mtx.vout.push_back(MakeAssetsVout(askamount - paid_amount,GetUnspendable(EVAL_ASSETS,0)));
                mtx.vout.push_back(MakeAssetsVout(paid_amount,mypk));
                mtx.vout.push_back(MakeAssetsVout(fillamount,pubkey2pk(origpubkey)));
                if ( CCchange != 0 )
                    mtx.vout.push_back(MakeAssetsVout(CCchange,mypk));
                fprintf(stderr,"remaining %llu -> origpubkey\n",(long long)remaining_required);
                return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeAssetOpRet(assetid2==zeroid?'E':'S',assetid,assetid2,remaining_required,origpubkey)));
            } else fprintf(stderr,"filltx not enough utxos\n");
        }
    }
    return(0);
}
