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

/*
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
 
 create
 vin.0: normal input
 vout.0: issuance assetoshis to CC
 vout.1: tag sent to normal address of AssetsCCaddress
 vout.2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['c'] [origpubkey] "<assetname>" "<description>"
 
 transfer
 vin.0: normal input
 vin.1 .. vin.n-1: valid CC outputs
 vout.0 to n-2: assetoshis output to CC
 vout.n-2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['t'] [assetid]
 
 buyoffer:
 vins.*: normal inputs (bid + change)
 vout.0: amount of bid to unspendable
 vout.1: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['b'] [assetid] [amount of asset required] [origpubkey]

 cancelbuy:
 vin.0: normal input
 vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
 vout.0: vin.1 value to original pubkey buyTx.vout[0].nValue -> [origpubkey]
 vout.1: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['o'] [assetid]
 
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
 vout.1: CC output for change (if any)
 vout.2: normal output for change (if any)
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
 vout.0: vin.1 assetoshis to original pubkey CC sellTx/exchangeTx.vout[0].nValue -> [origpubkey]
 vout.1: normal output for change (if any)
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

bool AssetsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)
{
    static uint256 zero;
    CTxDestination address; CTransaction vinTx,createTx; uint256 hashBlock,assetid,assetid2; int32_t i,starti,numvins,numvouts,preventCCvins,preventCCvouts; int64_t remaining_price,nValue,assetoshis,outputs,inputs,tmpprice,totalunits,ignore; std::vector<uint8_t> origpubkey,tmporigpubkey,ignorepubkey; uint8_t funcid; char destaddr[64],origaddr[64],CCaddr[64];
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    outputs = inputs = 0;
    preventCCvins = preventCCvouts = -1;
    if ( (funcid= DecodeAssetOpRet(tx.vout[numvouts-1].scriptPubKey,assetid,assetid2,remaining_price,origpubkey)) == 0 )
        return eval->Invalid("Invalid opreturn payload");
    fprintf(stderr,"AssetValidate (%c)\n",funcid);
    if ( funcid != 'o' && funcid != 'x' && eval->GetTxUnconfirmed(assetid,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset create txid");
    else if ( funcid != 'o' && funcid != 'x' && assetid2 != zero && eval->GetTxUnconfirmed(assetid2,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset2 create txid");
    else if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
        return eval->Invalid("illegal asset vin0");
    else if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else if ( funcid != 'c' )
    {
        if ( funcid == 't' )
            starti = 0;
        else starti = 1;
        if ( assetid == zero )
            return eval->Invalid("illegal assetid");
        else if ( AssetExactAmounts(cp,inputs,starti,outputs,eval,tx,assetid) == false )
            return eval->Invalid("asset inputs != outputs");
    }
    switch ( funcid )
    {
        case 'c': // create wont be called to be verified as it has no CC inputs
            //vin.0: normal input
            //vout.0: issuance assetoshis to CC
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['c'] [{"<assetname>":"<description>"}]
            return eval->Invalid("unexpected AssetValidate for createasset");
            break;
            
        case 't': // transfer
            //vin.0: normal input
            //vin.1 .. vin.n-1: valid CC outputs
            //vout.0 to n-2: assetoshis output to CC
            //vout.n-2: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['t'] [assetid]
            if ( inputs == 0 )
                return eval->Invalid("no asset inputs for transfer");
            fprintf(stderr,"transfer validated %.8f -> %.8f\n",(double)inputs/COIN,(double)outputs/COIN);
            break;
            
        case 'b': // buyoffer
            //vins.*: normal inputs (bid + change)
            //vout.0: amount of bid to unspendable
            //vout.1: normal output for change (if any)
            // vout.n-1: opreturn [EVAL_ASSETS] ['b'] [assetid] [amount of asset required] [origpubkey]
            if ( remaining_price == 0 )
                return eval->Invalid("illegal null amount for buyoffer");
            else if ( ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,0) == 0 )
                return eval->Invalid("invalid vout for buyoffer");
            preventCCvins = 1;
            preventCCvouts = 1;
            fprintf(stderr,"buy offer validated to destaddr.(%s)\n",cp->unspendableCCaddr);
            break;
            
        case 'o': // cancelbuy
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
            //vout.0: vin.1 value to original pubkey buyTx.vout[0].nValue -> [origpubkey]
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['o']
            if ( (nValue= AssetValidateBuyvin(cp,eval,tmpprice,tmporigpubkey,CCaddr,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( ConstrainVout(tx.vout[0],0,origaddr,nValue) == 0 )
                return eval->Invalid("invalid refund for cancelbuy");
            preventCCvins = 2;
            preventCCvouts = 0;
            fprintf(stderr,"cancelbuy validated to origaddr.(%s)\n",origaddr);
            break;
            
        case 'B': // fillbuy:
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
            //vin.2+: valid CC output satisfies buyoffer (*tx.vin[2])->nValue
            //vout.0: remaining amount of bid to unspendable
            //vout.1: vin.1 value to signer of vin.2
            //vout.2: vin.2 assetoshis to original pubkey
            //vout.3: CC output for assetoshis change (if any)
            //vout.4: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]
            preventCCvouts = 4;
            if ( (nValue= AssetValidateBuyvin(cp,eval,totalunits,tmporigpubkey,CCaddr,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( numvouts < 3 )
                return eval->Invalid("not enough vouts for fillbuy");
            else if ( tmporigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for fillbuy");
            else
            {
                if ( nValue != tx.vout[0].nValue+tx.vout[1].nValue )
                    return eval->Invalid("locked value doesnt match vout0+1 fillbuy");
                else if ( tx.vout[3].scriptPubKey.IsPayToCryptoCondition() != 0 )
                {
                    if ( ConstrainVout(tx.vout[2],1,CCaddr,0) == 0 )
                        return eval->Invalid("vout2 doesnt go to origpubkey fillbuy");
                    else if ( inputs != tx.vout[2].nValue+tx.vout[3].nValue )
                        return eval->Invalid("asset inputs doesnt match vout2+3 fillbuy");
                }
                else if ( ConstrainVout(tx.vout[2],1,CCaddr,inputs) == 0 )
                    return eval->Invalid("vout2 doesnt match inputs fillbuy");
                else if ( ConstrainVout(tx.vout[1],0,0,0) == 0 )
                    return eval->Invalid("vout1 is CC for fillbuy");
                else if ( ValidateBidRemainder(remaining_price,tx.vout[0].nValue,nValue,tx.vout[1].nValue,tx.vout[2].nValue,totalunits) == false )
                    return eval->Invalid("mismatched remainder for fillbuy");
                else if ( remaining_price != 0 )
                {
                    if ( ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,0) == 0 )
                        return eval->Invalid("mismatched vout0 AssetsCCaddr for fillbuy");
                }
            }
            fprintf(stderr,"fillbuy validated\n");
            break;
        case 'e': // selloffer
            break; // disable swaps
        case 's': // selloffer
            //vin.0: normal input
            //vin.1+: valid CC output for sale
            //vout.0: vin.1 assetoshis output to CC to unspendable
            //vout.1: CC output for change (if any)
            //vout.2: normal output for change (if any)
            //'s'.vout.n-1: opreturn [EVAL_ASSETS] ['s'] [assetid] [amount of native coin required] [origpubkey]
            //'e'.vout.n-1: opreturn [EVAL_ASSETS] ['e'] [assetid] [assetid2] [amount of asset2 required] [origpubkey]
            preventCCvouts = 1;
            if ( remaining_price == 0 )
                return eval->Invalid("illegal null remaining_price for selloffer");
            if ( tx.vout[1].scriptPubKey.IsPayToCryptoCondition() != 0 )
            {
                preventCCvouts++;
                if ( ConstrainVout(tx.vout[0],1,(char *)cp->unspendableCCaddr,0) == 0 )
                    return eval->Invalid("mismatched vout0 AssetsCCaddr for selloffer");
                else if ( tx.vout[0].nValue+tx.vout[1].nValue != inputs )
                    return eval->Invalid("mismatched vout0+vout1 total for selloffer");
            } else if ( ConstrainVout(tx.vout[0],1,(char *)cp->unspendableCCaddr,inputs) == 0 )
                return eval->Invalid("mismatched vout0 AssetsCCaddr for selloffer");
            //fprintf(stderr,"remaining.%d for sell\n",(int32_t)remaining_price);
            break;
            
        case 'x': // cancel
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from exchange or selloffer) sellTx/exchangeTx.vout[0] inputTx
            //vout.0: vin.1 assetoshis to original pubkey CC sellTx/exchangeTx.vout[0].nValue -> [origpubkey]
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['x'] [assetid]
            if ( (assetoshis= AssetValidateSellvin(cp,eval,tmpprice,tmporigpubkey,CCaddr,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( ConstrainVout(tx.vout[0],1,CCaddr,assetoshis) == 0 )
                return eval->Invalid("invalid vout for cancel");
            preventCCvins = 2;
            preventCCvouts = 1;
            break;
            
        case 'S': // fillsell
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 assetoshis from selloffer) sellTx.vout[0]
            //'S'.vin.2+: normal output that satisfies selloffer (*tx.vin[2])->nValue
            //vout.0: remaining assetoshis -> unspendable
            //vout.1: vin.1 assetoshis to signer of vin.2 sellTx.vout[0].nValue -> any
            //'S'.vout.2: vin.2 value to original pubkey [origpubkey]
            //vout.3: normal output for change (if any)
            //'S'.vout.n-1: opreturn [EVAL_ASSETS] ['S'] [assetid] [amount of coin still required] [origpubkey]
            if ( (assetoshis= AssetValidateSellvin(cp,eval,totalunits,tmporigpubkey,CCaddr,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( numvouts < 3 )
                return eval->Invalid("not enough vouts for fillask");
            else if ( tmporigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for fillask");
            else
            {
                if ( assetoshis != tx.vout[0].nValue+tx.vout[1].nValue )
                    return eval->Invalid("locked value doesnt match vout0+1 fillask");
                if ( ValidateAskRemainder(remaining_price,tx.vout[0].nValue,assetoshis,tx.vout[1].nValue,tx.vout[2].nValue,totalunits) == false )
                    return eval->Invalid("mismatched remainder for fillask");
                else if ( ConstrainVout(tx.vout[1],1,0,0) == 0 )
                    return eval->Invalid("normal vout1 for fillask");
                else if ( ConstrainVout(tx.vout[2],0,origaddr,0) == 0 )
                    return eval->Invalid("normal vout1 for fillask");
                else if ( remaining_price != 0 )
                {
                    if ( ConstrainVout(tx.vout[0],1,(char *)cp->unspendableCCaddr,0) == 0 )
                        return eval->Invalid("mismatched vout0 AssetsCCaddr for fill");
                }
            }
            fprintf(stderr,"fill validated\n");
            break;
        case 'E': // fillexchange
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
            if ( AssetExactAmounts(cp,inputs,1,outputs,eval,tx,assetid2) == false )
                eval->Invalid("asset2 inputs != outputs");
            if ( (assetoshis= AssetValidateSellvin(cp,eval,totalunits,tmporigpubkey,CCaddr,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( numvouts < 3 )
                return eval->Invalid("not enough vouts for fillex");
            else if ( tmporigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for fillex");
            else
            {
                if ( assetoshis != tx.vout[0].nValue+tx.vout[1].nValue )
                    return eval->Invalid("locked value doesnt match vout0+1 fillex");
                else if ( tx.vout[3].scriptPubKey.IsPayToCryptoCondition() != 0 )
                {
                    if ( ConstrainVout(tx.vout[2],1,CCaddr,0) == 0 )
                        return eval->Invalid("vout2 doesnt go to origpubkey fillex");
                    else if ( inputs != tx.vout[2].nValue+tx.vout[3].nValue )
                    {
                        fprintf(stderr,"inputs %.8f != %.8f + %.8f\n",(double)inputs/COIN,(double)tx.vout[2].nValue/COIN,(double)tx.vout[3].nValue/COIN);
                        return eval->Invalid("asset inputs doesnt match vout2+3 fillex");
                    }
                }
                else if ( ConstrainVout(tx.vout[2],1,CCaddr,inputs) == 0 )
                    return eval->Invalid("vout2 doesnt match inputs fillex");
                else if ( ConstrainVout(tx.vout[1],0,0,0) == 0 )
                    return eval->Invalid("vout1 is CC for fillex");
                fprintf(stderr,"assets vout0 %llu, vin1 %llu, vout2 %llu -> orig, vout1 %llu, total %llu\n",(long long)tx.vout[0].nValue,(long long)assetoshis,(long long)tx.vout[2].nValue,(long long)tx.vout[1].nValue,(long long)totalunits);
                if ( ValidateSwapRemainder(remaining_price,tx.vout[0].nValue,assetoshis,tx.vout[1].nValue,tx.vout[2].nValue,totalunits) == false )
                    return eval->Invalid("mismatched remainder for fillex");
                else if ( ConstrainVout(tx.vout[1],1,0,0) == 0 )
                    return eval->Invalid("normal vout1 for fillex");
                else if ( remaining_price != 0 )
                {
                    if ( ConstrainVout(tx.vout[0],1,(char *)cp->unspendableCCaddr,0) == 0 )
                        return eval->Invalid("mismatched vout0 AssetsCCaddr for fillex");
                }
            }
            fprintf(stderr,"fill validated\n");
            break;
    }
    return(PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts));
}


