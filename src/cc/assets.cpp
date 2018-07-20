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
#include "../base58.h"
#include "../script/sign.h"
#include "../wallet/wallet.h"

// code rpc

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
 vout.n-1: opreturn [EVAL_ASSETS] ['o']
 
 fillbuy:
 vin.0: normal input
 vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
 vin.2: valid CC output satisfies buyoffer (*tx.vin[2])->nValue
 vout.0: remaining amount of bid to unspendable
 vout.1: vin.1 value to signer of vin.2
 vout.2: vin.2 assetoshis to original pubkey
 vout.3: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]

 selloffer:
 vin.0: normal input
 vin.1: valid CC output for sale
 vout.0: vin.1 assetoshis output to CC to unspendable
 vout.1: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['s'] [assetid] [amount of native coin required] [origpubkey]
 
 exchange:
 vin.0: normal input
 vin.1: valid CC output
 vout.0: vin.1 assetoshis output to CC to unspendable
 vout.1: normal output for change (if any)
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
 vin.2: normal output that satisfies selloffer (*tx.vin[2])->nValue
 vout.0: remaining assetoshis -> unspendable
 vout.1: vin.1 assetoshis to signer of vin.2 sellTx.vout[0].nValue -> any
 vout.2: vin.2 value to original pubkey [origpubkey]
 vout.3: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['S'] [assetid] [amount of coin still required] [origpubkey]
 
 fillexchange:
 vin.0: normal input
 vin.1: unspendable.(vout.0 assetoshis from exchange) exchangeTx.vout[0]
 vin.2: valid CC assetid2 output that satisfies exchange (*tx.vin[2])->nValue
 vout.0: remaining assetoshis -> unspendable
 vout.1: vin.1 assetoshis to signer of vin.2 exchangeTx.vout[0].nValue -> any
 vout.2: vin.2 assetoshis2 to original pubkey [origpubkey]
 vout.3: normal output for change (if any)
 vout.n-1: opreturn [EVAL_ASSETS] ['E'] [assetid vin0+1] [assetid vin2] [remaining asset2 required] [origpubkey]
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

bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey)
{
    CTxDestination address;
    if ( ExtractDestination(scriptPubKey,address) != 0 )
    {
        strcpy(destaddr,(char *)CBitcoinAddress(address).ToString().c_str());
        return(true);
    }
    return(false);
}

/*
 vout.n-1: opreturn [EVAL_ASSETS] ['o']
 vout.n-1: opreturn [EVAL_ASSETS] ['c'] [{"<assetname>":"<description>"}]
 vout.n-1: opreturn [EVAL_ASSETS] ['t'] [assetid]
 vout.n-1: opreturn [EVAL_ASSETS] ['x'] [assetid]
 vout.n-1: opreturn [EVAL_ASSETS] ['b'] [assetid] [amount of asset required] [origpubkey]
 vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]
 vout.n-1: opreturn [EVAL_ASSETS] ['s'] [assetid] [amount of native coin required] [origpubkey]
 vout.n-1: opreturn [EVAL_ASSETS] ['S'] [assetid] [amount of coin still required] [origpubkey]
 vout.n-1: opreturn [EVAL_ASSETS] ['e'] [assetid] [assetid2] [amount of asset2 required] [origpubkey]
 vout.n-1: opreturn [EVAL_ASSETS] ['E'] [assetid vin0+1] [assetid vin2] [remaining asset2 required] [origpubkey]
*/

CScript EncodeCreateOpRet(uint8_t funcid,std::string name,std::string description)
{
    CScript opret; uint8_t evalcode = EVAL_ASSETS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << name << description);
    return(opret);
}
    
CScript EncodeOpRet(uint8_t funcid,uint256 assetid,uint256 assetid2,uint64_t price,std::vector<uint8_t> origpubkey)
{
    CScript opret; uint8_t evalcode = EVAL_ASSETS;
    switch ( funcid )
    {
        case 'o':
            opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid);
            break;
        case 't':  case 'x':
            opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << assetid);
            break;
        case 's': case 'b': case 'S': case 'B':
            opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << assetid << price << origpubkey);
            break;
        case 'E': case 'e':
            opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << assetid << assetid2 << price << origpubkey);
            break;
        default:
            fprintf(stderr,"EncodeOpRet: illegal funcid.%02x\n",funcid);
            opret << OP_RETURN;
            break;
   }
    return(opret);
}

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
    CTxOut(nValue,CCPubKey(payoutCond));
    cc_free(payoutCond);
    return(vout);
}

#ifdef ENABLE_WALLET
extern CWallet* pwalletMain;
#endif

CMutableTransaction CreateAsset(CPubKey pk,uint64_t assetsupply,uint256 utxotxid,int32_t utxovout,std::string name,std::string description)
{
    CMutableTransaction mtx; CTransaction vintx; uint256 hashBlock; uint64_t nValue,change,txfee=10000;
    if ( GetTransaction(utxotxid,vintx,hashBlock,false) != 0 )
    {
        if ( vintx.vout[utxovout].scriptPubKey.IsPayToCryptoCondition() == 0 && vintx.vout[utxovout].nValue >= assetsupply+txfee )
        {
            mtx.vin.push_back(CTxIn(utxotxid,utxovout,CScript()));
            mtx.vout.push_back(MakeAssetsVout(assetsupply,pk));
            nValue = vintx.vout[utxovout].nValue;
            if ( nValue > assetsupply+txfee )
            {
                change = nValue - (assetsupply+txfee);
                mtx.vout.push_back(CTxOut(change, CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG));
                mtx.vout.push_back(CTxOut(0,EncodeCreateOpRet('c',name,description)));
            }
#ifdef ENABLE_WALLET
            {
                CTransaction txNewConst(mtx); SignatureData sigdata; const CKeyStore& keystore = *pwalletMain;
                auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
                if ( ProduceSignature(TransactionSignatureCreator(&keystore,&txNewConst,0,nValue,SIGHASH_ALL),vintx.vout[utxovout].scriptPubKey,sigdata,consensusBranchId) != 0 )
                {
                    UpdateTransaction(mtx,0,sigdata);
                    std::string strHex = EncodeHexTx(mtx);
                    fprintf(stderr,"signed CreateAsset (%s -> %s) %s\n",name.c_str(),description.c_str(),strHex.c_str());
                } else fprintf(stderr,"signing error for CreateAsset\n");
            }
#endif
        }
    }
    return(mtx);
}

uint8_t DecodeOpRet(const CScript &scriptPubKey,uint256 &assetid,uint256 &assetid2,uint64_t &price,std::vector<uint8_t> &origpubkey)
{
    std::vector<uint8_t> vopret; uint8_t funcid=0,*script;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    memset(&assetid,0,sizeof(assetid));
    memset(&assetid2,0,sizeof(assetid2));
    price = 0;
    if ( script[0] == EVAL_ASSETS )
    {
        funcid = script[1];
        switch ( funcid )
        {
            case 'o':
                return(funcid);
                break;
            case 't':  case 'x':
                if ( E_UNMARSHAL(vopret, ss >> assetid) != 0 )
                    return(funcid);
                break;
            case 's': case 'b': case 'S': case 'B':
                if ( E_UNMARSHAL(vopret, ss >> assetid; ss >> price; ss >> origpubkey) != 0 )
                    return(funcid);
                break;
            case 'E': case 'e':
                if ( E_UNMARSHAL(vopret, ss >> assetid; ss >> assetid2; ss >> price; ss >> origpubkey) != 0 )
                    return(funcid);
                break;
            default:
                fprintf(stderr,"DecodeOpRet: illegal funcid.%02x\n",funcid);
                funcid = 0;
                break;
        }
    }
    return(funcid);
}

bool Getorigaddr(char *destaddr,CTransaction& tx)
{
    uint256 assetid,assetid2; uint64_t price,nValue=0; int32_t n; uint8_t funcid; std::vector<uint8_t> origpubkey; CScript script;
    n = tx.vout.size();
    if ( n == 0 || (funcid= DecodeOpRet(tx.vout[n-1].scriptPubKey,assetid,assetid2,price,origpubkey)) == 0 )
        return(false);
    script = CScript() << origpubkey << OP_CHECKSIG;
    return(Getscriptaddress(destaddr,script));
}

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

uint64_t IsAssetvout(uint64_t &price,std::vector<uint8_t> &origpubkey,CTransaction& tx,int32_t v,uint256 refassetid)
{
    uint256 assetid,assetid2; uint64_t nValue=0; int32_t n; uint8_t funcid;
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        n = tx.vout.size();
        if ( v >= n-1 )
            return(0);
        nValue = tx.vout[v].nValue;
        if ( (funcid= DecodeOpRet(tx.vout[n-1].scriptPubKey,assetid,assetid2,price,origpubkey)) == 0 )
            return(0);
        if ( funcid == 'c' )
        {
            if ( refassetid == tx.GetHash() && v == 0 )
                return(nValue);
        }
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
    return(0);
}

uint64_t AssetValidatevin(Eval* eval,char *origaddr,CTransaction &tx,CTransaction &vinTx)
{
    uint256 hashBlock; char destaddr[64];
    origaddr[0] = destaddr[0] = 0;
    if ( tx.vin[1].prevout.n != 0 )
        return eval->Invalid("vin1 needs to be buyvin.vout[0]");
    else if ( eval->GetTxUnconfirmed(tx.vin[1].prevout.hash,vinTx,hashBlock) == 0 )
        return eval->Invalid("always should find vin, but didnt");
   else if ( Getscriptaddress(destaddr,vinTx.vout[0].scriptPubKey) == 0 || strcmp(destaddr,Unspendableaddr) != 0 )
        return eval->Invalid("invalid vin unspendableaddr");
    else if ( vinTx.vout[0].nValue < 10000 )
        return eval->Invalid("invalid dust for buyvin");
    else if ( Getorigaddr(origaddr,vinTx) == 0 )
        return eval->Invalid("couldnt get origaddr for buyvin");
    else return(vinTx.vout[0].nValue);
}

uint64_t AssetValidateBuyvin(Eval* eval,uint64_t &tmpprice,std::vector<uint8_t> &tmporigpubkey,char *origaddr,CTransaction &tx)
{
    CTransaction vinTx; uint64_t nValue; uint256 assetid,assetid2;
    if ( (nValue= AssetValidatevin(eval,origaddr,tx,vinTx)) == 0 )
        return(0);
    else if ( vinTx.vout[0].scriptPubKey.IsPayToCryptoCondition() != 0 )
        return eval->Invalid("invalid CC vout0 for buyvin");
    else if ( DecodeOpRet(vinTx.vout[vinTx.vout.size()-1].scriptPubKey,assetid,assetid2,tmpprice,tmporigpubkey) == 0 )
        return eval->Invalid("invalid opreturn for buyvin");
    else return(nValue);
}

uint64_t AssetValidateSellvin(Eval* eval,uint64_t &tmpprice,std::vector<uint8_t> &tmporigpubkey,char *origaddr,CTransaction &tx,uint256 assetid)
{
    CTransaction vinTx; uint64_t nValue,assetoshis;
    if ( (nValue= AssetValidatevin(eval,origaddr,tx,vinTx)) == 0 )
        return(0);
    if ( (assetoshis= IsAssetvout(tmpprice,tmporigpubkey,vinTx,0,assetid)) != 0 )
        return eval->Invalid("invalid missing CC vout0 for sellvin");
    else return(assetoshis);
}

bool ValidateRemainder(uint64_t remaining_price,uint64_t remaining_nValue,uint64_t orig_nValue,uint64_t received,uint64_t paid,uint64_t totalprice)
{
    uint64_t price,recvprice;
    if ( orig_nValue == 0 || received == 0 || paid == 0 || totalprice == 0 )
        return(false);
    else if ( remaining_price < (totalprice - received) )
        return(false);
    else if ( remaining_nValue < (orig_nValue - paid) )
        return(false);
    else if ( remaining_nValue > 0 )
    {
        price = (totalprice * COIN) / orig_nValue;
        recvprice = (received * COIN) / paid;
        if ( recvprice < price )
            return(false);
    }
    return(true);
}

bool AssetValidate(Eval* eval,CTransaction &tx,int32_t numvouts,uint8_t funcid,uint256 assetid,uint256 assetid2,uint64_t remaining_price,std::vector<uint8_t> origpubkey)
{
    static uint256 zero;
    CTxDestination address; CTransaction vinTx; uint256 hashBlock; int32_t i,numvins; uint64_t nValue,assetoshis,outputs,inputs,tmpprice,ignore; std::vector<uint8_t> tmporigpubkey,ignorepubkey; char destaddr[64],origaddr[64];
    numvins = tx.vin.size();
    outputs = inputs = 0;
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
            return eval->Invalid("unexpected AssetValidate for createasset");
            break;
            
        case 't': // transfer
            //vin.0: normal input
            //vin.1 .. vin.n-1: valid CC outputs
            //vout.0 to n-2: assetoshis output to CC
            //vout.n-2: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['t'] [assetid]
            for (i=1; i<numvins; i++)
            {
                if ( IsAssetInput(tx.vin[i].scriptSig) != 0 )
                {
                    if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                        return eval->Invalid("always should find vin, but didnt");
                    else if ( (assetoshis= IsAssetvout(tmpprice,tmporigpubkey,vinTx,tx.vin[i].prevout.n,assetid)) != 0 )
                        inputs += assetoshis;
                }
            }
            for (i=0; i<numvouts-1; i++)
            {
                if ( (assetoshis= IsAssetvout(tmpprice,tmporigpubkey,tx,i,assetid)) != 0 )
                    outputs += assetoshis;
            }
            if ( inputs == 0 )
                return eval->Invalid("no asset inputs for transfer");
            else if ( inputs != outputs )
                return eval->Invalid("mismatched inputs and outputs for transfer");
            break;

        case 'b': // buyoffer
            //vins.*: normal inputs (bid + change)
            //vout.0: amount of bid to unspendable
            //vout.1: normal output for change (if any)
            // vout.n-1: opreturn [EVAL_ASSETS] ['b'] [assetid] [amount of asset required] [origpubkey]
            if ( remaining_price == 0 )
                return eval->Invalid("illegal null amount for buyoffer");
            for (i=1; i<numvins; i++)
            {
                if ( IsAssetInput(tx.vin[i].scriptSig) != 0 )
                    return eval->Invalid("invalid CC vin for buyoffer");
            }
            for (i=0; i<numvouts-1; i++)
            {
                if ( tx.vout[i].scriptPubKey.IsPayToCryptoCondition() != 0 )
                    return eval->Invalid("invalid CC vout for buyoffer");
                if ( i == 0 )
                {
                    if ( Getscriptaddress(destaddr,tx.vout[i].scriptPubKey) == 0 || strcmp(destaddr,Unspendableaddr) != 0 )
                        return eval->Invalid("invalid vout0 destaddr for buyoffer");
                    else if ( tx.vout[i].nValue < 10000 )
                        return eval->Invalid("invalid vout0 dust for buyoffer");
                }
            }
            break;
            
        case 'o': // cancelbuy
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
            //vout.0: vin.1 value to original pubkey buyTx.vout[0].nValue -> [origpubkey]
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['o']
            if ( (nValue= AssetValidateBuyvin(eval,tmpprice,tmporigpubkey,origaddr,tx)) == 0 )
                return(false);
            else if ( nValue != tx.vout[0].nValue )
                return eval->Invalid("mismatched refund for cancelbuy");
            else if ( Getscriptaddress(destaddr,tx.vout[0].scriptPubKey) == 0 || strcmp(destaddr,origaddr) != 0 )
                return eval->Invalid("invalid vout0 destaddr for cancelbuy");
            break;
            
        case 'B': // fillbuy:
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
            //vin.2: valid CC output satisfies buyoffer (*tx.vin[2])->nValue
            //vout.0: remaining amount of bid to unspendable
            //vout.1: vin.1 value to signer of vin.2
            //vout.2: vin.2 assetoshis to original pubkey
            //vout.3: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]
            if ( (nValue= AssetValidateBuyvin(eval,tmpprice,tmporigpubkey,origaddr,tx)) == 0 )
                return(false);
            else if ( tmporigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for fillbuy");
            else if ( IsAssetInput(tx.vin[2].scriptSig) != 0 )
            {
                if ( eval->GetTxUnconfirmed(tx.vin[2].prevout.hash,vinTx,hashBlock) == 0 )
                    return eval->Invalid("always should find vin, but didnt");
                else if ( (assetoshis= IsAssetvout(ignore,ignorepubkey,vinTx,tx.vin[2].prevout.n,assetid)) != 0 )
                {
                    if ( tx.vout[2].nValue != assetoshis )
                        return eval->Invalid("mismatched assetoshis for fillbuy");
                    else if ( Getscriptaddress(destaddr,tx.vout[2].scriptPubKey) == 0 || strcmp(destaddr,origaddr) != 0 )
                        return eval->Invalid("mismatched vout2 destaddr for fillbuy");
                    else if ( ValidateRemainder(remaining_price,tx.vout[0].nValue,nValue,tx.vout[1].nValue,assetoshis,tmpprice) == false )
                        return eval->Invalid("mismatched remainder for fillbuy");
                    else if ( remaining_price != 0 )
                    {
                        if ( remaining_price < 10000 )
                            return eval->Invalid("dust vout0 to unspendableaddr for fillbuy");
                        else if ( Getscriptaddress(destaddr,tx.vout[0].scriptPubKey) == 0 || strcmp(destaddr,Unspendableaddr) != 0 )
                            return eval->Invalid("mismatched vout0 unspendableaddr for fillbuy");
                    }
                } else return eval->Invalid("vin2 not enough asset2 for fillbuy");
            } else return eval->Invalid("vin2 not asset for fillbuy");
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
            if ( IsAssetInput(tx.vin[1].scriptSig) != 0 )
            {
                if ( eval->GetTxUnconfirmed(tx.vin[1].prevout.hash,vinTx,hashBlock) == 0 )
                    return eval->Invalid("always should find vin, but didnt");
                else if ( (assetoshis= IsAssetvout(tmpprice,tmporigpubkey,vinTx,tx.vin[1].prevout.n,assetid)) == 0 )
                    return eval->Invalid("illegal missing assetvin for selloffer");
                else if ( Getscriptaddress(destaddr,tx.vout[0].scriptPubKey) == 0 || strcmp(destaddr,Unspendableaddr) != 0 )
                    return eval->Invalid("mismatched vout0 unspendableaddr for selloffer");
                else if ( tx.vout[0].nValue != assetoshis )
                    return eval->Invalid("mismatched assetoshis for selloffer");
            } else return eval->Invalid("illegal normal vin1 for selloffer");
            break;
           
        case 'x': // cancel
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from exchange or selloffer) sellTx/exchangeTx.vout[0] inputTx
            //vout.0: vin.1 assetoshis to original pubkey CC sellTx/exchangeTx.vout[0].nValue -> [origpubkey]
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['x'] [assetid]
            if ( (assetoshis= AssetValidateSellvin(eval,tmpprice,tmporigpubkey,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( assetoshis != tx.vout[0].nValue )
                return eval->Invalid("mismatched refund for cancelbuy");
            else if ( Getscriptaddress(destaddr,tx.vout[0].scriptPubKey) == 0 || strcmp(destaddr,origaddr) != 0 )
                return eval->Invalid("invalid vout0 destaddr for cancel");
            break;
            
        case 'S': // fillsell
        case 'E': // fillexchange
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 assetoshis from selloffer) sellTx.vout[0]
            //'S'.vin.2: normal output that satisfies selloffer (*tx.vin[2])->nValue
            //'E'.vin.2: valid CC assetid2 output that satisfies exchange (*tx.vin[2])->nValue
            //vout.0: remaining assetoshis -> unspendable
            //vout.1: vin.1 assetoshis to signer of vin.2 sellTx.vout[0].nValue -> any
            //'S'.vout.2: vin.2 value to original pubkey [origpubkey]
            //'E'.vout.2: vin.2 assetoshis2 to original pubkey [origpubkey]
            //vout.3: normal output for change (if any)
            //'S'.vout.n-1: opreturn [EVAL_ASSETS] ['S'] [assetid] [amount of coin still required] [origpubkey]
            //'E'.vout.n-1: opreturn [EVAL_ASSETS] ['E'] [assetid vin0+1] [assetid vin2] [remaining asset2 required] [origpubkey]
            if ( (assetoshis= AssetValidateSellvin(eval,tmpprice,tmporigpubkey,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( tmporigpubkey != origpubkey )
                return eval->Invalid("mismatched origpubkeys for fillsell");
            else if ( funcid == 'S' && IsAssetInput(tx.vin[2].scriptSig) != 0 )
                return eval->Invalid("invalid vin2 is CC for fillsell");
            else if ( funcid == 'E' && IsAssetInput(tx.vin[2].scriptSig) == 0 )
                return eval->Invalid("invalid vin2 is CC for fillexchange");
            else if ( eval->GetTxUnconfirmed(tx.vin[2].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("always should find vin, but didnt");
            else if ( funcid == 'E' && (IsAssetvout(ignore,ignorepubkey,vinTx,tx.vin[2].prevout.n,assetid2) != tx.vout[2].nValue || tx.vout[2].nValue == 0) )
                return eval->Invalid("invalid asset2 vin value for fillexchange");
            else if ( funcid == 'E' && IsAssetvout(ignore,ignorepubkey,tx,2,assetid2) == 0 )
                return eval->Invalid("invalid asset2 voutvalue for fillexchange");
            else if ( Getscriptaddress(destaddr,tx.vout[2].scriptPubKey) == 0 || strcmp(destaddr,origaddr) != 0 )
                return eval->Invalid("mismatched vout2 destaddr for fill");
            else if ( vinTx.vout[tx.vin[2].prevout.n].nValue != tx.vout[2].nValue )
                return eval->Invalid("mismatched vout2 nValue for fill");
            else if ( ValidateRemainder(remaining_price,tx.vout[0].nValue,assetoshis,tx.vout[2].nValue,tx.vout[1].nValue,tmpprice) == false )
                return eval->Invalid("mismatched remainder for fill");
            else if ( IsAssetvout(ignore,ignorepubkey,vinTx,tx.vin[1].prevout.n,assetid) == 0 )
                return eval->Invalid("mismatched vout0 unspendableaddr for fill");
            else if ( remaining_price != 0 )
            {
                if ( Getscriptaddress(destaddr,tx.vout[0].scriptPubKey) == 0 || strcmp(destaddr,Unspendableaddr) != 0 )
                    return eval->Invalid("mismatched vout0 unspendableaddr for fill");
                else if ( IsAssetvout(ignore,ignorepubkey,tx,0,assetid) == 0 )
                    return eval->Invalid("not asset vout0 to unspendableaddr for fill");
                else if ( funcid == 'S' && remaining_price < 10000 )
                    return eval->Invalid("dust vout0 to unspendableaddr for fill");
            }
            break;
    }
    return(true);
}

bool ProcessAssets(Eval* eval, std::vector<uint8_t> paramsNull,const CTransaction &ctx, unsigned int nIn)
{
    static uint256 zero;
    CTransaction createTx; uint256 assetid,assetid2,hashBlock; uint8_t funcid; int32_t i,n; uint64_t amount; std::vector<uint8_t> origpubkey;
    CTransaction tx = *(CTransaction *)&ctx;
    if ( paramsNull.size() != 0 ) // Don't expect params
        return eval->Invalid("Cannot have params");
    if ( (n= tx.vout.size()) == 0 )
        return eval->Invalid("no-vouts");
    if ( (funcid= DecodeOpRet(tx.vout[n-1].scriptPubKey,assetid,assetid2,amount,origpubkey)) == 0 )
        return eval->Invalid("Invalid opreturn payload");
    if ( eval->GetTxUnconfirmed(assetid,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset create txid");
    if ( assetid2 != zero && eval->GetTxUnconfirmed(assetid2,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset2 create txid");
    return(AssetValidate(eval,tx,n,funcid,assetid,assetid2,amount,origpubkey));
}
