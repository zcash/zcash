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
#include "main.h"
#include "primitives/transaction.h"
#include "rpc/server.h"
#include "sync.h"
#include "util.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "script/standard.h"

#include "key_io.h"

#include <stdint.h>
#include <univalue.h>
#include <regex>

using namespace std;

extern std::string ASSETCHAINS_SELFIMPORT;
extern uint16_t ASSETCHAINS_CODAPORT, ASSETCHAINS_BEAMPORT;

int32_t komodo_MoM(int32_t *notarized_htp,uint256 *MoMp,uint256 *kmdtxidp,int32_t nHeight,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip);
int32_t komodo_MoMoMdata(char *hexstr,int32_t hexsize,struct komodo_ccdataMoMoM *mdata,char *symbol,int32_t kmdheight,int32_t notarized_height);
struct komodo_ccdata_entry *komodo_allMoMs(int32_t *nump,uint256 *MoMoMp,int32_t kmdstarti,int32_t kmdendi);
uint256 komodo_calcMoM(int32_t height,int32_t MoMdepth);
extern std::string ASSETCHAINS_SELFIMPORT;
uint256 Parseuint256(char *hexstr);

std::string MakeSelfImportSourceTx(CTxDestination &dest, int64_t amount, CMutableTransaction &mtx);
int32_t GetSelfimportProof(std::string source, CMutableTransaction &mtx, CScript &scriptPubKey, TxProof &proof, std::string rawsourcetx, int32_t &ivout, uint256 sourcetxid, uint64_t burnAmount);
std::string MakeGatewaysImportTx(uint64_t txfee, uint256 bindtxid, int32_t height, std::string refcoin, std::vector<uint8_t>proof, std::string rawburntx, int32_t ivout, uint256 burntxid);

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
    std::vector<uint8_t> rawproof; uint8_t *ptr; int32_t i; uint32_t ccid = ASSETCHAINS_CC;
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "migrate_converttoexport rawTx dest_symbol export_amount\n"
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

    CAmount burnAmount = AmountFromValue(params[2]);
    if (burnAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for export");
    {
        CAmount needed = 0;
        for (int i=0; i<tx.vout.size(); i++) needed += tx.vout[i].nValue;
        if (burnAmount < needed)
            throw runtime_error("export_amount too small");
    }
    //if ( ASSETCHAINS_SELFIMPORT.size() > 0 )
    //    throw runtime_error("self-import chains cant be fungible");
    rawproof.resize(strlen(ASSETCHAINS_SYMBOL));
    ptr = rawproof.data();
    for (i=0; i<rawproof.size(); i++)
        ptr[i] = ASSETCHAINS_SYMBOL[i];
    CTxOut burnOut = MakeBurnOutput(burnAmount, ccid, targetSymbol, tx.vout,rawproof);
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
        std::string hextx = MakeGatewaysImportTx(0, bindtxid, height, source, rawproof, rawsourcetx, ivout, sourcetxid);

        result.push_back(Pair("hex", hextx));
        result.push_back(Pair("UsedRawtxVout", ivout));   // notify user about the used vout of rawtx
    }
    return result;
}

UniValue getNotarisationsForBlock(const UniValue& params, bool fHelp)
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
}


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
