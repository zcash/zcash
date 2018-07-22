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
 vout.1: normal output for change (if any)
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
 vout.3: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]

 selloffer:
 vin.0: normal input
 vin.1: valid CC output for sale
 vout.0: vin.1 assetoshis output to CC to unspendable
 vout.1: CC output for change (if any)
 vout.2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['s'] [assetid] [amount of native coin required] [origpubkey]
 
 exchange:
 vin.0: normal input
 vin.1: valid CC output
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
 vout.3: normal output for change (if any)
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

bool AssetValidate(Eval* eval,CTransaction &tx,int32_t numvouts,uint8_t funcid,uint256 assetid,uint256 assetid2,uint64_t remaining_price,std::vector<uint8_t> origpubkey)
{
    static uint256 zero;
    CTxDestination address; CTransaction vinTx; uint256 hashBlock; int32_t i,numvins,preventCCvins,preventCCvouts; uint64_t nValue,assetoshis,outputs,inputs,tmpprice,ignore; std::vector<uint8_t> tmporigpubkey,ignorepubkey; char destaddr[64],origaddr[64],CCaddr[64];
    fprintf(stderr,"AssetValidate (%c)\n",funcid);
    numvins = tx.vin.size();
    outputs = inputs = 0;
    preventCCvins = preventCCvouts = -1;
    if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
        return eval->Invalid("illegal asset vin0");
    else if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else if ( funcid != 'c' )
    {
        if ( assetid == zero )
            return eval->Invalid("illegal assetid");
        else if ( AssetExactAmounts(eval,tx,assetid) == false )
            eval->Invalid("asset inputs != outputs");
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
            else if ( ConstrainAssetVout(tx.vout[0],1,(char *)AssetsCCaddr,0) == 0 )
                return eval->Invalid("invalid vout for buyoffer");
            preventCCvins = 1;
            preventCCvouts = 1;
            fprintf(stderr,"buy offer validated to destaddr.(%s)\n",(char *)AssetsCCaddr);
            break;
            
        case 'o': // cancelbuy
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
            //vout.0: vin.1 value to original pubkey buyTx.vout[0].nValue -> [origpubkey]
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['o']
            if ( (nValue= AssetValidateBuyvin(eval,tmpprice,tmporigpubkey,CCaddr,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( tmporigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for cancelbuy");
            else if ( ConstrainAssetVout(tx.vout[0],0,origaddr,nValue) == 0 )
                return eval->Invalid("invalid refund for cancelbuy");
            preventCCvins = 1;
            preventCCvouts = 0;
            fprintf(stderr,"cancelbuy validated to destaddr.(%s)\n",destaddr);
            break;
            
        case 'B': // fillbuy:
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
            //vin.2+: valid CC output satisfies buyoffer (*tx.vin[2])->nValue
            //vout.0: remaining amount of bid to unspendable
            //vout.1: vin.1 value to signer of vin.2
            //vout.2: vin.2 assetoshis to original pubkey
            //vout.3: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]
            preventCCvouts = 4;
            if ( (nValue= AssetValidateBuyvin(eval,tmpprice,tmporigpubkey,CCaddr,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( numvouts < 3 )
                return eval->Invalid("not enough vouts for fillbuy");
            else if ( tmporigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for fillbuy");
            else
            {
                if ( ConstrainAssetVout(tx.vout[1],0,0,0) == 0 )
                    return eval->Invalid("vout1 is CC for fillbuy");
                else if ( ConstrainAssetVout(tx.vout[2],1,CCaddr,0) == 0 )
                    return eval->Invalid("vout2 is normal for fillbuy");
                else if ( ValidateAssetRemainder(remaining_price,tx.vout[0].nValue,nValue,tx.vout[1].nValue,tx.vout[2].nValue,tmpprice) == false )
                    return eval->Invalid("mismatched remainder for fillbuy");
                else if ( remaining_price != 0 )
                {
                    if ( remaining_price < 10000 )
                        return eval->Invalid("dust vout0 to AssetsCCaddr for fillbuy");
                    else if ( ConstrainAssetVout(tx.vout[0],1,(char *)AssetsCCaddr,0) == 0 )
                        return eval->Invalid("mismatched vout0 AssetsCCaddr for fillbuy");
                }
            }
            fprintf(stderr,"fillbuy validated\n");
            break;
            
        case 's': // selloffer
        case 'e': // exchange
            //vin.0: normal input
            //vin.1: valid CC output for sale
            //vout.0: vin.1 assetoshis output to CC to unspendable
            //vout.1: normal output for change (if any)
            //'s'.vout.n-1: opreturn [EVAL_ASSETS] ['s'] [assetid] [amount of native coin required] [origpubkey]
            //'e'.vout.n-1: opreturn [EVAL_ASSETS] ['e'] [assetid] [assetid2] [amount of asset2 required] [origpubkey]
            if ( remaining_price == 0 )
                return eval->Invalid("illegal null remaining_price for selloffer");
            else if ( ConstrainAssetVout(tx.vout[0],1,(char *)AssetsCCaddr,0) == 0 )
                return eval->Invalid("mismatched vout0 AssetsCCaddr for selloffer");
            preventCCvouts = 1;
            break;
            
        case 'x': // cancel
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from exchange or selloffer) sellTx/exchangeTx.vout[0] inputTx
            //vout.0: vin.1 assetoshis to original pubkey CC sellTx/exchangeTx.vout[0].nValue -> [origpubkey]
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['x'] [assetid]
            if ( (assetoshis= AssetValidateSellvin(eval,tmpprice,tmporigpubkey,CCaddr,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( tmporigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for cancel");
            else if ( ConstrainAssetVout(tx.vout[0],1,CCaddr,assetoshis) == 0 )
                return eval->Invalid("invalid vout for cancel");
            preventCCvins = 2;
            preventCCvouts = 1;
            break;
            
        case 'S': // fillsell
        case 'E': // fillexchange
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 assetoshis from selloffer) sellTx.vout[0]
            //'S'.vin.2+: normal output that satisfies selloffer (*tx.vin[2])->nValue
            //'E'.vin.2+: valid CC assetid2 output that satisfies exchange (*tx.vin[2])->nValue
            //vout.0: remaining assetoshis -> unspendable
            //vout.1: vin.1 assetoshis to signer of vin.2 sellTx.vout[0].nValue -> any
            //'S'.vout.2: vin.2 value to original pubkey [origpubkey]
            //'E'.vout.2: vin.2 assetoshis2 to original pubkey [origpubkey]
            //vout.3: normal output for change (if any)
            //'S'.vout.n-1: opreturn [EVAL_ASSETS] ['S'] [assetid] [amount of coin still required] [origpubkey]
            //'E'.vout.n-1: opreturn [EVAL_ASSETS] ['E'] [assetid vin0+1] [assetid vin2] [remaining asset2 required] [origpubkey]
            if ( funcid == 'E' )
            {
                if ( AssetExactAmounts(eval,tx,assetid2) == false )
                    eval->Invalid("asset2 inputs != outputs");
            }
            if ( (assetoshis= AssetValidateSellvin(eval,tmpprice,tmporigpubkey,CCaddr,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( numvouts < 3 )
                return eval->Invalid("not enough vouts for fill");
            else if ( tmporigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for fill");
            else
            {
                if ( ValidateAssetRemainder(remaining_price,tx.vout[0].nValue,nValue,tx.vout[1].nValue,tx.vout[2].nValue,tmpprice) == false )
                    return eval->Invalid("mismatched remainder for fill");
                else if ( ConstrainAssetVout(tx.vout[1],1,0,0) == 0 )
                    return eval->Invalid("normal vout1 for fillask");
                else if ( funcid == 'E' && ConstrainAssetVout(tx.vout[2],1,CCaddr,0) == 0 )
                    return eval->Invalid("normal vout2 for fillask");
                else if ( funcid == 'S' && ConstrainAssetVout(tx.vout[2],0,origaddr,0) == 0 )
                    return eval->Invalid("CC vout2 for fillask");
                else if ( remaining_price != 0 )
                {
                    if ( remaining_price < 10000 )
                        return eval->Invalid("dust vout0 to AssetsCCaddr for fill");
                    else if ( ConstrainAssetVout(tx.vout[0],1,(char *)AssetsCCaddr,0) == 0 )
                        return eval->Invalid("mismatched vout0 AssetsCCaddr for fill");
                }
            }
            fprintf(stderr,"fill validated\n");
            break;
    }
    if ( preventCCvins >= 0 )
    {
        for (i=preventCCvins; i<numvins; i++)
        {
            if ( IsCCInput(tx.vin[i].scriptSig) != 0 )
                return eval->Invalid("invalid CC vin");
        }
    }
    if ( preventCCvouts >= 0 )
    {
        for (i=preventCCvouts; i<numvouts; i++)
        {
            if ( tx.vout[i].scriptPubKey.IsPayToCryptoCondition() != 0 )
                return eval->Invalid("invalid CC vout");
        }
    }
    return(true);
}

bool ProcessAssets(Eval* eval, std::vector<uint8_t> paramsNull,const CTransaction &ctx, unsigned int nIn)
{
    static uint256 zero,prevtxid;
    CTransaction createTx; uint256 txid,assetid,assetid2,hashBlock; uint8_t funcid; int32_t i,n; uint64_t amount; std::vector<uint8_t> origpubkey;
    txid = ctx.GetHash();
    if ( txid == prevtxid )
        return(true);
    CTransaction tx = *(CTransaction *)&ctx;
    if ( paramsNull.size() != 0 ) // Don't expect params
        return eval->Invalid("Cannot have params");
    else if ( (n= tx.vout.size()) == 0 )
        return eval->Invalid("no-vouts");
    else if ( (funcid= DecodeAssetOpRet(tx.vout[n-1].scriptPubKey,assetid,assetid2,amount,origpubkey)) == 0 )
        return eval->Invalid("Invalid opreturn payload");
    if ( eval->GetTxUnconfirmed(assetid,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset create txid");
    if ( assetid2 != zero && eval->GetTxUnconfirmed(assetid2,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset2 create txid");
    if ( AssetValidate(eval,tx,n,funcid,assetid,assetid2,amount,origpubkey) != 0 )
    {
        prevtxid = txid;
        return(true);
    } else return(false);
}

