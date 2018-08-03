#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "crosschain.h"
#include "importcoin.h"
#include "base58.h"
#include "consensus/validation.h"
#include "cc/eval.h"
#include "cc/utils.h"
#include "main.h"
#include "primitives/transaction.h"
#include "rpcserver.h"
#include "sync.h"
#include "util.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "script/standard.h"

#include <stdint.h>
#include <univalue.h>
#include <regex>


using namespace std;

int32_t komodo_MoM(int32_t *notarized_htp,uint256 *MoMp,uint256 *kmdtxidp,int32_t nHeight,uint256 *MoMoMp,int32_t *MoMoMoffsetp,int32_t *MoMoMdepthp,int32_t *kmdstartip,int32_t *kmdendip);
int32_t komodo_MoMoMdata(char *hexstr,int32_t hexsize,struct komodo_ccdataMoMoM *mdata,char *symbol,int32_t kmdheight,int32_t notarized_height);
struct komodo_ccdata_entry *komodo_allMoMs(int32_t *nump,uint256 *MoMoMp,int32_t kmdstarti,int32_t kmdendi);
uint256 komodo_calcMoM(int32_t height,int32_t MoMdepth);


UniValue assetchainproof(const UniValue& params, bool fHelp)
{
    uint256 hash;

    // parse params and get notarisation data for tx
    if ( fHelp || params.size() != 1)
        throw runtime_error("assetchainproof needs a txid");

    hash = uint256S(params[0].get_str());

    auto proof = GetAssetchainProof(hash);
    auto proofData = E_MARSHAL(ss << proof);
    return HexStr(proofData);
}


UniValue crosschainproof(const UniValue& params, bool fHelp)
{
    
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
        height = chainActive.Tip()->nHeight;
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

    CTxOut burnOut = MakeBurnOutput(burnAmount, ASSETCHAINS_CC, targetSymbol, tx.vout);
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
    TxProof proof = GetAssetchainProof(burnTx.GetHash());

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
