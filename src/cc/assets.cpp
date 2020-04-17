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
#include "CCtokens.h"

/*
 TODO: update:

 Assets can be created or transferred.
 
 native coins are also locked in the EVAL_ASSETS address, so we need a strict rule on when utxo in the special address are native coins and when they are assets. The specific rule that must not be violated is that vout0 for 'b'/'B' funcid are native coins. All other utxo locked in the special address are assets.
 
 To create an asset use CC EVAL_ASSETS to create a transaction where vout[0] funds the assets. Externally each satoshi can be interpreted to represent 1 asset, or 100 million satoshis for one asset with 8 decimals, and the other decimals in between. The interpretation of the number of decimals is left to the higher level usages.
 
 Once created, the assetid is the txid of the create transaction and using the assetid/0 it can spend the assets to however many outputs it creates. The restriction is that the last output must be an opreturn with the assetid. The sum of all but the first output needs to add up to the total assetoshis input. The first output is ignored and used for change from txfee.
 
 What this means is that vout 0 of the creation txid and vouts 1 ... n-2 for transfer vouts are assetoshi outputs.
 
 There is a special type of transfer to an unspendable address, that locks the asset and creates an offer for all. The details specified in the opreturn. In order to be compatible with the signing restrictions, the "unspendable" address is actually an address whose privkey is known to all. Funds sent to this address can only be spent if the swap parameters are fulfilled, or if the original pubkey cancels the offer by spending it.
 
 Types of transactions:
 create name:description -> txid for assetid
 transfer <pubkey> <assetid> -> [{address:amount}, ... ] // like withdraw api
 selloffer <pubkey> <txid/vout> <amount> -> cancel or fillsell -> mempool txid or rejected, might not confirm
 buyoffer <amount> <assetid> <required> -> cancelbuy or fillbuy -> mempool txid or rejected, might not confirm
 exchange <pubkey> <txid/vout> <required> <required assetid> -> cancel or fillexchange -> mempool txid or rejected, might not confirm
 
 assetsaddress <pubkey> // all assets end up in a special address for each pubkey
 assetbalance <pubkey> <assetid>
 assetutxos <pubkey> <assetid>
 assetsbalances <pubkey>
 asks <assetid>
 bids <assetid>
 swaps <assetid>
 
 valid CC output: create or transfer or buyoffer or selloffer or exchange or cancel or fill
 
 
 buyoffer:
 vins.*: normal inputs (bid + change)
 vout.0: amount of bid to unspendable
 vout.1: CC output for marker
 vout.2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['b'] [assetid] [amount of asset required] [origpubkey]

 cancelbuy:
 vin.0: normal input
 vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
 vin.2: CC marker from buyoffer for txfee
 vout.0: vin.1 value to original pubkey buyTx.vout[0].nValue -> [origpubkey]
 vout.1: vin.2 back to users pubkey
 vout.2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['o'] [assetid] 0 0 [origpubkey]
 
 fillbuy:
 vin.0: normal input
 vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
 vin.2+: valid CC output satisfies buyoffer (*tx.vin[2])->nValue
 vout.0: remaining amount of bid to unspendable
 vout.1: vin.1 value to signer of vin.2
 vout.2: vin.2 assetoshis to original pubkey
 vout.3: CC output for assetoshis change (if any)
 vout.4: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]

 selloffer:
 vin.0: normal input
 vin.1+: valid CC output for sale
 vout.0: vin.1 assetoshis output to CC to unspendable
 vout.1: CC output for marker
 vout.2: CC output for change (if any)
 vout.3: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['s'] [assetid] [amount of native coin required] [origpubkey]
 
 exchange:
 vin.0: normal input
 vin.1+: valid CC output
 vout.0: vin.1 assetoshis output to CC to unspendable
 vout.1: CC output for change (if any)
 vout.2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['e'] [assetid] [assetid2] [amount of asset2 required] [origpubkey]
 
 cancel:
 vin.0: normal input
 vin.1: unspendable.(vout.0 from exchange or selloffer) sellTx/exchangeTx.vout[0] inputTx
 vin.2: CC marker from selloffer for txfee
 vout.0: vin.1 assetoshis to original pubkey CC sellTx/exchangeTx.vout[0].nValue -> [origpubkey]
 vout.1: vin.2 back to users pubkey
 vout.2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['x'] [assetid]
 
 fillsell:
 vin.0: normal input
 vin.1: unspendable.(vout.0 assetoshis from selloffer) sellTx.vout[0]
 vin.2+: normal output that satisfies selloffer (*tx.vin[2])->nValue
 vout.0: remaining assetoshis -> unspendable
 vout.1: vin.1 assetoshis to signer of vin.2 sellTx.vout[0].nValue -> any
 vout.2: vin.2 value to original pubkey [origpubkey]
 vout.3: CC asset for change (if any)
 vout.4: CC asset2 for change (if any) 'E' only
 vout.5: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['S'] [assetid] [amount of coin still required] [origpubkey]
 
 fillexchange:
 vin.0: normal input
 vin.1: unspendable.(vout.0 assetoshis from exchange) exchangeTx.vout[0]
 vin.2+: valid CC assetid2 output that satisfies exchange (*tx.vin[2])->nValue
 vout.0: remaining assetoshis -> unspendable
 vout.1: vin.1 assetoshis to signer of vin.2 exchangeTx.vout[0].nValue -> any
 vout.2: vin.2 assetoshis2 to original pubkey [origpubkey]
 vout.3: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['E'] [assetid vin0+1] [assetid vin2] [remaining asset2 required] [origpubkey]
*/



// tx validation
static bool AssetsValidateInternal(struct CCcontract_info *cpAssets, Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    static uint256 zero;
    CTxDestination address; 
    CTransaction vinTx, createTx; 
    uint256 hashBlock, assetid, assetid2; 
	int32_t numvins, numvouts, preventCCvins, preventCCvouts; 
	int64_t unit_price, vin_unit_price; 
    std::vector<uint8_t> origpubkey, vinorigpubkey, vopretNonfungible;
	uint8_t funcid, evalCodeInOpret; 
	char destaddr[64], origNormalAddr[64], origTokensCCaddr[64], origCCaddrDummy[64]; 
    char tokensDualEvalUnspendableCCaddr[64], origAssetsCCaddr[64];

    CAmount tokens = 0;
    CAmount assetoshis = 0;

    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    
    // add specific chains exceptions for old token support:
    if (strcmp(ASSETCHAINS_SYMBOL, "SEC") == 0 && chainActive.Height() <= 144073)
        return true;
    
    if (strcmp(ASSETCHAINS_SYMBOL, "MGNX") == 0 && chainActive.Height() <= 210190)
        return true;
        
	if (numvouts == 0)
		return eval->Invalid("AssetValidate: no vouts");

    if((funcid = DecodeAssetTokenOpRet(tx.vout[numvouts-1].scriptPubKey, evalCodeInOpret, assetid, assetid2, unit_price, origpubkey)) == 0 )
        return eval->Invalid("AssetValidate: invalid opreturn payload");

    // non-fungible tokens support:
    GetNonfungibleData(assetid, vopretNonfungible);
    if (vopretNonfungible.size() > 0)
        cpAssets->evalcodeNFT = vopretNonfungible.begin()[0];

	// find dual-eval tokens global addr where tokens are locked:
	GetTokensCCaddress(cpAssets, tokensDualEvalUnspendableCCaddr, GetUnspendable(cpAssets, NULL));
    // originator cc address, this is for marker validation:
    GetCCaddress(cpAssets, origAssetsCCaddr, origpubkey); 

	// we need this for validating single-eval tokens' vins/vous:
	struct CCcontract_info *cpTokens, tokensC;
	cpTokens = CCinit(&tokensC, EVAL_TOKENS);

    //fprintf(stderr,"AssetValidate (%c)\n",funcid);

    // we do not need createTx (it is checked in cc tokens)
    //if( funcid != 'o' && funcid != 'x' && eval->GetTxUnconfirmed(assetid, createTx, hashBlock) == 0 )
    //    return eval->Invalid("cant find asset create txid");
    //else if( funcid != 'o' && funcid != 'x' && assetid2 != zero && eval->GetTxUnconfirmed(assetid2, createTx, hashBlock) == 0 )
    //    return eval->Invalid("cant find asset2 create txid");
    // else 

    if( IsCCInput(tx.vin[0].scriptSig) != 0 )   // vin0 should be normal vin
        return eval->Invalid("illegal asset vin0");
    else if( numvouts < 2 )
        return eval->Invalid("too few vouts");  // it was if(numvouts < 1) but it refers at least to vout[1] below
    
    if (assetid == zeroid)
        return eval->Invalid("illegal assetid");
    //if (!AssetGetCCInputs(cpAssets, tokensInputs, assetsInputs, eval, tx, assetid))  // set tokensInputs and assetsInputs
    //    return false;  // returns false if some problems with reading vintxes
    

    switch( funcid )
    {
        case 'c': 
            // see token cc
			return eval->Invalid("invalid asset funcid \'c\'");
            
        case 't': 
            // see token cc
			return eval->Invalid("invalid asset funcid \'t\'");
                        
        case 'b': // buyoffer
            //vins.*: normal inputs (bid + change)
            //vout.0: amount of bid to cc assets global address
            //vout.1: CC output for marker
            //vout.2: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['b'] [assetid] [amount of asset required] [origpubkey]

            // as we don't use tokenconvert 'b' does not have cc inputs and we should not be here:
            return eval->Invalid("invalid asset funcid (b)");
            
        case 'o': // cancelbuy
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
            //vin.2: CC marker from buyoffer for txfee
            //vout.0: vin.1 value to original pubkey buyTx.vout[0].nValue -> [origpubkey]
            //vout.1: vin.2 back to users pubkey
            //vout.2: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['o']
            if (assetoshis = AssetValidateBuyvin(cpAssets, eval, vin_unit_price, vinorigpubkey, origCCaddrDummy, origNormalAddr, tx, assetid) == 0)
                return(false);
            else if( ConstrainVout(tx.vout[0], 0, origNormalAddr, assetoshis) == false )
                return eval->Invalid("invalid refund for cancelbuy");
            else if( TotalPubkeyNormalInputs(tx, pubkey2pk(vinorigpubkey)) == 0 ) // check tx is signed by originator pubkey
                return eval->Invalid("not the owner pubkey");

            preventCCvins = 3;
            preventCCvouts = 0;
            //fprintf(stderr,"cancelbuy validated to origaddr.(%s)\n",origNormalAddr);
            break;
            
        case 'B': // fillbuy:
            //vin.0: normal input
            //vin.1: cc assets unspendable (vout.0 from buyoffer) buyTx.vout[0]
            //vin.2+: valid CC output satisfies buyoffer (*tx.vin[2])->nValue
            //vout.0: remaining amount of bid to cc assets global address
            //vout.1: vin.1 value to signer of vin.2
            //vout.2: vin.2 assetoshis to original pubkey
            //vout.3: CC output for assetoshis change (if any)
            //vout.4: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]
            preventCCvouts = 4;
			
            if( (assetoshis = AssetValidateBuyvin(cpAssets, eval, vin_unit_price, vinorigpubkey, origTokensCCaddr, origNormalAddr, tx, assetid)) == 0 )
                return false;
            else if( numvouts < 4 )
                return eval->Invalid("not enough vouts for fillbuy");
            else if( vinorigpubkey != origpubkey )    // originator pk does not change in the new opret
                return eval->Invalid("mismatched origpubkeys for fillbuy");
            else if (unit_price != vin_unit_price)
                return eval->Invalid("mismatched unit price for fillbuy");
            else
            {
                tokens = AssetsGetCCInputs(cpTokens, NULL, tx);
                if( assetoshis != tx.vout[0].nValue + tx.vout[1].nValue )             // coins -> global cc address (remainder) + normal self address
                    return eval->Invalid("locked value doesnt match vout0+1 fillbuy");
                else if( tx.vout[4].scriptPubKey.IsPayToCryptoCondition() != false )    // if token remainder exists 
                {
                    if( ConstrainVout(tx.vout[2], 1, origTokensCCaddr, 0) == false )	// tokens to originator cc addr (tokens+nonfungible evalcode)
                        return eval->Invalid("vout2 doesnt go to origpubkey fillbuy");
                    else if( tokens != tx.vout[2].nValue + tx.vout[4].nValue )    // tokens from cc global address -> token global addr (remainder) + originator cc address
                        return eval->Invalid("tokens inputs doesnt match vout2+3 fillbuy");
                    preventCCvouts ++;
                }
                else if( ConstrainVout(tx.vout[2], 1, origTokensCCaddr, tokens) == false )   // all tokens to originator cc addr, no cc change present
                    return eval->Invalid("vout2 doesnt match tokens inputs fillbuy");

                if( ConstrainVout(tx.vout[1], 0, NULL, 0) == false )                            // amount paid for tokens goes to normal addr (we can't check 'self' address)
                    return eval->Invalid("vout1 should be normal for fillbuy");
                else if( ConstrainVout(tx.vout[3], 1, origAssetsCCaddr, ASSETS_MARKER_AMOUNT) == false )       // marker to originator asset cc addr
                    return eval->Invalid("invalid marker for original pubkey");
                else if( ValidateBidRemainder(unit_price, tx.vout[0].nValue, assetoshis, tx.vout[1].nValue, tx.vout[2].nValue) == false ) // check real price and coins spending from global addr
                    return eval->Invalid("mismatched remainder for fillbuy");
                else { 
                    if( tx.vout[0].nValue  / unit_price != 0 ) // remaining tokens to buy
                    {
                        if( ConstrainVout(tx.vout[0], 1, cpAssets->unspendableCCaddr, 0) == false )  // if remainder sufficient to buy tokens -> coins to asset global cc addr
                            return eval->Invalid("mismatched vout0 AssetsCCaddr for fillbuy");
                    }
                    else 
                    {   // remaining_units == 0
                        if( ConstrainVout(tx.vout[0], 0, origNormalAddr, 0) == false )  // remainder less than token price should return to originator normal addr
                            return eval->Invalid("vout0 should be original normal address for fillbuy");
                    }
                }
            }
            //fprintf(stderr,"fillbuy validated\n");
            break;
        //case 'e': // selloffer
        //    break; // disable swaps
        case 's': // selloffer
            //vin.0: normal input
            //vin.1+: valid CC output for sale
            //vout.0: vin.1 assetoshis output to CC assets global address
            //vout.1: CC output to CC assets global address for marker
            //vout.2: CC output for change (if any)
            //vout.3: normal output for change (if any)
            //'s'.vout.n-1: opreturn [EVAL_ASSETS] ['s'] [assetid] [amount of native coin required] [origpubkey]
            //'e'.vout.n-1: opreturn [EVAL_ASSETS] ['e'] [assetid] [assetid2] [amount of asset2 required] [origpubkey]

            // as we don't use tokenconvert 's' does not have cc inputs and we should not be here:
            return eval->Invalid("invalid asset funcid (s)");
            
        case 'x': // cancel sell            
            //vin.0: normal input
            //vin.1: cc assets global address (vout.0 from exchange or selloffer) sellTx/exchangeTx.vout[0] inputTx
            //vin.2: CC marker from selloffer for txfee
            //vout.0: vin.1 assetoshis to original pubkey CC sellTx/exchangeTx.vout[0].nValue -> [origpubkey]
            //vout.1: vin.2 back to users pubkey
            //vout.2: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['x'] [assetid]

            if( (tokens = AssetValidateSellvin(cpAssets, eval, vin_unit_price, vinorigpubkey, origTokensCCaddr, origNormalAddr, tx, assetid)) == 0 )  // NOTE: 
                return false;
            else if (ConstrainVout(tx.vout[0], 1, origTokensCCaddr, tokens) == false)      // tokens returning to originator cc addr
                return eval->Invalid("invalid vout for cancel");
            else if (TotalPubkeyNormalInputs(tx, pubkey2pk(vinorigpubkey)) == 0)  // check tx is signed by originator pubkey
                return eval->Invalid("not the owner pubkey");
            preventCCvins = 3;
            preventCCvouts = 1;
            break;
            
        case 'S': // fillsell
            //vin.0: normal input
            //vin.1: cc assets global address (vout.0 assetoshis from selloffer) sellTx.vout[0]
            //'S'.vin.2+: normal output that satisfies selloffer (*tx.vin[2])->nValue
            //vout.0: remaining assetoshis -> cc assets global address
            //vout.1: vin.1 assetoshis to signer of vin.2 sellTx.vout[0].nValue -> any
            //'S'.vout.2: vin.2 value to original pubkey [origpubkey]
            //vout.3: normal output for change (if any)
            //'S'.vout.n-1: opreturn [EVAL_ASSETS] ['S'] [assetid] [amount of coin still required] [origpubkey]
			
            if( (tokens = AssetValidateSellvin(cpAssets, eval, vin_unit_price, vinorigpubkey, origTokensCCaddr, origNormalAddr, tx, assetid)) == 0 )
                return false;
            else if( numvouts < 4 )
                return eval->Invalid("not enough vouts for fillask");
            else if( vinorigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for fillask");
            else if (unit_price != vin_unit_price)
                return eval->Invalid("mismatched unit price for fillask");
            else
            {
                if( tokens != tx.vout[0].nValue + tx.vout[1].nValue )
                    return eval->Invalid("locked value doesnt match vout0+1 fillask");
                if( ValidateAskRemainder(unit_price, tx.vout[0].nValue, tokens, tx.vout[1].nValue, tx.vout[2].nValue) == false )
                    return eval->Invalid("mismatched remainder for fillask");
                else if( ConstrainVout(tx.vout[1], 1, NULL, 0) == false )                  // do not check tokens buyer's 'self' cc addr
                    return eval->Invalid("normal vout1 for fillask");
                else if( ConstrainVout(tx.vout[2], 0, origNormalAddr, 0) == false )        // coins to originator normal addr
                    return eval->Invalid("normal vout1 for fillask");
                else if( ConstrainVout(tx.vout[3], 1, origAssetsCCaddr, 10000) == false )  // marker to originator asset cc addr
                    return eval->Invalid("invalid marker for original pubkey");
                else // if( remaining_units != 0 )  // remaining expected amount in coins, should be anyway
                {
                    if( ConstrainVout(tx.vout[0], 1, tokensDualEvalUnspendableCCaddr, 0) == false )   // tokens remainder on global addr
                        return eval->Invalid("mismatched vout0 assets tokens dual unspendable CCaddr for fill sell");
                }
            }
            //fprintf(stderr,"fill validated\n");
            break;
        case 'E': // fillexchange	
			////////// not implemented yet ////////////
            return eval->Invalid("unexpected assets fillexchange funcid");
            break; // disable asset swaps
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 assetoshis from selloffer) sellTx.vout[0]
            //vin.2+: valid CC assetid2 output that satisfies exchange (*tx.vin[2])->nValue
            //vout.0: remaining assetoshis -> unspendable
            //vout.1: vin.1 assetoshis to signer of vin.2 sellTx.vout[0].nValue -> any
            //vout.2: vin.2+ assetoshis2 to original pubkey [origpubkey]
            //vout.3: CC output for asset2 change (if any)
            //vout.3/4: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['E'] [assetid vin0+1] [assetid vin2] [remaining asset2 required] [origpubkey]
            
			//if ( AssetExactAmounts(false, cp,inputs,outputs,eval,tx,assetid2) == false )    
            //    eval->Invalid("asset2 inputs != outputs");

			////////// not implemented yet ////////////
            if( (tokens = AssetValidateSellvin(cpTokens, eval, vin_unit_price, vinorigpubkey, origTokensCCaddr, origNormalAddr, tx, assetid)) == 0 )
                return(false);
            else if( numvouts < 3 )
                return eval->Invalid("not enough vouts for fillex");
            else if( vinorigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for fillex");
            else if (unit_price != vin_unit_price)
                return eval->Invalid("mismatched unit price for fillex");
            else
            {
                /*
                if( tokens != tx.vout[0].nValue + tx.vout[1].nValue )
                    return eval->Invalid("locked value doesnt match vout0+1 fillex");
                else if( tx.vout[3].scriptPubKey.IsPayToCryptoCondition() != false )
				////////// not implemented yet ////////////
                {
                    if( ConstrainVout(tx.vout[2], 1, origTokensCCaddr, 0) == false )
                        return eval->Invalid("vout2 doesnt go to origpubkey fillex");
                    else if( tokens != tx.vout[2].nValue + tx.vout[3].nValue )
                    {
                        fprintf(stderr,"inputs %.8f != %.8f + %.8f\n",(double)tokens/COIN,(double)tx.vout[2].nValue/COIN,(double)tx.vout[3].nValue/COIN);
                        return eval->Invalid("asset inputs doesnt match vout2+3 fillex");
                    }
                }
				////////// not implemented yet ////////////
                else if( ConstrainVout(tx.vout[2], 1, origTokensCCaddr, tokens) == false )  
                    return eval->Invalid("vout2 doesnt match inputs fillex");
                else if( ConstrainVout(tx.vout[1], 0, 0, 0) == false )
                    return eval->Invalid("vout1 is CC for fillex");
                fprintf(stderr,"assets vout0 %lld, tokens %lld, vout2 %lld -> orig, vout1 %lld, total %lld\n", (long long)tx.vout[0].nValue, (long long)tokens,(long long)tx.vout[2].nValue,(long long)tx.vout[1].nValue,(long long)orig_units);
                if( ValidateSwapRemainder(remaining_units, tx.vout[0].nValue, tokens, tx.vout[1].nValue, tx.vout[2].nValue, orig_units) == false )
                //    return eval->Invalid("mismatched remainder for fillex");
                else if( ConstrainVout(tx.vout[1], 1, 0, 0) == false )
				////////// not implemented yet ////////////
                    return eval->Invalid("normal vout1 for fillex");
                else if( remaining_units != 0 ) 
                {
                    if( ConstrainVout(tx.vout[0], 1, (char *)cpAssets->unspendableCCaddr, 0) == false )  // TODO: unsure about this, but this is not impl yet anyway
                        return eval->Invalid("mismatched vout0 AssetsCCaddr for fillex");
                }
                */
            }
			////////// not implemented yet ////////////
            //fprintf(stderr,"fill validated\n");
            break;

        default:
            fprintf(stderr,"illegal assets funcid.(%c)\n",funcid);
            return eval->Invalid("unexpected assets funcid");
            //break;
    }

	bool bPrevent = PreventCC(eval, tx, preventCCvins, numvins, preventCCvouts, numvouts);  // prevent presence of unknown cc vin or cc vouts in the tx
	//std::cerr << "AssetsValidate() PreventCC returned=" << bPrevent << std::endl;
	return (bPrevent);
}

// redirect to AssetsValidateInternal and log error
bool AssetsValidate(struct CCcontract_info *cpAssets, Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    if (!TokensIsVer1Active(NULL))   {	 
        bool valid = tokensv0::AssetsValidate(cpAssets, eval, tx, nIn); // call assets validation version 0
        if (!valid) 
            LOGSTREAMFN(ccassets_log, CCLOG_ERROR, stream << "v0 validation error: " << eval->state.GetRejectReason() << ", code: " << eval->state.GetRejectCode() << ", tx: " << HexStr(E_MARSHAL(ss << tx)) << std::endl);
        return valid;
    }

    if (!AssetsValidateInternal(cpAssets, eval, tx, nIn))    {
        LOGSTREAMFN(ccassets_log, CCLOG_ERROR, stream << "validation error: " << eval->state.GetRejectReason() << ", code: " << eval->state.GetRejectCode() << ", tx: " << HexStr(E_MARSHAL(ss << tx)) << std::endl);
        return false;
    }
    return true;
}
