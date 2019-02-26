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

//#define LEV_INFO 0
//#define LEV_DEBUG1 1
//#define LOGSTREAM(category, level, logoperator) { std::ostringstream stream; logoperator; for(int i = 0; i < level; i ++) if( LogAcceptCategory( (std::string(category) + (level > 0 ? std::string("-")+std::to_string(level) : std::string("") )).c_str() ) ) LogPrintStr(stream.str()); }

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

// utilities from gateways.cpp
uint256 BitcoinGetProofMerkleRoot(const std::vector<uint8_t> &proofData, std::vector<uint256> &txids);
uint256 GatewaysReverseScan(uint256 &txid, int32_t height, uint256 reforacletxid, uint256 batontxid);
int32_t GatewaysCointxidExists(struct CCcontract_info *cp, uint256 cointxid);
uint8_t DecodeGatewaysBindOpRet(char *depositaddr, const CScript &scriptPubKey, std::string &coin, uint256 &tokenid, int64_t &totalsupply, uint256 &oracletxid, uint8_t &M, uint8_t &N, std::vector<CPubKey> &pubkeys, uint8_t &taddr, uint8_t &prefix, uint8_t &prefix2);

// ac_import=chain support:
// encode opret for gateways import
CScript EncodeGatewaysImportTxOpRet(uint32_t targetCCid, std::string coin, uint256 bindtxid, std::vector<CPubKey> publishers, std::vector<uint256>txids, int32_t height, uint256 cointxid, int32_t claimvout, std::string rawburntx, std::vector<uint8_t>proof, CPubKey destpub, int64_t amount)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << targetCCid << coin << bindtxid << publishers << txids << height << cointxid << claimvout << rawburntx << proof << destpub << amount);
    return(opret);
}

bool ImportCoinGatewaysVerify(char *refdepositaddr, uint256 oracletxid, int32_t claimvout, std::string refcoin, uint256 burntxid, const std::string rawburntx, std::vector<uint8_t>proof, uint256 merkleroot)
{
    std::vector<uint256> txids; 
    uint256 proofroot, hashBlock, foundtxid = zeroid; 
    CTransaction oracletx, burntx; 
    std::string name, description, format; 
    char destaddr[64], destpubaddr[64], claimaddr[64]; 
    int32_t i, numvouts; 
    int64_t nValue = 0;

    if (myGetTransaction(oracletxid, oracletx, hashBlock) == 0 || (numvouts = oracletx.vout.size()) <= 0)
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "ImportCoinGatewaysVerify can't find oracletxid=" << oracletxid.GetHex() << std::endl);
        return false;
    }
    if (DecodeOraclesCreateOpRet(oracletx.vout[numvouts - 1].scriptPubKey, name, description, format) != 'C' || name != refcoin)
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "ImportCoinGatewaysVerify mismatched oracle name=" << name.c_str() << " != " << refcoin.c_str() << std::endl);
        return false;
    }
    proofroot = BitcoinGetProofMerkleRoot(proof, txids);
    if (proofroot != merkleroot)
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "ImportCoinGatewaysVerify mismatched proof merkleroot=" << proofroot.GetHex() << " and oracles merkleroot=" << merkleroot.GetHex() << std::endl);
        return false;
    }

    // check the burntxid is in the proof:
    if (std::find(txids.begin(), txids.end(), burntxid) == txids.end()) {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "ImportCoinGatewaysVerify invalid proof for this burntxid=" << burntxid.GetHex() << std::endl);
        return false;
    }

    /*
    if (DecodeHexTx(burntx, rawburntx) != 0)
    {
        Getscriptaddress(claimaddr, burntx.vout[claimvout].scriptPubKey);
        Getscriptaddress(destpubaddr, CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG);
        if (strcmp(claimaddr, destpubaddr) == 0)
        {
            for (i = 0; i<numvouts; i++)
            {
                Getscriptaddress(destaddr, burntx.vout[i].scriptPubKey);
                if (strcmp(refdepositaddr, destaddr) == 0)
                {
                    foundtxid = burntx.GetHash();
                    nValue = burntx.vout[i].nValue;
                    break;
                }
            }
        }
        else fprintf(stderr, "claimaddr.(%s) != destpubaddr.(%s)\n", claimaddr, destpubaddr);
    }*/

    /*
    if (foundtxid == burntxid)   {
        LOGSTREAM("importcoin", LEV_DEBUG1, stream << "verified proof for burntxid=" << burntxid.GetHex() << " in merkleroot merkleroot" << std::endl);
        return(nValue);
    }
    else {
    LOGSTREAM("importcoin", LEV_INFO, stream << "verified proof for burntxid=" << burntxid.GetHex() << " in merkleroot merkleroot" << std::endl);

        fprintf(stderr, "(%s) != (%s) or txid %s mismatch.%d or script mismatch\n", refdepositaddr, destaddr, uint256_str(str, foundtxid), foundtxid != burntxid);
        */

    LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "verified proof for burntxid=" << burntxid.GetHex() << " in trusted merkleroot" << std::endl);
    return true;
}


// make import tx with burntx and its proof of existence
std::string MakeGatewaysImportTx(uint64_t txfee, uint256 bindtxid, int32_t height, std::string refcoin, std::vector<uint8_t>proof, std::string rawburntx, int32_t ivout, uint256 burntxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction burntx, bindtx; 
    CPubKey mypk, gatewayspk; 
    uint256 oracletxid, merkleroot, mhash, hashBlock, tokenid, txid;
    int64_t totalsupply; 
    int32_t i, m, n, numvouts; 
    uint8_t M, N, taddr, prefix, prefix2; 
    std::string coin; 
    struct CCcontract_info *cp, C;
    std::vector<CPubKey> pubkeys, publishers; 
    std::vector<uint256>txids; 
    char depositaddr[64], txidaddr[64];

    cp = CCinit(&C, EVAL_GATEWAYS);
    /*if (txfee == 0)
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    gatewayspk = GetUnspendable(cp, 0); */

    if (!E_UNMARSHAL(ParseHex(rawburntx), ss >> burntx))
        return std::string("");

    CAmount amount = GetCoinImportValue(burntx);  // equal to int64_t

    LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "MakeGatewaysImportTx height=" << height << " coin=" << refcoin << " amount=" << (double)amount / COIN  << " pubkeys num=" << pubkeys.size() << std::endl);

    if (GetTransaction(bindtxid, bindtx, hashBlock, false) == 0 || (numvouts = bindtx.vout.size()) <= 0)
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx cant find bindtxid=" << bindtxid.GetHex() << std::endl);
        return("");
    }
/*    if (DecodeGatewaysBindOpRet(depositaddr, bindtx.vout[numvouts - 1].scriptPubKey, coin, tokenid, totalsupply, oracletxid, M, N, pubkeys, taddr, prefix, prefix2) != 'B' || refcoin != coin)
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx invalid coin - bindtxid=" << bindtxid.GetHex() << " coin=" << coin.c_str() << std::endl);
        return("");
    } eliminate link err */
    n = (int32_t)pubkeys.size();
    merkleroot = zeroid;
    for (i = m = 0; i < n; i++)
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx using pubkeys[" << i << "]=" << HexStr(pubkeys[i]) << std::endl);
        if ((mhash = GatewaysReverseScan(txid, height, oracletxid, OraclesBatontxid(oracletxid, pubkeys[i]))) != zeroid)
        {
            if (merkleroot == zeroid)
                merkleroot = mhash, m = 1;
            else if (mhash == merkleroot)
                m ++;
            publishers.push_back(pubkeys[i]);
            txids.push_back(txid);
        }
    }
    
    LOGSTREAM("importcoin", CCLOG_DEBUG1, stream << "MakeGatewaysImportTx burntxid=" << burntxid.GetHex() << " nodes m=" << m << " of n=" << n << std::endl);
    if (merkleroot == zeroid || m < n / 2) // none or less than half oracle nodes sent merkleroot
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx couldnt find merkleroot for block height=" << height << "coin=" << coin.c_str() << " oracleid=" << oracletxid.GetHex() << " m=" << m << " vs n=" << n << std::endl );
        return("");
    }
    if (GatewaysCointxidExists(cp, burntxid) != 0)
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx burntxid=" << burntxid.GetHex() << " already exists" << std::endl);
        return("");
    }
    if (!ImportCoinGatewaysVerify(depositaddr, oracletxid, ivout, coin, burntxid, rawburntx, proof, merkleroot))
    {
        LOGSTREAM("importcoin", CCLOG_INFO, stream << "MakeGatewaysImportTx could not validate burntx, txid=" << burntxid.GetHex() << std::endl);
        return("");
    }


    std::vector<uint256> leaftxids;
    BitcoinGetProofMerkleRoot(proof, leaftxids);
    MerkleBranch newBranch(0, leaftxids);
    TxProof txProof = std::make_pair(burntxid, newBranch);

    std::vector<CTxOut> vouts;



    return  HexStr(E_MARSHAL(ss << MakeImportCoinTransaction(txProof, burntx, vouts)));

    /*if (AddNormalinputs(mtx, mypk, 3 * txfee, 4) > 0)
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode, txfee, destpub));
        mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(CCtxidaddr(txidaddr, burntxid))) << OP_CHECKSIG));
        return(FinalizeCCTx(0, cp, mtx, mypk, txfee, EncodeGatewaysImportTxOpRet(0xFFFFFFFF, coin, bindtxid, publishers, txids, height, burntxid, ivout, rawburntx, proof, destpub, amount)));
    }
    LOGSTREAM("importcoin", LEV_INFO, stream << "MakeGatewaysImportTx coud not find normal imputs" << std::endl);*/
    return("");
}

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

// use proof from the above functions to validate the import

int32_t CheckBEAMimport(TxProof proof,std::vector<uint8_t> rawproof,CTransaction burnTx,std::vector<CTxOut> payouts)
{
    // check with dual-BEAM daemon via ASSETCHAINS_BEAMPORT for validity of burnTx
    return(-1);
}

int32_t CheckCODAimport(TxProof proof,std::vector<uint8_t> rawproof,CTransaction burnTx,std::vector<CTxOut> payouts)
{
    // check with dual-CODA daemon via ASSETCHAINS_CODAPORT for validity of burnTx
    return(-1);
}

int32_t CheckGATEWAYimport(TxProof proof,std::vector<uint8_t> rawproof,CTransaction burnTx,std::vector<CTxOut> payouts)
{
    // ASSETCHAINS_SELFIMPORT is coin
    // check for valid burn from external coin blockchain and if valid return(0);
    return(-1);
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

bool Eval::ImportCoin(const std::vector<uint8_t> params,const CTransaction &importTx,unsigned int nIn)
{
    TxProof proof; CTransaction burnTx; std::vector<CTxOut> payouts; uint64_t txfee = 10000;
    uint32_t targetCcid; std::string targetSymbol; uint256 payoutsHash; std::vector<uint8_t> rawproof;
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
            else if ( CheckCODAimport(proof,rawproof,burnTx,payouts) < 0 )
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
            else if ( CheckGATEWAYimport(proof,rawproof,burnTx,payouts) < 0 )
                return Invalid("GATEWAY-import-failure");
        }
    }
    return Valid();
}


