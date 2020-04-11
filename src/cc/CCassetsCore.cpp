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
#include "CCtokens.h"
#include <iomanip> 



/*
 The SetAssetFillamounts() and ValidateAssetRemainder() work in tandem to calculate the vouts for a fill and to validate the vouts, respectively.
 
 This pair of functions are critical to make sure the trading is correct and is the trickiest part of the assets contract.
 
 //vin.0: normal input
 //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
 //vin.2+: valid CC output satisfies buyoffer (*tx.vin[2])->nValue
 //vout.0: remaining amount of bid to unspendable
 //vout.1: vin.1 value to signer of vin.2
 //vout.2: vin.2 assetoshis to original pubkey
 //vout.3: CC output for assetoshis change (if any)
 //vout.4: normal output for change (if any)
 //vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]
    ValidateAssetRemainder(remaining_price,tx.vout[0].nValue,nValue,tx.vout[1].nValue,tx.vout[2].nValue,totalunits);
 
 Yes, this is quite confusing...
 
 In ValidateAssetRemainder the naming convention is nValue is the coin/asset with the offer on the books and "units" is what it is being paid in. The high level check is to make sure we didnt lose any coins or assets, the harder to validate is the actual price paid as the "orderbook" is in terms of the combined nValue for the combined totalunits.
 
 We assume that the effective unit cost in the orderbook is valid and that that amount was paid and also that any remainder will be close enough in effective unit cost to not matter. At the edge cases, this will probably be not true and maybe some orders wont be practically fillable when reduced to fractional state. However, the original pubkey that created the offer can always reclaim it.
*/

// validate:
// unit_price received for a token >= seller's unit_price
// remaining_nValue calculated correctly
bool ValidateBidRemainder(CAmount unit_price, int64_t remaining_nValue, int64_t orig_nValue, int64_t received_nValue, int64_t paid_units)
{
    int64_t received_unit_price;
    // int64_t new_unit_price = 0;
    if (orig_nValue == 0 || received_nValue == 0 || paid_units == 0)
    {
        CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s error any of these values can't be null: orig_nValue == %lld || received_nValue == %lld || paid_units == %lld\n", __func__, (long long)orig_nValue, (long long)received_nValue, (long long)paid_units);
        return(false);
    }
    /* we do not need this check
    else if (orig_units != (remaining_units + paid_units))
    {
        CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s error orig_remaining_units %lld != %lld (remaining_units %lld + %lld paid_units)\n", __func__, (long long)orig_units, (long long)(remaining_units + paid_units), (long long)remaining_units, (long long)paid_units);
        return(false);
    }*/
    else if (orig_nValue != (remaining_nValue + received_nValue))
    {
        CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s error orig_nValue %lld != %lld (remaining_nValue %lld + %lld received_nValue)\n", __func__, (long long)orig_nValue, (long long)(remaining_nValue - received_nValue), (long long)remaining_nValue, (long long)received_nValue);
        return(false);
    }
    else
    {
        //unit_price = AssetsGetUnitPrice(ordertxid);
        received_unit_price = (received_nValue / paid_units);
        if (received_unit_price > unit_price) // can't satisfy bid by sending tokens of higher unit price than requested
        {
            //fprintf(stderr, "%s error can't satisfy bid with higher unit price: received_unit_price %.8f > unit_price %.8f\n", __func__, (double)received_unit_price / (COIN), (double)unit_price / (COIN));
            CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s error can't satisfy bid with higher unit price: received_unit_price %.8f > unit_price %.8f\n", __func__, (double)received_unit_price / (COIN), (double)unit_price / (COIN));
            return(false);
        }
        CCLogPrintF(ccassets_log, CCLOG_DEBUG1, "%s orig_nValue %lld received_nValue %lld, paid_units %lld, received_unit_price %.8f <= unit_price %.8f\n", __func__, (long long)orig_nValue, (long long)received_nValue, (long long)paid_units, (double)received_unit_price / (COIN), (double)unit_price / (COIN));
    }
    return(true);
}

// unit_price is unit price set by the bidder (maximum)
// received_nValue is coins amount received by token seller from the bidder
// orig_nValue is bid amount
// paid_units is tokens paid to the bidder
// orig_units it the tokens amount the bidder wants to buy
// paid_unit_price is unit_price that token seller sells his tokens for
bool SetBidFillamounts(CAmount unit_price, int64_t &received_nValue, int64_t orig_nValue, int64_t &paid_units, int64_t orig_units, CAmount paid_unit_price)
{
    // int64_t remaining_nValue;

    //if (unit_price <= 0LL)
    //    unit_price = AssetsGetUnitPrice(ordertxid);
    if (orig_units == 0)
    {
        received_nValue = paid_units = 0;
        return(false);
    }
    if (paid_units > orig_units)   // not 
    {
        paid_units = 0;
        // received_nValue = orig_nValue;
        received_nValue = (paid_units * paid_unit_price);  // as paid unit_price might be less than original unit_price
        //  remaining_units = 0;
        fprintf(stderr, "%s not enough units!\n", __func__);
        return(false);
    }
    //remaining_units = (orig_units - paid_units);
    //unitprice = (orig_nValue * COIN) / totalunits;
    //received_nValue = (paidunits * unitprice) / COIN;
    //unit_price = (orig_nValue / orig_remaining_units);
    
    received_nValue = (paid_units * paid_unit_price);
    fprintf(stderr, "%s orig_units.%lld - paid_units.%lld, (orig_value.%lld - received_value.%lld)\n", __func__, 
            (long long)orig_units, (long long)paid_units, (long long)orig_nValue, (long long)received_nValue);
    if (unit_price > 0 && received_nValue > 0 && received_nValue <= orig_nValue)
    {
        CAmount remaining_nValue = (orig_nValue - received_nValue);
        return (ValidateBidRemainder(unit_price, remaining_nValue, orig_nValue, received_nValue, paid_units));
    }
    else 
    {
        fprintf(stderr, "%s incorrect values: unit_price %lld > 0, orig_value.%lld >= received_value.%lld\n", __func__, 
            (long long)unit_price, (long long)orig_nValue, (long long)received_nValue);
        return(false);
    }
}

// unit_price is the mininum token price set by the token seller
// fill_assetoshis is tokens purchased
// orig_assetoshis is available tokens to sell
// paid_nValue is the paid coins calculated as fill_assetoshis * paid_unit_price 
bool SetAskFillamounts(CAmount unit_price, int64_t fill_assetoshis, int64_t orig_assetoshis, int64_t paid_nValue)
{
    //int64_t remaining_assetoshis; 
    //double dunit_price;

    /*if (orig_nValue == 0)
    {
        fill_assetoshis = remaining_nValue = paid_nValue = 0;
        return(false);
    }
    if (paid_nValue >= orig_nValue) // TODO maybe if paid_nValue > orig_nValue just return error but not 
    {
        // paid_nValue = orig_nValue; <- do not reset it
        fill_assetoshis = orig_assetoshis;
        remaining_nValue = 0;
        fprintf(stderr, "%s ask order totally filled!\n", __func__);
        return(true);
    }*/
    if (orig_assetoshis == 0)   {
        fprintf(stderr, "%s ask order empty!\n", __func__);
        return false;
    }
    if (fill_assetoshis == 0)   {
        fprintf(stderr, "%s ask fill tokens is null!\n", __func__);
        return false;
    }
    CAmount paid_unit_price = paid_nValue / fill_assetoshis;
    //remaining_nValue = (orig_nValue - paid_nValue);
    //CAmount unit_price = AssetsGetUnitPrice(ordertxid);
    //remaining_nValue = orig_nValue - unit_price * fill_assetoshis;
    // dunit_price = ((double)orig_nValue / orig_assetoshis);
    // fill_assetoshis = (paid_nValue / dunit_price);  // back conversion -> could be loss of value
    fprintf(stderr, "%s paid_unit_price %lld fill_assetoshis %lld orig_assetoshis %lld unit_price %lld  fill_assetoshis %lld)\n", __func__, 
            (long long)paid_unit_price, (long long)fill_assetoshis, (long long)orig_assetoshis, (long long)unit_price, (long long)fill_assetoshis);
    if (paid_unit_price > 0 && fill_assetoshis <= orig_assetoshis)
    {
        CAmount remaining_assetoshis = (orig_assetoshis - fill_assetoshis);
        if (remaining_assetoshis == 0)
            fprintf(stderr, "%s ask order totally filled!\n", __func__);
        return ValidateAskRemainder(unit_price, remaining_assetoshis, orig_assetoshis, fill_assetoshis, paid_nValue);
    }
    else 
    {
        fprintf(stderr, "%s incorrect values paid_unit_price %lld > 0, fill_assetoshis %lld > 0 and <= orig_assetoshis %lld\n", __func__, 
            (long long)paid_unit_price, (long long)fill_assetoshis, (long long)orig_assetoshis);
        return(false);
    }
}

// validate: 
// paid unit_price for a token <= the bidder's unit_price
// remaining coins calculated correctly
bool ValidateAskRemainder(CAmount unit_price, int64_t remaining_assetoshis, int64_t orig_assetoshis, int64_t received_assetoshis, int64_t paid_nValue)
{
    int64_t paid_unit_price;
    //CAmount unit_price = AssetsGetUnitPrice(ordertxid);
    //int64_t new_unit_price = 0;

    if (orig_assetoshis == 0 || received_assetoshis == 0 || paid_nValue == 0)
    {
        CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s error any of these values can't be null: orig_assetoshis == %lld || received_assetoshis == %lld || paid_nValue == %lld\n", __func__, (long long)orig_assetoshis, (long long)received_assetoshis, (long long)paid_nValue);
        return(false);
    }
    // this check does not affect anything
    //else if (orig_nValue != (remaining_nValue + paid_nValue))
    /*else if (orig_nValue != (remaining_nValue + received_assetoshis * unit_price))  // remaining_nValue is calced from the source unit_price, not paid unit_price
    {
        CCLogPrintF(ccassets_log, CCLOG_ERROR,  "%s error orig_nValue %lld != %lld + (received_assetoshis %lld * unit_price %lld)\n", __func__, (long long)orig_nValue, (long long)remaining_nValue, (long long)received_assetoshis, (long long)unit_price);
        return(false);
    }*/
    else if (orig_assetoshis != (remaining_assetoshis + received_assetoshis))
    {
        CCLogPrintF(ccassets_log, CCLOG_ERROR,  "%s error orig_assetoshis %lld != %lld (remaining_nValue %lld + received_nValue %lld)\n", __func__, (long long)orig_assetoshis, (long long)(remaining_assetoshis - received_assetoshis), (long long)remaining_assetoshis, (long long)received_assetoshis);
        return(false);
    }
    else
    {
        // unit_price = (orig_nValue / orig_assetoshis);
        paid_unit_price = (paid_nValue / received_assetoshis);
        //if (remaining_nValue != 0)
        //    new_unit_price = (remaining_nValue / remaining_assetoshis);  // for debug printing
        if (paid_unit_price < unit_price)  // can't pay for ask with lower unit price than requested
        {
            //fprintf(stderr, "%s error can't pay for ask with lower price: paid_unit_price %.8f < %.8f unit_price\n", __func__, (double)paid_unit_price / COIN, (double)unit_price / COIN);
            CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s error can't pay for ask with lower price: paid_unit_price %.8f < unit_price %.8f\n", __func__, (double)paid_unit_price / COIN, (double)unit_price / COIN);

            return(false);
        }
        CCLogPrintF(ccassets_log, CCLOG_DEBUG1, "%s got paid_unit_price %.8f >= unit_price %.8f\n", __func__, (double)paid_unit_price / COIN, (double)unit_price / COIN);
    }
    return(true);
}

// not implemented
bool SetSwapFillamounts(CAmount unit_price, int64_t &received_assetoshis, int64_t orig_assetoshis, int64_t &paid_assetoshis2, int64_t total_assetoshis2)
{
	int64_t remaining_assetoshis; double dunitprice;
	if ( total_assetoshis2 == 0 )
	{
		fprintf(stderr,"%s total_assetoshis2.0 orig_assetoshis.%lld paid_assetoshis2.%lld\n", __func__, (long long)orig_assetoshis,(long long)paid_assetoshis2);
		received_assetoshis = paid_assetoshis2 = 0;
		return(false);
	}
	if ( paid_assetoshis2 >= total_assetoshis2 )
	{
		paid_assetoshis2 = total_assetoshis2;
		received_assetoshis = orig_assetoshis;
		// remaining_assetoshis2 = 0;
		fprintf(stderr,"%s swap order totally filled!\n", __func__);
		return(true);
	}
	// remaining_assetoshis2 = (total_assetoshis2 - paid_assetoshis2);
	dunitprice = ((double)total_assetoshis2 / orig_assetoshis);
	received_assetoshis = (paid_assetoshis2 / dunitprice);
	fprintf(stderr,"%s (%lld - %lld)\n", __func__, (long long)total_assetoshis2/COIN, (long long)paid_assetoshis2/COIN);
	fprintf(stderr,"%s unitprice %.8f received_assetoshis %lld orig %lld\n", __func__,dunitprice/COIN,(long long)received_assetoshis,(long long)orig_assetoshis);
	if ( fabs(dunitprice) > SMALLVAL && received_assetoshis > 0 && received_assetoshis <= orig_assetoshis )
	{
		remaining_assetoshis = (orig_assetoshis - received_assetoshis);
		return ValidateAskRemainder(unit_price, remaining_assetoshis,orig_assetoshis,received_assetoshis,paid_assetoshis2);
	} 
    else 
        return(false);
}

bool ValidateSwapRemainder(int64_t remaining_price, int64_t remaining_nValue, int64_t orig_nValue, int64_t received_nValue, int64_t paidunits, int64_t totalunits)
{
    int64_t unitprice, recvunitprice, newunitprice = 0;
    if (orig_nValue == 0 || received_nValue == 0 || paidunits == 0 || totalunits == 0)
    {
        CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s orig_nValue == %lld || received_nValue == %lld || paidunits == %lld || totalunits == %lld\n", __func__, (long long)orig_nValue, (long long)received_nValue, (long long)paidunits, (long long)totalunits);
        return(false);
    }
    else if (totalunits != (remaining_price + paidunits))
    {
        CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s totalunits %lld != %lld (remaining_price %lld + %lld paidunits)\n", __func__, (long long)totalunits, (long long)(remaining_price + paidunits), (long long)remaining_price, (long long)paidunits);
        return(false);
    }
    else if (orig_nValue != (remaining_nValue + received_nValue))
    {
        CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s orig_nValue %lld != %lld (remaining_nValue %lld + %lld received_nValue)\n", __func__, (long long)orig_nValue, (long long)(remaining_nValue - received_nValue), (long long)remaining_nValue, (long long)received_nValue);
        return(false);
    }
    else
    {
        unitprice = (orig_nValue * COIN) / totalunits;
        recvunitprice = (received_nValue * COIN) / paidunits;
        if (remaining_price != 0)
            newunitprice = (remaining_nValue * COIN) / remaining_price;
        if (recvunitprice < unitprice)
        {
            CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s error recvunitprice %.8f < %.8f unitprice, new unitprice %.8f\n", __func__, (double)recvunitprice / (COIN*COIN), (double)unitprice / (COIN*COIN), (double)newunitprice / (COIN*COIN));
            return(false);
        }
        fprintf(stderr, "%s recvunitprice %.8f >= %.8f unitprice, new unitprice %.8f\n", __func__, (double)recvunitprice / (COIN*COIN), (double)unitprice / (COIN*COIN), (double)newunitprice / (COIN*COIN));
    }
    return(true);
}

vscript_t EncodeAssetOpRet(uint8_t assetFuncId, uint256 assetid2, int64_t unit_price, std::vector<uint8_t> origpubkey)
{
    vscript_t vopret; 
	uint8_t evalcode = EVAL_ASSETS;
    uint8_t version = 1;

    switch ( assetFuncId )
    {
        //case 't': this cannot be here
		case 'x': case 'o':
			vopret = E_MARSHAL(ss << evalcode << assetFuncId << version);
            break;
        case 's': case 'b': case 'S': case 'B':
            vopret = E_MARSHAL(ss << evalcode << assetFuncId << version << unit_price << origpubkey);
            break;
        case 'E': case 'e':
            assetid2 = revuint256(assetid2);
            vopret = E_MARSHAL(ss << evalcode << assetFuncId << version << assetid2 << unit_price << origpubkey);
            break;
        default:
            CCLogPrintF(ccassets_log, CCLOG_DEBUG1, "%s illegal funcid.%02x\n", __func__, assetFuncId);
            break;
    }
    return(vopret);
}

uint8_t DecodeAssetTokenOpRet(const CScript &scriptPubKey, uint8_t &assetsEvalCode, uint256 &tokenid, uint256 &assetid2, int64_t &unit_price, std::vector<uint8_t> &origpubkey)
{
    vscript_t vopretAssets; //, vopretAssetsStripped;
	uint8_t *script, funcId = 0, assetsFuncId = 0, dummyAssetFuncId, dummyEvalCode, version;
	uint256 dummyTokenid;
	std::vector<CPubKey> voutPubkeysDummy;
    std::vector<vscript_t>  oprets;

	tokenid = zeroid;
	assetid2 = zeroid;
	unit_price = 0;
    assetsEvalCode = 0;
    assetsFuncId = 0;

	// First - decode token opret:
	funcId = DecodeTokenOpRetV1(scriptPubKey, tokenid, voutPubkeysDummy, oprets);
    GetOpReturnCCBlob(oprets, vopretAssets);

    LOGSTREAMFN(ccassets_log, CCLOG_DEBUG2, stream << "from DecodeTokenOpRet returned funcId=" << (int)funcId << std::endl);

	if (funcId == 0 || vopretAssets.size() < 2) {
        LOGSTREAMFN(ccassets_log, CCLOG_DEBUG1, stream << "incorrect opret or no asset's payload" << " funcId=" << (int)funcId << " vopretAssets.size()=" << vopretAssets.size() << std::endl);
		return (uint8_t)0;
	}

    // additional check to prevent crash
    if (vopretAssets.size() >= 2) {

        assetsEvalCode = vopretAssets.begin()[0];
        assetsFuncId = vopretAssets.begin()[1];

        LOGSTREAMFN(ccassets_log, CCLOG_DEBUG2, stream << "assetsEvalCode=" << (int)assetsEvalCode <<  " funcId=" << (char)(funcId ? funcId : ' ') << " assetsFuncId=" << (char)(assetsFuncId ? assetsFuncId : ' ') << std::endl);

        if (assetsEvalCode == EVAL_ASSETS)
        {
            //fprintf(stderr,"DecodeAssetTokenOpRet() decode.[%c] assetFuncId.[%c]\n", funcId, assetFuncId);
            switch (assetsFuncId)
            {
            case 'x': case 'o':
                if (vopretAssets.size() == 3)   // format is 'evalcode funcid version' 
                {
                    return(assetsFuncId);
                }
                break;
            case 's': case 'b': case 'S': case 'B':
                if (E_UNMARSHAL(vopretAssets, ss >> dummyEvalCode; ss >> dummyAssetFuncId; ss >> version; ss >> unit_price; ss >> origpubkey) != 0)
                {
                    //fprintf(stderr,"DecodeAssetTokenOpRet() got price %lld\n",(long long)price);
                    return(assetsFuncId);
                }
                break;
            case 'E': case 'e':
                // not implemented yet
                if (E_UNMARSHAL(vopretAssets, ss >> dummyEvalCode; ss >> dummyAssetFuncId; ss >> version; ss >> assetid2; ss >> unit_price; ss >> origpubkey) != 0)
                {
                    //fprintf(stderr,"DecodeAssetTokenOpRet() got price %lld\n",(long long)price);
                    assetid2 = revuint256(assetid2);
                    return(assetsFuncId);
                }
                break;
            default:
                break;
            }
        }
    }

    LOGSTREAMFN(ccassets_log, CCLOG_DEBUG1, stream << "no asset's payload or incorrect assets funcId or evalcode" << " funcId=" << (int)funcId << " vopretAssets.size()=" << vopretAssets.size() << " assetsEvalCode=" << assetsEvalCode << " assetsFuncId=" << assetsFuncId << std::endl);
    return (uint8_t)0;
}

// extract sell/buy owner's pubkey from the opret
uint8_t SetAssetOrigpubkey(std::vector<uint8_t> &origpubkey_out, CAmount &unit_price, const CTransaction &tx)
{
    uint256 assetid, assetid2;
    uint8_t evalCode, funcid;

    if (tx.vout.size() > 0 && (funcid = DecodeAssetTokenOpRet(tx.vout[tx.vout.size() - 1].scriptPubKey, evalCode, assetid, assetid2, unit_price, origpubkey_out)) != 0)
        return funcid;
    else
        return 0;
}

// Calculate seller/buyer's dest cc address from ask/bid tx funcid
bool GetAssetorigaddrs(struct CCcontract_info *cp, char *origCCaddr, char *origNormalAddr, const CTransaction& vintx)
{
    uint256 assetid, assetid2; 
    int64_t price,nValue=0; 
    int32_t n; 
    uint8_t vintxFuncId; 
	std::vector<uint8_t> origpubkey; 
	CScript script;
	uint8_t evalCode;

    n = vintx.vout.size();
    if( n == 0 || (vintxFuncId = DecodeAssetTokenOpRet(vintx.vout[n-1].scriptPubKey, evalCode, assetid, assetid2, price, origpubkey)) == 0 ) {
        LOGSTREAMFN(ccassets_log, CCLOG_INFO, stream << "could not get vintx opreturn" << std::endl);
        return(false);
    }

    struct CCcontract_info *cpTokens, tokensC;
    cpTokens = CCinit(&tokensC, EVAL_TOKENS);

	if (vintxFuncId == 's' || vintxFuncId == 'S' || vintxFuncId == 'b' || vintxFuncId == 'B') {
        cpTokens->evalcodeNFT = cp->evalcodeNFT;  // add non-fungible evalcode if present
        if (!GetTokensCCaddress(cpTokens, origCCaddr, pubkey2pk(origpubkey)))  // tokens to single-eval token or token+nonfungible
            return false;
	}
	else  {
		LOGSTREAMFN(ccassets_log, CCLOG_INFO, stream << "incorrect vintx funcid=" << (char)(vintxFuncId?vintxFuncId:' ') << std::endl);
		return false;
	}
    if (Getscriptaddress(origNormalAddr, CScript() << origpubkey << OP_CHECKSIG))
        return(true);
    else 
		return(false);
}


int64_t AssetValidateCCvin(struct CCcontract_info *cpAssets, Eval* eval, char *origCCaddr_out, char *origaddr_out, const CTransaction &tx, int32_t vini, CTransaction &vinTx)
{
	uint256 hashBlock;
    uint256 assetid, assetid2;
    uint256 vinAssetId, vinAssetId2;
	int64_t tmpprice, vinPrice;
    std::vector<uint8_t> tmporigpubkey;
    std::vector<uint8_t> vinOrigpubkey;
    uint8_t evalCode;
    uint8_t vinEvalCode;

	char destaddr[KOMODO_ADDRESS_BUFSIZE], unspendableAddr[KOMODO_ADDRESS_BUFSIZE];

    origaddr_out[0] = destaddr[0] = origCCaddr_out[0] = 0;

    uint8_t funcid = 0;
    uint8_t vinFuncId = 0;
	if (tx.vout.size() > 0) 
		funcid = DecodeAssetTokenOpRet(tx.vout.back().scriptPubKey, evalCode, assetid, assetid2, tmpprice, tmporigpubkey);
    else
        return eval->Invalid("no vouts in tx");

    if( tx.vin.size() < 2 )
        return eval->Invalid("not enough for CC vins");
    else if(tx.vin[vini].prevout.n != ASSETS_GLOBALADDR_VOUT)   // check gobal addr vout number == 0
        return eval->Invalid("vin1 needs to be buyvin.vout[0]");
    else if(eval->GetTxUnconfirmed(tx.vin[vini].prevout.hash, vinTx, hashBlock) == false)
    {
		LOGSTREAMFN(ccassets_log, CCLOG_ERROR, stream << "cannot load vintx for vin=" << vini << " vintx id=" << tx.vin[vini].prevout.hash.GetHex() << std::endl);
        return eval->Invalid("could not load previous tx or it has too few vouts");
    }
    else if (vinTx.vout.size() < 1 || (vinFuncId = DecodeAssetTokenOpRet(vinTx.vout.back().scriptPubKey, vinEvalCode, vinAssetId, vinAssetId2, vinPrice, vinOrigpubkey)) == 0)
        return eval->Invalid("could not find assets opreturn in previous tx");
	// if fillSell or cancelSell --> should spend tokens from dual-eval token-assets global addr:
    else if((funcid == 'S' || funcid == 'x') && 
       (tx.vin[vini].prevout.n >= vinTx.vout.size() ||  // check bounds
		Getscriptaddress(destaddr, vinTx.vout[tx.vin[vini].prevout.n].scriptPubKey) == false || 
		!GetTokensCCaddress(cpAssets, unspendableAddr, GetUnspendable(cpAssets, NULL)) ||                  
		strcmp(destaddr, unspendableAddr) != 0))
    {
        CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s cc addr %s is not dual token-evalcode=0x%02x asset unspendable addr %s\n", __func__, destaddr, (int)cpAssets->evalcode, unspendableAddr);
        return eval->Invalid("invalid vin assets CCaddr");
    }
	// if fillBuy or cancelBuy --> should spend coins from asset cc global addr
	else if ((funcid == 'B' || funcid == 'o') && 
	   (tx.vin[vini].prevout.n >= vinTx.vout.size() ||  // check bounds
        Getscriptaddress(destaddr, vinTx.vout[tx.vin[vini].prevout.n].scriptPubKey) == false ||
		!GetCCaddress(cpAssets, unspendableAddr, GetUnspendable(cpAssets, NULL)) ||
		strcmp(destaddr, unspendableAddr) != 0))
	{
        CCLogPrintF(ccassets_log, CCLOG_ERROR, "%s cc addr %s is not evalcode=0x%02x asset unspendable addr %s\n", __func__, destaddr, (int)cpAssets->evalcode, unspendableAddr);
		return eval->Invalid("invalid vin assets CCaddr");
	}
    // end of check source unspendable cc address
    //else if ( vinTx.vout[0].nValue < 10000 )
    //    return eval->Invalid("invalid dust for buyvin");
    // get user dest cc and normal addresses:
    else if(GetAssetorigaddrs(cpAssets, origCCaddr_out, origaddr_out, vinTx) == false)  
        return eval->Invalid("couldnt get origaddr for buyvin");

    //fprintf(stderr,"AssetValidateCCvin() got %.8f to origaddr.(%s)\n", (double)vinTx.vout[tx.vin[vini].prevout.n].nValue/COIN,origaddr);

    // check that vinTx B or S has assets cc vins:
    if (vinFuncId == 'B' || vinFuncId == 'S')
    {
        bool found = false;
        for (auto const &vin : vinTx.vin)
            if (cpAssets->ismyvin(vin.scriptSig))  {
                found = true;      
                break;
            }
        if (!found)
            return eval->Invalid("no assets cc vins in previous fillbuy or fillsell tx");
    }

    // check no more other vins spending from global addr:
    if (AssetsGetCCInputs(cpAssets, unspendableAddr, tx) != vinTx.vout[ASSETS_GLOBALADDR_VOUT].nValue)
        return eval->Invalid("invalid assets cc vins found");

    if (vinTx.vout[ASSETS_GLOBALADDR_VOUT].nValue == 0)
        return eval->Invalid("null value in previous tx CC vout0");

    return(vinTx.vout[ASSETS_GLOBALADDR_VOUT].nValue);
}

int64_t AssetValidateBuyvin(struct CCcontract_info *cpAssets, Eval* eval, int64_t &unit_price, std::vector<uint8_t> &origpubkey_out, char *origCCaddr_out, char *origaddr_out, const CTransaction &tx, uint256 refassetid)
{
    CTransaction vinTx; int64_t nValue; uint256 assetid, assetid2; uint8_t funcid, evalCode;

    origCCaddr_out[0] = origaddr_out[0] = 0;
    // validate locked coins on Assets vin[1]
    if ((nValue = AssetValidateCCvin(cpAssets, eval, origCCaddr_out, origaddr_out, tx, ASSETS_GLOBALADDR_VIN, vinTx)) == 0)
        return(0);
    else if (vinTx.vout.size() < 2)
        return eval->Invalid("invalid previous tx, too few vouts");
    else if (vinTx.vout[0].scriptPubKey.IsPayToCryptoCondition() == false)
        return eval->Invalid("invalid not cc vout0 for buyvin");
    else if ((funcid = DecodeAssetTokenOpRet(vinTx.vout[vinTx.vout.size() - 1].scriptPubKey, evalCode, assetid, assetid2, unit_price, origpubkey_out)) == 'b' &&
        vinTx.vout[1].scriptPubKey.IsPayToCryptoCondition() == false)  // marker is only in 'b'?
        return eval->Invalid("invalid not cc vout1 for buyvin");
    else
    {
        //fprintf(stderr,"have %.8f checking assetid origaddr.(%s)\n",(double)nValue/COIN,origaddr);
        if (vinTx.vout.size() > 0 && funcid != 'b' && funcid != 'B')
            return eval->Invalid("invalid opreturn for buyvin");
        else if (refassetid != assetid)
            return eval->Invalid("invalid assetid for buyvin");
        //int32_t i; for (i=31; i>=0; i--)
        //    fprintf(stderr,"%02x",((uint8_t *)&assetid)[i]);
        //fprintf(stderr," AssetValidateBuyvin assetid for %s\n",origaddr);
    }
    return(nValue);
}

int64_t AssetValidateSellvin(struct CCcontract_info *cpAssets, Eval* eval, int64_t &unit_price, std::vector<uint8_t> &origpubkey_out, char *origCCaddr_out, char *origaddr_out, const CTransaction &tx, uint256 assetid)
{
    CTransaction vinTx; int64_t nValue, assetoshis;

    //fprintf(stderr,"AssetValidateSellvin()\n");
    if ((nValue = AssetValidateCCvin(cpAssets, eval, origCCaddr_out, origaddr_out, tx, ASSETS_GLOBALADDR_VIN, vinTx)) == 0)
        return(0);
    
    if ((assetoshis = IsAssetvout(cpAssets, unit_price, origpubkey_out, vinTx, ASSETS_GLOBALADDR_VOUT, assetid)) == 0)
        return eval->Invalid("invalid missing CC vout0 for sellvin");
    else
        return(assetoshis);
}


// validates opret for asset tx:
bool ValidateAssetOpret(CTransaction tx, int32_t v, uint256 assetid, int64_t &remaining_units_out, std::vector<uint8_t> &origpubkey_out) 
{
	uint256 assetidOpret, assetidOpret2;
	uint8_t funcid, evalCode;

	// this is just for log messages indentation fur debugging recursive calls:
	int32_t n = tx.vout.size();

	if ((funcid = DecodeAssetTokenOpRet(tx.vout.back().scriptPubKey, evalCode, assetidOpret, assetidOpret2, remaining_units_out, origpubkey_out)) == 0)
	{
        LOGSTREAMFN(ccassets_log, CCLOG_DEBUG1, stream << "called DecodeAssetTokenOpRet returned funcId=0 for opret from txid=" << tx.GetHash().GetHex() << std::endl);
		return(false);
	}
	else if (funcid != 'E')
	{
		if (assetid != zeroid && assetidOpret == assetid)
		{
			//std::cerr  << "ValidateAssetOpret() returns true for not 'E', funcid=" << (char)funcid << std::endl;
			return(true);
		}
	}
	else if (funcid == 'E')  // NOTE: not implemented yet!
	{
		if (v < 2 && assetid != zeroid && assetidOpret == assetid)
			return(true);
		else if (v == 2 && assetid != zeroid && assetidOpret2 == assetid)
			return(true);
	}
	return false;
}  

// Checks if the vout is a really Asset CC vout
int64_t IsAssetvout(struct CCcontract_info *cp, int64_t &remaining_units_out, std::vector<uint8_t> &origpubkey_out, const CTransaction& tx, int32_t v, uint256 refassetid)
{
    // just check boundaries:
    int32_t n = tx.vout.size();
    if (v >= n - 1) {  
        LOGSTREAMFN(ccassets_log, CCLOG_ERROR, stream << "internal err: (v >= n - 1), returning 0" << std::endl);
        return -1;
    }

	if (tx.vout[v].scriptPubKey.IsPayToCryptoCondition())
	{
		if (!ValidateAssetOpret(tx, v, refassetid, remaining_units_out, origpubkey_out)) {
            LOGSTREAMFN(ccassets_log, CCLOG_ERROR, stream << "invalid assets opreturn" << std::endl);
            return -1;
        }
        return tx.vout[v].nValue;
	}
	return(0);
} 

// sets cc inputs vs cc outputs and ensures they are equal:
/*bool AssetsGetCCInputs(struct CCcontract_info *cpAssets, CAmount &tokensInputs, int64_t &assetsInputs, Eval* eval, const CTransaction &tx, uint256 assetid)
{
	CTransaction vinTx; 
    uint256 hashBlock; 
	int32_t numvins = tx.vin.size();
	int32_t numvouts = tx.vout.size();
	tokensInputs = assetsInputs = 0;

	struct CCcontract_info *cpTokens, C;
	cpTokens = CCinit(&C, EVAL_TOKENS);

	for (int32_t i = 0; i<numvins; i++)
	{												    // only tokens are relevant!!
		if (cpTokens->ismyvin(tx.vin[i].scriptSig) || cpAssets->ismyvin(tx.vin[i].scriptSig))
		{
			if (myGetTransaction(tx.vin[i].prevout.hash, vinTx, hashBlock))
			{
                if (cpTokens->ismyvin(tx.vin[i].scriptSig))
                {
                    CAmount tokens = IsTokensvout(true, true, cpTokens, NULL, vinTx, tx.vin[i].prevout.n, assetid);
                    std::cerr << __func__ << " IsTokensvout tokens=" << tokens << " i=" << i << std::endl;

                    if (tokens < 0)     {
                        LOGSTREAMFN(ccassets_log, CCLOG_ERROR, stream << "invalid token input vintx for vin.i=" << i << " vin txid=" << tx.vin[i].prevout.hash.GetHex() << std::endl);
                        return (!eval) ? false : eval->Invalid("invalid tokens input");
                    }
                    else if (tokens > 0)	
                    {	
                        LOGSTREAMFN(ccassets_log, CCLOG_DEBUG2, stream << "adding tokens amount=" << tokens << " for vin=" << i << std::endl);		
                        tokensInputs += tokens;
                    }
                }
                else 
                {
                    CAmount dummy_remaining;
                    vuint8_t dummy_origpubkey;
                    CAmount assets = IsAssetvout(cpAssets, dummy_remaining, dummy_origpubkey, vinTx, tx.vin[i].prevout.n, assetid);
                    std::cerr << __func__ << " IsAssetvout assets=" << assets << " i=" << i << std::endl;
                    if (assets < 0)    {
                        LOGSTREAMFN(ccassets_log, CCLOG_ERROR, stream << "invalid assets input vintx for vin.i=" << i << " vin txid=" << tx.vin[i].prevout.hash.GetHex() << std::endl);
                        return (!eval) ? false : eval->Invalid("invalid assets input");
                    }		
                    if (assets > 0)
                    {
                        LOGSTREAMFN(ccassets_log, CCLOG_DEBUG2, stream << "adding assets amount=" << assets << " for vin=" << i << std::endl);		
                        assetsInputs += assets;
                    }
                }
			}
            else {
                LOGSTREAMFN(ccassets_log, CCLOG_ERROR, stream << "cannot load vintx for i." << i << " vin txid=" << tx.vin[i].prevout.hash.GetHex() << std::endl);
				return (!eval) ? false : eval->Invalid("always should find vin tx, but didnt");
            }
		}
	}
	return(true);
}*/

// get tx's vin inputs for cp and optional global addr
CAmount AssetsGetCCInputs(struct CCcontract_info *cp, const char *addr, const CTransaction &tx)
{
	CTransaction vinTx; 
    uint256 hashBlock; 
	CAmount inputs = 0LL;

	//struct CCcontract_info *cpTokens, C;
	//cpTokens = CCinit(&C, EVAL_TOKENS);

	for (int32_t i = 0; i < tx.vin.size(); i++)
	{												    
		if (cp->ismyvin(tx.vin[i].scriptSig))
		{
			if (myGetTransaction(tx.vin[i].prevout.hash, vinTx, hashBlock))
			{
                char scriptaddr[KOMODO_ADDRESS_BUFSIZE];
                if (addr == NULL || Getscriptaddress(scriptaddr, vinTx.vout[tx.vin[i].prevout.n].scriptPubKey) && strcmp(scriptaddr, addr) == 0)  {
                    std::cerr << __func__ << " adding amount=" << vinTx.vout[tx.vin[i].prevout.n].nValue << " for vin i=" << i << " eval=" << std::hex << (int)cp->evalcode << std::resetiosflags(std::ios::hex) << std::endl;
                    inputs += vinTx.vout[tx.vin[i].prevout.n].nValue;
                }
			}
		}
	}
	return inputs;
}

/*
uint256 AssetsGetPrevOrdertxid(const CTransaction &tx)
{
    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_ASSETS);

    for (int32_t i; i < tx.vin.size(); i ++)
        if (cp->ismyvin(tx.vin[i].scriptSig))
            return tx.vin[i].prevout.hash;
    return zeroid;
}

// get unit price from the initial order rx
CAmount AssetsGetUnitPrice(uint256 ordertxid)
{
    CTransaction ordertx;
    uint256 hashBlock;
    uint8_t funcid = 0;

    do {
        if (myGetTransaction(ordertxid, ordertx, hashBlock))
        {
            uint8_t evalCode; 
            uint256 assetid, assetid2; 
            int64_t amount; 
            std::vector<uint8_t> origpubkey;

            funcid = DecodeAssetTokenOpRet(ordertx.vout.back().scriptPubKey, evalCode, assetid, assetid2, amount, origpubkey);
            // price is defined by the first order tx;  
            if (funcid == 's')
                return ordertx.vout[0].nValue > 0 ? amount / ordertx.vout[0].nValue : -1; 
            if (funcid == 'b')
                return amount > 0 ? ordertx.vout[0].nValue / amount : -1; 
        }
        else
            return -1L;
        ordertxid =  AssetsGetPrevOrdertxid(ordertx);
    } 
    while(funcid != 0 && ordertxid != zeroid);

    return 0L;
}
*/

