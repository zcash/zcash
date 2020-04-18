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

// #include "CCHeir_v0.h"
#include "heir_validate_v0.h"
#include <iomanip>

namespace heirv0 {

// makes coin initial tx opret
vscript_t EncodeHeirCreateOpRet(uint8_t funcid, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t inactivityTimeSec, std::string heirName, std::string memo)
{
    uint8_t evalcode = EVAL_HEIR;
    
    return /*CScript() << OP_RETURN <<*/ E_MARSHAL(ss << evalcode << funcid << ownerPubkey << heirPubkey << inactivityTimeSec << heirName << memo);
}

// makes coin additional tx opret
vscript_t EncodeHeirOpRet(uint8_t funcid,  uint256 fundingtxid, uint8_t hasHeirSpendingBegun)
{
    uint8_t evalcode = EVAL_HEIR;
    
    fundingtxid = revuint256(fundingtxid);
    return /*CScript() << OP_RETURN <<*/ E_MARSHAL(ss << evalcode << funcid << fundingtxid << hasHeirSpendingBegun);
}


// decode opret vout for Heir contract
uint8_t _DecodeHeirOpRet(vscript_t vopret, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, std::string& memo, uint256& fundingTxidInOpret, uint8_t &hasHeirSpendingBegun, bool noLogging)
{
    uint8_t evalCodeInOpret = 0;
    uint8_t heirFuncId = 0;
    
    fundingTxidInOpret = zeroid; //to init
    
    evalCodeInOpret = vopret.begin()[0];
    
    if (vopret.size() > 1 && evalCodeInOpret == EVAL_HEIR) {
        // NOTE: it unmarshals for all F, A and C
        uint8_t heirFuncId = 0;
        hasHeirSpendingBegun = 0;
        
        bool result = E_UNMARSHAL(vopret, { ss >> evalCodeInOpret; ss >> heirFuncId;					
            if (heirFuncId == 'F') {																	
				ss >> ownerPubkey; ss >> heirPubkey; ss >> inactivityTime; ss >> heirName; ss >> memo;	
            }																							
            else {																						
                ss >> fundingTxidInOpret >> hasHeirSpendingBegun;										
            }																							
        });
        
        if (!result)	{
            // if (!noLogging) std::cerr << "_DecodeHeirOpRet() could not unmarshal opret, evalCode=" << (int)evalCodeInOpret << std::endl;
            return (uint8_t)0;
        }
        
        /* std::cerr << "DecodeHeirOpRet()"
         << " heirFuncId=" << (char)(heirFuncId ? heirFuncId : ' ')
         << " ownerPubkey=" << HexStr(ownerPubkey)
         << " heirPubkey=" << HexStr(heirPubkey)
         << " heirName=" << heirName << " inactivityTime=" << inactivityTime
         << " hasHeirSpendingBegun=" << (int)hasHeirSpendingBegun << std::endl; */
        
        if (isMyFuncId(heirFuncId)) {
            fundingTxidInOpret = revuint256(fundingTxidInOpret);
            return heirFuncId;
        }
        else	{
            if(!noLogging) std::cerr << "_DecodeHeirOpRet() unexpected opret type, heirFuncId=" << (char)(heirFuncId ? heirFuncId : ' ') << std::endl;
        }
    }
    else {
        if (!noLogging) std::cerr << "_DecodeHeirOpRet() not a heir opret, vopretExtra.size() == 0 or not EVAL_HEIR evalcode=" << (int)evalCodeInOpret << std::endl;
    }
    return (uint8_t)0;
}

// decode combined opret:
uint8_t _DecodeHeirEitherOpRet(CScript scriptPubKey, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, std::string& memo, uint256& fundingTxidInOpret, uint8_t &hasHeirSpendingBegun, bool noLogging)
{
	uint8_t evalCodeTokens = 0;
	std::vector<CPubKey> voutPubkeysDummy;
    std::vector<vscript_t> oprets;
    vscript_t vopretExtra /*, vopretStripped*/;


	if (DecodeTokenOpRetV1(scriptPubKey, tokenid, voutPubkeysDummy, oprets) != 0 && GetOpReturnCCBlob(oprets, vopretExtra)) {
        /* if (vopretExtra.size() > 1) {
            // restore the second opret:

            /* unmarshalled in DecodeTokenOpRet:
            if (!E_UNMARSHAL(vopretExtra, { ss >> vopretStripped; })) {  //strip string size
                if (!noLogging) std::cerr << "_DecodeHeirEitherOpret() could not unmarshal vopretStripped" << std::endl;
                return (uint8_t)0;
            }
        } */
        if (vopretExtra.size() < 1) {
			if (!noLogging) std::cerr << "_DecodeHeirEitherOpret() empty vopretExtra" << std::endl;
			return (uint8_t)0;
		}
	}
	else 	{
		GetOpReturnData(scriptPubKey, vopretExtra);
	}
    
    return _DecodeHeirOpRet(vopretExtra, ownerPubkey, heirPubkey, inactivityTime, heirName, memo, fundingTxidInOpret, hasHeirSpendingBegun, noLogging);
}

// overload to decode opret in fundingtxid:
uint8_t DecodeHeirEitherOpRet(CScript scriptPubKey, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, std::string& memo, bool noLogging) {
    uint256 dummyFundingTxidInOpret;
    uint8_t dummyHasHeirSpendingBegun;
    
    return _DecodeHeirEitherOpRet(scriptPubKey, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName, memo, dummyFundingTxidInOpret, dummyHasHeirSpendingBegun, noLogging);
}

// overload to decode opret in A and C heir tx:
uint8_t DecodeHeirEitherOpRet(CScript scriptPubKey, uint256 &tokenid, uint256 &fundingTxidInOpret, uint8_t &hasHeirSpendingBegun, bool noLogging) {
    CPubKey dummyOwnerPubkey, dummyHeirPubkey;
    int64_t dummyInactivityTime;
    std::string dummyHeirName, dummyMemo;
    
    return _DecodeHeirEitherOpRet(scriptPubKey, tokenid, dummyOwnerPubkey, dummyHeirPubkey, dummyInactivityTime, dummyHeirName, dummyMemo, fundingTxidInOpret, hasHeirSpendingBegun, noLogging);
}

// check if pubkey is in vins
void CheckVinPubkey(std::vector<CTxIn> vins, CPubKey pubkey, bool &hasPubkey, bool &hasOtherPubkey) {

	hasPubkey = false;
	hasOtherPubkey = false;

	for (auto vin : vins) {
		CPubKey vinPubkey = check_signing_pubkey(vin.scriptSig);
		if (vinPubkey.IsValid())   {
			if (vinPubkey == pubkey) 
				hasPubkey = true;
			if (vinPubkey != pubkey)
				hasOtherPubkey = true;
		}
	}
}

/**
 * find the latest funding tx: it may be the first F tx or one of A or C tx's
 * Note: this function is also called from validation code (use non-locking calls)
 */
uint256 _FindLatestFundingTx(uint256 fundingtxid, uint8_t& funcId, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, std::string& memo, CScript& fundingOpretScript, uint8_t &hasHeirSpendingBegun)
{
    CTransaction fundingtx;
    uint256 hashBlock;
    const bool allowSlow = false;
    
    //char markeraddr[64];
    //CCtxidaddr(markeraddr, fundingtxid);
    //SetCCunspents(unspentOutputs, markeraddr,true);
    
    hasHeirSpendingBegun = 0;
    funcId = 0;
    
    // get initial funding tx and set it as initial lasttx:
    if (myGetTransaction(fundingtxid, fundingtx, hashBlock) && fundingtx.vout.size()) {
        
        CScript heirScript = (fundingtx.vout.size() > 0) ? fundingtx.vout[fundingtx.vout.size() - 1].scriptPubKey : CScript();
        uint8_t funcId = DecodeHeirEitherOpRet(heirScript, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName, memo, true);
        if (funcId != 0) {
            // found at least funding tx!
            //std::cerr << "FindLatestFundingTx() lasttx currently is fundingtx, txid=" << fundingtxid.GetHex() << " opreturn type=" << (char)funcId << '\n';
            fundingOpretScript = fundingtx.vout[fundingtx.vout.size() - 1].scriptPubKey;
        } else {
            std::cerr << "FindLatestFundingTx() could not decode opreturn for fundingtxid=" << fundingtxid.GetHex() << '\n';
            return zeroid;
        }
    } else {
        std::cerr << "FindLatestFundingTx() could not find funding tx for fundingtxid=" << fundingtxid.GetHex() << '\n';
        return zeroid;
    }
    
    // TODO: correct cc addr:
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>> unspentOutputs;
    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_HEIR);
    char coinaddr[64];
    GetCCaddress1of2(cp, coinaddr, ownerPubkey, heirPubkey); // get the address of cryptocondition '1 of 2 pubkeys'
    
    SetCCunspents(unspentOutputs, coinaddr,true);				 // get vector with tx's with unspent vouts of 1of2pubkey address:
    //std::cerr << "FindLatestFundingTx() using 1of2address=" << coinaddr << " unspentOutputs.size()=" << unspentOutputs.size() << '\n';
    
    int32_t maxBlockHeight = 0; // max block height
    uint256 latesttxid = fundingtxid;
    
    // try to find the last funding or spending tx by checking fundingtxid in 'opreturn':
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>>::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++) {
        CTransaction regtx;
        uint256 hash;
        
        uint256 txid = it->first.txhash;
        //std::cerr << "FindLatestFundingTx() checking unspents for txid=" << txid.GetHex() << '\n';
        
        int32_t blockHeight = (int32_t)it->second.blockHeight;
        
        //NOTE: maybe called from validation code:
        if (myGetTransaction(txid, regtx, hash)) {
            //std::cerr << "FindLatestFundingTx() found tx for txid=" << txid.GetHex() << " blockHeight=" << blockHeight << " maxBlockHeight=" << maxBlockHeight << '\n';
            uint256 fundingTxidInOpret;
            uint256 tokenidInOpret;  // not to contaminate the tokenid from the params!
            uint8_t tmpFuncId;
            uint8_t hasHeirSpendingBegunInOpret;
            
            CScript heirScript = (regtx.vout.size() > 0) ? regtx.vout[regtx.vout.size() - 1].scriptPubKey : CScript();
            tmpFuncId = DecodeHeirEitherOpRet(heirScript, tokenidInOpret, fundingTxidInOpret, hasHeirSpendingBegunInOpret, true);
            if (tmpFuncId != 0 && fundingtxid == fundingTxidInOpret && (tokenid == zeroid || tokenid == tokenidInOpret)) {  // check tokenid also
                
                if (blockHeight > maxBlockHeight) {

					// check owner pubkey in vins
					bool isOwner = false;
					bool isNonOwner = false;

					CheckVinPubkey(regtx.vin, ownerPubkey, isOwner, isNonOwner);

					// we ignore 'donations' tx (with non-owner inputs) for calculating if heir is allowed to spend:
					if (isOwner && !isNonOwner) {
						hasHeirSpendingBegun = hasHeirSpendingBegunInOpret;
						maxBlockHeight = blockHeight;
						latesttxid = txid;
						funcId = tmpFuncId;
					}
                    
                    //std::cerr << "FindLatestFundingTx() txid=" << latesttxid.GetHex() << " at blockHeight=" << maxBlockHeight
                    //	<< " opreturn type=" << (char)(funcId ? funcId : ' ') << " hasHeirSpendingBegun=" << (int)hasHeirSpendingBegun << " - set as current lasttxid" << '\n';
                }
            }
        }
    }
    
    return latesttxid;
}

};