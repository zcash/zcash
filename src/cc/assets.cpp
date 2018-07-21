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
#include "../core_io.h"
#include "../script/sign.h"
#include "../wallet/wallet.h"
#include <univalue.h>
#include <exception>

extern uint8_t NOTARY_PUBKEY33[33];
uint256 Parseuint256(char *hexstr);

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

#ifdef ENABLE_WALLET
extern CWallet* pwalletMain;
#endif

//BTCD Address: RAssetsAtGnvwgK9gVHBbAU4sVTah1hAm5
//BTCD Privkey: UvtvQVgVScXEYm4J3r4nE4nbFuGXSVM5pKec8VWXwgG9dmpWBuDh
//BTCD Address: RSavingsEYcivt2DFsxsKeCjqArV6oVtVZ
//BTCD Privkey: Ux6XQekTxokko6gZHz24B7PUsmUQtWFzG2W9nUA8jba7UoVbPBF4

static uint256 zeroid;
const char *Unspendableaddr = "RFYE2yL3KknWdHK6uNhvWacYsCUtwzjY3u";
char Unspendablehex[67] = { "02adf84e0e075cf90868bd4e3d34a03420e034719649c41f371fc70d8e33aa2702" };
uint8_t Unspendablepriv[32] = { 0x9b, 0x17, 0x66, 0xe5, 0x82, 0x66, 0xac, 0xb6, 0xba, 0x43, 0x83, 0x74, 0xf7, 0x63, 0x11, 0x3b, 0xf0, 0xf3, 0x50, 0x6f, 0xd9, 0x6b, 0x67, 0x85, 0xf9, 0x7a, 0xf0, 0x54, 0x4d, 0xb1, 0x30, 0x77 };

CPubKey pubkey2pk(std::vector<uint8_t> pubkey)
{
    CPubKey pk; int32_t i,n; uint8_t *dest,*pubkey33;
    n = pubkey.size();
    dest = (uint8_t *)pk.begin();
    pubkey33 = (uint8_t *)pubkey.data();
    for (i=0; i<n; i++)
        dest[i] = pubkey33[i];
    return(pk);
}

CPubKey GetUnspendable(uint8_t evalcode,uint8_t *unspendablepriv)
{
    static CPubKey nullpk;
    if ( unspendablepriv != 0 )
        memset(unspendablepriv,0,32);
    if ( evalcode == EVAL_ASSETS )
    {
        if ( unspendablepriv != 0 )
            memcpy(unspendablepriv,Unspendablepriv,32);
    } else return(nullpk);
    return(pubkey2pk(ParseHex(Unspendablehex)));
}

CScript EncodeCreateOpRet(uint8_t funcid,std::vector<uint8_t> origpubkey,std::string name,std::string description)
{
    CScript opret; uint8_t evalcode = EVAL_ASSETS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << origpubkey << name << description);
    return(opret);
}

uint256 revuint256(uint256 txid)
{
    uint256 revtxid; int32_t i;
    for (i=31; i>=0; i--)
        ((uint8_t *)&revtxid)[31-i] = ((uint8_t *)&txid)[i];
    return(revtxid);
}

CScript EncodeOpRet(uint8_t funcid,uint256 assetid,uint256 assetid2,uint64_t price,std::vector<uint8_t> origpubkey)
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

bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey)
{
    CTxDestination address; txnouttype whichType;
    //int32_t i; uint8_t *ptr = (uint8_t *)scriptPubKey.data();
    //for (i=0; i<scriptPubKey.size(); i++)
    //    fprintf(stderr,"%02x",ptr[i]);
    //fprintf(stderr," scriptPubKey\n");
    if ( ExtractDestination(scriptPubKey,address) != 0 )
    {
        strcpy(destaddr,(char *)CBitcoinAddress(address).ToString().c_str());
        //fprintf(stderr,"destaddr.(%s)\n",destaddr);
        return(true);
    }
    fprintf(stderr,"ExtractDestination failed\n");
    return(false);
}

std::vector<uint8_t> Mypubkey()
{
    std::vector<uint8_t> pubkey; int32_t i; uint8_t *dest,*pubkey33;
    pubkey33 = NOTARY_PUBKEY33;
    pubkey.resize(33);
    dest = pubkey.data();
    for (i=0; i<33; i++)
        dest[i] = pubkey33[i];
    return(pubkey);
}

bool Myprivkey(uint8_t myprivkey[])
{
    char coinaddr[64]; std::string strAddress; char *dest; int32_t i,n; CBitcoinAddress address; CKeyID keyID; CKey vchSecret;
    if ( Getscriptaddress(coinaddr,CScript() << Mypubkey() << OP_CHECKSIG) != 0 )
    {
        n = (int32_t)strlen(coinaddr);
        strAddress.resize(n+1);
        dest = (char *)strAddress.data();
        for (i=0; i<n; i++)
            dest[i] = coinaddr[i];
        dest[i] = 0;
        if ( address.SetString(strAddress) != 0 && address.GetKeyID(keyID) != 0 )
        {
#ifdef ENABLE_WALLET
            if ( pwalletMain->GetKey(keyID,vchSecret) != 0 )
            {
                memcpy(myprivkey,vchSecret.begin(),32);
                //for (i=0; i<32; i++)
                //    fprintf(stderr,"%02x",myprivkey[i]);
                //fprintf(stderr," found privkey!\n");
                return(true);
            }
#endif
        }
    }
    fprintf(stderr,"privkey for the -pubkey= address is not in the wallet, importprivkey!\n");
    return(false);
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
    vout = CTxOut(nValue,CCPubKey(payoutCond));
    cc_free(payoutCond);
    return(vout);
}

CC *MakeCC(uint8_t evalcode,CPubKey pk)
{
    if ( evalcode == EVAL_ASSETS )
    {
        std::vector<CC*> pks;
        pks.push_back(CCNewSecp256k1(pk));
        CC *assetCC = CCNewEval(E_MARSHAL(ss << evalcode));
        CC *Sig = CCNewThreshold(1, pks);
        return CCNewThreshold(2, {assetCC, Sig});
    } else return(0);
}

bool GetCCaddress(uint8_t evalcode,char *destaddr,CPubKey pk)
{
    CC *payoutCond;
    destaddr[0] = 0;
    if ( evalcode == EVAL_ASSETS )
    {
        if ( (payoutCond= MakeAssetCond(pk)) != 0 )
        {
            Getscriptaddress(destaddr,CCPubKey(payoutCond));
            cc_free(payoutCond);
        }
        return(destaddr[0] != 0);
    }
    fprintf(stderr,"%02x is invalid evalcode\n",evalcode);
    return false;
}
           
uint8_t DecodeOpRet(const CScript &scriptPubKey,uint256 &assetid,uint256 &assetid2,uint64_t &price,std::vector<uint8_t> &origpubkey)
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
        fprintf(stderr,"decode.[%c]\n",funcid);
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
                fprintf(stderr,"DecodeOpRet: illegal funcid.%02x\n",funcid);
                funcid = 0;
                break;
        }
    }
    return(funcid);
}

bool SetOrigpubkey(std::vector<uint8_t> &origpubkey,uint64_t &price,CTransaction &tx)
{
    uint256 assetid,assetid2;
    if ( DecodeOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,assetid,assetid2,price,origpubkey) != 0 )
        return(true);
    else return(false);
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
       
bool IsCCInput(CScript const& scriptSig)
{
    CC *cond;
    if ( (cond= GetCryptoCondition(scriptSig)) == 0 )
        return false;
    cc_free(cond);
    return true;
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
        nValue = tx.vout[v].nValue;
        fprintf(stderr,"CC vout v.%d of n.%d %.8f\n",v,n,(double)nValue/COIN);
        if ( v >= n-1 )
            return(0);
        if ( (funcid= DecodeOpRet(tx.vout[n-1].scriptPubKey,assetid,assetid2,price,origpubkey)) == 0 )
        {
            fprintf(stderr,"null decodeopret\n");
            return(0);
        }
        else if ( funcid == 'c' )
        {
            int32_t i;
            for (i=31; i>=0; i--)
                fprintf(stderr,"%02x",((uint8_t *)&refassetid)[i]);
            fprintf(stderr," isassetvout %s/v%d\n",tx.GetHash().ToString().c_str(),v);
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
    fprintf(stderr,"Isassetvout: normal output v.%d %.8f\n",v,(double)tx.vout[v].nValue/COIN);
    return(0);
}

bool SignTx(CMutableTransaction &mtx,int32_t vini,uint64_t utxovalue,const CScript scriptPubKey)
{
#ifdef ENABLE_WALLET
    CTransaction txNewConst(mtx); SignatureData sigdata; const CKeyStore& keystore = *pwalletMain;
    auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    if ( ProduceSignature(TransactionSignatureCreator(&keystore,&txNewConst,vini,utxovalue,SIGHASH_ALL),scriptPubKey,sigdata,consensusBranchId) != 0 )
    {
        UpdateTransaction(mtx,vini,sigdata);
        return(true);
    } else fprintf(stderr,"signing error for CreateAsset vini.%d %.8f\n",vini,(double)utxovalue/COIN);
#else
    return(false);
#endif
}

std::string FinalizeCCTx(uint8_t evalcode,CMutableTransaction &mtx,CPubKey mypk,uint64_t txfee,CScript opret)
{
    CTransaction vintx; std::string hex; uint256 hashBlock; uint64_t vinimask=0,utxovalues[64],change,totaloutputs=0,totalinputs=0; int32_t i,utxovout,n,err = 0; char myaddr[64],destaddr[64],unspendable[64]; uint8_t *privkey,myprivkey[32],unspendablepriv[32],*msg32 = 0; CC *mycond=0,*othercond=0,*cond; CPubKey unspendablepk;
    n = mtx.vout.size();
    for (i=0; i<n; i++)
    {
        //if ( mtx.vout[i].scriptPubKey.IsPayToCryptoCondition() == 0 )
            totaloutputs += mtx.vout[i].nValue;
    }
    if ( (n= mtx.vin.size()) > 64 )
    {
        fprintf(stderr,"FinalizeAssetTx: %d is too many vins\n",n);
        return(0);
    }
    Myprivkey(myprivkey);
    unspendablepk = GetUnspendable(evalcode,unspendablepriv);
    GetCCaddress(evalcode,myaddr,mypk);
    mycond = MakeCC(evalcode,mypk);
    GetCCaddress(evalcode,unspendable,unspendablepk);
    othercond = MakeCC(evalcode,unspendablepk);
    //fprintf(stderr,"myCCaddr.(%s) %p vs unspendable.(%s) %p\n",myaddr,mycond,unspendable,othercond);
    memset(utxovalues,0,sizeof(utxovalues));
    for (i=0; i<n; i++)
    {
        if ( GetTransaction(mtx.vin[i].prevout.hash,vintx,hashBlock,false) != 0 )
        {
            utxovout = mtx.vin[i].prevout.n;
            utxovalues[i] = vintx.vout[utxovout].nValue;
            totalinputs += utxovalues[i];
            if ( vintx.vout[utxovout].scriptPubKey.IsPayToCryptoCondition() == 0 )
            {
                //fprintf(stderr,"vin.%d is normal %.8f\n",i,(double)utxovalues[i]/COIN);
                vinimask |= (1LL << i);
            }
            else
            {
            }
        } else fprintf(stderr,"FinalizeCCTx couldnt find %s\n",mtx.vin[i].prevout.hash.ToString().c_str());
    }
    if ( totalinputs >= totaloutputs+2*txfee )
    {
        change = totalinputs - (totaloutputs+txfee);
        mtx.vout.push_back(CTxOut(change,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
    }
    mtx.vout.push_back(CTxOut(0,opret));
    PrecomputedTransactionData txdata(mtx);
    n = mtx.vin.size();
    for (i=0; i<n; i++)
    {
        if ( GetTransaction(mtx.vin[i].prevout.hash,vintx,hashBlock,false) != 0 )
        {
            utxovout = mtx.vin[i].prevout.n;
            if ( vintx.vout[utxovout].scriptPubKey.IsPayToCryptoCondition() == 0 )
            {
                if ( SignTx(mtx,i,vintx.vout[utxovout].nValue,vintx.vout[utxovout].scriptPubKey) == 0 )
                    fprintf(stderr,"signing error for vini.%d of %llx\n",i,(long long)vinimask);
            }
            else
            {
                Getscriptaddress(destaddr,vintx.vout[utxovout].scriptPubKey);
                //fprintf(stderr,"vin.%d is CC %.8f -> (%s)\n",i,(double)utxovalues[i]/COIN,destaddr);
                if ( strcmp(destaddr,myaddr) == 0 )
                {
                    privkey = myprivkey;
                    cond = mycond;
                    //fprintf(stderr,"my CC addr.(%s)\n",myaddr);
                }
                else if ( strcmp(destaddr,unspendable) == 0 )
                {
                    privkey = unspendablepriv;
                    cond = othercond;
                    //fprintf(stderr,"unspendable CC addr.(%s)\n",unspendable);
                }
                else
                {
                    fprintf(stderr,"vini.%d has unknown CC address.(%s)\n",i,destaddr);
                    continue;
                }
                uint256 sighash = SignatureHash(CCPubKey(cond), mtx, i, SIGHASH_ALL, 0, 0, &txdata);
                if ( cc_signTreeSecp256k1Msg32(cond,privkey,sighash.begin()) != 0 )
                    mtx.vin[i].scriptSig = CCSig(cond);
                else fprintf(stderr,"vini.%d has CC signing error address.(%s)\n",i,destaddr);
            }
        } else fprintf(stderr,"FinalizeAssetTx couldnt find %s\n",mtx.vin[i].prevout.hash.ToString().c_str());
    }
    if ( mycond != 0 )
        cc_free(mycond);
    if ( othercond != 0 )
        cc_free(othercond);
    std::string strHex = EncodeHexTx(mtx);
    if ( strHex.size() > 0 )
        return(strHex);
    else return(0);
}

           
bool ValidateRemainder(uint64_t remaining_price,uint64_t remaining_nValue,uint64_t orig_nValue,uint64_t received,uint64_t paid,uint64_t totalprice)
{
    uint64_t price,recvprice;
    if ( orig_nValue == 0 || received == 0 || paid == 0 || totalprice == 0 )
    {
        fprintf(stderr,"ValidateRemainder: orig_nValue == %llu || received == %llu || paid == %llu || totalprice == %llu\n",(long long)orig_nValue,(long long)received,(long long)paid,(long long)totalprice);
        return(false);
    }
    else if ( remaining_price < (totalprice - received) )
    {
        fprintf(stderr,"ValidateRemainder: remaining_price %llu < %llu (totalprice %llu - %llu received)\n",(long long)remaining_price,(long long)(totalprice - received),(long long)totalprice,(long long)received);
        return(false);
    }
    else if ( remaining_nValue < (orig_nValue - paid) )
    {
        fprintf(stderr,"ValidateRemainder: remaining_nValue %llu < %llu (totalprice %llu - %llu received)\n",(long long)remaining_nValue,(long long)(orig_nValue - paid),(long long)orig_nValue,(long long)paid);
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

bool SetFillamounts(uint64_t &paid,uint64_t &remaining_price,uint64_t orig_nValue,uint64_t received,uint64_t totalprice)
{
    uint64_t remaining_nValue,price,mult;
    remaining_price = (totalprice - received);
    price = (totalprice * COIN) / orig_nValue;
    mult = (received * COIN);
    if ( price > 0 && (paid= mult / price) > 0 )
    {
        if ( (mult % price) != 0 )
            paid--;
        remaining_nValue = (orig_nValue - paid);
        return(ValidateRemainder(remaining_price,remaining_nValue,orig_nValue,received,paid,totalprice));
    } else return(false);
}
           
/*uint64_t IsNormalUtxo(uint64_t output,uint64_t txfee,uint256 feetxid,int32_t feevout)
{
    CTransaction vintx; uint256 hashBlock; uint64_t nValue;
    if ( GetTransaction(feetxid,vintx,hashBlock,false) != 0 )
    {
        nValue = vintx.vout[feevout].nValue;
        if ( vintx.vout[feevout].scriptPubKey.IsPayToCryptoCondition() == 0 && nValue >= output+txfee )
            return(nValue);
    }
    return(0);
}*/

uint64_t AddCCinputs(CMutableTransaction &mtx,CPubKey mypk,uint256 assetid,uint64_t total)
{
    uint64_t totalinputs = 0;
    mtx.vin.push_back(CTxIn(Parseuint256((char *)"5117c5f5f7b077c3f8ef08bc0f5789d6b53a6fea61ee0a51b5c829797bd81a57"),0,CScript()));
    totalinputs = COIN;
    return(totalinputs);
}
       
uint64_t AddNormalinputs(CMutableTransaction &mtx,CPubKey mypk,uint64_t total,int32_t maxinputs)
{
    int32_t n = 0; uint64_t nValue,totalinputs = 0; std::vector<COutput> vecOutputs;
#ifdef ENABLE_WALLET
    const CKeyStore& keystore = *pwalletMain;
    assert(pwalletMain != NULL);
    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
    BOOST_FOREACH(const COutput& out, vecOutputs)
    {
        if ( out.fSpendable != 0 )
        {
            mtx.vin.push_back(CTxIn(out.tx->GetHash(),out.i,CScript()));
            nValue = out.tx->vout[out.i].nValue;
            totalinputs += nValue;
            if ( totalinputs >= total || n >= maxinputs )
                break;
        }
    }
    if ( totalinputs >= total )
        return(totalinputs);
#endif
    return(0);
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
    if ( AddNormalinputs(mtx,mypk,assetsupply+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeAssetsVout(assetsupply,mypk));
        return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeCreateOpRet('c',Mypubkey(),name,description)));
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
            if ( (inputs= AddCCinputs(mtx,mypk,assetid,total)) > 0 )
            {
                if ( inputs > total )
                    CCchange = (inputs - total);
                //for (i=0; i<n; i++)
                    mtx.vout.push_back(MakeAssetsVout(total,pubkey2pk(destpubkey)));
                if ( CCchange != 0 )
                    mtx.vout.push_back(MakeAssetsVout(CCchange,mypk));
                return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeOpRet('t',assetid,zeroid,0,Mypubkey())));
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
        return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeOpRet('b',assetid,zeroid,pricetotal,Mypubkey())));
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
            return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeOpRet('o',assetid,zeroid,0,Mypubkey())));
        }
    }
    fprintf(stderr,"no normal inputs\n");
    return(0);
}

std::string FillBuyOffer(uint64_t txfee,uint256 assetid,uint256 bidtxid,uint256 filltxid,int32_t fillvout)
{
    CTransaction vintx,filltx; uint256 hashBlock; CMutableTransaction mtx; CPubKey mypk; std::vector<uint8_t> origpubkey,tmppubkey; int32_t bidvout=0; uint64_t tmpprice,origprice,bidamount,paid_amount,fillamount,remaining_required;
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        if ( GetTransaction(bidtxid,vintx,hashBlock,false) != 0 && GetTransaction(filltxid,filltx,hashBlock,false) != 0 )
        {
            bidamount = vintx.vout[bidvout].nValue;
            SetOrigpubkey(origpubkey,origprice,vintx);
            fillamount = filltx.vout[fillvout].nValue;
            mtx.vin.push_back(CTxIn(bidtxid,bidvout,CScript()));
            if ( IsAssetvout(tmpprice,tmppubkey,filltx,fillvout,assetid) == fillamount )
            {
                mtx.vin.push_back(CTxIn(filltxid,fillvout,CScript())); // CC
                SetFillamounts(paid_amount,remaining_required,bidamount,fillamount,origprice);
                mtx.vout.push_back(CTxOut(bidamount - paid_amount,CScript() << ParseHex(Unspendablehex) << OP_CHECKSIG));
                mtx.vout.push_back(CTxOut(paid_amount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                mtx.vout.push_back(MakeAssetsVout(fillamount,pubkey2pk(origpubkey)));
                return(FinalizeCCTx(EVAL_ASSETS,mtx,mypk,txfee,EncodeOpRet('B',assetid,zeroid,remaining_required,origpubkey)));
            } else fprintf(stderr,"filltx wasnt for assetid\n");
        }
    }
    return(0);
}

/*

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

uint64_t AssetValidatevin(Eval* eval,char *origaddr,CTransaction &tx,CTransaction &vinTx)
{
    uint256 hashBlock; char destaddr[64],unspendable[64];
    origaddr[0] = destaddr[0] = 0;
    GetCCaddress(EVAL_ASSETS,unspendable,GetUnspendable(EVAL_ASSETS,0));
    if ( tx.vin[1].prevout.n != 0 )
        return eval->Invalid("vin1 needs to be buyvin.vout[0]");
    else if ( eval->GetTxUnconfirmed(tx.vin[1].prevout.hash,vinTx,hashBlock) == 0 )
        return eval->Invalid("always should find vin, but didnt");
    else if ( Getscriptaddress(destaddr,vinTx.vout[0].scriptPubKey) == 0 || strcmp(destaddr,unspendable) != 0 )
    {
        fprintf(stderr,"%s vs %s\n",destaddr,unspendable);
        return eval->Invalid("invalid vin unspendableaddr");
    }
    else if ( vinTx.vout[0].nValue < 10000 )
        return eval->Invalid("invalid dust for buyvin");
    else if ( Getorigaddr(origaddr,vinTx) == 0 )
        return eval->Invalid("couldnt get origaddr for buyvin");
    fprintf(stderr,"Got %.8f to origaddr.(%s)\n",(double)vinTx.vout[0].nValue/COIN,origaddr);
    return(vinTx.vout[0].nValue);
}

uint64_t AssetValidateBuyvin(Eval* eval,uint64_t &tmpprice,std::vector<uint8_t> &tmporigpubkey,char *origaddr,CTransaction &tx,uint256 refassetid)
{
    CTransaction vinTx; uint64_t nValue; uint256 assetid,assetid2;
    fprintf(stderr,"AssetValidateBuyvin\n");
    if ( (nValue= AssetValidatevin(eval,origaddr,tx,vinTx)) == 0 )
        return(0);
    else if ( vinTx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
        return eval->Invalid("invalid normal vout0 for buyvin");
    else
    {
        fprintf(stderr,"have %.8f checking assetid\n",(double)nValue/COIN);
        if ( DecodeOpRet(vinTx.vout[vinTx.vout.size()-1].scriptPubKey,assetid,assetid2,tmpprice,tmporigpubkey) != 'b' )
            return eval->Invalid("invalid opreturn for buyvin");
        else if ( refassetid != assetid )
            return eval->Invalid("invalid assetid for buyvin");
    }
    return(nValue);
}

uint64_t AssetValidateSellvin(Eval* eval,uint64_t &tmpprice,std::vector<uint8_t> &tmporigpubkey,char *origaddr,CTransaction &tx,uint256 assetid)
{
    CTransaction vinTx; uint64_t nValue,assetoshis;
    fprintf(stderr,"AssetValidateSellvin\n");
    if ( (nValue= AssetValidatevin(eval,origaddr,tx,vinTx)) == 0 )
        return(0);
    if ( (assetoshis= IsAssetvout(tmpprice,tmporigpubkey,vinTx,0,assetid)) != 0 )
        return eval->Invalid("invalid missing CC vout0 for sellvin");
    else return(assetoshis);
}

bool AssetValidate(Eval* eval,CTransaction &tx,int32_t numvouts,uint8_t funcid,uint256 assetid,uint256 assetid2,uint64_t remaining_price,std::vector<uint8_t> origpubkey)
{
    static uint256 zero;
    CTxDestination address; CTransaction vinTx; uint256 hashBlock; int32_t i,numvins; uint64_t nValue,assetoshis,outputs,inputs,tmpprice,ignore; std::vector<uint8_t> tmporigpubkey,ignorepubkey; char destaddr[64],origaddr[64];
    fprintf(stderr,"AssetValidate\n");
    numvins = tx.vin.size();
    outputs = inputs = 0;
    if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
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
            fprintf(stderr,"transfer validated %.8f -> %.8f\n",(double)inputs/COIN,(double)outputs/COIN);
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
                if ( IsCCInput(tx.vin[i].scriptSig) != 0 )
                    return eval->Invalid("invalid CC vin for buyoffer");
            }
            destaddr[0] = 0;
            for (i=0; i<numvouts-1; i++)
            {
                if ( i == 0 )
                {
                    if ( tx.vout[i].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("invalid normal vout for buyoffer");
                    else if ( Getscriptaddress(destaddr,tx.vout[i].scriptPubKey) == 0 || strcmp(destaddr,Unspendableaddr) != 0 )
                        return eval->Invalid("invalid vout0 destaddr for buyoffer");
                    else if ( tx.vout[i].nValue < 10000 )
                        return eval->Invalid("invalid vout0 dust for buyoffer");
                }
                else if ( tx.vout[i].scriptPubKey.IsPayToCryptoCondition() != 0 )
                    return eval->Invalid("invalid CC vout for buyoffer");
            }
            fprintf(stderr,"buy offer validated to destaddr.(%s)\n",destaddr);
            break;
            
        case 'o': // cancelbuy
            //vin.0: normal input
            //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
            //vout.0: vin.1 value to original pubkey buyTx.vout[0].nValue -> [origpubkey]
            //vout.1: normal output for change (if any)
            //vout.n-1: opreturn [EVAL_ASSETS] ['o']
            if ( (nValue= AssetValidateBuyvin(eval,tmpprice,tmporigpubkey,origaddr,tx,assetid)) == 0 )
                return(false);
            else if ( nValue != tx.vout[0].nValue )
                return eval->Invalid("mismatched refund for cancelbuy");
            else if ( Getscriptaddress(destaddr,tx.vout[0].scriptPubKey) == 0 || strcmp(destaddr,origaddr) != 0 )
                return eval->Invalid("invalid vout0 destaddr for cancelbuy");
            fprintf(stderr,"cancelbuy validated to destaddr.(%s)\n",destaddr);
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
            if ( (nValue= AssetValidateBuyvin(eval,tmpprice,tmporigpubkey,origaddr,tx,assetid)) == 0 )
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
    fprintf(stderr,"done Process assets\n");
    return(true);
}

bool ProcessAssets(Eval* eval, std::vector<uint8_t> paramsNull,const CTransaction &ctx, unsigned int nIn)
{
    static uint256 zero;
    CTransaction createTx; uint256 assetid,assetid2,hashBlock; uint8_t funcid; int32_t i,n; uint64_t amount; std::vector<uint8_t> origpubkey;
    CTransaction tx = *(CTransaction *)&ctx;
    fprintf(stderr,"Process assets\n");
    if ( paramsNull.size() != 0 ) // Don't expect params
        return eval->Invalid("Cannot have params");
    else if ( (n= tx.vout.size()) == 0 )
        return eval->Invalid("no-vouts");
    else if ( (funcid= DecodeOpRet(tx.vout[n-1].scriptPubKey,assetid,assetid2,amount,origpubkey)) == 0 )
        return eval->Invalid("Invalid opreturn payload");
    fprintf(stderr,"checking assetid tx\n");
    if ( eval->GetTxUnconfirmed(assetid,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset create txid");
    if ( assetid2 != zero && eval->GetTxUnconfirmed(assetid2,createTx,hashBlock) == 0 )
        return eval->Invalid("cant find asset2 create txid");
    return(AssetValidate(eval,tx,n,funcid,assetid,assetid2,amount,origpubkey));
}
