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
#include "cc/CCtokens.h"

#include "key_io.h"
#define CODA_BURN_ADDRESS "KPrrRoPfHOnNpZZQ6laHXdQDkSQDkVHaN0V+LizLlHxz7NaA59sBAAAA"
/*
 * CC Eval method for import coin.
 *
 * This method should control every parameter of the ImportCoin transaction, since it has no signature
 * to protect it from malleability.
 
 ##### 0xffffffff is a special CCid for single chain/dual daemon imports
 */

extern std::string ASSETCHAINS_SELFIMPORT;
extern uint16_t ASSETCHAINS_CODAPORT,ASSETCHAINS_BEAMPORT;
extern uint8_t ASSETCHAINS_OVERRIDE_PUBKEY33[33];
extern uint256 KOMODO_EARLYTXID;

// utilities from gateways.cpp
uint256 BitcoinGetProofMerkleRoot(const std::vector<uint8_t> &proofData, std::vector<uint256> &txids);
uint256 GatewaysReverseScan(uint256 &txid, int32_t height, uint256 reforacletxid, uint256 batontxid);
int32_t GatewaysCointxidExists(struct CCcontract_info *cp, uint256 cointxid);
uint8_t DecodeImportGatewayBindOpRet(char *burnaddr,const CScript &scriptPubKey,std::string &coin,uint256 &oracletxid,uint8_t &M,uint8_t &N,std::vector<CPubKey> &importgatewaypubkeys,uint8_t &taddr,uint8_t &prefix,uint8_t &prefix2,uint8_t &wiftype);
int64_t ImportGatewayVerify(char *refburnaddr,uint256 oracletxid,int32_t claimvout,std::string refcoin,uint256 burntxid,const std::string deposithex,std::vector<uint8_t>proof,uint256 merkleroot,CPubKey destpub,uint8_t taddr,uint8_t prefix,uint8_t prefix2);
char *nonportable_path(char *str);
char *portable_path(char *str);
void *loadfile(char *fname,uint8_t **bufp,long *lenp,long *allocsizep);
void *filestr(long *allocsizep,char *_fname);

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

// makes source tx for self import tx
CMutableTransaction MakeSelfImportSourceTx(CTxDestination &dest, int64_t amount)
{
    const int64_t txfee = 10000;
    int64_t inputs, change;
    CPubKey myPubKey = Mypubkey();
    struct CCcontract_info *cpDummy, C;

    cpDummy = CCinit(&C, EVAL_TOKENS);  // this is just for FinalizeCCTx to work

    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

    if (AddNormalinputs(mtx, myPubKey, 2 * txfee, 4) == 0) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeSelfImportSourceTx() warning: cannot find normal inputs for txfee" << std::endl);
    }
    
    CScript scriptPubKey = GetScriptForDestination(dest);
    mtx.vout.push_back(CTxOut(txfee, scriptPubKey));

    //make opret with 'burned' amount:
    FinalizeCCTx(0, cpDummy, mtx, myPubKey, txfee, CScript() << OP_RETURN << E_MARSHAL(ss << (uint8_t)EVAL_IMPORTCOIN << (uint8_t)'A' << amount));
    return mtx;
}

// make sure vin is signed by pubkey33
bool CheckVinPubKey(const CTransaction &sourcetx, int32_t i, uint8_t pubkey33[33])
{
    CTransaction vintx;
    uint256 blockHash;
    char destaddr[64], pkaddr[64];

    if (i < 0 || i >= sourcetx.vin.size())
        return false;

    if( !myGetTransaction(sourcetx.vin[i].prevout.hash, vintx, blockHash) ) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckVinPubKey() could not load vintx" << sourcetx.vin[i].prevout.hash.GetHex() << std::endl);
        return false;
    }
    if( sourcetx.vin[i].prevout.n < vintx.vout.size() && Getscriptaddress(destaddr, vintx.vout[sourcetx.vin[i].prevout.n].scriptPubKey) != 0 )
    {
        pubkey2addr(pkaddr, pubkey33);
        if (strcmp(pkaddr, destaddr) == 0) {
            return true;
        }
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckVinPubKey() mismatched vin[" << i << "].prevout.n=" << sourcetx.vin[i].prevout.n << " -> destaddr=" << destaddr << " vs pkaddr=" << pkaddr << std::endl);
    }
    return false;
}

// ac_import=PUBKEY support:
// prepare a tx for creating import tx and quasi-burn tx
int32_t GetSelfimportProof(const CMutableTransaction sourceMtx, CMutableTransaction &templateMtx, ImportProof &proofNull) // find burnTx with hash from "other" daemon
{
    MerkleBranch newBranch; 
    CMutableTransaction tmpmtx; 
    //CTransaction sourcetx; 

    tmpmtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

    /*
    if (!E_UNMARSHAL(ParseHex(rawsourcetx), ss >> sourcetx)) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "GetSelfimportProof: could not unmarshal source tx" << std::endl);
        return(-1);
    }

    if (sourcetx.vout.size() == 0) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "GetSelfimportProof: vout size is 0" << std::endl);
        return -1;
    } */

	/*if (ivout < 0) {  // "ivout < 0" means "find"  
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
    } */

    int32_t ivout = 0;

	// LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "GetSelfimportProof: using vout[" << ivout << "] of the passed rawtx" << std::endl);

    CScript scriptPubKey = sourceMtx.vout[ivout].scriptPubKey;

	//mtx is template for import tx
    templateMtx = sourceMtx;
    templateMtx.fOverwintered = tmpmtx.fOverwintered;
    
    //malleability fix for burn tx:
    //mtx.nExpiryHeight = tmpmtx.nExpiryHeight;
    templateMtx.nExpiryHeight = sourceMtx.nExpiryHeight;

    templateMtx.nVersionGroupId = tmpmtx.nVersionGroupId;
    templateMtx.nVersion = tmpmtx.nVersion;
    templateMtx.vout.clear();
    templateMtx.vout.resize(1);

    uint8_t evalCode, funcId;
    int64_t burnAmount;
    vscript_t vopret;
    if( !GetOpReturnData(sourceMtx.vout.back().scriptPubKey, vopret) ||
        !E_UNMARSHAL(vopret, ss >> evalCode; ss >> funcId; ss >> burnAmount)) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "GetSelfimportProof() could not unmarshal source tx opret" << std::endl);
        return -1;
    }
    templateMtx.vout[0].nValue = burnAmount;
    templateMtx.vout[0].scriptPubKey = scriptPubKey;

    // not sure we need this now as we create sourcetx ourselves:
    /*if (sourcetx.GetHash() != sourcetxid) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "GetSelfimportProof: passed source txid incorrect" << std::endl);
        return(-1);
    }*/

    // check ac_pubkey:
    if (!CheckVinPubKey(sourceMtx, 0, ASSETCHAINS_OVERRIDE_PUBKEY33)) {
        return -1;
    }
    proofNull = ImportProof(std::make_pair(sourceMtx.GetHash(), newBranch));
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
            uint256 bindtxid,std::vector<CPubKey> publishers,std::vector<uint256> txids,uint256 burntxid,int32_t height,int32_t burnvout,std::string rawburntx,CPubKey destpub, int64_t amount)
{
    CTransaction oracletx,bindtx,regtx; int32_t i,m,n=0,numvouts; uint8_t M,N,taddr,prefix,prefix2,wiftype;
    uint256 txid,oracletxid,tmporacletxid,merkleroot,mhash,hashBlock; 
    std::string name,desc,format,coin; std::vector<CTxOut> vouts; CPubKey regpk;
    std::vector<CPubKey> pubkeys,tmppubkeys,tmppublishers; char markeraddr[64],deposit[64],destaddr[64],tmpdest[64]; int64_t datafee;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    // ASSETCHAINS_SELFIMPORT is coin
    if (KOMODO_EARLYTXID!=zeroid && bindtxid!=KOMODO_EARLYTXID)
    { 
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport invalid import gateway. On this chain only valid import gateway is " << KOMODO_EARLYTXID.GetHex() << std::endl);
        return(-1);
    }
    // check for valid burn from external coin blockchain and if valid return(0);
    if (myGetTransaction(bindtxid, bindtx, hashBlock) == 0 || (numvouts = bindtx.vout.size()) <= 0)
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport cant find bindtxid=" << bindtxid.GetHex() << std::endl);
        return(-1);
    }
    else if (DecodeImportGatewayBindOpRet(deposit,bindtx.vout[numvouts - 1].scriptPubKey,coin,oracletxid,M,N,tmppubkeys,taddr,prefix,prefix2,wiftype) != 'B')
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "CheckGATEWAYimport invalid bind tx. bindtxid=" << bindtxid.GetHex() << std::endl);
        return(-1);
    }
    else if (refcoin!=coin)
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport invalid coin " << refcoin << "!=" << coin << std::endl);
        return(-1);
    }
    else if ( N == 0 || N > 15 || M > N )
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport invalid N or M " << std::endl);
        return(-1);
    }
    else if (tmppubkeys.size()!=N)
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport not enough pubkeys for given N " << std::endl);
        return(-1);
    }
    else if (komodo_txnotarizedconfirmed(bindtxid) == false)
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport bindtx not yet confirmed/notarized" << std::endl);
        return(-1);
    }
    else if (myGetTransaction(oracletxid, oracletx, hashBlock) == 0 || (numvouts = oracletx.vout.size()) <= 0)
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport cant find oracletxid=" << oracletxid.GetHex() << std::endl);
        return(-1);
    }
    else if (DecodeOraclesCreateOpRet(oracletx.vout[numvouts - 1].scriptPubKey,name,desc,format) != 'C')
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport invalid oracle tx. oracletxid=" << oracletxid.GetHex() << std::endl);
        return(-1);
    }
    else if (name!=refcoin || format!="Ihh")
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport invalid oracle name or format tx. oracletxid=" << oracletxid.GetHex() << " name=" << name << " format=" << format << std::endl);
        return(-1);
    }
    CCtxidaddr(markeraddr,oracletxid);
    SetCCunspents(unspentOutputs,markeraddr,false);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        if ( myGetTransaction(txid,regtx,hashBlock) != 0 && regtx.vout.size() > 0
            && DecodeOraclesOpRet(regtx.vout[regtx.vout.size()-1].scriptPubKey,tmporacletxid,regpk,datafee) == 'R' && oracletxid == tmporacletxid )
        {
            pubkeys.push_back(regpk);
            n++;
        }
    }
    if (pubkeys.size()!=tmppubkeys.size())
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport different number of bind and oracle pubkeys " << tmppubkeys.size() << "!=" << pubkeys.size() << std::endl);
        return(-1);
    }
    merkleroot = zeroid;
    for (i = m = 0; i < n; i++)
    {
        if ((mhash = CCOraclesReverseScan("importgateway-1",txid, height, oracletxid, OraclesBatontxid(oracletxid, pubkeys[i]))) != zeroid)
        {
            if (merkleroot == zeroid)
                merkleroot = mhash, m = 1;
            else if (mhash == merkleroot)
                m ++;
            tmppublishers.push_back(pubkeys[i]);
        }
    }
    if (publishers.size()!=tmppublishers.size())
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport different number of publishers for burtx in oracle" << std::endl);
        return(-1);
    }    
    else if (merkleroot == zeroid || m < n / 2) // none or less than half oracle nodes sent merkleroot
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport couldnt find merkleroot for block height=" << height << "coin=" << refcoin.c_str() << " oracleid=" << oracletxid.GetHex() << " m=" << m << " vs n=" << n << std::endl );
        return(-1);
    }
    else if ( ImportGatewayVerify(deposit,oracletxid,burnvout,refcoin,burntxid,rawburntx,proof,merkleroot,destpub,taddr,prefix,prefix2) != amount )
    {
        CCerror = strprintf("burntxid didnt validate !");
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return(-1);
    }
    else if (importTx.vout[0].nValue!=amount)
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport import amount different than in burntx" << std::endl);
        return(-1);
    }
    Getscriptaddress(destaddr,importTx.vout[0].scriptPubKey);
    Getscriptaddress(tmpdest,CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG);
    if (strcmp(destaddr,tmpdest)!=0)
    {
        LOGSTREAM("importgateway", CCLOG_INFO, stream << "CheckGATEWAYimport import coins destination different than in burntx" << std::endl);
        return(-1);
    }  
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
    if (!CheckVinPubKey(sourcetx, 0, ASSETCHAINS_OVERRIDE_PUBKEY33)) {
        return -1;
    }

    // get source tx opret:
    std::vector<uint8_t> vopret;
    uint8_t evalCode, funcId;
    int64_t amount;

    if (!GetOpReturnData(sourcetx.vout.back().scriptPubKey, vopret) || 
        vopret.size() == 0 || 
        !E_UNMARSHAL(vopret, ss >> evalCode; ss >> funcId; ss >> amount) || 
        evalCode != EVAL_IMPORTCOIN || funcId != 'A') {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "none or incorrect opret to validate in source txid=" << sourcetxid.GetHex() << std::endl);
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

bool CheckMigration(Eval *eval, const CTransaction &importTx, const CTransaction &burnTx, std::vector<CTxOut> & payouts, const ImportProof &proof, const std::vector<uint8_t> &rawproof)
{
    vscript_t vimportOpret;
    if (!GetOpReturnData(importTx.vout.back().scriptPubKey, vimportOpret)  ||        
        vimportOpret.empty())
        return eval->Invalid("invalid-import-tx-no-opret");


    uint256 tokenid = zeroid;
    if (vimportOpret.begin()[0] == EVAL_TOKENS) { // for tokens (new opret with tokens)
        if ( is_STAKED(ASSETCHAINS_SYMBOL) == 1 )
            return eval->Invalid("no-tokens-migrate-on-LABS");
        struct CCcontract_info *cpTokens, CCtokens_info;
        std::vector<std::pair<uint8_t, vscript_t>>  oprets;
        uint8_t evalCodeInOpret;
        std::vector<CPubKey> voutTokenPubkeys;
        vscript_t vnonfungibleOpret;

        cpTokens = CCinit(&CCtokens_info, EVAL_TOKENS);

        if (DecodeTokenOpRet(importTx.vout.back().scriptPubKey, evalCodeInOpret, tokenid, voutTokenPubkeys, oprets) == 0)
            return eval->Invalid("cannot-decode-import-tx-token-opret");

        uint8_t nonfungibleEvalCode = EVAL_TOKENS; // init to no non-fungibles
        GetOpretBlob(oprets, OPRETID_NONFUNGIBLEDATA, vnonfungibleOpret);
        if (!vnonfungibleOpret.empty())
            nonfungibleEvalCode = vnonfungibleOpret.begin()[0];

        // check if burn tx at least has cc evaltoken vins (we cannot get cc input)
        bool hasTokenVin = false;
        for (auto vin : burnTx.vin)
            if (cpTokens->ismyvin(vin.scriptSig))
                hasTokenVin = true;
        if (!hasTokenVin)
            return eval->Invalid("burn-tx-has-no-token-vins");

        // calc outputs for burn tx
        CAmount ccBurnOutputs = 0;
        for (auto v : burnTx.vout)
            if (v.scriptPubKey.IsPayToCryptoCondition() &&
                CTxOut(v.nValue, v.scriptPubKey) == MakeTokensCC1vout(nonfungibleEvalCode, v.nValue, pubkey2pk(ParseHex(CC_BURNPUBKEY))))  // burned to dead pubkey
                ccBurnOutputs += v.nValue;

        // calc outputs for import tx
        CAmount ccImportOutputs = 0;
        for (auto v : importTx.vout)
            if (v.scriptPubKey.IsPayToCryptoCondition() &&
                !IsTokenMarkerVout(v))  // should not be marker here
                ccImportOutputs += v.nValue;

        if (ccBurnOutputs != ccImportOutputs)
            return eval->Invalid("token-cc-burned-output-not-equal-cc-imported-output");

    }
    else if (vimportOpret.begin()[0] != EVAL_IMPORTCOIN) {
        return eval->Invalid("import-tx-incorrect-opret-eval");
    }

    // for tokens check burn, import, tokenbase tx 
    if (!tokenid.IsNull()) {

        std::string sourceSymbol;
        CTransaction tokenbaseTx;
        if (!E_UNMARSHAL(rawproof, ss >> sourceSymbol; ss >> tokenbaseTx))
            return eval->Invalid("cannot-unmarshal-rawproof-for-tokens");

        uint256 sourceTokenId;
        std::vector<std::pair<uint8_t, vscript_t>>  oprets;
        uint8_t evalCodeInOpret;
        std::vector<CPubKey> voutTokenPubkeys;
        if (burnTx.vout.size() > 0 && DecodeTokenOpRet(burnTx.vout.back().scriptPubKey, evalCodeInOpret, sourceTokenId, voutTokenPubkeys, oprets) == 0)
            return eval->Invalid("cannot-decode-burn-tx-token-opret");

        if (sourceTokenId != tokenbaseTx.GetHash())              // check tokenid in burn tx opret maches the passed tokenbase tx (to prevent cheating by importing user)
            return eval->Invalid("incorrect-token-creation-tx-passed");

        std::vector<std::pair<uint8_t, vscript_t>>  opretsSrc;
        vscript_t vorigpubkeySrc;
        std::string nameSrc, descSrc;
        if (DecodeTokenCreateOpRet(tokenbaseTx.vout.back().scriptPubKey, vorigpubkeySrc, nameSrc, descSrc, opretsSrc) == 0)
            return eval->Invalid("cannot-decode-token-creation-tx");

        std::vector<std::pair<uint8_t, vscript_t>>  opretsImport;
        vscript_t vorigpubkeyImport;
        std::string nameImport, descImport;
        if (importTx.vout.size() == 0 || DecodeTokenCreateOpRet(importTx.vout.back().scriptPubKey, vorigpubkeySrc, nameSrc, descSrc, opretsImport) == 0)
            return eval->Invalid("cannot-decode-token-import-tx");

        // check that name,pubkey,description in import tx correspond ones in token creation tx in the source chain:
        if (vorigpubkeySrc != vorigpubkeyImport ||
            nameSrc != nameImport ||
            descSrc != descImport)
            return eval->Invalid("import-tx-token-params-incorrect");
    }


    // Check burntx shows correct outputs hash
//    if (payoutsHash != SerializeHash(payouts))  // done in ImportCoin
//        return eval->Invalid("wrong-payouts");


    TxProof merkleBranchProof;
    std::vector<uint256> notaryTxids;

    // Check proof confirms existance of burnTx
    if (proof.IsMerkleBranch(merkleBranchProof)) {
        uint256 target = merkleBranchProof.second.Exec(burnTx.GetHash());
        LOGSTREAM("importcoin", CCLOG_DEBUG2, stream << "Eval::ImportCoin() momom target=" << target.GetHex() << " merkleBranchProof.first=" << merkleBranchProof.first.GetHex() << std::endl);
        if (!CheckMoMoM(merkleBranchProof.first, target)) {
            LOGSTREAM("importcoin", CCLOG_INFO, stream << "MoMoM check failed for importtx=" << importTx.GetHash().GetHex() << std::endl);
            return eval->Invalid("momom-check-fail");
        }
    }
    else if (proof.IsNotaryTxids(notaryTxids)) {
        if (!CheckNotariesApproval(burnTx.GetHash(), notaryTxids)) {
            return eval->Invalid("notaries-approval-check-fail");
        }
    }
    else {
        return eval->Invalid("invalid-import-proof");
    }

/*  if (vimportOpret.begin()[0] == EVAL_TOKENS)
        return eval->Invalid("test-invalid-tokens-are-good!!");
    else
        return eval->Invalid("test-invalid-coins-are-good!!"); */
    return true;
}

bool Eval::ImportCoin(const std::vector<uint8_t> params, const CTransaction &importTx, unsigned int nIn)
{
    ImportProof proof; 
    CTransaction burnTx; 
    std::vector<CTxOut> payouts; 
    CAmount txfee = 10000, amount;
    int32_t height, burnvout; 
    std::vector<CPubKey> publishers;
    uint32_t targetCcid; 
    std::string targetSymbol, srcaddr, destaddr, receipt, rawburntx; 
    uint256 payoutsHash, bindtxid, burntxid;
    std::vector<uint8_t> rawproof;
    std::vector<uint256> txids; 
    CPubKey destpub;

    LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "Validating import tx..., txid=" << importTx.GetHash().GetHex() << std::endl);

    if (strcmp(ASSETCHAINS_SYMBOL, "CFEKDIMXY6") == 0 && chainActive.Height() <= 44693)
        return true;

    if (importTx.vout.size() < 2)
        return Invalid("too-few-vouts");
    // params
    if (!UnmarshalImportTx(importTx, proof, burnTx, payouts))
        return Invalid("invalid-params");
    // Control all aspects of this transaction
    // It should not be at all malleable
    if (ASSETCHAINS_SELFIMPORT!="PEGSCC" && MakeImportCoinTransaction(proof, burnTx, payouts, importTx.nExpiryHeight).GetHash() != importTx.GetHash())  // ExistsImportTombstone prevents from duplication
        return Invalid("non-canonical");
    // burn params
    if (!UnmarshalBurnTx(burnTx, targetSymbol, &targetCcid, payoutsHash, rawproof))
        return Invalid("invalid-burn-tx");
    
    if (burnTx.vout.size() == 0)
        return Invalid("invalid-burn-tx-no-vouts");

    // check burned normal amount >= import amount  && burned amount <= import amount + txfee (extra txfee is for miners and relaying, see GetImportCoinValue() func)
    CAmount burnAmount = burnTx.vout.back().nValue;
    if (burnAmount == 0)
        return Invalid("invalid-burn-amount");
    CAmount totalOut = 0;
    for (auto v : importTx.vout)
        if (!v.scriptPubKey.IsPayToCryptoCondition())
            totalOut += v.nValue;
    if (totalOut > burnAmount || totalOut < burnAmount - txfee)
        return Invalid("payout-too-high-or-too-low");

    // Check burntx shows correct outputs hash
    if (payoutsHash != SerializeHash(payouts))
        return Invalid("wrong-payouts");
    if (targetCcid < KOMODO_FIRSTFUNGIBLEID)
        return Invalid("chain-not-fungible");

    if ( targetCcid != 0xffffffff )
    {

        if (targetCcid != GetAssetchainsCC() || targetSymbol != GetAssetchainsSymbol())
            return Invalid("importcoin-wrong-chain");

        if (!CheckMigration(this, importTx, burnTx, payouts, proof, rawproof))
            return false;  // eval->Invalid() is called in the func
    }
    else
    {
        TxProof merkleBranchProof;
        if (!proof.IsMerkleBranch(merkleBranchProof)) 
            return Invalid("invalid-import-proof-for-0xFFFFFFFF");

        if ( targetSymbol == "BEAM" )
        {
            if ( ASSETCHAINS_BEAMPORT == 0 )
                return Invalid("BEAM-import-without-port");
            else if ( CheckBEAMimport(merkleBranchProof,rawproof,burnTx,payouts) < 0 )
                return Invalid("BEAM-import-failure");
        }
        else if ( targetSymbol == "CODA" )
        {
            if ( ASSETCHAINS_CODAPORT == 0 )
                return Invalid("CODA-import-without-port");
            else if ( UnmarshalBurnTx(burnTx,srcaddr,receipt)==0 || CheckCODAimport(importTx,burnTx,payouts,srcaddr,receipt) < 0 )
                return Invalid("CODA-import-failure");
        }
        else if ( targetSymbol == "PEGSCC" )
        {
            if ( ASSETCHAINS_SELFIMPORT != "PEGSCC" )
                return Invalid("PEGSCC-import-when-not PEGSCC");
            // else if ( CheckPUBKEYimport(merkleBranchProof,rawproof,burnTx,payouts) < 0 )
            //     return Invalid("PEGSCC-import-failure");
        }
        else if ( targetSymbol == "PUBKEY" )
        {
            if ( ASSETCHAINS_SELFIMPORT != "PUBKEY" )
                return Invalid("PUBKEY-import-when-not PUBKEY");
            else if ( CheckPUBKEYimport(merkleBranchProof,rawproof,burnTx,payouts) < 0 )
                return Invalid("PUBKEY-import-failure");
        }
        else
        {
            if ( targetSymbol != ASSETCHAINS_SELFIMPORT )
                return Invalid("invalid-gateway-import-coin");
            else if ( UnmarshalBurnTx(burnTx,bindtxid,publishers,txids,burntxid,height,burnvout,rawburntx,destpub,amount)==0 || CheckGATEWAYimport(importTx,burnTx,targetSymbol,rawproof,bindtxid,publishers,txids,burntxid,height,burnvout,rawburntx,destpub,amount) < 0 )
                return Invalid("GATEWAY-import-failure");
        }
    }

    // return Invalid("test-invalid");
    LOGSTREAM("importcoin", CCLOG_DEBUG2, stream << "Valid import tx! txid=" << importTx.GetHash().GetHex() << std::endl);
       
    return Valid();
}
