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

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "crosschain.h"
#include "notarisationdb.h"
#include "importcoin.h"
#include "base58.h"
#include "consensus/validation.h"
#include "cc/eval.h"
#include "cc/utils.h"
#include "cc/CCinclude.h"
#include "main.h"
#include "primitives/transaction.h"
#include "rpc/server.h"
#include "sync.h"
#include "util.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "script/standard.h"
#include "notaries_staked.h"

#include "key_io.h"
#include "cc/CCImportGateway.h"
#include "cc/CCtokens.h"

#include <stdint.h>
#include <univalue.h>
#include <regex>

using namespace std;

extern std::string CCerror;
extern std::string ASSETCHAINS_SELFIMPORT;
extern uint16_t ASSETCHAINS_CODAPORT, ASSETCHAINS_BEAMPORT;
int32_t ensure_CCrequirements(uint8_t evalcode);
bool EnsureWalletIsAvailable(bool avoidException);


int32_t komodo_MoM(int32_t *notarized_htp,uint256 *MoMp,uint256 *kmdtxidp,int32_t nHeight,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip);
int32_t komodo_MoMoMdata(char *hexstr,int32_t hexsize,struct komodo_ccdataMoMoM *mdata,char *symbol,int32_t kmdheight,int32_t notarized_height);
struct komodo_ccdata_entry *komodo_allMoMs(int32_t *nump,uint256 *MoMoMp,int32_t kmdstarti,int32_t kmdendi);
uint256 komodo_calcMoM(int32_t height,int32_t MoMdepth);
int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);
extern std::string ASSETCHAINS_SELFIMPORT;

//std::string MakeSelfImportSourceTx(CTxDestination &dest, int64_t amount, CMutableTransaction &mtx);
//int32_t GetSelfimportProof(std::string source, CMutableTransaction &mtx, CScript &scriptPubKey, TxProof &proof, std::string rawsourcetx, int32_t &ivout, uint256 sourcetxid, uint64_t burnAmount);
std::string MakeCodaImportTx(uint64_t txfee, std::string receipt, std::string srcaddr, std::vector<CTxOut> vouts);

UniValue assetchainproof(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 hash;

    // parse params and get notarisation data for tx
    if ( fHelp || params.size() != 1)
        throw runtime_error("assetchainproof needs a txid");

    hash = uint256S(params[0].get_str());
    CTransaction tx;
    auto proof = GetAssetchainProof(hash,tx);
    auto proofData = E_MARSHAL(ss << proof);
    return HexStr(proofData);
}


UniValue crosschainproof(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue ret(UniValue::VOBJ);
    //fprintf(stderr,"crosschainproof needs to be implemented\n");
    return(ret);
}


UniValue height_MoM(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    int32_t height,depth,notarized_height,MoMoMdepth,MoMoMoffset,kmdstarti,kmdendi; uint256 MoM,MoMoM,kmdtxid; uint32_t timestamp = 0; UniValue ret(UniValue::VOBJ); UniValue a(UniValue::VARR);
    if ( fHelp || params.size() != 1 )
        throw runtime_error("height_MoM height\n");
    LOCK(cs_main);
    height = atoi(params[0].get_str().c_str());
    if ( height <= 0 )
    {
        if ( chainActive.Tip() == 0 )
        {
            ret.push_back(Pair("error",(char *)"no active chain yet"));
            return(ret);
        }
        height = chainActive.Tip()->GetHeight();
    }
    //fprintf(stderr,"height_MoM height.%d\n",height);
    depth = komodo_MoM(&notarized_height,&MoM,&kmdtxid,height,&MoMoM,&MoMoMoffset,&MoMoMdepth,&kmdstarti,&kmdendi);
    ret.push_back(Pair("coin",(char *)(ASSETCHAINS_SYMBOL[0] == 0 ? "KMD" : ASSETCHAINS_SYMBOL)));
    ret.push_back(Pair("height",height));
    ret.push_back(Pair("timestamp",(uint64_t)timestamp));
    if ( depth > 0 )
    {
        ret.push_back(Pair("depth",depth));
        ret.push_back(Pair("notarized_height",notarized_height));
        ret.push_back(Pair("MoM",MoM.GetHex()));
        ret.push_back(Pair("kmdtxid",kmdtxid.GetHex()));
        if ( ASSETCHAINS_SYMBOL[0] != 0 )
        {
            ret.push_back(Pair("MoMoM",MoMoM.GetHex()));
            ret.push_back(Pair("MoMoMoffset",MoMoMoffset));
            ret.push_back(Pair("MoMoMdepth",MoMoMdepth));
            ret.push_back(Pair("kmdstarti",kmdstarti));
            ret.push_back(Pair("kmdendi",kmdendi));
        }
    } else ret.push_back(Pair("error",(char *)"no MoM for height"));

    return ret;
}

UniValue MoMoMdata(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if ( fHelp || params.size() != 3 )
        throw runtime_error("MoMoMdata symbol kmdheight ccid\n");
    UniValue ret(UniValue::VOBJ);
    char* symbol = (char *)params[0].get_str().c_str();
    int kmdheight = atoi(params[1].get_str().c_str());
    uint32_t ccid = atoi(params[2].get_str().c_str());
    ret.push_back(Pair("coin",symbol));
    ret.push_back(Pair("kmdheight",kmdheight-5));
    ret.push_back(Pair("ccid", (int) ccid));

    uint256 destNotarisationTxid;
    std::vector<uint256> moms;
    uint256 MoMoM = CalculateProofRoot(symbol, ccid, kmdheight-5, moms, destNotarisationTxid);

    UniValue valMoms(UniValue::VARR);
    for (int i=0; i<moms.size(); i++) valMoms.push_back(moms[i].GetHex());
    ret.push_back(Pair("MoMs", valMoms));
    ret.push_back(Pair("notarization_hash", destNotarisationTxid.GetHex()));
    ret.push_back(Pair("MoMoM", MoMoM.GetHex()));
    auto vmomomdata = E_MARSHAL(ss << MoMoM; ss << ((uint32_t)0));
    ret.push_back(Pair("data", HexStr(vmomomdata)));
    return ret;
}


UniValue calc_MoM(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    int32_t height,MoMdepth; uint256 MoM; UniValue ret(UniValue::VOBJ); UniValue a(UniValue::VARR);
    if ( fHelp || params.size() != 2 )
        throw runtime_error("calc_MoM height MoMdepth\n");
    LOCK(cs_main);
    height = atoi(params[0].get_str().c_str());
    MoMdepth = atoi(params[1].get_str().c_str());
    if ( height <= 0 || MoMdepth <= 0 || MoMdepth >= height )
        throw runtime_error("calc_MoM illegal height or MoMdepth\n");
    //fprintf(stderr,"height_MoM height.%d\n",height);
    MoM = komodo_calcMoM(height,MoMdepth);
    ret.push_back(Pair("coin",(char *)(ASSETCHAINS_SYMBOL[0] == 0 ? "KMD" : ASSETCHAINS_SYMBOL)));
    ret.push_back(Pair("height",height));
    ret.push_back(Pair("MoMdepth",MoMdepth));
    ret.push_back(Pair("MoM",MoM.GetHex()));
    return ret;
}


UniValue migrate_converttoexport(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    std::vector<uint8_t> rawproof; uint8_t *ptr; uint8_t i; uint32_t ccid = ASSETCHAINS_CC; uint64_t txfee = 10000;
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "migrate_converttoexport rawTx dest_symbol\n"
            "\nConvert a raw transaction to a cross-chain export.\n"
            "If neccesary, the transaction should be funded using fundrawtransaction.\n"
            "Finally, the transaction should be signed using signrawtransaction\n"
            "The finished export transaction, plus the payouts, should be passed to "
            "the \"migrate_createimporttransaction\" method to get the corresponding "
            "import transaction.\n"
            );

    if (ASSETCHAINS_CC < KOMODO_FIRSTFUNGIBLEID)
        throw runtime_error("-ac_cc < KOMODO_FIRSTFUNGIBLEID");

    if (ASSETCHAINS_SYMBOL[0] == 0)
        throw runtime_error("Must be called on assetchain");

    vector<uint8_t> txData(ParseHexV(params[0], "argument 1"));
    CMutableTransaction tx;
    if (!E_UNMARSHAL(txData, ss >> tx))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");

    string targetSymbol = params[1].get_str();
    if (targetSymbol.size() == 0 || targetSymbol.size() > 32)
        throw runtime_error("targetSymbol length must be >0 and <=32");

    if (strcmp(ASSETCHAINS_SYMBOL,targetSymbol.c_str()) == 0)
        throw runtime_error("cant send a coin to the same chain");
    
    /// Tested 44 vins p2pkh inputs as working. Set this at 25, but its a tx size limit. 
    // likely with a single RPC you can limit it by the size of tx.
    if (tx.vout.size() > 25)
        throw JSONRPCError(RPC_TYPE_ERROR, "Cannot have more than 50 vins, transaction too large.");

    CAmount burnAmount = 0;
    
    for (int i=0; i<tx.vout.size(); i++) burnAmount += tx.vout[i].nValue;
    if (burnAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Cannot export a negative or zero value.");
    // This is due to MAX MONEY in target. We set it at min 1 million coins, so you cant export more than 1 million,
    // without knowing the MAX money on the target this was the easiest solution. 
    if (burnAmount > 1000000LL*COIN)
        throw JSONRPCError(RPC_TYPE_ERROR, "Cannot export more than 1 million coins per export.");

    /* note: we marshal to rawproof in a different way (to be able to add other objects)
    rawproof.resize(strlen(ASSETCHAINS_SYMBOL));
    ptr = rawproof.data();
    for (i=0; i<rawproof.size(); i++)
        ptr[i] = ASSETCHAINS_SYMBOL[i]; */
    const std::string chainSymbol(ASSETCHAINS_SYMBOL);
    rawproof = E_MARSHAL(ss << chainSymbol); // add src chain name 

    CTxOut burnOut = MakeBurnOutput(burnAmount+txfee, ccid, targetSymbol, tx.vout,rawproof);
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("payouts", HexStr(E_MARSHAL(ss << tx.vout))));
    tx.vout.clear();
    tx.vout.push_back(burnOut);
    ret.push_back(Pair("exportTx", HexStr(E_MARSHAL(ss << tx))));
    return ret;
}

// creates burn tx as an alternative to 'migrate_converttoexport()'
UniValue migrate_createburntransaction(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue ret(UniValue::VOBJ);
    //uint8_t *ptr; 
    //uint8_t i; 
    uint32_t ccid = ASSETCHAINS_CC; 
    int64_t txfee = 10000;

    if (fHelp || params.size() != 3 && params.size() != 4)
        throw runtime_error(
            "migrate_createburntransaction dest_symbol dest_addr amount [tokenid]\n"
            "\nCreates a raw burn transaction to make a cross-chain coin or non-fungible token transfer.\n"
            "The parameters:\n"
            "dest_symbol   destination chain ac_name\n"
            "dest_addr     address on the destination chain where coins are to be sent or pubkey if tokens are to be sent\n"
            "amount        amount in coins to be burned on the source chain and sent to the destination address/pubkey on the destination chain, for tokens should be equal to 1\n"
            "tokenid       token id, if tokens are transferred (optional). Only non-fungible tokens are supported\n"
            "\n"
            "The transaction should be sent using sendrawtransaction to the source chain\n"
            "The finished burn transaction and payouts should be also passed to "
            "the \"migrate_createimporttransaction\" method to get the corresponding import transaction.\n"
        );

    if (ASSETCHAINS_CC < KOMODO_FIRSTFUNGIBLEID)
        throw runtime_error("-ac_cc < KOMODO_FIRSTFUNGIBLEID");

    if (ASSETCHAINS_SYMBOL[0] == 0)
        throw runtime_error("Must be called on assetchain");

    // if -pubkey not set it sends change to null pubkey. 
    // we need a better way to return errors from this function!
    if (ensure_CCrequirements(225) < 0)
        throw runtime_error("You need to set -pubkey, or run setpukbey RPC, or imports are disabled on this chain.");

    string targetSymbol = params[0].get_str();
    if (targetSymbol.size() == 0 || targetSymbol.size() > 32)
        throw runtime_error("targetSymbol length must be >0 and <=32");

    if (strcmp(ASSETCHAINS_SYMBOL, targetSymbol.c_str()) == 0)
        throw runtime_error("cant send a coin to the same chain");

    std::string dest_addr_or_pubkey = params[1].get_str();

    CAmount burnAmount;
    if(params.size() == 3)
        burnAmount = (CAmount)( atof(params[2].get_str().c_str()) * COIN + 0.00000000499999 );
    else
        burnAmount = atoll(params[2].get_str().c_str());

    if (burnAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Cannot export a negative or zero value.");
    if (burnAmount > 1000000LL * COIN)
        throw JSONRPCError(RPC_TYPE_ERROR, "Cannot export more than 1 million coins per export.");

    uint256 tokenid = zeroid;
    if( params.size() == 4 )
        tokenid = Parseuint256(params[3].get_str().c_str());
        
    if ( tokenid != zeroid && strcmp("LABS", targetSymbol.c_str()))
        throw JSONRPCError(RPC_TYPE_ERROR, "There is no tokens support on LABS.");

    CPubKey myPubKey = Mypubkey();
    struct CCcontract_info *cpTokens, C;
    cpTokens = CCinit(&C, EVAL_TOKENS);

    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

    const std::string chainSymbol(ASSETCHAINS_SYMBOL);
    std::vector<uint8_t> rawproof; //(chainSymbol.begin(), chainSymbol.end());

    if (tokenid.IsNull()) {        // coins
        int64_t inputs;
        if ((inputs = AddNormalinputs(mtx, myPubKey, burnAmount + txfee, 10)) == 0) {
            throw runtime_error("not enough funds, or need to merge utxos first\n");
        }

        CTxDestination txdest = DecodeDestination(dest_addr_or_pubkey.c_str());
        CScript scriptPubKey = GetScriptForDestination(txdest);
        if (!scriptPubKey.IsPayToPublicKeyHash()) {
            throw JSONRPCError(RPC_TYPE_ERROR, "Incorrect destination addr.");
        }
        mtx.vout.push_back(CTxOut(burnAmount, scriptPubKey));               // 'model' vout
        ret.push_back(Pair("payouts", HexStr(E_MARSHAL(ss << mtx.vout))));  // save 'model' vout

        rawproof = E_MARSHAL(ss << chainSymbol); // add src chain name 

        CTxOut burnOut = MakeBurnOutput(burnAmount+txfee, ccid, targetSymbol, mtx.vout, rawproof);  //make opret with burned amount

        mtx.vout.clear();               // remove 'model' vout

        int64_t change = inputs - (burnAmount+txfee);
        if (change != 0)
            mtx.vout.push_back(CTxOut(change, CScript() << ParseHex(HexStr(myPubKey)) << OP_CHECKSIG)); // make change here to prevent it from making in FinalizeCCtx

        mtx.vout.push_back(burnOut);    // mtx now has only burned vout (that is, amount sent to OP_RETURN making it unspendable)
        //std::string exportTxHex = FinalizeCCTx(0, cpTokens, mtx, myPubKey, txfee, CScript());  // no change no opret

    }
    else {   // tokens
        CTransaction tokenbasetx;
        uint256 hashBlock;
        vscript_t vopretNonfungible;
        vscript_t vopretBurnData;
        std::vector<uint8_t> vorigpubkey, vdestpubkey;
        std::string name, description;
        std::vector<std::pair<uint8_t, vscript_t>>  oprets;

        if (!myGetTransaction(tokenid, tokenbasetx, hashBlock))
            throw runtime_error("Could not load token creation tx\n");

        // check if it is non-fungible tx and get its second evalcode from non-fungible payload
        if (tokenbasetx.vout.size() == 0)
            throw runtime_error("No vouts in token tx\n");

        if (DecodeTokenCreateOpRet(tokenbasetx.vout.back().scriptPubKey, vorigpubkey, name, description, oprets) != 'c')
            throw runtime_error("Incorrect token creation tx\n");
        GetOpretBlob(oprets, OPRETID_NONFUNGIBLEDATA, vopretNonfungible);
        /* allow fungible tokens:
        if (vopretNonfungible.empty())
            throw runtime_error("No non-fungible token data\n"); */

        uint8_t destEvalCode = EVAL_TOKENS;
        if (!vopretNonfungible.empty())
            destEvalCode = vopretNonfungible.begin()[0];

        // check non-fungible tokens amount
        if (!vopretNonfungible.empty() && burnAmount != 1)
            throw JSONRPCError(RPC_TYPE_ERROR, "For non-fungible tokens amount should be equal to 1.");

        vdestpubkey = ParseHex(dest_addr_or_pubkey);
        CPubKey destPubKey = pubkey2pk(vdestpubkey);
        if (!destPubKey.IsValid())
            throw runtime_error("Invalid destination pubkey\n");

        int64_t inputs;
        if ((inputs = AddNormalinputs(mtx, myPubKey, txfee, 1)) == 0)  // for miners in dest chain
            throw runtime_error("No normal input found for two txfee\n");
      
        int64_t ccInputs;
        if ((ccInputs = AddTokenCCInputs(cpTokens, mtx, myPubKey, tokenid, burnAmount, 4)) < burnAmount)
            throw runtime_error("No token inputs found (please try to consolidate tokens)\n"); 

        // make payouts  (which will be in the import tx with token):
        mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, txfee, GetUnspendable(cpTokens, NULL)));  // new marker to token cc addr, burnable and validated, vout position now changed to 0 (from 1)
        mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, burnAmount, destPubKey));

        std::vector<std::pair<uint8_t, vscript_t>> voprets;
        if (!vopretNonfungible.empty())
            voprets.push_back(std::make_pair(OPRETID_NONFUNGIBLEDATA, vopretNonfungible));  // add additional opret with non-fungible data

        mtx.vout.push_back(CTxOut((CAmount)0, EncodeTokenCreateOpRet('c', vorigpubkey, name, description, voprets)));  // make token import opret
        ret.push_back(Pair("payouts", HexStr(E_MARSHAL(ss << mtx.vout))));  // save payouts for import tx

        rawproof = E_MARSHAL(ss << chainSymbol << tokenbasetx); // add src chain name and token creation tx

        CTxOut burnOut = MakeBurnOutput(0, ccid, targetSymbol, mtx.vout, rawproof);  //make opret with amount=0 because tokens are burned, not coins (see next vout) 
        mtx.vout.clear();  // remove payouts

        // now make burn transaction:
        mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, burnAmount, pubkey2pk(ParseHex(CC_BURNPUBKEY))));    // burn tokens
                                                                                                                
        int64_t change = inputs - txfee;
        if (change != 0)
            mtx.vout.push_back(CTxOut(change, CScript() << ParseHex(HexStr(myPubKey)) << OP_CHECKSIG));         // make change here to prevent it from making in FinalizeCCtx

        std::vector<CPubKey> voutTokenPubkeys;
        voutTokenPubkeys.push_back(pubkey2pk(ParseHex(CC_BURNPUBKEY)));  // maybe we do not need this because ccTokens has the const for burn pubkey

        int64_t ccChange = ccInputs - burnAmount;
        if (ccChange != 0)
            mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, ccChange, myPubKey));

        GetOpReturnData(burnOut.scriptPubKey, vopretBurnData);
        mtx.vout.push_back(CTxOut(txfee, EncodeTokenOpRet(tokenid, voutTokenPubkeys, std::make_pair(OPRETID_BURNDATA, vopretBurnData))));  //burn txfee for miners in dest chain
    }

    std::string burnTxHex = FinalizeCCTx(0, cpTokens, mtx, myPubKey, txfee, CScript()); //no change, no opret
    ret.push_back(Pair("BurnTxHex", burnTxHex));
    return ret;
}

// util func to check burn tx and source chain params
void CheckBurnTxSource(uint256 burntxid, UniValue &info) {

    CTransaction burnTx;
    uint256 blockHash;

    if (!myGetTransaction(burntxid, burnTx, blockHash))
        throw std::runtime_error("Cannot find burn transaction");

    if (blockHash.IsNull())
        throw std::runtime_error("Burn tx still in mempool");

    uint256 payoutsHash;
    std::string targetSymbol;
    uint32_t targetCCid;
    std::vector<uint8_t> rawproof;

    if (!UnmarshalBurnTx(burnTx, targetSymbol, &targetCCid, payoutsHash, rawproof))
        throw std::runtime_error("Cannot unmarshal burn tx data");

    vscript_t vopret;
    std::string sourceSymbol;
    CTransaction tokenbasetxStored;
    uint256 tokenid = zeroid;

    if (burnTx.vout.size() > 0 && GetOpReturnData(burnTx.vout.back().scriptPubKey, vopret) && !vopret.empty())   {
        if (vopret.begin()[0] == EVAL_TOKENS) {
            if (!E_UNMARSHAL(rawproof, ss >> sourceSymbol; ss >> tokenbasetxStored))
                throw std::runtime_error("Cannot unmarshal rawproof for tokens");

            uint8_t evalCode;
            std::vector<CPubKey> voutPubkeys;
            std::vector<std::pair<uint8_t, vscript_t>> oprets;
            if( DecodeTokenOpRet(burnTx.vout.back().scriptPubKey, evalCode, tokenid, voutPubkeys, oprets) == 0 )
                throw std::runtime_error("Cannot decode token opret in burn tx");

            if( tokenid != tokenbasetxStored.GetHash() )
                throw std::runtime_error("Incorrect tokenbase in burn tx");

            CTransaction tokenbasetx;
            uint256 hashBlock;
            if (!myGetTransaction(tokenid, tokenbasetx, hashBlock)) {
                throw std::runtime_error("Could not load tokenbase tx");
            }

            // check if nonfungible data present
            if (tokenbasetx.vout.size() > 0) {
                std::vector<uint8_t> origpubkey;
                std::string name, description;
                std::vector<std::pair<uint8_t, vscript_t>>  oprets;

                vscript_t vopretNonfungible;
                if (DecodeTokenCreateOpRet(tokenbasetx.vout.back().scriptPubKey, origpubkey, name, description, oprets) == 'c') {
                    GetOpretBlob(oprets, OPRETID_NONFUNGIBLEDATA, vopretNonfungible);
                    if (vopretNonfungible.empty())
                        throw std::runtime_error("Could not migrate fungible tokens");
                }
                else
                    throw std::runtime_error("Could not decode opreturn in tokenbase tx");
            }
            else
                throw std::runtime_error("Incorrect tokenbase tx: not opreturn");


            struct CCcontract_info *cpTokens, CCtokens_info;
            cpTokens = CCinit(&CCtokens_info, EVAL_TOKENS);
            int64_t ccInputs = 0, ccOutputs = 0;
            if( !TokensExactAmounts(true, cpTokens, ccInputs, ccOutputs, NULL, burnTx, tokenid) )
                throw std::runtime_error("Incorrect token burn tx: cc inputs <> cc outputs");
        }
        else if (vopret.begin()[0] == EVAL_IMPORTCOIN) {
            if (!E_UNMARSHAL(rawproof, ss >> sourceSymbol))
                throw std::runtime_error("Cannot unmarshal rawproof for coins");
        }
        else
            throw std::runtime_error("Incorrect eval code in opreturn");
    }
    else 
        throw std::runtime_error("No opreturn in burn tx");
    

    if (sourceSymbol != ASSETCHAINS_SYMBOL)
        throw std::runtime_error("Incorrect source chain in rawproof");

    if (targetCCid != ASSETCHAINS_CC)
        throw std::runtime_error("Incorrect CCid in burn tx");

    if (targetSymbol == ASSETCHAINS_SYMBOL)
        throw std::runtime_error("Must not be called on the destination chain");

    // fill info to return for the notary operator (if manual notarization) or user
    info.push_back(Pair("SourceSymbol", sourceSymbol));
    info.push_back(Pair("TargetSymbol", targetSymbol));
    info.push_back(Pair("TargetCCid", std::to_string(targetCCid)));
    if (!tokenid.IsNull())
        info.push_back(Pair("tokenid", tokenid.GetHex()));

}

/*
 * The process to migrate funds from a chain to chain
 *
 * 1.Create a transaction on assetchain (deprecated):
 * 1.1 generaterawtransaction
 * 1.2 migrate_converttoexport
 * 1.3 fundrawtransaction
 * 1.4 signrawtransaction
 *
 * alternatively, burn (export) transaction may be created with this new rpc call:
 * 1. migrate_createburntransaction
 *
 * next steps:
 * 2. migrate_createimporttransaction
 * 3. migrate_completeimporttransaction
 */

UniValue migrate_createimporttransaction(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() < 2)
        throw runtime_error("migrate_createimporttransaction burnTx payouts [notarytxid-1]..[notarytxid-N]\n\n"
                "Create an importTx given a burnTx and the corresponding payouts, hex encoded\n"
                "optional notarytxids are txids of notary operator proofs of burn tx existense (from destination chain).\n"
                "Do not make subsequent call to migrate_completeimporttransaction if notary txids are set");

    if (ASSETCHAINS_CC < KOMODO_FIRSTFUNGIBLEID)
        throw runtime_error("-ac_cc < KOMODO_FIRSTFUNGIBLEID");

    if (ASSETCHAINS_SYMBOL[0] == 0)
        throw runtime_error("Must be called on assetchain");

    vector<uint8_t> txData(ParseHexV(params[0], "argument 1"));

    CTransaction burnTx;
    if (!E_UNMARSHAL(txData, ss >> burnTx))
        throw runtime_error("Couldn't parse burnTx");

    if( burnTx.vin.size() == 0 )
        throw runtime_error("No vins in the burnTx");

    if (burnTx.vout.size() == 0)
        throw runtime_error("No vouts in the burnTx");


    vector<CTxOut> payouts;
    if (!E_UNMARSHAL(ParseHexV(params[1], "argument 2"), ss >> payouts))
        throw runtime_error("Couldn't parse payouts");

    ImportProof importProof;
    if (params.size() == 2) {  // standard MoMoM based notarization
        // get MoM import proof
        importProof = ImportProof(GetAssetchainProof(burnTx.GetHash(), burnTx));
    }
    else   {  // notarization by manual operators notary tx
        UniValue info(UniValue::VOBJ);
        CheckBurnTxSource(burnTx.GetHash(), info);

        // get notary import proof
        std::vector<uint256> notaryTxids;
        for (int i = 2; i < params.size(); i++) {
            uint256 txid = Parseuint256(params[i].get_str().c_str());
            if (txid.IsNull())
                throw runtime_error("Incorrect notary approval txid");
            notaryTxids.push_back(txid);
        }
        importProof = ImportProof(notaryTxids);
    }

    CTransaction importTx = MakeImportCoinTransaction(importProof, burnTx, payouts);

    std::string importTxHex = HexStr(E_MARSHAL(ss << importTx));
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("ImportTxHex", importTxHex));
    return ret;
}

UniValue migrate_completeimporttransaction(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("migrate_completeimporttransaction importTx [offset]\n\n"
                "Takes a cross chain import tx with proof generated on assetchain "
                "and extends proof to target chain proof root\n"
                "offset is optional, use it to increase the used KMD height, use when import fails.");

    if (ASSETCHAINS_SYMBOL[0] != 0)
        throw runtime_error("Must be called on KMD");

    CTransaction importTx;
    if (!E_UNMARSHAL(ParseHexV(params[0], "argument 1"), ss >> importTx))
        throw runtime_error("Couldn't parse importTx");
    
    int32_t offset = 0;
    if ( params.size() == 2 )
        offset = params[1].get_int();

    CompleteImportTransaction(importTx, offset);

    std::string importTxHex = HexStr(E_MARSHAL(ss << importTx));
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("ImportTxHex", importTxHex));
    return ret;
}

/*
* Alternate coin migration solution if MoMoM migration has failed
*
* The workflow:
* On the source chain user calls migrate_createburntransaction, sends the burn tx to the chain and sends its txid and the source chain name to the notary operators (off-chain)
* the notary operators call migrate_checkburntransactionsource on the source chain
* on the destination chain the notary operators call migrate_createnotaryapprovaltransaction and pass the burn txid and txoutproof received from the previous call, 
* the notary operators send the approval transactions to the chain and send their txids to the user (off-chain)
* on the source chain the user calls migrate_createimporttransaction and passes to it notary txids as additional parameters
* then the user sends the import transaction to the destination chain (where the notary approvals will be validated)
*/

// checks if burn tx exists and params stored in the burn tx match to the source chain
// returns txproof
// run it on the source chain
UniValue migrate_checkburntransactionsource(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("migrate_checkburntransactionsource burntxid\n\n"
            "checks if params stored in the burn tx match to its tx chain");

    if (ASSETCHAINS_SYMBOL[0] == 0)
        throw runtime_error("Must be called on asset chain");

    uint256 burntxid = Parseuint256(params[0].get_str().c_str());
    UniValue result(UniValue::VOBJ);
    CheckBurnTxSource(burntxid, result);  // check and get burn tx data

    // get tx proof for burn tx
    UniValue nextparams(UniValue::VARR);
    UniValue txids(UniValue::VARR);
    txids.push_back(burntxid.GetHex());
    nextparams.push_back(txids);
    result.push_back(Pair("TxOutProof", gettxoutproof(nextparams, false, mypk)));  // get txoutproof
    result.push_back(Pair("result", "success"));  // get txoutproof

    return result;
}

// creates a tx for the dest chain with txproof
// used as a momom-backup manual import solution
// run it on the dest chain
UniValue migrate_createnotaryapprovaltransaction(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("migrate_createnotaryapprovaltransaction burntxid txoutproof\n\n"
            "Creates a tx for destination chain with burn tx proof\n"
            "txoutproof should be retrieved by komodo-cli migrate_checkburntransactionsource call on the source chain\n" );

    if (ASSETCHAINS_SYMBOL[0] == 0)
        throw runtime_error("Must be called on asset chain");

    uint256 burntxid = Parseuint256(params[0].get_str().c_str());
    if (burntxid.IsNull())
        throw runtime_error("Couldn't parse burntxid or it is null");

    std::vector<uint8_t> proofData = ParseHex(params[1].get_str());
    CMerkleBlock merkleBlock;
    std::vector<uint256> prooftxids;
    if (!E_UNMARSHAL(proofData, ss >> merkleBlock))
        throw runtime_error("Couldn't parse txoutproof");

    merkleBlock.txn.ExtractMatches(prooftxids);
    if (std::find(prooftxids.begin(), prooftxids.end(), burntxid) == prooftxids.end())
        throw runtime_error("No burntxid in txoutproof");

    const int64_t txfee = 10000;
    struct CCcontract_info *cpDummy, C;
    cpDummy = CCinit(&C, EVAL_TOKENS);  // just for FinalizeCCtx to work 

    // creating a tx with proof:
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    if (AddNormalinputs(mtx, Mypubkey(), txfee*2, 4) == 0) 
        throw runtime_error("Cannot find normal inputs\n");
    
    mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(Mypubkey())) << OP_CHECKSIG));
    std::string notaryTxHex = FinalizeCCTx(0, cpDummy, mtx, Mypubkey(), txfee, CScript() << OP_RETURN << E_MARSHAL(ss << proofData;));

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("NotaryTxHex", notaryTxHex));
    return result;
}

// creates a source 'quasi-burn' tx for AC_PUBKEY
// run it on the same asset chain
UniValue selfimport(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ);
    std::string destaddr;
    std::string source; 
    std::string sourceTxHex;
    std::string importTxHex;
    CTransaction burnTx; 
    CTxOut burnOut; 
    uint64_t burnAmount; 
    uint256 sourcetxid, blockHash; 
	std::vector<CTxOut> vouts; 
	std::vector<uint8_t> rawproof;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("selfimport only works on -ac_import chains");

    if (fHelp || params.size() != 2)
        throw runtime_error("selfimport destaddr amount\n"
                  //TODO:   "or selfimport rawburntx burntxid {nvout|\"find\"} rawproof source bindtxid height} \n"
                            "\ncreates self import coin transaction");

    destaddr = params[0].get_str();
    burnAmount = atof(params[1].get_str().c_str()) * COIN + 0.00000000499999;

    source = ASSETCHAINS_SELFIMPORT;   //defaults to -ac_import=... param

    if (source == "BEAM")
    {
        if (ASSETCHAINS_BEAMPORT == 0)
            return(-1);
        // confirm via ASSETCHAINS_BEAMPORT that burnTx/hash is a valid BEAM burn
        // return(0);
        return -1;
    }
    else if (source == "CODA")
    {
        if (ASSETCHAINS_CODAPORT == 0)
            return(-1);
        // confirm via ASSETCHAINS_CODAPORT that burnTx/hash is a valid CODA burn
        // return(0);
        return -1;
    }
    else if (source == "PEGSCC")
    {
        return -1;
    }
    else if (source == "PUBKEY")
    {
        ImportProof proofNull;
        CTxDestination dest = DecodeDestination(destaddr.c_str());
        CMutableTransaction sourceMtx = MakeSelfImportSourceTx(dest, burnAmount);  // make self-import source tx
        vscript_t rawProofEmpty;
        
        CMutableTransaction templateMtx;
        // prepare self-import 'quasi-burn' tx and also create vout for import tx (in mtx.vout):
        if (GetSelfimportProof(sourceMtx, templateMtx, proofNull) < 0)
            throw std::runtime_error("Failed creating selfimport template tx");

        vouts = templateMtx.vout;
        burnOut = MakeBurnOutput(burnAmount, 0xffffffff, ASSETCHAINS_SELFIMPORT, vouts, rawProofEmpty);
        templateMtx.vout.clear();
        templateMtx.vout.push_back(burnOut);	// burn tx has only opret with vouts and optional proof

        burnTx = templateMtx;					// complete the creation of 'quasi-burn' tx

        sourceTxHex = HexStr(E_MARSHAL(ss << sourceMtx));
        importTxHex = HexStr(E_MARSHAL(ss << MakeImportCoinTransaction(proofNull, burnTx, vouts)));
      
        result.push_back(Pair("SourceTxHex", sourceTxHex));
        result.push_back(Pair("ImportTxHex", importTxHex));
 
        return result;
    }
    else if (source == ASSETCHAINS_SELFIMPORT)
    {
        return -1;
    }
    return result;
}

bool GetNotarisationNotaries(uint8_t notarypubkeys[64][33], int8_t &numNN, const std::vector<CTxIn> &vin, std::vector<int8_t> &NotarisationNotaries);


UniValue importdual(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ);
    CMutableTransaction mtx;
    std::string hex,source,sourceaddr,destaddr,burntxid; uint64_t burnAmount;
    CPubKey destpub; std::vector<CTxOut> vouts;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importdual only works on -ac_import chains");

    if (fHelp || params.size() < 4)
        throw runtime_error("burntxid source_addr dest_pubkey amount\n");

    CCerror = "";

    burntxid = params[0].get_str();
    sourceaddr = params[1].get_str();
    destaddr = params[2].get_str();
    burnAmount = atof(params[3].get_str().c_str()) * COIN + 0.00000000499999;

    source = ASSETCHAINS_SELFIMPORT;   //defaults to -ac_import=... param

    CTxDestination dest = DecodeDestination(destaddr.c_str());
    CScript scriptPubKey = GetScriptForDestination(dest);
    vouts.push_back(CTxOut(burnAmount,scriptPubKey));

    if (source == "BEAM")
    {
        if (ASSETCHAINS_BEAMPORT == 0)
            return(-1);
        // confirm via ASSETCHAINS_BEAMPORT that burnTx/hash is a valid BEAM burn
        // return(0);
        return -1;
    }
    else if (source == "CODA")
    {
        if (ASSETCHAINS_CODAPORT == 0)
            return(-1);
        hex=MakeCodaImportTx(0,burntxid,sourceaddr,vouts);
        // confirm via ASSETCHAINS_CODAPORT that burnTx/hash is a valid CODA burn
        // return(0);
    }
    else if (source == "PEGSCC")
    {
        return -1;
    }
    RETURN_IF_ERROR(CCerror);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt importdual");
    return result;
}

UniValue importgatewayinfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 txid;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importgatewaybind only works on -ac_import chains");
    if ( fHelp || params.size() != 1 )
        throw runtime_error("importgatewayinfo bindtxid\n");
    txid = Parseuint256(params[0].get_str().c_str());
    return(ImportGatewayInfo(txid));
}


UniValue importgatewaybind(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ);
    CMutableTransaction mtx; std::vector<unsigned char> pubkey;
    std::string hex,coin; int32_t i,M,N; std::vector<CPubKey> pubkeys;
    uint256 oracletxid; uint8_t p1,p2,p3,p4;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importgatewaybind only works on -ac_import chains");
    if ( fHelp || params.size() != 8) 
        throw runtime_error("use \'importgatewaybind coin orcletxid M N pubkeys pubtype p2shtype wiftype [taddr]\' to bind an import gateway\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    CCerror = "";
    coin = params[0].get_str();
    oracletxid = Parseuint256(params[1].get_str().c_str()); 
    M = atoi(params[2].get_str().c_str());
    N = atoi(params[3].get_str().c_str());
    if ( M > N || N == 0 || N > 15 )
        throw runtime_error("illegal M or N > 15\n");
    if ( params.size() < 4+N+3 )
        throw runtime_error("not enough parameters for N pubkeys\n");
    for (i=0; i<N; i++)
    {       
        pubkey = ParseHex(params[4+i].get_str().c_str());
        if (pubkey.size()!= 33)
            throw runtime_error("invalid destination pubkey");
        pubkeys.push_back(pubkey2pk(pubkey));
    }
    p1 = atoi((char *)params[4+N].get_str().c_str());
    p2 = atoi((char *)params[4+N+1].get_str().c_str());
    p3 = atoi((char *)params[4+N+2].get_str().c_str());
    if (params.size() == 7+N+1) p4 = atoi((char *)params[7+N].get_str().c_str());
    if (coin == "BEAM" || coin == "CODA")
    {
        ERR_RESULT("for BEAM and CODA import use importdual RPC");
        return result;
    }
    else if (coin != ASSETCHAINS_SELFIMPORT)
    {
        ERR_RESULT("source coin not equal to ac_import name");
        return result;
    }
    hex = ImportGatewayBind(0, coin, oracletxid, M, N, pubkeys, p1, p2, p3, p4);
    RETURN_IF_ERROR(CCerror);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt importgatewaybind");
    return result;
}

UniValue importgatewaydeposit(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ);
    CMutableTransaction mtx; std::vector<uint8_t> rawproof;
    std::string hex,coin,rawburntx; int32_t height,burnvout; int64_t amount;
    CPubKey destpub; std::vector<CTxOut> vouts; uint256 bindtxid,burntxid;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importgatewaydeposit only works on -ac_import chains");
    if ( fHelp || params.size() != 9) 
        throw runtime_error("use \'importgatewaydeposit bindtxid height coin burntxid nvout rawburntx rawproof destpub amount\' to import deposited coins\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    CCerror = "";
    bindtxid = Parseuint256(params[0].get_str().c_str()); 
    height = atoi(params[1].get_str().c_str());
    coin = params[2].get_str();
    burntxid = Parseuint256(params[3].get_str().c_str()); 
    burnvout = atoi(params[4].get_str().c_str());
    rawburntx = params[5].get_str();
    rawproof = ParseHex(params[6].get_str());
    destpub = ParseHex(params[7].get_str());
    amount = atof(params[8].get_str().c_str()) * COIN + 0.00000000499999;
    if (coin == "BEAM" || coin == "CODA")
    {
        ERR_RESULT("for BEAM and CODA import use importdual RPC");
        return result;
    }
    else if (coin != ASSETCHAINS_SELFIMPORT)
    {
        ERR_RESULT("source coin not equal to ac_import name");
        return result;
    }
    hex = ImportGatewayDeposit(0, bindtxid, height, coin, burntxid, burnvout, rawburntx, rawproof, destpub, amount);
    RETURN_IF_ERROR(CCerror);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt importgatewaydeposit");
    return result;
}

UniValue importgatewaywithdraw(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ);
    CMutableTransaction mtx; std::vector<uint8_t> rawproof;
    std::string hex,coin,rawburntx; int64_t amount; int32_t height,burnvout;
    CPubKey destpub; std::vector<CTxOut> vouts; uint256 bindtxid,burntxid;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importgatewaywithdraw only works on -ac_import chains");
    if ( fHelp || params.size() != 4) 
        throw runtime_error("use \'importgatewaywithdraw bindtxid coin withdrawpub amount\' to burn imported coins and withdraw them on external chain\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    CCerror = "";
    bindtxid = Parseuint256(params[0].get_str().c_str()); 
    coin = params[1].get_str();
    destpub = ParseHex(params[2].get_str());
    amount = atof((char *)params[3].get_str().c_str()) * COIN + 0.00000000499999;
    if (coin == "BEAM" || coin == "CODA")
    {
        ERR_RESULT("for BEAM and CODA import use importdual RPC");
        return result;
    }
    else if (coin != ASSETCHAINS_SELFIMPORT)
    {
        ERR_RESULT("source coin not equal to ac_import name");
        return result;
    }
    hex = ImportGatewayWithdraw(0, bindtxid, coin, destpub, amount);
    RETURN_IF_ERROR(CCerror);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt importgatewaywithdraw");
    return result;
}

UniValue importgatewaypartialsign(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string coin,parthex,hex; uint256 txid;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importgatewayspartialsign only works on -ac_import chains");
    if ( fHelp || params.size() != 3 )
        throw runtime_error("importgatewayspartialsign txidaddr refcoin hex\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    txid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    parthex = params[2].get_str();
    hex = ImportGatewayPartialSign(0,txid,coin,parthex);
    RETURN_IF_ERROR(CCerror);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex",hex));
    } else ERR_RESULT("couldnt importgatewayspartialsign");
    return(result);
}

UniValue importgatewaycompletesigning(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 withdrawtxid; std::string txhex,hex,coin;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importgatewaycompletesigning only works on -ac_import chains");
    if ( fHelp || params.size() != 3 )
        throw runtime_error("importgatewaycompletesigning withdrawtxid coin hex\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    withdrawtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    txhex = params[2].get_str();
    hex = ImportGatewayCompleteSigning(0,withdrawtxid,coin,txhex);
    RETURN_IF_ERROR(CCerror);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt importgatewaycompletesigning");
    return(result);
}

UniValue importgatewaymarkdone(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 completetxid; std::string hex,coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("importgatewaymarkdone completesigningtx coin\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    completetxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    hex = ImportGatewayMarkDone(0,completetxid,coin);
    RETURN_IF_ERROR(CCerror);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt importgatewaymarkdone");
    return(result);
}

UniValue importgatewaypendingwithdraws(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 bindtxid; std::string coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("importgatewaypendingwithdraws bindtxid coin\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    return(ImportGatewayPendingWithdraws(bindtxid,coin));
}

UniValue importgatewayprocessed(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 bindtxid; std::string coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("importgatewayprocessed bindtxid coin\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    return(ImportGatewayProcessedWithdraws(bindtxid,coin));
}

UniValue importgatewayexternaladdress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 bindtxid; CPubKey pubkey;

    if ( fHelp || params.size() != 2)
        throw runtime_error("importgatewayexternaladdress bindtxid pubkey\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    pubkey = ParseHex(params[1].get_str().c_str());
    return(ImportGatewayExternalAddress(bindtxid,pubkey));
}

UniValue importgatewaydumpprivkey(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 bindtxid;

    if ( fHelp || params.size() != 2)
        throw runtime_error("importgatewaydumpprivkey bindtxid address\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    std::string strAddress = params[1].get_str();
    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid transparent address");
    }
    const CKeyID *keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    }
    CKey vchSecret;
    // if (!pwalletMain->GetKey(*keyID, vchSecret)) {
    //     throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + strAddress + " is not known");
    //}
    return(ImportGatewayDumpPrivKey(bindtxid,vchSecret));
}

UniValue getNotarisationsForBlock(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    // TODO take timestamp as param, and loop blockindex to get starting/finish height.
    if (fHelp || params.size() != 1)
        throw runtime_error("getNotarisationsForBlock height\n\n"
                "Takes a block height and returns notarisation information "
                "within the block");

    LOCK(cs_main);
    int32_t height = params[0].get_int();
    if ( height < 0 || height > chainActive.Height() )
        throw runtime_error("height out of range.\n");
    
    uint256 blockHash = chainActive[height]->GetBlockHash(); 
    
    NotarisationsInBlock nibs;
    GetBlockNotarisations(blockHash, nibs);
    UniValue out(UniValue::VOBJ);
    //out.push_back(make_pair("blocktime",(int)));
    UniValue labs(UniValue::VARR);
    UniValue kmd(UniValue::VARR);
    int8_t numNN = 0, numSN = 0; uint8_t notarypubkeys[64][33] = {0}; uint8_t LABSpubkeys[64][33] = {0};
    numNN = komodo_notaries(notarypubkeys, height, chainActive[height]->nTime);
    numSN = numStakedNotaries(LABSpubkeys,STAKED_era(chainActive[height]->nTime));

    BOOST_FOREACH(const Notarisation& n, nibs)
    {
        UniValue item(UniValue::VOBJ); UniValue notaryarr(UniValue::VARR); std::vector<int8_t> NotarisationNotaries;
        uint256 hash; CTransaction tx;
        if ( myGetTransaction(n.first,tx,hash) )
        {
            if ( is_STAKED(n.second.symbol) != 0 )
            {
                if ( !GetNotarisationNotaries(LABSpubkeys, numSN, tx.vin, NotarisationNotaries) )
                    continue;
            }
            else 
            {
                if ( !GetNotarisationNotaries(notarypubkeys, numNN, tx.vin, NotarisationNotaries) )
                    continue;
            }
        }
        item.push_back(make_pair("txid", n.first.GetHex()));
        item.push_back(make_pair("chain", n.second.symbol));
        item.push_back(make_pair("height", (int)n.second.height));
        item.push_back(make_pair("blockhash", n.second.blockHash.GetHex()));
        //item.push_back(make_pair("KMD_height", height)); // for when timstamp input is used.
        
        for ( auto notary : NotarisationNotaries )
            notaryarr.push_back(notary);
        item.push_back(make_pair("notaries",notaryarr));
        if ( is_STAKED(n.second.symbol) != 0 )
            labs.push_back(item);
        else 
            kmd.push_back(item);
    }
    out.push_back(make_pair("KMD", kmd));
    out.push_back(make_pair("LABS", labs));
    return out;
}

/*UniValue getNotarisationsForBlock(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("getNotarisationsForBlock blockHash\n\n"
                "Takes a block hash and returns notarisation transactions "
                "within the block");

    uint256 blockHash = uint256S(params[0].get_str());

    NotarisationsInBlock nibs;
    GetBlockNotarisations(blockHash, nibs);
    UniValue out(UniValue::VARR);
    BOOST_FOREACH(const Notarisation& n, nibs)
    {
        UniValue item(UniValue::VARR);
        item.push_back(n.first.GetHex());
        item.push_back(HexStr(E_MARSHAL(ss << n.second)));
        out.push_back(item);
    }
    return out;
}*/


UniValue scanNotarisationsDB(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("scanNotarisationsDB blockHeight symbol [blocksLimit=1440]\n\n"
                "Scans notarisationsdb backwards from height for a notarisation"
                " of given symbol");
    int height = atoi(params[0].get_str().c_str());
    std::string symbol = params[1].get_str().c_str();

    int limit = 1440;
    if (params.size() > 2) {
        limit = atoi(params[2].get_str().c_str());
    }

    if (height == 0) {
        height = chainActive.Height();
    }

    Notarisation nota;
    int matchedHeight = ScanNotarisationsDB(height, symbol, limit, nota);
    if (!matchedHeight) return NullUniValue;
    UniValue out(UniValue::VOBJ);
    out.pushKV("height", matchedHeight);
    out.pushKV("hash", nota.first.GetHex());
    out.pushKV("opreturn", HexStr(E_MARSHAL(ss << nota.second)));
    return out;
}

UniValue getimports(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getimports \"hash|height\"\n"
            "\n\n"
            "\nResult:\n"
            "{\n"
            "  \"imports\" : [                  (json array)\n"
            "       \"transactionid\" : {       (json object)\n"
            "           \"value\" :             (numeric)\n"
            "           \"address\" :           (string)\n"
            "           \"export\" {                (json object)\n"
            "               \"txid\" :              (string)\n"
            "               \"value\" :             (numeric)\n"
            "               \"chain\" :             (string)\n"
            "           }\n"
            "       }"
            "  ]\n"
            "  \"TotalImported\" :              (numeric)\n"
            "  \"time\" :                       (numeric)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getimports", "\"00000000febc373a1da2bd9f887b105ad79ddc26ac26c2b28652d64e5207c5b5\"")
            + HelpExampleRpc("getimports", "\"00000000febc373a1da2bd9f887b105ad79ddc26ac26c2b28652d64e5207c5b5\"")
            + HelpExampleCli("getimports", "12800")
            + HelpExampleRpc("getimports", "12800")
        );

    LOCK(cs_main);

    std::string strHash = params[0].get_str();

    // If height is supplied, find the hash
    if (strHash.size() < (2 * sizeof(uint256))) {
        // std::stoi allows characters, whereas we want to be strict
        regex r("[[:digit:]]+");
        if (!regex_match(strHash, r)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block height parameter");
        }

        int nHeight = -1;
        try {
            nHeight = std::stoi(strHash);
        }
        catch (const std::exception &e) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block height parameter");
        }

        if (nHeight < 0 || nHeight > chainActive.Height()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
        }
        strHash = chainActive[nHeight]->GetBlockHash().GetHex();
    }

    uint256 hash(uint256S(strHash));

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];

    if (fHavePruned && !(pblockindex->nStatus & BLOCK_HAVE_DATA) && pblockindex->nTx > 0)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block not available (pruned data)");

    if(!ReadBlockFromDisk(block, pblockindex,1))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

    UniValue result(UniValue::VOBJ);
    CAmount TotalImported = 0;
    UniValue imports(UniValue::VARR);
    BOOST_FOREACH(const CTransaction&tx, block.vtx)
    {
        if(tx.IsCoinImport())
        {
            UniValue objTx(UniValue::VOBJ);
            objTx.push_back(Pair("txid",tx.GetHash().ToString()));
            ImportProof proof; CTransaction burnTx; std::vector<CTxOut> payouts; CTxDestination importaddress;
            TotalImported += tx.vout[0].nValue; // were vouts swapped? 
            objTx.push_back(Pair("amount", ValueFromAmount(tx.vout[1].nValue)));
            if (ExtractDestination(tx.vout[1].scriptPubKey, importaddress))
            {
                objTx.push_back(Pair("address", CBitcoinAddress(importaddress).ToString()));
            }
            UniValue objBurnTx(UniValue::VOBJ);      
            CPubKey vinPubkey;
            if (UnmarshalImportTx(tx, proof, burnTx, payouts)) 
            {
                if (burnTx.vout.size() == 0)
                    continue;
                objBurnTx.push_back(Pair("txid", burnTx.GetHash().ToString()));
                objBurnTx.push_back(Pair("amount", ValueFromAmount(burnTx.vout.back().nValue)));
                // extract op_return to get burn source chain.
                std::vector<uint8_t> burnOpret; std::string targetSymbol; uint32_t targetCCid; uint256 payoutsHash; std::vector<uint8_t>rawproof;
                if (UnmarshalBurnTx(burnTx, targetSymbol, &targetCCid, payoutsHash, rawproof))
                {
                    if (rawproof.size() > 0)
                    {
                        std::string sourceSymbol;
                        CTransaction tokenbasetx;
                        E_UNMARSHAL(rawproof,   ss >> sourceSymbol; 
                                                if (!ss.eof())
                                                    ss >> tokenbasetx );
                        objBurnTx.push_back(Pair("source", sourceSymbol));
                        if( !tokenbasetx.IsNull() )
                            objBurnTx.push_back(Pair("tokenid", tokenbasetx.GetHash().GetHex()));
                    }
                }
            }
            objTx.push_back(Pair("export", objBurnTx));
            imports.push_back(objTx);
        }
    }
    result.push_back(Pair("imports", imports));
    result.push_back(Pair("TotalImported", TotalImported > 0 ? ValueFromAmount(TotalImported) : 0 ));    
    result.push_back(Pair("time", block.GetBlockTime()));
    return result;
}


// outputs burn transactions in the wallet 
UniValue getwalletburntransactions(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getwalletburntransactions \"count\"\n\n"
            "Lists most recent wallet burn transactions up to \'count\' parameter\n"
            "parameter \'count\' is optional. If omitted, defaults to 10 burn transactions"
            "\n\n"
            "\nResult:\n"
            "[\n"
            "    {\n"
            "       \"txid\": (string)\n"
            "       \"burnedAmount\" : (numeric)\n"
            "       \"targetSymbol\" : (string)\n"
            "       \"targetCCid\" : (numeric)\n"
            "    }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getwalletburntransactions", "100")
            + HelpExampleRpc("getwalletburntransactions", "100")
            + HelpExampleCli("getwalletburntransactions", "")
            + HelpExampleRpc("getwalletburntransactions", "")
        );

    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strAccount = "*";
    isminefilter filter = ISMINE_SPENDABLE;
    int nCount = 10;

    if (params.size() == 1)
        nCount = atoi(params[0].get_str());
    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");

    UniValue ret(UniValue::VARR);

    std::list<CAccountingEntry> acentries;
    CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, strAccount);

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
    {
        CWalletTx *const pwtx = (*it).second.first;
        if (pwtx != 0)
        {
            LOGSTREAM("importcoin", CCLOG_DEBUG2, stream << "pwtx iterpos=" << (int32_t)pwtx->nOrderPos << " txid=" << pwtx->GetHash().GetHex() << std::endl);
            vscript_t vopret;
            std::string targetSymbol;
            uint32_t targetCCid; uint256 payoutsHash;
            std::vector<uint8_t> rawproof;

            if (pwtx->vout.size() > 0 && GetOpReturnData(pwtx->vout.back().scriptPubKey, vopret) && !vopret.empty() &&
                UnmarshalBurnTx(*pwtx, targetSymbol, &targetCCid, payoutsHash, rawproof)) {
                UniValue entry(UniValue::VOBJ);
                entry.push_back(Pair("txid", pwtx->GetHash().GetHex()));

                if (vopret.begin()[0] == EVAL_TOKENS) {
                    // get burned token value
                    std::vector<std::pair<uint8_t, vscript_t>>  oprets;
                    uint256 tokenid;
                    uint8_t evalCodeInOpret;
                    std::vector<CPubKey> voutTokenPubkeys;

                    //skip token opret:
                    if (DecodeTokenOpRet(pwtx->vout.back().scriptPubKey, evalCodeInOpret, tokenid, voutTokenPubkeys, oprets) != 0) {
                        CTransaction tokenbasetx;
                        uint256 hashBlock;

                        if (myGetTransaction(tokenid, tokenbasetx, hashBlock)) {
                            std::vector<uint8_t> vorigpubkey;
                            std::string name, description;
                            std::vector<std::pair<uint8_t, vscript_t>>  oprets;

                            if (tokenbasetx.vout.size() > 0 &&
                                DecodeTokenCreateOpRet(tokenbasetx.vout.back().scriptPubKey, vorigpubkey, name, description, oprets) == 'c')
                            {
                                uint8_t destEvalCode = EVAL_TOKENS; // init set to fungible token:
                                vscript_t vopretNonfungible;
                                GetOpretBlob(oprets, OPRETID_NONFUNGIBLEDATA, vopretNonfungible);
                                if (!vopretNonfungible.empty())
                                    destEvalCode = vopretNonfungible.begin()[0];

                                int64_t burnAmount = 0;
                                for (auto v : pwtx->vout)
                                    if (v.scriptPubKey.IsPayToCryptoCondition() &&
                                        CTxOut(v.nValue, v.scriptPubKey) == MakeTokensCC1vout(destEvalCode ? destEvalCode : EVAL_TOKENS, v.nValue, pubkey2pk(ParseHex(CC_BURNPUBKEY))))  // burned to dead pubkey
                                        burnAmount += v.nValue;

                                entry.push_back(Pair("burnedAmount", ValueFromAmount(burnAmount)));
                                entry.push_back(Pair("tokenid", tokenid.GetHex()));
                            }
                        }
                    }
                }
                else 
                    entry.push_back(Pair("burnedAmount", ValueFromAmount(pwtx->vout.back().nValue)));   // coins

                // check for corrupted strings (look for non-printable chars) from some older versions 
                // which caused "couldn't parse reply from server" error on client:
                if (std::find_if(targetSymbol.begin(), targetSymbol.end(), [](int c) {return !std::isprint(c);}) != targetSymbol.end()) 
                    targetSymbol = "<value corrupted>";
                
                entry.push_back(Pair("targetSymbol", targetSymbol));
                entry.push_back(Pair("targetCCid", std::to_string(targetCCid)));
                if (mytxid_inmempool(pwtx->GetHash()))
                    entry.push_back(Pair("inMempool", "yes"));
                ret.push_back(entry);
            }
        } //else fprintf(stderr,"null pwtx\n
        if ((int)ret.size() >= (nCount))
            break;
    }
    // ret is newest to oldest

    if (nCount > (int)ret.size())
        nCount = ret.size();

    vector<UniValue> arrTmp = ret.getValues();

    vector<UniValue>::iterator first = arrTmp.begin();
    vector<UniValue>::iterator last = arrTmp.begin();
    std::advance(last, nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}
