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

#include "cc/eval.h"
#include "cc/utils.h"
#include "importcoin.h"
#include "crosschain.h"
#include "primitives/transaction.h"
#include "cc/CCinclude.h"
#include <openssl/sha.h>

#include "key_io.h"
#define CODA_BURN_ADDRESS "KPrrRoPfHOnNpZZQ6laHXdQDkSQDkVHaN0V+LizLlHxz7NaA59sBAAAA"

extern std::string ASSETCHAINS_SELFIMPORT;
extern uint16_t ASSETCHAINS_CODAPORT,ASSETCHAINS_BEAMPORT;
extern uint8_t ASSETCHAINS_OVERRIDE_PUBKEY33[33];

// utilities from gateways.cpp
uint256 BitcoinGetProofMerkleRoot(const std::vector<uint8_t> &proofData, std::vector<uint256> &txids);
uint256 GatewaysReverseScan(uint256 &txid, int32_t height, uint256 reforacletxid, uint256 batontxid);
int32_t GatewaysCointxidExists(struct CCcontract_info *cp, uint256 cointxid);
uint8_t DecodeGatewaysBindOpRet(char *depositaddr,const CScript &scriptPubKey,uint256 &tokenid,std::string &coin,int64_t &totalsupply,uint256 &oracletxid,uint8_t &M,uint8_t &N,std::vector<CPubKey> &gatewaypubkeys,uint8_t &taddr,uint8_t &prefix,uint8_t &prefix2,uint8_t &wiftype);
char *nonportable_path(char *str);
char *portable_path(char *str);
void *loadfile(char *fname,uint8_t **bufp,long *lenp,long *allocsizep);
void *filestr(long *allocsizep,char *_fname);

// ac_import=chain support:
// encode opret for gateways import
CScript EncodeImportTxOpRet(uint32_t targetCCid, std::string coin, std::vector<CPubKey> publishers, std::vector<uint256>txids, int32_t height, uint256 cointxid, int32_t claimvout, std::string rawburntx, std::vector<uint8_t>proof, CPubKey destpub, int64_t amount)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << targetCCid << coin << publishers << txids << height << cointxid << claimvout << rawburntx << proof << destpub << amount);
    return(opret);
}

CScript EncodeGatewaysImportTxOpRet(uint32_t targetCCid, std::string coin, uint256 bindtxid, std::vector<CPubKey> publishers, std::vector<uint256>txids, int32_t height, uint256 cointxid, int32_t claimvout, std::string rawburntx, std::vector<uint8_t>proof, CPubKey destpub, int64_t amount)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << targetCCid << coin << bindtxid << publishers << txids << height << cointxid << claimvout << rawburntx << proof << destpub << amount);
    return(opret);
}

CScript EncodeCodaImportTxOpRet(uint32_t targetCCid, std::string coin, std::string burntx, uint256 bindtxid, CPubKey destpub, int64_t amount)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << targetCCid <<  burntx << bindtxid << destpub << amount);
    return(opret);
}

cJSON* CodaRPC(char **retstr,char const *arg0,char const *arg1,char const *arg2,char const *arg3,char const *arg4,char const *arg5)
{
    char cmdstr[5000],fname[256],*jsonstr;
    long fsize;
    cJSON *retjson=NULL;

    sprintf(fname,"/tmp/coda.%s",arg0);
    sprintf(cmdstr,"coda.exe client %s %s %s %s %s %s > %s 2>&1",arg0,arg1,arg2,arg3,arg4,arg5,fname);
    *retstr = 0;
    if (system(cmdstr)<0) return (retjson);  
    if ( (jsonstr=(char *)filestr(&fsize,fname)) != 0 )
    {
        jsonstr[strlen(jsonstr)-1]='\0';
        if ( (strncmp(jsonstr,"Merkle List of transactions:",28)!=0) || (retjson= cJSON_Parse(jsonstr+29)) == 0)
            *retstr=jsonstr;
        else free(jsonstr);
    }
    return(retjson);    
}

bool ImportCoinGatewaysVerify(CTransaction oracletx, int32_t claimvout, std::string refcoin, uint256 burntxid, const std::string rawburntx, std::vector<uint8_t>proof, uint256 merkleroot)
{
    std::vector<uint256> txids; 
    uint256 proofroot;
    std::string name, description, format; 
    int32_t i, numvouts; 

    if (DecodeOraclesCreateOpRet(oracletx.vout[numvouts - 1].scriptPubKey, name, description, format) != 'C' || name != refcoin)
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "ImportCoinGatewaysVerify mismatched oracle name=" << name.c_str() << " != " << refcoin.c_str() << std::endl);
        return false;
    }
    
    LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "verified proof for burntxid=" << burntxid.GetHex() << " in trusted merkleroot" << std::endl);
    return true;
}

// make import tx with burntx and its proof of existence
// std::string MakeGatewaysImportTx(uint64_t txfee, uint256 oracletxid, int32_t height, std::string refcoin, std::vector<uint8_t> proof, std::string rawburntx, int32_t ivout, uint256 burntxid,std::string destaddr)
// {
//     CMutableTransaction burntx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
//     CTransaction oracletx,regtx; CPubKey regpk; 
//     uint256 proofroot,txid,tmporacletxid,merkleroot,mhash,hashBlock; int32_t i,m,n=0,numvouts; 
//     std::string name,desc,format; std::vector<CTxOut> vouts;
//     std::vector<CPubKey> pubkeys; std::vector<uint256>txids; 
//     char markeraddr[64]; int64_t datafee;
//     std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

//     if (!E_UNMARSHAL(ParseHex(rawburntx), ss >> burntx))
//         return std::string("");
//     CAmount amount = GetCoinImportValue(burntx);  // equal to int64_t
//     LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "MakeGatewaysImportTx height=" << height << " coin=" << refcoin << " amount=" << (double)amount / COIN  << " pubkeys num=" << pubkeys.size() << std::endl);
//     if (GetTransaction(oracletxid, oracletx, hashBlock, false) == 0 || (numvouts = oracletx.vout.size()) <= 0)
//     {
//         LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx cant find oracletxid=" << oracletxid.GetHex() << std::endl);
//         return("");
//     }
//     if (DecodeOraclesCreateOpRet(oracletx.vout[numvouts - 1].scriptPubKey,name,desc,format) != 'C')
//     {
//         LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx invalid oracle tx. oracletxid=" << oracletxid.GetHex() << std::endl);
//         return("");
//     }
//     if (name!=refcoin || format!="Ihh")
//     {
//         LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx invalid oracle name or format tx. oracletxid=" << oracletxid.GetHex() << " name=" << name << " format=" << format << std::endl);
//         return("");
//     }
//     CCtxidaddr(markeraddr,oracletxid);
//     SetCCunspents(unspentOutputs,markeraddr,true);
//     for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
//     {
//         txid = it->first.txhash;
//         if ( GetTransaction(txid,regtx,hashBlock,false) != 0 && regtx.vout.size() > 0
//             && DecodeOraclesOpRet(regtx.vout[regtx.vout.size()-1].scriptPubKey,tmporacletxid,regpk,datafee) == 'R' && oracletxid == tmporacletxid )
//         {
//             pubkeys.push_back(regpk);
//             n++;
//         }
//     }
//     merkleroot = zeroid;
//     for (i = m = 0; i < n; i++)
//     {
//         LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "MakeGatewaysImportTx using pubkeys[" << i << "]=" << HexStr(pubkeys[i]) << std::endl);
//         if ((mhash = CCOraclesReverseScan("importcoind-1",txid, height, oracletxid, OraclesBatontxid(oracletxid, pubkeys[i]))) != zeroid)
//         {
//             if (merkleroot == zeroid)
//                 merkleroot = mhash, m = 1;
//             else if (mhash == merkleroot)
//                 m ++;
//             txids.push_back(txid);
//         }
//     }    
//     LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "MakeGatewaysImportTx burntxid=" << burntxid.GetHex() << " nodes m=" << m << " of n=" << n << std::endl);
//     if (merkleroot == zeroid || m < n / 2) // none or less than half oracle nodes sent merkleroot
//     {
//         LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx couldnt find merkleroot for block height=" << height << "coin=" << refcoin.c_str() << " oracleid=" << oracletxid.GetHex() << " m=" << m << " vs n=" << n << std::endl );
//         return("");
//     }
//     proofroot = BitcoinGetProofMerkleRoot(proof, txids);
//     if (proofroot != merkleroot)
//     {
//         LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx mismatched proof merkleroot=" << proofroot.GetHex() << " and oracles merkleroot=" << merkleroot.GetHex() << std::endl);
//         return("");
//     }
//     // check the burntxid is in the proof:
//     if (std::find(txids.begin(), txids.end(), burntxid) == txids.end()) {
//         LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx invalid proof for this burntxid=" << burntxid.GetHex() << std::endl);
//         return("");
//     }
//     burntx.vout.push_back(MakeBurnOutput(amount,0xffffffff,refcoin,vouts,proof,oracletxid,height));
//     std::vector<uint256> leaftxids;
//     BitcoinGetProofMerkleRoot(proof, leaftxids);
//     MerkleBranch newBranch(0, leaftxids);
//     TxProof txProof = std::make_pair(burntxid, newBranch);
//     CTxDestination dest = DecodeDestination(destaddr.c_str());
//     CScript scriptPubKey = GetScriptForDestination(dest);
//     vouts.push_back(CTxOut(amount,scriptPubKey));
//     return  HexStr(E_MARSHAL(ss << MakeImportCoinTransaction(txProof, burntx, vouts)));
// }

// makes source tx for self import tx
std::string MakeSelfImportSourceTx(CTxDestination &dest, int64_t amount, CMutableTransaction &mtx) 
{
    const int64_t txfee = 10000;
    int64_t inputs, change;
    CPubKey myPubKey = Mypubkey();
    struct CCcontract_info *cpDummy, C;

    cpDummy = CCinit(&C, EVAL_TOKENS);

    mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

    if( (inputs = AddNormalinputs(mtx, myPubKey, txfee, 4)) == 0 ) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeSelfImportSourceTx: cannot find normal imputs for txfee" << std::endl);
        return std::string("");
    }

    CScript scriptPubKey = GetScriptForDestination(dest);
    mtx.vout.push_back(CTxOut(txfee, scriptPubKey));
    change = inputs - txfee;
    if( change != 0 )
        mtx.vout.push_back(CTxOut(change, CScript() << ParseHex(HexStr(myPubKey)) << OP_CHECKSIG));

    //make opret with amount:
    return FinalizeCCTx(0, cpDummy, mtx, myPubKey, txfee, CScript() << OP_RETURN << E_MARSHAL(ss << (uint8_t)EVAL_IMPORTCOIN << (uint8_t)'A' << amount));
}

// make sure vin0 is signed by ASSETCHAINS_OVERRIDE_PUBKEY33
int32_t CheckVin0PubKey(const CTransaction &sourcetx)
{
    CTransaction vintx;
    uint256 blockHash;
    char destaddr[64], pkaddr[64];

    if( !myGetTransaction(sourcetx.vin[0].prevout.hash, vintx, blockHash) ) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckVin0PubKey() could not load vintx" << sourcetx.vin[0].prevout.hash.GetHex() << std::endl);
        return(-1);
    }
    if( sourcetx.vin[0].prevout.n < vintx.vout.size() && Getscriptaddress(destaddr, vintx.vout[sourcetx.vin[0].prevout.n].scriptPubKey) != 0 )
    {
        pubkey2addr(pkaddr, ASSETCHAINS_OVERRIDE_PUBKEY33);
        if (strcmp(pkaddr, destaddr) == 0) {
            return(0);
        }
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckVin0PubKey() mismatched vin0[prevout.n=" << sourcetx.vin[0].prevout.n << "] -> destaddr=" << destaddr << " vs pkaddr=" << pkaddr << std::endl);
    }
    return -1;
}

// ac_import=PUBKEY support:
// prepare a tx for creating import tx and quasi-burn tx
int32_t GetSelfimportProof(std::string source, CMutableTransaction &mtx, CScript &scriptPubKey, TxProof &proof, std::string rawsourcetx, int32_t &ivout, uint256 sourcetxid, uint64_t burnAmount) // find burnTx with hash from "other" daemon
{
    MerkleBranch newBranch; 
    CMutableTransaction tmpmtx; 
    CTransaction sourcetx; 

    tmpmtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

    if (!E_UNMARSHAL(ParseHex(rawsourcetx), ss >> sourcetx)) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "GetSelfimportProof: could not unmarshal source tx" << std::endl);
        return(-1);
    }

    if (sourcetx.vout.size() == 0) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "GetSelfimportProof: vout size is 0" << std::endl);
        return -1;
    }

	if (ivout < 0) {  // "ivout < 0" means "find"  
		// try to find vout
		CPubKey myPubkey = Mypubkey();
		ivout = 0;
		// skip change:
		if (sourcetx.vout[ivout].scriptPubKey == (CScript() << ParseHex(HexStr(myPubkey)) << OP_CHECKSIG))
			ivout++;
	}

    if (ivout >= sourcetx.vout.size()) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "GetSelfimportProof: needed vout not found" << std::endl);
        return -1;
    }

	LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "GetSelfimportProof: using vout[" << ivout << "] of the passed rawtx" << std::endl);

    scriptPubKey = sourcetx.vout[ivout].scriptPubKey;

	//mtx is template for import tx
    mtx = sourcetx;
    mtx.fOverwintered = tmpmtx.fOverwintered;
    
    //malleability fix for burn tx:
    //mtx.nExpiryHeight = tmpmtx.nExpiryHeight;
    mtx.nExpiryHeight = sourcetx.nExpiryHeight;

    mtx.nVersionGroupId = tmpmtx.nVersionGroupId;
    mtx.nVersion = tmpmtx.nVersion;
    mtx.vout.clear();
    mtx.vout.resize(1);
    mtx.vout[0].nValue = burnAmount;
    mtx.vout[0].scriptPubKey = scriptPubKey;

    // not sure we need this now as we create sourcetx ourselves:
    if (sourcetx.GetHash() != sourcetxid) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "GetSelfimportProof: passed source txid incorrect" << std::endl);
        return(-1);
    }

    // check ac_pubkey:
    if (CheckVin0PubKey(sourcetx) < 0) {
        return -1;
    }
    proof = std::make_pair(sourcetxid, newBranch);
    return 0;
}

// make import tx with burntx and dual daemon
std::string MakeCodaImportTx(uint64_t txfee, std::string receipt, std::string srcaddr, std::vector<CTxOut> vouts)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()),burntx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk; uint256 codaburntxid; std::vector<unsigned char> dummyproof;
    int32_t i,numvouts,n,m; std::string coin,error; struct CCcontract_info *cp, C;
    cJSON *result,*tmp,*tmp1; unsigned char hash[SHA256_DIGEST_LENGTH+1];
    char out[SHA256_DIGEST_LENGTH*2+1],*retstr,*destaddr,*receiver; TxProof txProof; uint64_t amount;

    cp = CCinit(&C, EVAL_GATEWAYS);
    if (txfee == 0)
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, receipt.c_str(), receipt.size());
    SHA256_Final(hash, &sha256);
    for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(out + (i * 2), "%02x", hash[i]);
    }
    out[65]='\0';
    LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "MakeCodaImportTx: hash=" << out << std::endl);
    codaburntxid.SetHex(out);
    LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "MakeCodaImportTx: receipt=" << receipt << " codaburntxid=" << codaburntxid.GetHex().data() << " amount=" << (double)amount / COIN  << std::endl);
    result=CodaRPC(&retstr,"prove-payment","-address",srcaddr.c_str(),"-receipt-chain-hash",receipt.c_str(),"");
    if (result==0)
    {
        if (retstr!=0)
        {            
            CCerror=std::string("CodaRPC: ")+retstr;
            free(retstr);
        }
        return("");
    }
    else
    {
        if ((tmp=jobj(jitem(jarray(&n,result,(char *)"payments"),0),(char *)"payload"))!=0 && (destaddr=jstr(jobj(tmp,(char *)"common"),(char *)"memo"))!=0 &&
            (receiver=jstr(jitem(jarray(&m,tmp,(char *)"body"),1),(char *)"receiver"))!=0 && (amount=j64bits(jitem(jarray(&m,tmp,(char *)"body"),1),(char *)"amount"))!=0) 
        {
            LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "MakeCodaImportTx: receiver=" << receiver << " destaddr=" << destaddr << " amount=" << amount << std::endl);
            if (strcmp(receiver,CODA_BURN_ADDRESS)!=0)
            {
                CCerror="MakeCodaImportTx: invalid burn address, coins do not go to predefined burn address - ";
                CCerror+=CODA_BURN_ADDRESS;
                LOGSTREAM("importcoin", CCLOG_INFO, stream << CCerror << std::endl);
                free(result);
                return("");
            }
            CTxDestination dest = DecodeDestination(destaddr);
            CScript scriptPubKey = GetScriptForDestination(dest);
            if (vouts[0]!=CTxOut(amount*COIN,scriptPubKey))
            {
                CCerror="MakeCodaImportTx: invalid destination address, burnTx memo!=importTx destination";
                LOGSTREAM("importcoin", CCLOG_INFO, stream << CCerror << std::endl);
                free(result);
                return("");
            }
            if (amount*COIN!=vouts[0].nValue)
            {
                CCerror="MakeCodaImportTx: invalid amount, burnTx amount!=importTx amount";
                LOGSTREAM("importcoin", CCLOG_INFO, stream << CCerror << std::endl);
                free(result);
                return("");
            }
            burntx.vin.push_back(CTxIn(codaburntxid,0,CScript()));
            burntx.vout.push_back(MakeBurnOutput(amount*COIN,0xffffffff,"CODA",vouts,dummyproof,srcaddr,receipt));
            return HexStr(E_MARSHAL(ss << MakeImportCoinTransaction(txProof,burntx,vouts)));
        }
        else
        {
            CCerror="MakeCodaImportTx: invalid Coda burn tx";
            LOGSTREAM("importcoin", CCLOG_INFO, stream << CCerror << std::endl);
            free(result);
            return("");
        }
        
    }
    CCerror="MakeCodaImportTx: error fetching Coda tx";
    LOGSTREAM("importcoin", CCLOG_INFO, stream << CCerror << std::endl);
    free(result);
    return("");
}

// use proof from the above functions to validate the import

int32_t CheckBEAMimport(TxProof proof,std::vector<uint8_t> rawproof,CTransaction burnTx,std::vector<CTxOut> payouts)
{
    // check with dual-BEAM daemon via ASSETCHAINS_BEAMPORT for validity of burnTx
    return(-1);
}

int32_t CheckCODAimport(CTransaction importTx,CTransaction burnTx,std::vector<CTxOut> payouts,std::string srcaddr,std::string receipt)
{
    cJSON *result,*tmp,*tmp1; char *retstr,out[SHA256_DIGEST_LENGTH*2+1]; unsigned char hash[SHA256_DIGEST_LENGTH+1]; int i,n,m;
    SHA256_CTX sha256; uint256 codaburntxid; char *destaddr,*receiver; uint64_t amount;

    // check with dual-CODA daemon via ASSETCHAINS_CODAPORT for validity of burnTx
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, receipt.c_str(), receipt.size());
    SHA256_Final(hash, &sha256);
    for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(out + (i * 2), "%02x", hash[i]);
    }
    out[65]='\0';
    codaburntxid.SetHex(out);
    result=CodaRPC(&retstr,"prove-payment","-address",srcaddr.c_str(),"-receipt-chain-hash",receipt.c_str(),"");
    if (result==0)
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "CodaRPC error: " << retstr << std::endl);
        free(retstr);
        return (-1);
    }
    else
    {
        if ((tmp=jobj(jitem(jarray(&n,result,(char *)"payments"),0),(char *)"payload"))==0 || (destaddr=jstr(jobj(tmp,(char *)"common"),(char *)"memo"))==0 ||
            (receiver=jstr(jitem(jarray(&m,tmp,(char *)"body"),1),(char *)"receiver"))==0 || (amount=j64bits(jitem(jarray(&m,tmp,(char *)"body"),1),(char *)"amount"))==0) 
        {
            LOGSTREAM("importcoin", CCLOG_INFO, stream << "Invalid Coda burn tx" << jprint(result,1) << std::endl);
            free(result);
            return (-1);
        }
        CTxDestination dest = DecodeDestination(destaddr);
        CScript scriptPubKey = GetScriptForDestination(dest);
        if (payouts[0]!=CTxOut(amount*COIN,scriptPubKey));
        {
            LOGSTREAM("importcoin", CCLOG_INFO, stream << "Destination address in burn tx does not match destination in import tx" << std::endl);
            free(result);
            return (-1);
        }
        if (strcmp(receiver,CODA_BURN_ADDRESS)!=0)
        {
            LOGSTREAM("importcoin", CCLOG_INFO, stream << "Invalid burn address " << jstr(tmp1,(char *)"receiver") << std::endl);
            free(result);
            return (-1);
        }
        if (amount*COIN!=payouts[0].nValue)
        {
            LOGSTREAM("importcoin", CCLOG_INFO, stream << "Burn amount and import amount not matching, " << j64bits(tmp,(char *)"amount") << " - " << payouts[0].nValue/COIN << std::endl);
            free(result);
            return (-1);
        } 
        if (burnTx.vin[0].prevout.hash!=codaburntxid || importTx.vin[0].prevout.hash!=burnTx.GetHash())
        {
            LOGSTREAM("importcoin", CCLOG_INFO, stream << "Invalid import/burn tx vin" << std::endl);
            free(result);
            return (-1);
        }
        free(result);
    }    
    return(0);
}

int32_t CheckGATEWAYimport(CTransaction importTx,CTransaction burnTx,std::string refcoin,std::vector<uint8_t> proof,
            uint256 bindtxid,std::vector<CPubKey> publishers,std::vector<uint256> txids,int32_t height,int32_t burnvout,std::string rawburntx,CPubKey destpub)
{
    // CTransaction oracletx,regtx; CPubKey regpk; 
    // uint256 proofroot,txid,tmporacletxid,merkleroot,mhash,hashBlock; int32_t i,m,n=0,numvouts; 
    // std::string name,desc,format; std::vector<CTxOut> vouts;
    // std::vector<CPubKey> pubkeys; std::vector<uint256>txids; 
    // char markeraddr[64]; int64_t datafee;
    // std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    // ASSETCHAINS_SELFIMPORT is coin
    // check for valid burn from external coin blockchain and if valid return(0);
    // if (GetTransaction(oracletxid, oracletx, hashBlock, false) == 0 || (numvouts = oracletx.vout.size()) <= 0)
    // {
    //     LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckGATEWAYimport cant find oracletxid=" << oracletxid.GetHex() << std::endl);
    //     return(-1);
    // }
    // if (DecodeOraclesCreateOpRet(oracletx.vout[numvouts - 1].scriptPubKey,name,desc,format) != 'C')
    // {
    //     LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckGATEWAYimport invalid oracle tx. oracletxid=" << oracletxid.GetHex() << std::endl);
    //     return(-1);
    // }
    // if (name!=refcoin || format!="Ihh")
    // {
    //     LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckGATEWAYimport invalid oracle name or format tx. oracletxid=" << oracletxid.GetHex() << " name=" << name << " format=" << format << std::endl);
    //     return(-1);
    // }
    // CCtxidaddr(markeraddr,oracletxid);
    // SetCCunspents(unspentOutputs,markeraddr,true);
    // for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    // {
    //     txid = it->first.txhash;
    //     if ( GetTransaction(txid,regtx,hashBlock,false) != 0 && regtx.vout.size() > 0
    //         && DecodeOraclesOpRet(regtx.vout[regtx.vout.size()-1].scriptPubKey,tmporacletxid,regpk,datafee) == 'R' && oracletxid == tmporacletxid )
    //     {
    //         pubkeys.push_back(regpk);
    //         n++;
    //     }
    // }
    // merkleroot = zeroid;
    // for (i = m = 0; i < n; i++)
    // {
    //     if ((mhash = CCOraclesReverseScan("importcoind-1",txid, height, oracletxid, OraclesBatontxid(oracletxid, pubkeys[i]))) != zeroid)
    //     {
    //         if (merkleroot == zeroid)
    //             merkleroot = mhash, m = 1;
    //         else if (mhash == merkleroot)
    //             m ++;
    //         txids.push_back(txid);
    //     }
    // }    
    // if (merkleroot == zeroid || m < n / 2) // none or less than half oracle nodes sent merkleroot
    // {
    //     LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckGATEWAYimport couldnt find merkleroot for block height=" << height << "coin=" << refcoin.c_str() << " oracleid=" << oracletxid.GetHex() << " m=" << m << " vs n=" << n << std::endl );
    //     return(-1);
    // }
    // proofroot = BitcoinGetProofMerkleRoot(proof, txids);
    // if (proofroot != merkleroot)
    // {
    //     LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckGATEWAYimport mismatched proof merkleroot=" << proofroot.GetHex() << " and oracles merkleroot=" << merkleroot.GetHex() << std::endl);
    //     return(-1);
    // }
    // // check the burntxid is in the proof:
    // if (std::find(txids.begin(), txids.end(), burnTx.GetHash()) == txids.end()) {
    //     LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckGATEWAYimport invalid proof for this burntxid=" << burnTx.GetHash().GetHex() << std::endl);
    //     return(-1);
    // }
    return(0);
}

int32_t CheckPUBKEYimport(TxProof proof,std::vector<uint8_t> rawproof,CTransaction burnTx,std::vector<CTxOut> payouts)
{
    // if burnTx has ASSETCHAINS_PUBKEY vin, it is valid return(0);
    LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "proof txid=" << proof.first.GetHex() << std::endl);

    uint256 sourcetxid = proof.first, hashBlock;
    CTransaction sourcetx;

    if (!myGetTransaction(sourcetxid, sourcetx, hashBlock)) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "could not load source txid=" << sourcetxid.GetHex() << std::endl);
        return -1;
    }

    if (sourcetx.vout.size() == 0) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "no vouts in source txid=" << sourcetxid.GetHex() << std::endl);
        return -1;
    }

    // might be malleable:
    if (burnTx.nExpiryHeight != sourcetx.nExpiryHeight) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "burntx nExpiryHeight incorrect for source txid=" << sourcetxid.GetHex() << std::endl);
        return -1;
    }

    //ac_pubkey check:
    if (CheckVin0PubKey(sourcetx) < 0) {
        return -1;
    }

    // get source tx opret:
    std::vector<uint8_t> vopret;
    uint8_t evalCode, funcId;
    int64_t amount;

    GetOpReturnData(sourcetx.vout.back().scriptPubKey, vopret);
    if (vopret.size() == 0 || !E_UNMARSHAL(vopret, ss >> evalCode; ss >> funcId; ss >> amount) || evalCode != EVAL_IMPORTCOIN || funcId != 'A') {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "no or incorrect opret to validate in source txid=" << sourcetxid.GetHex() << std::endl);
        return -1;
    }

    LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "importTx amount=" << payouts[0].nValue << " burnTx amount=" << burnTx.vout[0].nValue << " opret amount=" << amount << " source txid=" << sourcetxid.GetHex() << std::endl);

    // amount malleability check with the opret from the source tx: 
    if (payouts[0].nValue != amount) { // assume that burntx amount is checked in the common code in Eval::ImportCoin()
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "importTx amount != amount in the opret of source txid=" << sourcetxid.GetHex() << std::endl);
        return -1;
    }

    return(0);
}

/*
 * CC Eval method for import coin.
 *
 * This method should control every parameter of the ImportCoin transaction, since it has no signature
 * to protect it from malleability.
 
 ##### 0xffffffff is a special CCid for single chain/dual daemon imports
 */
bool Eval::ImportCoin(const std::vector<uint8_t> params,const CTransaction &importTx,unsigned int nIn)
{
    TxProof proof; CTransaction burnTx; std::vector<CTxOut> payouts; uint64_t txfee = 10000; int32_t height,burnvout; std::vector<CPubKey> publishers;
    uint32_t targetCcid; std::string targetSymbol,srcaddr,destaddr,receipt,rawburntx; uint256 payoutsHash,bindtxid; std::vector<uint8_t> rawproof;
    std::vector<uint256> txids; CPubKey destpub;

    if ( importTx.vout.size() < 2 )
        return Invalid("too-few-vouts");
    // params
    if (!UnmarshalImportTx(importTx, proof, burnTx, payouts))
        return Invalid("invalid-params");
    // Control all aspects of this transaction
    // It should not be at all malleable
    if (MakeImportCoinTransaction(proof, burnTx, payouts, importTx.nExpiryHeight).GetHash() != importTx.GetHash())  // ExistsImportTombstone prevents from duplication
        return Invalid("non-canonical");
    // burn params
    if (!UnmarshalBurnTx(burnTx, targetSymbol, &targetCcid, payoutsHash, rawproof))
        return Invalid("invalid-burn-tx");
    // check burn amount
    {
        uint64_t burnAmount = burnTx.vout.back().nValue;
        if (burnAmount == 0)
            return Invalid("invalid-burn-amount");
        uint64_t totalOut = 0;
        for (int i=0; i<importTx.vout.size(); i++)
            totalOut += importTx.vout[i].nValue;
        if (totalOut > burnAmount || totalOut < burnAmount-txfee )
            return Invalid("payout-too-high-or-too-low");
    }
    // Check burntx shows correct outputs hash
    if (payoutsHash != SerializeHash(payouts))
        return Invalid("wrong-payouts");
    if (targetCcid < KOMODO_FIRSTFUNGIBLEID)
        return Invalid("chain-not-fungible");
    // Check proof confirms existance of burnTx
    if ( targetCcid != 0xffffffff )
    {
        if ( targetCcid != GetAssetchainsCC() || targetSymbol != GetAssetchainsSymbol() )
            return Invalid("importcoin-wrong-chain");
        uint256 target = proof.second.Exec(burnTx.GetHash());
        if (!CheckMoMoM(proof.first, target))
            return Invalid("momom-check-fail");
    }
    else
    {
        if ( targetSymbol == "BEAM" )
        {
            if ( ASSETCHAINS_BEAMPORT == 0 )
                return Invalid("BEAM-import-without-port");
            else if ( CheckBEAMimport(proof,rawproof,burnTx,payouts) < 0 )
                return Invalid("BEAM-import-failure");
        }
        else if ( targetSymbol == "CODA" )
        {
            if ( ASSETCHAINS_CODAPORT == 0 )
                return Invalid("CODA-import-without-port");
            else if ( UnmarshalBurnTx(burnTx,srcaddr,receipt)==0 || CheckCODAimport(importTx,burnTx,payouts,srcaddr,receipt) < 0 )
                return Invalid("CODA-import-failure");
        }
        else if ( targetSymbol == "PUBKEY" )
        {
            if ( ASSETCHAINS_SELFIMPORT != "PUBKEY" )
                return Invalid("PUBKEY-import-when-notPUBKEY");
            else if ( CheckPUBKEYimport(proof,rawproof,burnTx,payouts) < 0 )
                return Invalid("PUBKEY-import-failure");
        }
        else
        {
            if ( targetSymbol != ASSETCHAINS_SELFIMPORT )
                return Invalid("invalid-gateway-import-coin");
             else if ( UnmarshalBurnTx(burnTx,bindtxid,publishers,txids,height,burnvout,rawburntx,destpub)==0 || CheckGATEWAYimport(importTx,burnTx,targetSymbol,rawproof,bindtxid,publishers,txids,height,burnvout,rawburntx,destpub) < 0 )
                return Invalid("GATEWAY-import-failure");
        }
    }
    return Valid();
}
