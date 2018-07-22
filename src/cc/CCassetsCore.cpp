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

CC *MakeAssetCond(CPubKey pk)
{
    std::vector<CC*> pks; uint8_t evalcode = EVAL_ASSETS;
    pks.push_back(CCNewSecp256k1(pk));
    CC *assetCC = CCNewEval(E_MARSHAL(ss << evalcode));
    CC *Sig = CCNewThreshold(1, pks);
    return CCNewThreshold(2, {assetCC, Sig});
}

CTxOut MakeAssetsVout(CAmount nValue,CPubKey pk)
{
    CTxOut vout;
    CC *payoutCond = MakeAssetCond(pk);
    vout = CTxOut(nValue,CCPubKey(payoutCond));
    cc_free(payoutCond);
    return(vout);
}

CScript EncodeAssetCreateOpRet(uint8_t funcid,std::vector<uint8_t> origpubkey,std::string name,std::string description)
{
    CScript opret; uint8_t evalcode = EVAL_ASSETS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << origpubkey << name << description);
    return(opret);
}

CScript EncodeAssetOpRet(uint8_t funcid,uint256 assetid,uint256 assetid2,uint64_t price,std::vector<uint8_t> origpubkey)
{
    CScript opret; uint8_t evalcode = EVAL_ASSETS;
    assetid = revuint256(assetid);
    switch ( funcid )
    {
        case 't':  case 'x': case 'o':
            opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << assetid);
            break;
        case 's': case 'b': case 'S': case 'B':
            opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << assetid << price << origpubkey);
            break;
        case 'E': case 'e':
            assetid2 = revuint256(assetid2);
            opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << assetid << assetid2 << price << origpubkey);
            break;
        default:
            fprintf(stderr,"EncodeOpRet: illegal funcid.%02x\n",funcid);
            opret << OP_RETURN;
            break;
    }
    return(opret);
}

uint8_t DecodeAssetOpRet(const CScript &scriptPubKey,uint256 &assetid,uint256 &assetid2,uint64_t &price,std::vector<uint8_t> &origpubkey)
{
    std::vector<uint8_t> vopret; uint8_t funcid=0,*script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    memset(&assetid,0,sizeof(assetid));
    memset(&assetid2,0,sizeof(assetid2));
    price = 0;
    if ( script[0] == EVAL_ASSETS )
    {
        funcid = script[1];
        //fprintf(stderr,"decode.[%c]\n",funcid);
        switch ( funcid )
        {
            case 'c': return(funcid);
                break;
            case 't':  case 'x': case 'o':
                if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> assetid) != 0 )
                {
                    assetid = revuint256(assetid);
                    return(funcid);
                }
                break;
            case 's': case 'b': case 'S': case 'B':
                if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> assetid; ss >> price; ss >> origpubkey) != 0 )
                {
                    assetid = revuint256(assetid);
                    //fprintf(stderr,"got price %llu\n",(long long)price);
                    return(funcid);
                }
                break;
            case 'E': case 'e':
                if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> assetid; ss >> assetid2; ss >> price; ss >> origpubkey) != 0 )
                {
                    fprintf(stderr,"got price %llu\n",(long long)price);
                    assetid = revuint256(assetid);
                    assetid2 = revuint256(assetid2);
                    return(funcid);
                }
                break;
            default:
                fprintf(stderr,"DecodeAssetOpRet: illegal funcid.%02x\n",funcid);
                funcid = 0;
                break;
        }
    }
    return(funcid);
}

bool SetAssetOrigpubkey(std::vector<uint8_t> &origpubkey,uint64_t &price,CTransaction &tx)
{
    uint256 assetid,assetid2;
    if ( DecodeAssetOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,assetid,assetid2,price,origpubkey) != 0 )
        return(true);
    else return(false);
}
           
bool GetAssetorigaddrs(char *CCaddr,char *destaddr,CTransaction& tx)
{
    uint256 assetid,assetid2; uint64_t price,nValue=0; int32_t n; uint8_t funcid; std::vector<uint8_t> origpubkey; CScript script;
    n = tx.vout.size();
    if ( n == 0 || (funcid= DecodeAssetOpRet(tx.vout[n-1].scriptPubKey,assetid,assetid2,price,origpubkey)) == 0 )
        return(false);
    if ( GetCCaddress(EVAL_ASSETS,destaddr,pubkey2pk(origpubkey)) != 0 && Getscriptaddress(destaddr,CScript() << origpubkey << OP_CHECKSIG) != 0 )
        return(true);
    else return(false);
}

uint64_t IsAssetvout(uint64_t &price,std::vector<uint8_t> &origpubkey,CTransaction& tx,int32_t v,uint256 refassetid)
{
    uint256 assetid,assetid2; uint64_t nValue=0; int32_t n; uint8_t funcid;
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        n = tx.vout.size();
        nValue = tx.vout[v].nValue;
        //fprintf(stderr,"CC vout v.%d of n.%d %.8f\n",v,n,(double)nValue/COIN);
        if ( v >= n-1 )
            return(0);
        if ( (funcid= DecodeAssetOpRet(tx.vout[n-1].scriptPubKey,assetid,assetid2,price,origpubkey)) == 0 )
        {
            fprintf(stderr,"null decodeopret\n");
            return(0);
        }
        else if ( funcid == 'c' )
        {
            if ( refassetid == tx.GetHash() && v == 0 )
                return(nValue);
        }
        else if ( funcid == 'b' || funcid == 'B' )
            return(0);
        else if ( funcid != 'E' )
        {
            if ( assetid == refassetid )
                return(nValue);
        }
        else if ( funcid == 'E' )
        {
            if ( v < 2 && assetid == refassetid )
                return(nValue);
            else if ( v == 2 && assetid2 == refassetid )
                return(nValue);
        }
    }
    //fprintf(stderr,"Isassetvout: normal output v.%d %.8f\n",v,(double)tx.vout[v].nValue/COIN);
    return(0);
}

bool ValidateAssetRemainder(uint64_t remaining_price,uint64_t remaining_nValue,uint64_t orig_nValue,uint64_t received,uint64_t paid,uint64_t totalprice)
{
    uint64_t price,recvprice;
    if ( orig_nValue == 0 || received == 0 || paid == 0 || totalprice == 0 )
    {
        fprintf(stderr,"ValidateAssetRemainder: orig_nValue == %llu || received == %llu || paid == %llu || totalprice == %llu\n",(long long)orig_nValue,(long long)received,(long long)paid,(long long)totalprice);
        return(false);
    }
    else if ( remaining_price < (totalprice - received) )
    {
        fprintf(stderr,"ValidateAssetRemainder: remaining_price %llu < %llu (totalprice %llu - %llu received)\n",(long long)remaining_price,(long long)(totalprice - received),(long long)totalprice,(long long)received);
        return(false);
    }
    else if ( remaining_nValue < (orig_nValue - paid) )
    {
        fprintf(stderr,"ValidateAssetRemainder: remaining_nValue %llu < %llu (totalprice %llu - %llu received)\n",(long long)remaining_nValue,(long long)(orig_nValue - paid),(long long)orig_nValue,(long long)paid);
        return(false);
    }
    else if ( remaining_nValue > 0 )
    {
        price = (totalprice * COIN) / orig_nValue;
        recvprice = (received * COIN) / paid;
        if ( recvprice < price )
        {
            fprintf(stderr,"recvprice %llu < %llu price\n",(long long)recvprice,(long long)price);
            return(false);
        }
    }
    return(true);
}

bool SetAssetFillamounts(uint64_t &paid,uint64_t &remaining_price,uint64_t orig_nValue,uint64_t &received,uint64_t totalprice)
{
    uint64_t remaining_nValue,price,mult;
    if ( received >= totalprice )
        received = totalprice;
    remaining_price = (totalprice - received);
    price = (totalprice * COIN) / orig_nValue;
    mult = (received * COIN);
    fprintf(stderr,"remaining %llu price %llu, mult %llu, totalprice %llu, received %llu, paid %llu\n",(long long)remaining_price,(long long)price,(long long)mult,(long long)totalprice,(long long)received,(long long)mult / price);
    if ( price > 0 && (paid= mult / price) > 0 )
    {
        if ( (mult % price) != 0 )
            paid--;
        remaining_nValue = (orig_nValue - paid);
        return(ValidateAssetRemainder(remaining_price,remaining_nValue,orig_nValue,received,paid,totalprice));
    } else return(false);
}

uint64_t AssetValidateCCvin(Eval* eval,char *CCaddr,char *origaddr,CTransaction &tx,CTransaction &vinTx)
{
    uint256 hashBlock; char destaddr[64];
    origaddr[0] = destaddr[0] = 0;
    if ( tx.vin.size() < 2 )
        return eval->Invalid("not enough for CC vins");
    else if ( tx.vin[1].prevout.n != 0 )
        return eval->Invalid("vin1 needs to be buyvin.vout[0]");
    else if ( eval->GetTxUnconfirmed(tx.vin[1].prevout.hash,vinTx,hashBlock) == 0 )
        return eval->Invalid("always should find vin, but didnt");
    else if ( Getscriptaddress(destaddr,vinTx.vout[0].scriptPubKey) == 0 || strcmp(destaddr,(char *)AssetsCCaddr) != 0 )
    {
        fprintf(stderr,"%s vs %s\n",destaddr,(char *)AssetsCCaddr);
        return eval->Invalid("invalid vin AssetsCCaddr");
    }
    else if ( vinTx.vout[0].nValue < 10000 )
        return eval->Invalid("invalid dust for buyvin");
    else if ( GetAssetorigaddrs(CCaddr,origaddr,vinTx) == 0 )
        return eval->Invalid("couldnt get origaddr for buyvin");
    fprintf(stderr,"Got %.8f to origaddr.(%s)\n",(double)vinTx.vout[0].nValue/COIN,origaddr);
    return(vinTx.vout[0].nValue);
}

uint64_t AssetValidateBuyvin(Eval* eval,uint64_t &tmpprice,std::vector<uint8_t> &tmporigpubkey,char *CCaddr,char *origaddr,CTransaction &tx,uint256 refassetid)
{
    CTransaction vinTx; uint64_t nValue; uint256 assetid,assetid2; uint8_t funcid;
    if ( (nValue= AssetValidateCCvin(eval,CCaddr,origaddr,tx,vinTx)) == 0 )
        return(0);
    else if ( vinTx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
        return eval->Invalid("invalid normal vout0 for buyvin");
    else
    {
        //fprintf(stderr,"have %.8f checking assetid origaddr.(%s)\n",(double)nValue/COIN,origaddr);
        if ( (funcid= DecodeAssetOpRet(vinTx.vout[vinTx.vout.size()-1].scriptPubKey,assetid,assetid2,tmpprice,tmporigpubkey)) != 'b' && funcid != 'B' )
            return eval->Invalid("invalid opreturn for buyvin");
        else if ( refassetid != assetid )
        {
            //for (i=32; i>=0; i--)
            //    fprintf(stderr,"%02x",((uint8_t *)&assetid)[i]);
            //fprintf(stderr," AssetValidateBuyvin\n");
            return eval->Invalid("invalid assetid for buyvin");
        }
    }
    return(nValue);
}

uint64_t AssetValidateSellvin(Eval* eval,uint64_t &tmpprice,std::vector<uint8_t> &tmporigpubkey,char *CCaddr,char *origaddr,CTransaction &tx,uint256 assetid)
{
    CTransaction vinTx; uint64_t nValue,assetoshis;
    fprintf(stderr,"AssetValidateSellvin\n");
    if ( (nValue= AssetValidateCCvin(eval,CCaddr,origaddr,tx,vinTx)) == 0 )
        return(0);
    if ( (assetoshis= IsAssetvout(tmpprice,tmporigpubkey,vinTx,0,assetid)) != 0 )
        return eval->Invalid("invalid missing CC vout0 for sellvin");
    else return(assetoshis);
}

bool ConstrainAssetVout(CTxOut vout,int32_t CCflag,char *cmpaddr,uint64_t nValue)
{
    char destaddr[64];
    if ( vout.scriptPubKey.IsPayToCryptoCondition() != CCflag )
        return(false);
    else if ( cmpaddr != 0 && (Getscriptaddress(destaddr,vout.scriptPubKey) == 0 || strcmp(destaddr,cmpaddr) != 0) )
        return(false);
    else if ( (nValue == 0 && vout.nValue < 10000) || nValue != vout.nValue )
        return(false);
    else return(true);
}

bool AssetExactAmounts(Eval* eval,CTransaction &tx,uint256 assetid)
{
    CTransaction vinTx; uint256 hashBlock; int32_t i,numvins,numvouts; uint64_t inputs=0,outputs=0,assetoshis; std::vector<uint8_t> tmporigpubkey; uint64_t tmpprice;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    for (i=1; i<numvins; i++)
    {
        if ( IsAssetsInput(tx.vin[i].scriptSig) != 0 )
        {
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("always should find vin, but didnt");
            else if ( (assetoshis= IsAssetvout(tmpprice,tmporigpubkey,vinTx,tx.vin[i].prevout.n,assetid)) != 0 )
                inputs += assetoshis;
        }
    }
    for (i=0; i<numvouts; i++)
    {
        if ( (assetoshis= IsAssetvout(tmpprice,tmporigpubkey,tx,i,assetid)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs )
        return(false);
    else return(true);
}
