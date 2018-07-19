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
#include "../script/standard.h"

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
 vout.n-1: opreturn [EVAL_ASSETS] ['c'] [{"<assetname>":"<description>"}]
 
 transfer
 vin.0: normal input
 vin.1 .. vin.n-1: valid CC outputs
 vout.0 to n-2: assetoshis output to CC
 vout.n-2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['t'] [assetid]
 
 selloffer:
 vin.0: normal input
 vin.1: valid CC output for sale
 vout.0: vin.1 assetoshis output to CC to unspendable
 vout.1: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['s'] [assetid] [amount of native coin required]
 
 buyoffer:
 vins.*: normal inputs (bid + change)
 vout.0: amount of bid to unspendable
 vout.1: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['b'] [assetid] [amount of asset required]
 
 exchange:
 vin.0: normal input
 vin.1: valid CC output
 vout.0: vin.1 assetoshis output to CC to unspendable
 vout.1: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['e'] [assetid] [assetid2] [amount of asset2 required]
 
 cancelbuy:
 vin.0: normal input
 vin.1: unspendable.(vout.1 from buyoffer)
 vout.0: vin.1 value to original pubkey
 vout.1: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['o'] [assetid]
 
 cancel:
 vin.0: normal input
 vin.1: unspendable.(vout.1 from exchange or selloffer)
 vout.0: vin.1 assetoshis to original pubkey CC
 vout.1: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['x'] [assetid]
 
 fillsell:
 vin.0: normal input
 vin.1: unspendable.(vout.1 assetoshis from selloffer)
 vin.2: normal output that satisfies selloffer
 vout.0: vin.1 assetoshis to signer of vin.2
 vout.1: vin.2 value to original pubkey
 vout.2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['S'] [assetid]
 
 fillbuy:
 vin.0: normal input
 vin.1: unspendable.(vout.1 from buyoffer)
 vin.2: valid CC output satisfies buyoffer
 vout.0: vin.1 value to signer of vin.2
 vout.1: vin.2 assetoshis to original pubkey
 vout.2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid]
 
 fillexchange:
 vin.0: normal input
 vin.1: unspendable.(vout.1 assetoshis from exchange)
 vin.2: valid CC output that satisfies exchange
 vout.0: vin.1 assetoshis to signer of vin.2
 vout.1: vin.2 assetoshis to original pubkey
 vout.2: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['E'] [assetid vin1] [assetid vin2]
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
# Our CC is a 2 of 2 where the subconditions are an secp256k1, and an EVAL code calling 0xe3 (EVAL_ASSETS).
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

const char *Unspendableaddr = "RHTcNNYXEZhLGRcXspA2H4gw2v4u6w8MNp";

CC* GetCryptoCondition(CScript const& scriptSig)
{
    auto pc = scriptSig.begin();
    opcodetype opcode;
    std::vector<unsigned char> ffbin;
    if (scriptSig.GetOp(pc, opcode, ffbin))
        return cc_readFulfillmentBinary((uint8_t*)ffbin.data(), ffbin.size()-1);
}

bool IsAssetInput(CScript const& scriptSig)
{
    CC *cond;
    if (!(cond = GetCryptoCondition(scriptSig)))
        return false;
    // Recurse the CC tree to find asset condition
    auto findEval = [&] (CC *cond, struct CCVisitor _) {
        bool r = cc_typeId(cond) == CC_Eval && cond->codeLength == 1 && cond->code[0] == EVAL_ASSETS;
        // false for a match, true for continue
        return r ? 0 : 1;
    };
    CCVisitor visitor = {findEval, (uint8_t*)"", 0, NULL};
    bool out =! cc_visit(cond, visitor);
    cc_free(cond);
    return out;
}

uint8_t DecodeOpRet(CScript const& scriptPubKey,uint256 &assetid,uint256 &assetid2,uint64_t &amount)
{
    std::vector<uint8_t> vopret; uint8_t funcid=0,*script;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    memset(&assetid,0,sizeof(assetid));
    memset(&assetid2,0,sizeof(assetid2));
    amount = 0;
    if ( script[0] == EVAL_ASSETS )
    {
        funcid = script[1];
        switch ( funcid )
        {
           case 't': case 'o': case 'x': case 'S': case 'B':
                if ( E_UNMARSHAL(vopret, ss >> assetid) != 0 )
                    return(funcid);
                break;
            case 's': case 'b':
                if ( E_UNMARSHAL(vopret, ss >> assetid; ss >> amount) != 0 )
                    return(funcid);
                break;
            case 'E':
                if ( E_UNMARSHAL(vopret, ss >> assetid; ss >> assetid2) != 0 )
                    return(funcid);
                break;
            case 'e':
                if ( E_UNMARSHAL(vopret, ss >> assetid; ss >> assetid2; ss >> amount) != 0 )
                    return(funcid);
                break;
        }
    }
    return(funcid);
}

uint64_t IsAssetTx(uint64_t *origamountp,uint8_t funcid,int32_t vini,uint256 assetid,CTransaction& inputTx,int32_t v)
{
    // Either the tx will be a CREATE (no CC inputs) or a TRANSFER (1 or more CC inputs)
    uint256 inputassetid,inputassetid2; uint64_t amount; uint8_t inputfuncid; unsigned int i,n,r = 0;
    if ( origamountp != 0 )
        *origamountp = 0;
    for (i=0; i<inputTx.vin.size(); i++)
        r += IsAssetInput(inputTx.vin[i].scriptSig) ? 1 : 0;
    if ( r > 0 )
    {
        n = inputTx.vout.size();
        if ( (inputfuncid= DecodeOpRet(inputTx.vout[n-1].scriptPubKey,inputassetid,inputassetid2,amount)) == 0 )
            return(0);
        if ( origamountp != 0 )
            *origamountp = amount;
        if ( v >= n-1 )
            return(0);
        if ( inputfuncid != 'E' || vini == 1 )
        {
            if ( inputassetid == assetid )
                return(inputTx.vout[v].nValue);
        }
        else // 'E' and vini == 2
        {
            if ( inputassetid2 == assetid )
                return(inputTx.vout[v].nValue);
        }
    }
    else if ( r == 0 ) // It's a CREATE, compare hash directly
    {
        if ( assetid == inputTx.GetHash() && v == 0 ) // asset create must use vout[0] for the asset
            return(inputTx.vout[v].nValue);
    }
    // should never happen
    fprintf(stderr, "Illegal state detected, mixed inputs for asset at: %s\n",assetid.GetHex().data());
    return(0);
}

uint64_t AssetIsvalidCCvin(uint8_t funcid,uint256 assetid,uint64_t *origamountp,uint64_t *origoutp,char *origaddr,const CTransaction &tx,int32_t vini)
{
    CTransaction inputTx,origTx; CTxDestination address; uint256 hashBlock; int32_t v; uint64_t nValue = 0;
    v = tx.vin[vini].prevout.n;
    if ( eval->GetTxUnconfirmed(tx.vin[vini].prevout.hash,inputTx,hashBlock) == 0 )
        return eval->Invalid("always should find vin, but didnt");
    if ( IsAssetInput(tx.vin[vini].scriptSig) != 0 )
    {
        if ( (nValue= IsAssetTx(origamountp,funcid,vini,assetid,inputTx,v)) == 0 )
            return eval->Invalid("Non asset / wrong asset input tx");
    }
    if ( origaddr != 0 && origoutp != 0 )
    {
        origaddr[0] = 0;
        *origoutp = 0;
        if ( eval->GetTxUnconfirmed(InputTx.vin[vini].prevout.hash,origTx,hashBlock) == 0 )
            return eval->Invalid("always should find origtx, but didnt for cancelbuy");
        if ( ExtractDestination(origTx.vout[InputTx.vin[vini].prevout.n].scriptPubKey,address) == 0 )
            return eval->Invalid("no origtx address for cancelbuy");
        strcpy(origaddr,(char *)CBitcoinAddress(address).ToString().c_str());
        *origoutp = InputTx.vout[0].nValue;
    }
    return(nValue);
}

uint64_t AssetIsvalidvin(uint64_t *origamountp,uint64_t *origoutp,char *origaddr,const CTransaction &tx,int32_t vini)
{
    CTransaction inputTx,origTx; CTxDestination address; uint256 hashBlock,inputassetid,inputassetid2; int32_t v; uint64_t amount,nValue = 0;
    v = tx.vin[vini].prevout.n;
    *origamountp = 0;
    if ( eval->GetTxUnconfirmed(tx.vin[vini].prevout.hash,inputTx,hashBlock) == 0 )
        return eval->Invalid("always should find vin, but didnt");
    if ( (inputfuncid= DecodeOpRet(inputTx.vout[inputTx.vout.size()-1].scriptPubKey,inputassetid,inputassetid2,amount)) == 0 )
        return(0);
    nValue = inputTx.vout[v].nValue;
    *origamountp = amount;
    if ( origaddr != 0 && origoutp != 0 )
    {
        origaddr[0] = 0;
        *origoutp = 0;
        if ( eval->GetTxUnconfirmed(InputTx.vin[vini].prevout.hash,origTx,hashBlock) == 0 )
            return eval->Invalid("always should find origtx, but didnt for cancelbuy");
        if ( ExtractDestination(origTx.vout[InputTx.vin[vini].prevout.n].scriptPubKey,address) == 0 )
            return eval->Invalid("no origtx address for cancelbuy");
        strcpy(origaddr,(char *)CBitcoinAddress(address).ToString().c_str());
        *origoutp = InputTx.vout[0].nValue;
    }
    return(nValue);
}

uint64_t Assetvini2val(int32_t needasset,uint256 assetid,const CTransaction &tx)
{
    CTransaction inputTx; uint256 hashBlock;
    if ( needasset != 0 && IsAssetInput(tx.vin[2].scriptSig) == 0 )
        return eval->Invalid("vini2 is not CC");
    if ( eval->GetTxUnconfirmed(tx.vin[2].prevout.hash,inputTx,hashBlock) == 0 )
        return eval->Invalid("always should find vin, but didnt");
    return(inputTx.vout[tx.vin[2].prevout.n]);
}

uint64_t AssetIsvalidvout0(const CTransaction &tx,uint64_t nValue)
{
    CTxDestination address;
    if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
        return eval->Invalid("illegal normal vout0");
    if ( tx.vout[0].nValue != nValue )
        return eval->Invalid("nValue mismatch vout0");
    if ( ExtractDestination(tx.vout[0].scriptPubKey,address) == 0 )
        return eval->Invalid("no vout0 address");
    if ( strcmp(Unspendableaddr,(char *)CBitcoinAddress(address).ToString().c_str()) != 0 )
        return eval->Invalid("invalid vout0 address");
    return(nValue);
}

bool AssetValidate(Eval* eval,const CTransaction &tx,int32_t numvouts,uint8_t funcid,uint256 assetid,uint256 assetid2,uint64_t amount)
{
    static uint256 zero;
    CTxDestination address; uint256 hashBlock; int32_t i,r,numvins; uint64_t nValue,vini2val,origamount,outputs,assetoshis,assetoshis2; char origaddr[64];
    numvins = tx.vin.size();
    assetoshis = assetoshis2 = outputs = 0;
    if ( IsAssetInput(tx.vin[0].scriptSig) != 0 )
        return eval->Invalid("illegal asset vin0");
    if ( funcid != 'c' && assetid == zero )
        return eval->Invalid("illegal assetid");
    switch ( funcid )
    {
        case 'c': // create wont be called to be verified as it has no CC inputs
            //vin.0: normal input
            //vout.0: issuance assetoshis to CC
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['c'] [{"<assetname>":"<description>"}]
            if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                return eval->Invalid("illegal normal vout0 for createasset");
            break;
            
        case 't': // transfer
            //vin.0: normal input
            //vin.1 .. vin.n-1: valid CC outputs
            //vout.0 to n-2: assetoshis output to CC
            //vout.n-2: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['t'] [assetid]
            for (r=0,i=1; i<numvins; i++)
            {
                if ( (nValue= AssetIsvalidCCvin(funcid,assetid,0,0,0,tx,i)) != 0 )
                    assetoshis += nValue;
                else return eval->Invalid("illegal normal vin for transfer");
            }
            for (i=0; i<numvouts-1; i++)
            {
                if ( tx.vout[i].scriptPubKey.IsPayToCryptoCondition() != 0 )
                    outputs += tx.vout[i].nValue;
                else if ( i < n-2 )
                    return eval->Invalid("non-CC asset transfer vout");
            }
            if ( assetoshis == 0 )
                return eval->Invalid("no asset inputs for transfer");
            else if ( assetoshis != outputs )
                return eval->Invalid("mismatched inputs and outputs for transfer");
            break;
  
        case 's': // selloffer
            //vin.0: normal input
            //vin.1: valid CC output for sale
            //vout.0: vin.1 assetoshis output to CC to unspendable
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['s'] [assetid] [amount of native coin required]
            if ( amount == 0 )
                return eval->Invalid("illegal null amount for selloffer");
            if ( (nValue= AssetIsvalidCCvin(funcid,assetid,0,0,0,tx,1)) != 0 )
            {
                if ( AssetIsvalidvout0(tx,nValue) != nValue )
                    return(false);
            } else return eval->Invalid("illegal standard vini.1 for selloffer");
            break;
           
        case 'b': // buyoffer
            //vins.*: normal inputs (bid + change)
            //vout.0: amount of bid to unspendable
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['b'] [assetid] [amount of asset required]
            if ( amount == 0 )
                return eval->Invalid("illegal null amount for buyoffer");
            for (i=1; i<numvins; i++)
            {
                if ( (nValue= AssetIsvalidCCvin(funcid,assetid,0,0,0,tx,i)) != 0 )
                    return eval->Invalid("invalid CC vin for buyoffer");
            }
            for (i=0; i<numvouts-1; i++)
            {
                if ( tx.vout[i].scriptPubKey.IsPayToCryptoCondition() != 0 )
                    return eval->Invalid("invalid CC vout for buyoffer");
            }
            break;
    
        case 'e': // exchange
            //vin.0: normal input
            //vin.1: valid CC output
            //vout.0: vin.1 assetoshis output to CC to unspendable
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['e'] [assetid] [assetid2] [amount of asset2 required]
            if ( assetid2 == zero )
                return eval->Invalid("illegal assetid2 for exchange");
            if ( amount == 0 )
                return eval->Invalid("illegal null amount for exchange");
            if ( (nValue= AssetIsvalidCCvin(funcid,assetid,0,0,0,tx,1)) != 0 )
            {
                if ( AssetIsvalidvout0(tx,nValue) != nValue )
                    return(false);
            } else return eval->Invalid("illegal standard vini.1 for exchange");
            break;
           
        case 'o': // cancelbuy
            //vin.0: normal input
            //vin.1: unspendable.(vout.1 from buyoffer)
            //vout.0: vin.1 value to original pubkey
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['o'] [assetid]
            if ( AssetIsvalidCCvin(funcid,0,&origamount,&origout,origaddr,tx,1) != 0 )
                return eval->Invalid("illegal CC vin.1 for cancelbuy");
            if ( strcmp(Unspendableaddr,(char *)CBitcoinAddress(address).ToString().c_str()) != 0 )
                return eval->Invalid("invalid vout0 address for cancelbuy");
            if ( origout != tx.vout[0].nValue )
                return eval->Invalid("mismatched value for cancelbuy");
            if ( ExtractDestination(tx.vout[0].scriptPubKey,address) == 0 )
                return eval->Invalid("no vout0 address for cancelbuy");
            if ( strcmp(origaddr,(char *)CBitcoinAddress(address).ToString().c_str()) != 0 )
                return eval->Invalid("mismatched origaddr for cancelbuy");
            break;
            
        case 'x': // cancel
            //vin.0: normal input
            //vin.1: unspendable.(vout.1 from exchange or selloffer)
            //vout.0: vin.1 assetoshis to original pubkey CC
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['x'] [assetid]
            if ( (nValue= AssetIsvalidCCvin(funcid,assetid,&origamount,&origout,origaddr,tx,1)) != 0 )
            {
                if ( origout != tx.vout[0].nValue )
                    return eval->Invalid("mismatched value for cancel");
                if ( ExtractDestination(tx.vout[0].scriptPubKey,address) == 0 )
                    return eval->Invalid("no vout0 address for cancel");
                if ( strcmp(origaddr,(char *)CBitcoinAddress(address).ToString().c_str()) != 0 )
                    return eval->Invalid("mismatched origaddr for cancel");
            } else return eval->Invalid("illegal standard vini.1 for cancel");
            break;
            
        case 'S': // fillsell
            //vin.0: normal input
            //vin.1: unspendable.(vout.1 assetoshis from selloffer)
            //vin.2: normal output that satisfies selloffer, need to extract from opreturn
            //vout.0: vin.1 assetoshis to signer of vin.2
            //vout.1: vin.2 value to original pubkey
            //vout.2: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['S'] [assetid]
            if ( (vini2val= Assetvini2val(0,assetid,tx)) == 0 )
                return eval->Invalid("no vini2val for fillsell");
            if ( (nValue= AssetIsvalidCCvin(funcid,assetid,&origamount,&origout,origaddr,tx,1)) != 0 )
            {
                if ( origout != tx.vout[0].nValue )
                    return eval->Invalid("mismatched value for fillsell");
                // vout address can be any
                if ( ExtractDestination(tx.vout[1].scriptPubKey,address) == 0 )
                    return eval->Invalid("no vout0 address for fillsell");
                if ( strcmp(origaddr,(char *)CBitcoinAddress(address).ToString().c_str()) != 0 )
                    return eval->Invalid("mismatched origaddr for fillsell");
                if ( vini2val < origamount )
                    return eval->Invalid("not enough paid for fillsell");
                if ( vini2val != tx.vout[1].nValue )
                    return eval->Invalid("mismatched vout.1 val for fillsell");
            } else return eval->Invalid("illegal standard vini.1 for fillsell");
            break;
           
        case 'B': // fillbuy:
            //vin.0: normal input
            //vin.1: unspendable.(vout.1 from buyoffer)
            //vin.2: valid CC output satisfies buyoffer
            //vout.0: vin.1 value to signer of vin.2
            //vout.1: vin.2 assetoshis to original pubkey
            //vout.2: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid]
            if ( (vini2val= Assetvini2val(1,assetid,tx)) == 0 )
                return eval->Invalid("no vini2val for fillbuy");
            if ( (nValue= AssetIsvalidvin(&origamount,&origout,origaddr,tx,1)) != 0 )
            {
                if ( origout != tx.vout[0].nValue )
                    return eval->Invalid("mismatched value for fillbuy");
                if ( ExtractDestination(tx.vout[1].scriptPubKey,address) == 0 )
                    return eval->Invalid("no vout0 address for fillbuy");
                if ( strcmp(origaddr,(char *)CBitcoinAddress(address).ToString().c_str()) != 0 )
                    return eval->Invalid("mismatched origaddr for fillbuy");
                if ( vini2val < origamount )
                    return eval->Invalid("not enough paid for fillbuy");
                if ( vini2val != tx.vout[1].nValue )
                    return eval->Invalid("mismatched vout.1 val for fillbuy");
            } else return eval->Invalid("illegal CC vini.1 for fillbuy");
            break;
       
        case 'E': // fillexchange:
            //vin.0: normal input
            /*vin.1: unspendable.(vout.1 assetoshis from exchange)
            vin.2: valid CC output that satisfies exchange
            vout.0: vin.1 assetoshis to signer of vin.2
            vout.1: vin.2 assetoshis to original pubkey*/
            //vout.2: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['E'] [assetid vin1] [assetid vin2]
            if ( assetid2 == zero )
                return eval->Invalid("illegal assetid2 for fillexchange");
            break;
    }
    return(true);
}

bool ProcessAssets(Eval* eval, std::vector<uint8_t> paramsNull, const CTransaction &tx, unsigned int nIn)
{
    static uint256 zero;
    CTransaction createTx; uint256 assetid,assetid2,hashBlock; uint8_t funcid; int32_t i,n; uint64_t amount;
    if ( paramsNull.size() != 0 ) // Don't expect params
        return eval->Invalid("Cannot have params");
    if ( (n= tx.vout.size()) == 0 )
        return eval->Invalid("no-vouts");
    if ( (funcid= DecodeOpRet(tx.vout[n-1].scriptPubKey,assetid,assetid2,amount)) == 0 )
        return eval->Invalid("Invalid opreturn payload");
    if ( eval->GetTxUnconfirmed(assetid,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset create txid");
    if ( assetid2 != zero && eval->GetTxUnconfirmed(assetid2,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset2 create txid");
    return(AssetValidate(eval,tx,n,funcid,assetid,assetid2,amount));
    
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
    //return true;
}
