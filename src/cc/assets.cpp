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

#include <cc/eval.h>
#include <script/cc.h>
#include <script/script.h>
#include <cryptoconditions.h>

/*
 Assets can be created or transferred.
 
 To create an asset use CC EVAL_ASSETS to create a transaction where vout[0] funds the assets. Externally each satoshi can be interpreted to represent 1 unit, or 100 million satoshis. The interpretation of the number of decimals is left to the higher level usages.
 
 Once created, the assetid is the txid of the create transaction and using the assetid/0 it can spend the assets to however many outputs it creates. The restriction is that the last output must be an opreturn with the assetid. The sum of all but the first output needs to add up to the total assetoshis input. The first output is ignored and used for change from txfee.
 
 What this means is that vout 0 of the creation txid and vouts 1 ... n-2 for transfer vouts are assetoshi outputs.
 
*/

 
/*
# Using Hoek to mangle asset transactions
# Install haskell stack & hoek
curl -sSL https://get.haskellstack.org/ | sh
git clone https://github.com/libscott/hoek; cd hoek; stack install
# Let...
addr=RHTcNNYXEZhLGRcXspA2H4gw2v4u6w8MNp
wif=UsNAMqFwntEpuFBTbG28e3uAJxBNRM8Vi5FxAqHfoRJJNoZ84Esj
pk=02184e11939da3805808cd18921a8b592b98bbaf9f506da8b272ebc3c5fa4d045c
# Our CC is a 2 of 2 where the subconditions are an secp256k1, and an EVAL code calling 0xe3 (EVAL_ASSET).
cc='{"type":"threshold-sha-256","threshold":2,"subfulfillments":[{"type":"eval-sha-256","code":"e3"},{"type":"secp256k1-sha-256","publicKey":"02184e11939da3805808cd18921a8b592b98bbaf9f506da8b272ebc3c5fa4d045c"}]}'
# 1. Create a asset: Just use regular inputs and only colored outputs
createTx='{"inputs": [{"txid":"51b78168d94ec307e2855697209275d477e05d8647caf29cb9e38fb6a4661145","idx":0,"script":{"address":"'$addr'"}}],"outputs":[{"amount":10,"script":{"condition":'$cc'}}]}'
# 2. Transfer a asset: use CC inputs, CC outputs, and an OP_RETURN output with the txid of the tx that created the asset (change the txid):
transferTx='{"inputs": [{"txid":"51b78168d94ec307e2855697209275d477e05d8647caf29cb9e38fb6a4661145","idx":0,"script":{"fulfillment":'$cc'}}],"outputs":[{"amount":0,"script":{"op_return":"cafabda044ac904d56cee79bbbf3ed9b3891a69000ed08f0ddf0a3dd620a3ea6"}},{"amount":10,"script":{"condition":'$cc'}}]}'
# 3. Sign and encode
function signEncodeTx () {
    signed=`hoek signTx '{"privateKeys":["'$wif'"],"tx":'"$1"'}'`;
    hoek encodeTx "$signed"
}
signEncodeTx "$createTx"
signEncodeTx "$transferTx"
*/


CC* GetCryptoCondition(CScript const& scriptSig)
{
    auto pc = scriptSig.begin();
    opcodetype opcode;
    std::vector<unsigned char> ffbin;
    if (scriptSig.GetOp(pc, opcode, ffbin))
        return cc_readFulfillmentBinary((uint8_t*)ffbin.data(), ffbin.size()-1);
}

bool IsAssetOutput(CScript const& scriptPubKey)
{
    int32_t scriptlen; const uint8_t *script = scriptPubKey.data();
    scriptlen = scriptPubKey.size();
    if ( script[scriptlen - 1] != OP_CRYPTOCONDITION )
        return(false);
    return(true);
}

bool IsAssetInput(CScript const& scriptSig)
{
    CC* cond;
    if (!(cond = GetCryptoCondition(scriptSig)))
        return false;

    // Recurse the CC tree to find asset condition
    auto findEval = [&] (CC *cond, struct CCVisitor _) {
        bool r = cc_typeId(cond) == CC_Eval && cond->codeLength == 1 && cond->code[0] == EVAL_ASSET;
        // false for a match, true for continue
        return r ? 0 : 1;
    };
    CCVisitor visitor = {findEval, (uint8_t*)"", 0, NULL};
    bool out =! cc_visit(cond, visitor);
    cc_free(cond);
    return out;
}

// Get the coin ID from opret
bool DecodeOpRet(CScript const& scriptPubKey, uint256 &coinId)
{
    std::vector<uint8_t> vopret;
    GetOpReturnData(scriptPubKey, vopret);
    return E_UNMARSHAL(vopret, ss >> coinId);
}

uint64_t IsAssetTx(uint256 coinId,CTransactionRef& inputTx,int32_t v)
{
    // Either the tx will be a CREATE (no CC inputs) or a TRANSFER (1 or more CC inputs)
    uint256 inputCoinId; unsigned int i,n,r = 0;
    for (i=0; i<inputTx.vin.size(); i++)
        r += IsAssetInput(inputTx.vin[i].scriptSig) ? 1 : 0;
    if ( r > 0 ) // It's a TRANSFER, check coin ID
    {
        n = inputTx.vout.size();
        if ( DecodeOpRet(inputTx.vout[n-1].scriptPubKey,inputCoinId) == 0 )
            return(0);
        if ( inputCoinId == coinId && v > 0 && v < n-1 )
            return(inputTx.vout[v].nValue);
    }
    else if ( r == 0 ) // It's a CREATE, compare hash directly
    {
        if ( coinId == inputTx.GetHash() && v == 0 ) // asset create must use vout[0] for the asset
            return(inputTx.vout[v].nValue);
    }
    // should never happen
    fprintf(stderr, "Illegal state detected, mixed inputs for asset at: %s\n",coinId.GetHex().data());
    return(0);
}

/*
 * assets using CC defines 2 possible types of transaction, a CREATE and a TRANSFER.
 * A CREATE has only regular (non CC) inputs, and a TRANSFER has one or more CC inputs.
 *
 * If the asset EVAL routine is being called, then the tx is inevitably a TRANSFER
 * becuase in the case of a CREATE routine, no CC is triggered.
 *
 * The coin ID is the ID of the CREATE transaction.
 */

bool ProcessAssets(Eval* eval, std::vector<uint8_t> paramsNull, const CTransaction &tx, unsigned int nIn)
{
    CTransaction inputTx,createTx; uint256 coinId,hashBlock; int32_t i,n; uint64_t nValue,outputs=0,assetoshis = 0;
    if ( paramsNull.size() != 0 ) // Don't expect params
        return eval->Invalid("Cannot have params");
    if ( (n= tx.vout.size()) == 0 )
        return eval->Invalid("no-vouts");
    for (i=1; i<n-1; i++)
    {
        if ( IsCCOutput(tx.vout[i].scriptPubKey) == 0 )
            return eval->Invalid("non-CC asset vout");
        outputs += tx.vout[i].nValue;
    }
    if ( !DecodeOpRet(tx.vout[n-1].scriptPubKey, coinId) ) // Expect last output to be an opreturn containing move data
        return eval->Invalid("Invalid opreturn payload");
    if ( eval->GetTxUnconfirmed(coinId,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset create txid");
    if ( createTx.vout.size() == 0 || IsCCOutput(createTx.vout[0].scriptPubKey) == 0 )
        return eval->Invalid("asset create txid is not asset CC");
    for (i=0; i<tx.vin.size(); i++) // Check inputs
    {
        if ( IsAssetInput(tx.vin[i].scriptSig) != 0 )
        {
            // We also need to validate that our input transactions are legit - either they
            // themselves have CC inputs of the same type, or, they have a token definition and
            // no CC inputs.
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,inputTx,hashBlock) == 0 )
                return eval->Invalid("This should never happen");
            if ( (nValue= IsAssetTx(coinId,inputTx,tx.vin[i].prevout.n)) == 0 )
                return eval->Invalid("Non colored / wrong asset input tx");
            assetoshis += nValue;
        }
    }
    if ( assetoshis == 0 )
        return eval->Invalid("no asset inputs");
    else if ( assetoshis != outputs )
        return eval->Invalid("mismatched inputs and outputs");
    
    // We're unable to ensure that all the outputs have the correct
    // CC Eval code because we only have the scriptPubKey which is a hash of the
    // condition. The effect of this is that we cannot control the outputs, and therefore
    // are able to burn or "lose" units or our colored token and they will be spent as the
    // regular chain token, but we are able to control when units of the asset get
    // created because we can always vet the inputs.
    //
    // Units of the colored token are always 1:1 with the token being input. No additional
    // supply is created on the chain. Effectively, fees are paid in units of the colored token.
    // This leaves something to be desired, because for example, if you wanted to create a
    // single asset that could transfer ownership, usually you would create an asset with a 
    // supply of 1, ie can not be divided further, however in this case there would be nothing
    // left to pay fees with. Implementing separate supply and other details are possible, by
    // encoding further information in the OP_RETURNS.

    // It's valid
    return true;
}
