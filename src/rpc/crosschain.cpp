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

#include <stdint.h>
#include <univalue.h>
#include <regex>

using namespace std;

#define RETURN_IF_ERROR(CCerror) if ( CCerror != "" ) { ERR_RESULT(CCerror); return(result); }
#define ERR_RESULT(x) result.push_back(Pair("result", "error")) , result.push_back(Pair("error", x));

extern std::string CCerror;
extern std::string ASSETCHAINS_SELFIMPORT;
extern uint16_t ASSETCHAINS_CODAPORT, ASSETCHAINS_BEAMPORT;
int32_t ensure_CCrequirements(uint8_t evalcode);

int32_t komodo_MoM(int32_t *notarized_htp,uint256 *MoMp,uint256 *kmdtxidp,int32_t nHeight,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip);
int32_t komodo_MoMoMdata(char *hexstr,int32_t hexsize,struct komodo_ccdataMoMoM *mdata,char *symbol,int32_t kmdheight,int32_t notarized_height);
struct komodo_ccdata_entry *komodo_allMoMs(int32_t *nump,uint256 *MoMoMp,int32_t kmdstarti,int32_t kmdendi);
uint256 komodo_calcMoM(int32_t height,int32_t MoMdepth);
int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);
extern std::string ASSETCHAINS_SELFIMPORT;

std::string MakeSelfImportSourceTx(CTxDestination &dest, int64_t amount, CMutableTransaction &mtx);
int32_t GetSelfimportProof(std::string source, CMutableTransaction &mtx, CScript &scriptPubKey, TxProof &proof, std::string rawsourcetx, int32_t &ivout, uint256 sourcetxid, uint64_t burnAmount);
std::string MakeCodaImportTx(uint64_t txfee, std::string receipt, std::string srcaddr, std::vector<CTxOut> vouts);

UniValue assetchainproof(const UniValue& params, bool fHelp)
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


UniValue crosschainproof(const UniValue& params, bool fHelp)
{
    UniValue ret(UniValue::VOBJ);
    //fprintf(stderr,"crosschainproof needs to be implemented\n");
    return(ret);
}


UniValue height_MoM(const UniValue& params, bool fHelp)
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

UniValue MoMoMdata(const UniValue& params, bool fHelp)
{
    if ( fHelp || params.size() != 3 )
        throw runtime_error("MoMoMdata symbol kmdheight ccid\n");
    UniValue ret(UniValue::VOBJ);
    char* symbol = (char *)params[0].get_str().c_str();
    int kmdheight = atoi(params[1].get_str().c_str());
    uint32_t ccid = atoi(params[2].get_str().c_str());
    ret.push_back(Pair("coin",symbol));
    ret.push_back(Pair("kmdheight",kmdheight));
    ret.push_back(Pair("ccid", (int) ccid));

    uint256 destNotarisationTxid;
    std::vector<uint256> moms;
    uint256 MoMoM = CalculateProofRoot(symbol, ccid, kmdheight, moms, destNotarisationTxid);

    UniValue valMoms(UniValue::VARR);
    for (int i=0; i<moms.size(); i++) valMoms.push_back(moms[i].GetHex());
    ret.push_back(Pair("MoMs", valMoms));
    ret.push_back(Pair("notarization_hash", destNotarisationTxid.GetHex()));
    ret.push_back(Pair("MoMoM", MoMoM.GetHex()));
    auto vmomomdata = E_MARSHAL(ss << MoMoM; ss << ((uint32_t)0));
    ret.push_back(Pair("data", HexStr(vmomomdata)));
    return ret;
}


UniValue calc_MoM(const UniValue& params, bool fHelp)
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


UniValue migrate_converttoexport(const UniValue& params, bool fHelp)
{
    std::vector<uint8_t> rawproof; uint8_t *ptr; uint8_t i; uint32_t ccid = ASSETCHAINS_CC; uint64_t txfee = 10000;
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "migrate_converttoexport rawTx dest_symbol\n"
            "\nConvert a raw transaction to a cross-chain export.\n"
            "If neccesary, the transaction should be funded using fundrawtransaction.\n"
            "Finally, the transaction should be signed using signrawtransaction\n"
            "The finished export transaction, plus the payouts, should be passed to "
            "the \"migrate_createimporttransaction\" method on a KMD node to get the corresponding "
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

    CAmount burnAmount = 0;
    
    for (int i=0; i<tx.vout.size(); i++) burnAmount += tx.vout[i].nValue;
    if (burnAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Cannot export a negative or zero value.");
    if (burnAmount > 1000000LL*COIN)
        throw JSONRPCError(RPC_TYPE_ERROR, "Cannot export more than 1 million coins per export.");

    rawproof.resize(strlen(ASSETCHAINS_SYMBOL));
    ptr = rawproof.data();
    for (i=0; i<rawproof.size(); i++)
        ptr[i] = ASSETCHAINS_SYMBOL[i];
    CTxOut burnOut = MakeBurnOutput(burnAmount+txfee, ccid, targetSymbol, tx.vout,rawproof);
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("payouts", HexStr(E_MARSHAL(ss << tx.vout))));
    tx.vout.clear();
    tx.vout.push_back(burnOut);
    ret.push_back(Pair("exportTx", HexStr(E_MARSHAL(ss << tx))));
    return ret;
}


/*
 * The process to migrate funds
 *
 * Create a transaction on assetchain:
 *
 * generaterawtransaction
 * migrate_converttoexport
 * fundrawtransaction
 * signrawtransaction
 *
 * migrate_createimportransaction
 * migrate_completeimporttransaction
 */

UniValue migrate_createimporttransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("migrate_createimporttransaction burnTx payouts\n\n"
                "Create an importTx given a burnTx and the corresponding payouts, hex encoded");

    if (ASSETCHAINS_CC < KOMODO_FIRSTFUNGIBLEID)
        throw runtime_error("-ac_cc < KOMODO_FIRSTFUNGIBLEID");

    if (ASSETCHAINS_SYMBOL[0] == 0)
        throw runtime_error("Must be called on assetchain");

    vector<uint8_t> txData(ParseHexV(params[0], "argument 1"));

    CTransaction burnTx;
    if (!E_UNMARSHAL(txData, ss >> burnTx))
        throw runtime_error("Couldn't parse burnTx");


    vector<CTxOut> payouts;
    if (!E_UNMARSHAL(ParseHexV(params[1], "argument 2"), ss >> payouts))
        throw runtime_error("Couldn't parse payouts");

    uint256 txid = burnTx.GetHash();
    TxProof proof = GetAssetchainProof(burnTx.GetHash(),burnTx);

    CTransaction importTx = MakeImportCoinTransaction(proof, burnTx, payouts);

    return HexStr(E_MARSHAL(ss << importTx));
}


UniValue migrate_completeimporttransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("migrate_completeimporttransaction importTx\n\n"
                "Takes a cross chain import tx with proof generated on assetchain "
                "and extends proof to target chain proof root");

    if (ASSETCHAINS_SYMBOL[0] != 0)
        throw runtime_error("Must be called on KMD");

    CTransaction importTx;
    if (!E_UNMARSHAL(ParseHexV(params[0], "argument 1"), ss >> importTx))
        throw runtime_error("Couldn't parse importTx");

    CompleteImportTransaction(importTx);

    return HexStr(E_MARSHAL(ss << importTx));
}

UniValue selfimport(const UniValue& params, bool fHelp)
{
    UniValue result(UniValue::VOBJ);
    CMutableTransaction sourceMtx, templateMtx;
    std::string destaddr;
    std::string source; 
    std::string rawsourcetx;
    CTransaction burnTx; 
    CTxOut burnOut; 
    uint64_t burnAmount; 
    uint256 sourcetxid, blockHash; 
	std::vector<CTxOut> vouts; 
	std::vector<uint8_t> rawproof, rawproofEmpty;
    int32_t ivout = 0;
    CScript scriptPubKey;
    TxProof proof;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("selfimport only works on -ac_import chains");

    if (fHelp || params.size() != 2)
        throw runtime_error("selfimport destaddr amount\n"
                  //old:    "selfimport rawsourcetx sourcetxid {nvout|\"find\"} amount \n"
                  //TODO:   "or selfimport rawburntx burntxid {nvout|\"find\"} rawproof source bindtxid height} \n"
                            "\ncreates self import coin transaction");

/* OLD selfimport schema:
    rawsourcetx = params[0].get_str();
    sourcetxid = Parseuint256((char *)params[1].get_str().c_str()); // allow for txid != hash(rawtx)

	int32_t ivout = -1;
	if( params[2].get_str() != "find" ) {
		if( !std::all_of(params[2].get_str().begin(), params[2].get_str().end(), ::isdigit) )  // check if not all chars are digit
			throw std::runtime_error("incorrect nvout param");

		ivout = atoi(params[2].get_str().c_str());
	}

    burnAmount = atof(params[3].get_str().c_str()) * COIN + 0.00000000499999;  */

    destaddr = params[0].get_str();
    burnAmount = atof(params[1].get_str().c_str()) * COIN + 0.00000000499999;

    source = ASSETCHAINS_SELFIMPORT;   //defaults to -ac_import=... param
    /* TODO for gateways:
    if ( params.size() >= 5 )
    {
        rawproof = ParseHex(params[4].get_str().c_str());
        if ( params.size() == 6 )
            source = params[5].get_str();
    }  */

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
    else if (source == "PUBKEY")
    {

        CTxDestination dest = DecodeDestination(destaddr.c_str());
        rawsourcetx = MakeSelfImportSourceTx(dest, burnAmount, sourceMtx);
        sourcetxid = sourceMtx.GetHash();

        // prepare self-import 'quasi-burn' tx and also create vout for import tx (in mtx.vout):
        if (GetSelfimportProof(source, templateMtx, scriptPubKey, proof, rawsourcetx, ivout, sourcetxid, burnAmount) < 0)
            throw std::runtime_error("Failed validating selfimport");

        vouts = templateMtx.vout;
        burnOut = MakeBurnOutput(burnAmount, 0xffffffff, ASSETCHAINS_SELFIMPORT, vouts, rawproofEmpty);
        templateMtx.vout.clear();
        templateMtx.vout.push_back(burnOut);	// burn tx has only opret with vouts and optional proof

        burnTx = templateMtx;					// complete the creation of 'quasi-burn' tx

        std::string hextx = HexStr(E_MARSHAL(ss << MakeImportCoinTransaction(proof, burnTx, vouts)));

        CTxDestination address;
        bool fValidAddress = ExtractDestination(scriptPubKey, address);
       
        result.push_back(Pair("sourceTxHex", rawsourcetx));
        result.push_back(Pair("importTxHex", hextx));
        result.push_back(Pair("UsedRawtxVout", ivout));   // notify user about the used vout of rawtx
        result.push_back(Pair("DestinationAddress", EncodeDestination(address)));  // notify user about the address where the funds will be sent

        return result;
    }
    else if (source == ASSETCHAINS_SELFIMPORT)
    {
        throw std::runtime_error("not implemented yet\n");

        if (params.size() != 8) 
            throw runtime_error("use \'selfimport rawburntx burntxid nvout rawproof source bindtxid height\' to import from a coin chain\n");
       
        uint256 bindtxid = Parseuint256((char *)params[6].get_str().c_str()); 
        int32_t height = atoi((char *)params[7].get_str().c_str());


        // source is external coin is the assetchains symbol in the burnTx OP_RETURN
        // burnAmount, rawtx and rawproof should be enough for gatewaysdeposit equivalent
        //std::string hextx = MakeGatewaysImportTx(0, bindtxid, height, source, rawproof, rawsourcetx, ivout, "");

        // result.push_back(Pair("hex", hextx));
        // result.push_back(Pair("UsedRawtxVout", ivout));   // notify user about the used vout of rawtx
    }
    return result;
}

bool GetNotarisationNotaries(uint8_t notarypubkeys[64][33], int8_t &numNN, const std::vector<CTxIn> &vin, std::vector<int8_t> &NotarisationNotaries);


UniValue importdual(const UniValue& params, bool fHelp)
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
    RETURN_IF_ERROR(CCerror);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt importdual");
    return result;
}

UniValue importgatewayinfo(const UniValue& params, bool fHelp)
{
    uint256 txid;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importgatewaybind only works on -ac_import chains");
    if ( fHelp || params.size() != 1 )
        throw runtime_error("importgatewayinfo bindtxid\n");
    txid = Parseuint256(params[0].get_str().c_str());
    return(ImportGatewayInfo(txid));
}

UniValue importgatewaybind(const UniValue& params, bool fHelp)
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
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
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

UniValue importgatewaydeposit(const UniValue& params, bool fHelp)
{
    UniValue result(UniValue::VOBJ);
    CMutableTransaction mtx; std::vector<uint8_t> rawproof;
    std::string hex,coin,rawburntx; int32_t height,burnvout;
    CPubKey destpub; std::vector<CTxOut> vouts; uint256 bindtxid,burntxid;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importgatewaydeposit only works on -ac_import chains");
    if ( fHelp || params.size() != 8) 
        throw runtime_error("use \'importgatewaydeposit bindtxid height coin burntxid nvout rawburntx rawproof destpub\' to import deposited coins\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
    CCerror = "";
    bindtxid = Parseuint256(params[0].get_str().c_str()); 
    height = atoi(params[1].get_str().c_str());
    coin = params[2].get_str();
    burntxid = Parseuint256(params[3].get_str().c_str()); 
    burnvout = atoi(params[4].get_str().c_str());
    rawburntx = params[5].get_str();
    rawproof = ParseHex(params[6].get_str());
    destpub = ParseHex(params[7].get_str());
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
    hex = ImportGatewayDeposit(0, bindtxid, height, coin, burntxid, burnvout, rawburntx, rawproof, destpub);
    RETURN_IF_ERROR(CCerror);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } else ERR_RESULT("couldnt importgatewaydeposit");
    return result;
}

UniValue importgatewaywithdraw(const UniValue& params, bool fHelp)
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
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
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

UniValue importgatewaypartialsign(const UniValue& params, bool fHelp)
{
    UniValue result(UniValue::VOBJ); std::string coin,parthex,hex; uint256 txid;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importgatewayspartialsign only works on -ac_import chains");
    if ( fHelp || params.size() != 3 )
        throw runtime_error("importgatewayspartialsign txidaddr refcoin hex\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
    txid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    parthex = params[2].get_str();
    hex = ImportGatewayPartialSign(0,txid,coin,parthex);
    if ( hex.size() > 0 )
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex",hex));
    } else ERR_RESULT("couldnt importgatewayspartialsign");
    return(result);
}

UniValue importgatewaycompletesigning(const UniValue& params, bool fHelp)
{
    UniValue result(UniValue::VOBJ); uint256 withdrawtxid; std::string txhex,hex,coin;

    if ( ASSETCHAINS_SELFIMPORT.size() == 0 )
        throw runtime_error("importgatewaycompletesigning only works on -ac_import chains");
    if ( fHelp || params.size() != 3 )
        throw runtime_error("importgatewaycompletesigning withdrawtxid coin hex\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
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

UniValue importgatewaymarkdone(const UniValue& params, bool fHelp)
{
    UniValue result(UniValue::VOBJ); uint256 completetxid; std::string hex,coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("importgatewaymarkdone completesigningtx coin\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
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

UniValue importgatewaypendingdeposits(const UniValue& params, bool fHelp)
{
    uint256 bindtxid; std::string coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("importgatewaypendingdeposits bindtxid coin\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    return(ImportGatewayPendingDeposits(bindtxid,coin));
}

UniValue importgatewaypendingwithdraws(const UniValue& params, bool fHelp)
{
    uint256 bindtxid; std::string coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("importgatewaypendingwithdraws bindtxid coin\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    return(ImportGatewayPendingWithdraws(bindtxid,coin));
}

UniValue importgatewayprocessed(const UniValue& params, bool fHelp)
{
    uint256 bindtxid; std::string coin;
    if ( fHelp || params.size() != 2 )
        throw runtime_error("importgatewayprocessed bindtxid coin\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    coin = params[1].get_str();
    return(ImportGatewayProcessedWithdraws(bindtxid,coin));
}

UniValue importgatewayexternaladdress(const UniValue& params, bool fHelp)
{
    uint256 bindtxid; CPubKey pubkey;

    if ( fHelp || params.size() != 2)
        throw runtime_error("importgatewayexternaladdress bindtxid pubkey\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
    bindtxid = Parseuint256((char *)params[0].get_str().c_str());
    pubkey = ParseHex(params[1].get_str().c_str());
    return(ImportGatewayExternalAddress(bindtxid,pubkey));
}

UniValue importgatewaydumpprivkey(const UniValue& params, bool fHelp)
{
    uint256 bindtxid;

    if ( fHelp || params.size() != 2)
        throw runtime_error("importgatewaydumpprivkey bindtxid address\n");
    if ( ensure_CCrequirements(EVAL_IMPORTGATEWAY) < 0 )
        throw runtime_error("to use CC contracts, you need to launch daemon with valid -pubkey= for an address in your wallet\n");
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

UniValue getNotarisationsForBlock(const UniValue& params, bool fHelp)
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
    // Gets KMD notaries on KMD... but LABS notaries on labs chains needs to be fixed so LABS are identified on KMD.
    int8_t numNN = 0; uint8_t notarypubkeys[64][33] = {0};
    numNN = komodo_notaries(notarypubkeys, height, chainActive[height]->nTime);
    
    BOOST_FOREACH(const Notarisation& n, nibs)
    {
        UniValue item(UniValue::VOBJ); UniValue notaryarr(UniValue::VARR); std::vector<int8_t> NotarisationNotaries;
        if ( is_STAKED(n.second.symbol) != 0 )
            continue; // for now just skip this... need to fetch diff pubkeys for these chains. labs.push_back(item);
        uint256 hash; CTransaction tx;
        if ( GetTransaction(n.first,tx,hash,false) )
        {
            if ( !GetNotarisationNotaries(notarypubkeys, numNN, tx.vin, NotarisationNotaries) )
                continue;
            if ( NotarisationNotaries.size() < numNN/5 )
                continue;
        }
        item.push_back(make_pair("txid", n.first.GetHex()));
        item.push_back(make_pair("chain", n.second.symbol));
        item.push_back(make_pair("height", (int)n.second.height));
        item.push_back(make_pair("blockhash", n.second.blockHash.GetHex()));
        item.push_back(make_pair("KMD_height", height)); // for when timstamp input is used.
        
        for ( auto notary : NotarisationNotaries )
            notaryarr.push_back(notary);
        item.push_back(make_pair("notaries",notaryarr));
        kmd.push_back(item);
    }
    out.push_back(make_pair("KMD", kmd));
    //out.push_back(make_pair("LABS", labs));
    return out;
}

/*UniValue getNotarisationsForBlock(const UniValue& params, bool fHelp)
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


UniValue scanNotarisationsDB(const UniValue& params, bool fHelp)
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

UniValue getimports(const UniValue& params, bool fHelp)
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
            TxProof proof; CTransaction burnTx; std::vector<CTxOut> payouts; CTxDestination importaddress;
            TotalImported += tx.vout[0].nValue;
            objTx.push_back(Pair("amount", ValueFromAmount(tx.vout[0].nValue)));
            if (ExtractDestination(tx.vout[0].scriptPubKey, importaddress))
            {
                objTx.push_back(Pair("address", CBitcoinAddress(importaddress).ToString()));
            }
            UniValue objBurnTx(UniValue::VOBJ);            
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
                        std::string sourceSymbol(rawproof.begin(), rawproof.end());
                        objBurnTx.push_back(Pair("source", sourceSymbol));
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
