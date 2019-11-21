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

#include "CCHeir.h"
#include "heir_validate.h"
#include <iomanip>

class CoinHelper;
class TokenHelper;

/*
 The idea of Heir CC is to allow crypto inheritance.
 A special 1of2 CC address is created that is freely spendable by the creator (funds owner).
 The owner may add additional funds to this 1of2 address.
 The heir is only allowed to spend after "the specified amount of idle blocks" (changed to "the owner inactivityTime").
 The idea is that if the address doesnt spend any funds for a year (or whatever amount set), then it is time to allow the heir to spend.
 "The design requires the heir to spend all the funds at once" (this requirement was changed to "after the inactivity time both the heir and owner may freely spend available funds")
 After the first heir spending a flag is set that spending is allowed for the heir whether the owner adds more funds or spends them.
 This Heir contract supports both coins and tokens.
 */

// tx validation code

// Plan validation runner, it may be called twice - for coins and tokens
// (sadly we cannot have yet 'templatized' lambdas, if we could we could capture all these params inside HeirValidation()...)
template <typename Helper> bool RunValidationPlans(uint8_t funcId, struct CCcontract_info* cp, Eval* eval, const CTransaction& tx, uint256 latestTxid, CScript fundingOpretScript, uint8_t hasHeirSpendingBegun)
{
    int32_t numvins = tx.vin.size();
    int32_t numvouts = tx.vout.size();
    
    // setup validation framework (please see its description in heir_validate.h):
    // validation 'plans':
    CInputValidationPlan<CValidatorBase> vinPlan;
    COutputValidationPlan<CValidatorBase> voutPlan;
    
    // vin 'identifiers'
    CNormalInputIdentifier normalInputIdentifier(cp);
    CCCInputIdentifier ccInputIdentifier(cp);
    
    // vin and vout 'validators'
    // not used, too strict for 2 pubkeys: CMyPubkeyVoutValidator<Helper>	normalInputValidator(cp, fundingOpretScript, true);								// check normal input for this opret cause this is first tx
    CCC1of2AddressValidator<Helper> cc1of2ValidatorThis(cp, fundingOpretScript, "checking this tx opreturn:");		// 1of2add validator with pubkeys from this tx opreturn
    CHeirSpendValidator<Helper>	    heirSpendValidator(cp, fundingOpretScript, latestTxid, hasHeirSpendingBegun);	// check if heir allowed to spend
    
    // only for tokens:
    CMyPubkeyVoutValidator<TokenHelper>	ownerCCaddrValidator(cp, fundingOpretScript, false);						// check if this correct owner's cc user addr corresponding to opret
    COpRetValidator<Helper>		    opRetValidator(cp, fundingOpretScript);											// compare opRets in this and last tx
	CMarkerValidator<Helper>	    markerValidator(cp);															// initial tx marker spending protection
    CNullValidator<Helper>			nullValidator(cp);
    
    switch (funcId) {
        case 'F': // fund tokens (only for tokens)
            // vin validation plan:
            vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&normalInputIdentifier, &nullValidator);			// txfee vin
            vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&ccInputIdentifier, &markerValidator, &ownerCCaddrValidator);		// check cc owner addr
            
            // vout validation plan:
            voutPlan.pushValidators<CValidatorBase>(0, &cc1of2ValidatorThis);												// check 1of2 addr funding
            // do not check change at this time
            // no checking for opret yet
            break;
            
        case 'A': // add tokens (only for tokens)
            // vin validation plan:
            vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&normalInputIdentifier, &nullValidator);		// txfee vin
            vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&ccInputIdentifier, &markerValidator, &ownerCCaddrValidator);   // check cc owner addr
            
            // vout validation plan:
            voutPlan.pushValidators<CValidatorBase>(0, &cc1of2ValidatorThis);												// check 1of2 addr funding
            // do not check change at this time
            voutPlan.pushValidators<CValidatorBase>(numvouts - 1, &opRetValidator);											// opreturn check, NOTE: only for C or A:
            break;
            
        case 'C':  // spend coins or tokens
            // vin validation plan:
            vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&normalInputIdentifier, &nullValidator);			// txfee vin
            vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&ccInputIdentifier, &markerValidator, &cc1of2ValidatorThis);		// cc1of2 funding addr
            
            // vout validation plan:
            voutPlan.pushValidators<CValidatorBase>(0, &heirSpendValidator);													// check if heir is allowed to spend
            voutPlan.pushValidators<CValidatorBase>(numvouts - 1, &opRetValidator);												// opreturn check, NOTE: only for C or A
            break;
    }
    
    // call vin/vout validation
    if (!vinPlan.validate(tx, eval))
        return false;
    if (!voutPlan.validate(tx, eval))
        return false;
    
    return true;
}

/**
 * Tx validation entry function
 */
bool HeirValidate(struct CCcontract_info* cpHeir, Eval* eval, const CTransaction& tx, uint32_t nIn)
{
    int32_t numvins = tx.vin.size();
    int32_t numvouts = tx.vout.size();
    //int32_t preventCCvins = -1;
    //int32_t preventCCvouts = -1;
    
    struct CCcontract_info *cpTokens, tokensC;
    cpTokens = CCinit(&tokensC, EVAL_TOKENS);
    
    if (numvouts < 1)
        return eval->Invalid("no vouts");
    
    //if (chainActive.Height() < 741)
    //	return true;
    
    uint256 fundingTxidInOpret = zeroid, latestTxid = zeroid, dummyTokenid, tokenidThis, tokenid = zeroid;
    
    CScript fundingTxOpRetScript;
    uint8_t hasHeirSpendingBegun = 0, hasHeirSpendingBegunDummy;
    
    CScript opret = (tx.vout.size() > 0) ? tx.vout[tx.vout.size() - 1].scriptPubKey : CScript(); // check boundary
    uint8_t funcId = DecodeHeirEitherOpRet(opret, tokenidThis, fundingTxidInOpret, hasHeirSpendingBegunDummy, true);
    if (funcId == 0)
        return eval->Invalid("invalid opreturn format");
    
    if (funcId != 'F') {
        if (fundingTxidInOpret == zeroid) {
            return eval->Invalid("incorrect tx opreturn: no fundingtxid present");
        }
        latestTxid = FindLatestFundingTx(fundingTxidInOpret, tokenid, fundingTxOpRetScript, hasHeirSpendingBegun);
        
        if( tokenid != zeroid && tokenid != tokenidThis )
            return eval->Invalid("incorrect tx tokenid");
        
        if (latestTxid == zeroid) {
            return eval->Invalid("no fundingtx found");
        }
    }
    else {
        fundingTxOpRetScript = opret;
    }
    
    std::cerr << "HeirValidate funcid=" << (char)funcId << " evalcode=" << (int)cpHeir->evalcode << std::endl;
    
    //////////////// temp ////////////////////////
    ///return true;
    
    switch (funcId) {
        case 'F':
            // fund coins:
            // vins.*: normal inputs
            // -----------------------------
            // vout.0: funding CC 1of2 addr for the owner and heir
            // vout.1: txfee for CC addr used as a marker
            // vout.2: normal change
            // vout.n-1: opreturn 'F' ownerpk heirpk inactivitytime heirname
            
            // fund tokens:
            // vin.0: normal inputs txfee
            // vins.1+: user's CC addr inputs
            // -----------------------
            // vout.0: funding heir CC 1of2 addr for the owner and heir
            // vout.1: txfee for CC addr used as a marker
            // vout.2: normal change
            // vout.n-1: opreturn 't' tokenid 'F' ownerpk heirpk inactivitytime heirname tokenid
            if (tokenid != zeroid)
                return RunValidationPlans<TokenHelper>(funcId, cpTokens, eval, tx, latestTxid, fundingTxOpRetScript, hasHeirSpendingBegun);
            else
                return eval->Invalid("unexpected HeirValidate for heirfund");
            // break;
            
        case 'A':
            // add funding coins:
            // vins.*: normal inputs
            // ------------------------
            // vout.0: funding CC 1of2 addr for the owner and heir
            // vout.1: normal change
            // vout.n-1: opreturn 'A' ownerpk heirpk inactivitytime fundingtx
            
            // add funding tokens:
            // vins.0: normal inputs txfee
            // vins.1+: user's CC addr inputs
            // ------------------------
            // vout.0: funding CC 1of2 addr for the owner and heir
            // vout.1: normal change
            // vout.n-1: opreturn 't' tokenid 'A' ownerpk heirpk inactivitytime fundingtx
            if (tokenid != zeroid)
                return RunValidationPlans<TokenHelper>(funcId, cpTokens, eval, tx, latestTxid, fundingTxOpRetScript, hasHeirSpendingBegun);
            else
                return eval->Invalid("unexpected HeirValidate for heiradd");
            //break;
            
        case 'C':
            // claim coins:
            // vin.0: normal input txfee
            // vin.1+: input from CC 1of2 addr
            // -------------------------------------
            // vout.0: normal output to owner or heir address
            // vout.1: change to CC 1of2 addr
            // vout.2: change to user's addr from txfee input if any
            // vout.n-1: opreturn 'C' ownerpk heirpk inactivitytime fundingtx
            
            // claim tokens:
            // vin.0: normal input txfee
            // vin.1+: input from CC 1of2 addr
            // --------------------------------------------
            // vout.0: output to user's cc address
            // vout.1: change to CC 1of2 addr
            // vout.2: change to normal from txfee input if any
            // vout.n-1: opreturn 't' tokenid 'C' ownerpk heirpk inactivitytime fundingtx
            if (tokenid != zeroid)
                return RunValidationPlans<TokenHelper>(funcId, cpTokens, eval, tx, latestTxid, fundingTxOpRetScript, hasHeirSpendingBegun);
            else
                return RunValidationPlans<CoinHelper>(funcId, cpHeir, eval, tx, latestTxid, fundingTxOpRetScript, hasHeirSpendingBegun);
            // break;
            
        default:
            std::cerr << "HeirValidate() illegal heir funcid=" << (char)funcId << std::endl;
            return eval->Invalid("unexpected HeirValidate funcid");
            // break;
    }
    return eval->Invalid("unexpected");   //    (PreventCC(eval, tx, preventCCvins, numvins, preventCCvouts, numvouts));
}
// end of consensus code


// helper functions used in implementations of rpc calls (in rpcwallet.cpp) or validation code

/**
 * Checks if vout is to cryptocondition address
 * @return vout value in satoshis
 */
template <class Helper> int64_t IsHeirFundingVout(struct CCcontract_info* cp, const CTransaction& tx, int32_t voutIndex, CPubKey ownerPubkey, CPubKey heirPubkey)
{
    char destaddr[65], heirFundingAddr[65];
    
    Helper::GetCoinsOrTokensCCaddress1of2(cp, heirFundingAddr, ownerPubkey, heirPubkey);
    if (tx.vout[voutIndex].scriptPubKey.IsPayToCryptoCondition() != 0) {
        // NOTE: dimxy it was unsafe 'Getscriptaddress(destaddr,tx.vout[voutIndex].scriptPubKey) > 0' here:
        if (Getscriptaddress(destaddr, tx.vout[voutIndex].scriptPubKey) && strcmp(destaddr, heirFundingAddr) == 0)
            return (tx.vout[voutIndex].nValue);
        else
            std::cerr << "IsHeirFundingVout() heirFundingAddr=" << heirFundingAddr << " not equal to destaddr=" << destaddr << std::endl;
    }
    return (0);
}

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
            if (!noLogging) std::cerr << "_DecodeHeirOpRet() could not unmarshal opret, evalCode=" << (int)evalCodeInOpret << std::endl;
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
    std::vector<std::pair<uint8_t, vscript_t>> oprets;
    vscript_t vopretExtra /*, vopretStripped*/;


	if (DecodeTokenOpRet(scriptPubKey, evalCodeTokens, tokenid, voutPubkeysDummy, oprets) != 0 && GetOpretBlob(oprets, OPRETID_HEIRDATA, vopretExtra)) {
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

// overload for validation code
uint256 FindLatestFundingTx(uint256 fundingtxid, uint256 &tokenid, CScript& opRetScript, uint8_t &hasHeirSpendingBegun)
{
    uint8_t funcId;
    CPubKey ownerPubkey;
    CPubKey heirPubkey;
    int64_t inactivityTime;
    std::string heirName, memo;
    
    return _FindLatestFundingTx(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName, memo, opRetScript, hasHeirSpendingBegun);
}

// overload for transaction creation code
uint256 FindLatestFundingTx(uint256 fundingtxid, uint8_t& funcId, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, std::string& memo, uint8_t &hasHeirSpendingBegun)
{
    CScript opRetScript;
    
    return _FindLatestFundingTx(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName, memo, opRetScript, hasHeirSpendingBegun);
}

// add inputs of 1 of 2 cc address
template <class Helper> int64_t Add1of2AddressInputs(struct CCcontract_info* cp, uint256 fundingtxid, CMutableTransaction& mtx, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t total, int32_t maxinputs)
{
    // TODO: add threshold check
    int64_t nValue, voutValue, totalinputs = 0;
    CTransaction heirtx;
    int32_t n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>> unspentOutputs;
    
    char coinaddr[64];
    Helper::GetCoinsOrTokensCCaddress1of2(coinaddr, ownerPubkey, heirPubkey);   // get address of cryptocondition '1 of 2 pubkeys'
    SetCCunspents(unspentOutputs, coinaddr,true);
    
    //   char markeraddr[64];
    //   CCtxidaddr(markeraddr, fundingtxid);
    //   SetCCunspents(unspentOutputs, markeraddr,true);
    
    std::cerr << "Add1of2AddressInputs() using 1of2addr=" << coinaddr << " unspentOutputs.size()=" << unspentOutputs.size() << std::endl;
    
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>>::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++) {
        uint256 txid = it->first.txhash;
        uint256 hashBlock;
        int32_t voutIndex = (int32_t)it->first.index;
        // no need to prevent dup
        // dimxy: maybe it is good to put tx's in cache?
        
        //std::cerr << "Add1of2AddressInputs() txid=" << txid.GetHex() << std::endl;
        
        if (myGetTransaction(txid, heirtx, hashBlock) != 0) {
            uint256 tokenid;
            uint256 fundingTxidInOpret;
            uint8_t hasHeirSpendingBegunDummy;
            
            CScript heirScript = (heirtx.vout.size() > 0) ? heirtx.vout[heirtx.vout.size() - 1].scriptPubKey : CScript();   // check boundary
            uint8_t funcId = DecodeHeirEitherOpRet(heirScript, tokenid, fundingTxidInOpret, hasHeirSpendingBegunDummy, false);
            
            if ((txid == fundingtxid || fundingTxidInOpret == fundingtxid) &&
                funcId != 0 &&
                isMyFuncId(funcId) &&
                (typeid(Helper) != typeid(TokenHelper) || IsTokensvout(true, true, cp, nullptr, heirtx, voutIndex, tokenid) > 0) && // token validation logic
                //(voutValue = IsHeirFundingVout<Helper>(cp, heirtx, voutIndex, ownerPubkey, heirPubkey)) > 0 &&		// heir contract vout validation logic - not used since we moved to 2-eval vouts
                !myIsutxo_spentinmempool(ignoretxid,ignorevin,txid, voutIndex))
            {
                std::cerr << "Add1of2AddressInputs() satoshis=" << it->second.satoshis << std::endl;
                if (total != 0 && maxinputs != 0)
                    mtx.vin.push_back(CTxIn(txid, voutIndex, CScript()));
                nValue = it->second.satoshis;
                totalinputs += nValue;
                n++;
                if ((total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs))
                    break;
            }
        }
    }
    return totalinputs;
}

/**
 * enumerate all tx's sending to CCHeir 1of2address and calc total lifetime funds
 */
template <class Helper> int64_t LifetimeHeirContractFunds(struct CCcontract_info* cp, uint256 fundingtxid, CPubKey ownerPubkey, CPubKey heirPubkey)
{
    char coinaddr[64];
    Helper::GetCoinsOrTokensCCaddress1of2(coinaddr, ownerPubkey, heirPubkey); // get the address of cryptocondition '1 of 2 pubkeys'
    
    std::vector<std::pair<CAddressIndexKey, CAmount>> addressIndexes;
    SetCCtxids(addressIndexes, coinaddr,true);
    
    //fprintf(stderr,"LifetimeHeirContractFunds() scan lifetime of %s\n",coinaddr);
    int64_t total = 0;
    for (std::vector<std::pair<CAddressIndexKey, CAmount>>::const_iterator it = addressIndexes.begin(); it != addressIndexes.end(); it++) {
        uint256 hashBlock;
        uint256 txid = it->first.txhash;
        CTransaction heirtx;
        
        // TODO: check all funding tx should contain unspendable markers
        if (myGetTransaction(txid, heirtx, hashBlock) && heirtx.vout.size() > 0) {
            uint256 tokenid;
            uint256 fundingTxidInOpret;
            uint8_t hasHeirSpendingBegunDummy;
            const int32_t ivout = 0;
            
            CScript heirScript = (heirtx.vout.size() > 0) ? heirtx.vout[heirtx.vout.size() - 1].scriptPubKey : CScript();   // check boundary
            uint8_t funcId = DecodeHeirEitherOpRet(heirScript, tokenid, fundingTxidInOpret, hasHeirSpendingBegunDummy, false);
            
            //std::cerr << "LifetimeHeirContractFunds() found tx=" << txid.GetHex() << " vout[0].nValue=" << subtx.vout[ccVoutIdx].nValue << " opreturn=" << (char)funcId << '\n';
            
            if (funcId != 0 &&
                (txid == fundingtxid || fundingTxidInOpret == fundingtxid) &&
                isMyFuncId(funcId) && !isSpendingTx(funcId) &&
                (typeid(Helper) != typeid(TokenHelper) || IsTokensvout(true, true, cp, nullptr, heirtx, ivout, tokenid) > 0) &&
                !myIsutxo_spentinmempool(ignoretxid,ignorevin,txid, ivout)) // exclude tx in mempool
            {
                total += it->second; // dont do this: tx.vout[ivout].nValue; // in vin[0] always is the pay to 1of2 addr (funding or change)
                //std::cerr << "LifetimeHeirContractFunds() added tx=" << txid.GetHex() << " it->second=" << it->second << " vout[0].nValue=" << tx.vout[ivout].nValue << " opreturn=" << (char)funcId << '\n';
            }
        }
    }
    return (total);
}

/* rpc functions' implementation: */

/**
 * heirfund rpc call implementation
 * creates tx for initial funds deposit on cryptocondition address which locks funds for spending by either of address.
 * and also for setting spending plan for the funds' owner and heir
 * @return fundingtxid handle for subsequent references to this heir funding plan
 */
template <typename Helper> UniValue _HeirFund(int64_t txfee, int64_t amount, std::string heirName, CPubKey heirPubkey, int64_t inactivityTimeSec, std::string memo, uint256 tokenid)
{
    UniValue result(UniValue::VOBJ);
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    struct CCcontract_info *cp, C;
    
    cp = CCinit(&C, Helper::getMyEval());
    if (txfee == 0)
        txfee = 10000;

	int64_t markerfee = 10000;
    
    //std::cerr << "HeirFund() amount=" << amount << " txfee=" << txfee << " heirPubkey IsValid()=" << heirPubkey.IsValid() << " inactivityTime(sec)=" << inactivityTimeSec << " tokenid=" << tokenid.GetHex() << std::endl;
        
    CPubKey myPubkey = pubkey2pk(Mypubkey());
    
    if (!tokenid.IsNull())  // add normals only for tokens
    {
        if (AddNormalinputs(mtx, myPubkey, txfee + markerfee, 4) < txfee + markerfee)
        {
            std::cerr << "HeirFund() could not find normal inputs for txfee" << std::endl;
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "could not find normal inputs for txfee"));
            return result;
        }
    }
        
    int64_t inputs;
    int64_t addAmount = tokenid.IsNull() ? (txfee + markerfee + amount) : amount;   // for coins add txfee markerfee amount in one call
    if ((inputs=Helper::addOwnerInputs(tokenid, mtx, myPubkey, addAmount, (int32_t)64)) >= addAmount) 
    { 
		mtx.vout.push_back(Helper::make1of2Vout(amount, myPubkey, heirPubkey));
            
        // add a marker for finding all plans in HeirList()
        // TODO: change marker either to cc or normal txidaddr unspendable
		struct CCcontract_info *cpHeir, heirC;  
		cpHeir = CCinit(&heirC, EVAL_HEIR);
        CPubKey heirUnspendablePubKey = GetUnspendable(cpHeir, 0);
        // mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(heirUnspendablePubKey)) << OP_CHECKSIG));  <-- bad marker cause it was spendable by anyone
		mtx.vout.push_back(MakeCC1vout(EVAL_HEIR, markerfee, heirUnspendablePubKey));		// this marker spending is disabled in the validation code
            
        if (!tokenid.IsNull()) 
        {
            int64_t ccChange = 0;
            // calc and add token change vout:
            if (inputs > amount)
                ccChange = (inputs - amount); //  -txfee <-- txfee pays user

            //std::cerr << "HeirFund() inputs=" << inputs << " amount=" << amount << " txfee=" << txfee << " change=" << change << '\n';

            if (ccChange != 0)
                mtx.vout.push_back(Helper::makeUserVout(ccChange, myPubkey));
        }

		// check owner pubkey in vins
		bool hasMypubkey = false;
		bool hasNotMypubkey = false;

		CheckVinPubkey(mtx.vin, myPubkey, hasMypubkey, hasNotMypubkey);

		// for initial funding do not allow to sign by non-owner key:
		if (hasNotMypubkey) {
			result.push_back(Pair("result", "error"));
			result.push_back(Pair("error", "using non-owner inputs not allowed"));
			return result;
		}

        // add 1of2 vout token validation pubkeys - used only for tokens
        std::vector<CPubKey> voutTokenPubkeys;
        voutTokenPubkeys.push_back(myPubkey);
        voutTokenPubkeys.push_back(heirPubkey);
            
        // add change for txfee and opreturn vouts and sign tx:
        std::string rawhextx = FinalizeCCTx(0, cp, mtx, myPubkey, txfee,
                                            Helper::makeCreateOpRet(tokenid, voutTokenPubkeys, myPubkey, heirPubkey, inactivityTimeSec, heirName, memo));
        if (!rawhextx.empty()) {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", rawhextx));
        }
        else {
            std::cerr << "HeirAdd error in FinalizeCCtx" << std::endl;
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "sign error"));
        }
    }
    else {  // TODO: need result return unification with heiradd and claim
        std::cerr << "HeirFund() could not find owner inputs for amount (normal inputs for coins, cc inputs for tokens)" << std::endl;
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "could not find owner inputs"));
    }
    return result;
}

// if no these callers - it could not link
UniValue HeirFundCoinCaller(int64_t txfee, int64_t coins, std::string heirName, CPubKey heirPubkey, int64_t inactivityTimeSec, std::string memo){
    return _HeirFund<CoinHelper>(txfee, coins, heirName, heirPubkey, inactivityTimeSec,  memo, zeroid);
}

UniValue HeirFundTokenCaller(int64_t txfee, int64_t satoshis, std::string heirName, CPubKey heirPubkey, int64_t inactivityTimeSec, std::string memo, uint256 tokenid) {
    return _HeirFund<TokenHelper>(txfee, satoshis, heirName, heirPubkey, inactivityTimeSec, memo, tokenid);
}

/**
 * heiradd rpc call implementation
 * creates tx to add more funds to cryptocondition address for spending by either funds' owner or heir
 * @return result object with raw tx or error text
 */
template <class Helper> UniValue _HeirAdd(uint256 fundingtxid, int64_t txfee, int64_t amount, uint256 latesttxid, uint8_t funcId, uint256 tokenid, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t inactivityTimeSec, uint8_t hasHeirSpendingBegun)
{
    UniValue result(UniValue::VOBJ);
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    struct CCcontract_info *cp, C;
    std::string rawhex;
    
    cp = CCinit(&C, Helper::getMyEval());  // for tokens shoud be EVAL_TOKENS to sign it correctly!
    
    if (txfee == 0)
        txfee = 10000;
    
	int64_t markerfee = 10000;

    CPubKey myPubkey = pubkey2pk(Mypubkey());
    
    // check if it is the owner
    if (myPubkey != ownerPubkey) {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "adding funds is only allowed for the owner of this contract"));
        return result;
    }
    
    if (!tokenid.IsNull())  // add normals only for tokens
    {
        if (AddNormalinputs(mtx, myPubkey, txfee + markerfee, 4) < txfee + markerfee)
        {
            std::cerr << "HeirFund() could not find normal inputs for txfee" << std::endl;
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "could not find normal inputs for txfee"));
            return result;
        }
    }

    int64_t inputs;
    int64_t addAmount = tokenid.IsNull() ? (txfee + markerfee + amount) : amount;  // for coins add txfee markerfee amount in one call
    if ((inputs = Helper::addOwnerInputs(tokenid, mtx, myPubkey, addAmount, 64)) >= addAmount) { // TODO: why 64 max inputs?
            
        // we do not use markers anymore - storing data in opreturn is better
        // add marker vout:
        /* char markeraddr[64];
            CPubKey markerpubkey = CCtxidaddr(markeraddr, fundingtxid);
            mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG)); // txfee 1, txfee 2 - for miners
            std::cerr << "HeirAdd() adding markeraddr=" << markeraddr << '\n'; */
            
        // add cryptocondition to spend this funded amount for either pk
        mtx.vout.push_back(Helper::make1of2Vout(amount, ownerPubkey, heirPubkey));

		char markeraddr[64];
		CPubKey markerPubkey = CCtxidaddr(markeraddr, fundingtxid);
		mtx.vout.push_back(CTxOut(markerfee, CScript() << ParseHex(HexStr(markerPubkey)) << OP_CHECKSIG));		// marker to prevent archiving of the funds add vouts

        if (!tokenid.IsNull())
        {
            int64_t ccChange = 0;

            if (inputs > amount)
                ccChange = (inputs - amount); //  -txfee <-- txfee pays user
            //std::cerr << "HeirAdd() inputs=" << inputs << " amount=" << amount << " txfee=" << txfee << " change=" << change << '\n';

            if (ccChange != 0) 
                mtx.vout.push_back(Helper::makeUserVout(ccChange, myPubkey));
        }

		// check owner pubkey in vins
		bool hasMypubkey = false;
		bool hasNotMypubkey = false;

		CheckVinPubkey(mtx.vin, myPubkey, hasMypubkey, hasNotMypubkey);

		// for additional funding do not allow to sign by both owner and non-owner keys (is this a donation or not?):
		if (hasMypubkey && hasNotMypubkey) {
			result.push_back(Pair("result", "error"));
			result.push_back(Pair("error", "using both owner and non-owner inputs is not allowed"));
			return result;
		}
            
		// warn the user he's making a donation if this is all non-owner keys:
		if (hasNotMypubkey) {
			result.push_back(Pair("result", "warning"));
			result.push_back(Pair("warning", "you are about to make a donation to heir fund"));
		}
		else	{
			result.push_back(Pair("result", "success"));
		}

        // add 1of2 vout validation pubkeys - needed only for tokens:
        std::vector<CPubKey> voutTokenPubkeys;
        voutTokenPubkeys.push_back(ownerPubkey);
        voutTokenPubkeys.push_back(heirPubkey);
            
        // add opreturn 'A'  and sign tx:						
        std::string rawhextx = (FinalizeCCTx(0, cp, mtx, myPubkey, txfee,
                                                Helper::makeAddOpRet(tokenid, voutTokenPubkeys, fundingtxid, hasHeirSpendingBegun)));
            
        if (!rawhextx.empty()) {
            result.push_back(Pair("hex", rawhextx));
        }
        else	{
            std::cerr << "HeirAdd error in FinalizeCCtx" << std::endl;
			result.clear();
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "sign error"));
        }
    }
    else {
        std::cerr << "HeirAdd cannot find owner inputs for amount (normal inputs for coins, cc inputs for tokens)" << std::endl;
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "can't find owner inputs"));
    }    
    return result;
}


UniValue HeirAddCaller(uint256 fundingtxid, int64_t txfee, std::string strAmount) {
    
    CPubKey ownerPubkey, heirPubkey;
    int64_t inactivityTimeSec;
    
    uint256 latesttxid, tokenid = zeroid;
    uint8_t funcId;
    std::string heirName, memo;
    uint8_t hasHeirSpendingBegun = 0;
    
    // get latest tx to see if it is a token or coin
    if ((latesttxid = FindLatestFundingTx(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, memo, hasHeirSpendingBegun)) != zeroid) 
    {
		if (tokenid == zeroid) {
			int64_t amount = 0;
			if (!ParseFixedPoint(strAmount, 8, &amount) || amount <= 0 ) {
				UniValue result(UniValue::VOBJ);
				result.push_back(Pair("result", "error"));
				result.push_back(Pair("error", "invalid amount"));
				return result;
			}
			return _HeirAdd<CoinHelper>(fundingtxid, txfee, amount, latesttxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, hasHeirSpendingBegun);
		}
		else 
        {
			int64_t amount = atoll(strAmount.c_str());
			if (amount <= 0) {
				UniValue result(UniValue::VOBJ);
				result.push_back(Pair("result", "error"));
				result.push_back(Pair("error", "invalid amount"));
				return result;
			}
			return _HeirAdd<TokenHelper>(fundingtxid, txfee, amount, latesttxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, hasHeirSpendingBegun);
		}
    }
    else {
        UniValue result(UniValue::VOBJ);
        
        fprintf(stderr, "HeirAdd() can't find any heir CC funding tx's\n");
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "can't find any heir CC funding transactions"));
        return result;
    }
}


/**
 * heirclaim rpc call implementation
 * creates tx to spend funds from cryptocondition address by either funds' owner or heir
 * @return result object with raw tx or error text
 */
template <typename Helper>UniValue _HeirClaim(uint256 fundingtxid, int64_t txfee, int64_t amount, uint256 latesttxid, uint8_t funcId, uint256 tokenid, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t inactivityTimeSec, uint8_t hasHeirSpendingBegun)
{
    UniValue result(UniValue::VOBJ);
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey myPubkey;
    int64_t inputs, change = 0;
    struct CCcontract_info *cp, C;
    
    cp = CCinit(&C, EVAL_HEIR);
    if (txfee == 0)
        txfee = 10000;
    
    int32_t numblocks;
    uint64_t durationSec = 0;
    
    // we do not need to find duration if spending already has begun
    if (!hasHeirSpendingBegun) {
        durationSec = CCduration(numblocks, latesttxid);
        std::cerr << "HeirClaim() duration=" << durationSec << " inactivityTime=" << inactivityTimeSec << " numblocks=" << numblocks << std::endl;
    }
    
    // spending is allowed if there is already spending tx or inactivity time
    //bool isAllowedToHeir = (funcId == 'C' || durationSec > inactivityTimeSec) ? true : false;
    bool isAllowedToHeir = (hasHeirSpendingBegun || durationSec > inactivityTimeSec) ? true : false;
    myPubkey = pubkey2pk(Mypubkey());
    
    // if it is the heir, check if spending not allowed to heir yet
    if (myPubkey == heirPubkey && !isAllowedToHeir) {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "spending is not allowed yet for the heir"));
        return result;
    }
    
    // we do not use markers any more:
    // we allow owner to spend funds at any time:
    // if it is the owner, check if spending already allowed to heir
    /* if (myPubkey == ownerPubkey && isAllowedToHeir) {
     result.push_back(Pair("result", "spending is not already allowed for the owner"));
     return result;
     }	*/
    
    // add spending txfee from the calling user
    if (AddNormalinputs(mtx, myPubkey, txfee, 3) > 0) {
        
        // add spending from cc 1of2 address
        if ((inputs = Add1of2AddressInputs<Helper>(cp, fundingtxid, mtx, ownerPubkey, heirPubkey, amount, 60)) >= amount) // TODO: why only 60 inputs?
        {
            /*if (inputs < amount) {
             std::cerr << "HeirClaim() cant find enough HeirCC 1of2 inputs, found=" << inputs << " required=" << amount << std::endl;
             result.push_back(Pair("result", "error"));
             result.push_back(Pair("error", "can't find heir CC funding"));
             
             return result;
             }*/
            
            // add vout with amount to claiming address
            mtx.vout.push_back(Helper::makeUserVout(amount, myPubkey));  // vout[0]
            
            // calc and add change vout:
            if (inputs > amount)
                change = (inputs - amount); //  -txfee <-- txfee pays user
            
            //std::cerr << "HeirClaim() inputs=" << inputs << " amount=" << amount << " txfee=" << txfee << " change=" << change << '\n';
            
            // change to 1of2 funding addr:
            if (change != 0) {											   // vout[1]
                mtx.vout.push_back(Helper::make1of2Vout(change, ownerPubkey, heirPubkey)); // using always pubkeys from OP_RETURN in order to not mixing them up!
            }
            
            // add marker vout:
            /*char markeraddr[64];
             CPubKey markerpubkey = CCtxidaddr(markeraddr, fundingtxid);
             // NOTE: amount = 0 is not working: causes error code: -26, error message : 64 : dust
             mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG)); // txfee 1, txfee 2 - for miners
             std::cerr << "HeirClaim() adding markeraddr=" << markeraddr << '\n'; */
            
            // get address of 1of2 cond
            char coinaddr[64];
            Helper::GetCoinsOrTokensCCaddress1of2(coinaddr, ownerPubkey, heirPubkey);
            
            // retrieve priv key addresses for FinalizeCCtx:
            uint8_t myprivkey[32];
            Myprivkey(myprivkey);
            
            // set pubkeys for finding 1of2 cc in FinalizeCCtx to sign it:
            Helper::CCaddrCoinsOrTokens1of2set(cp, ownerPubkey, heirPubkey, coinaddr);
            
            // add 1of2 vout validation pubkeys (this is for tokens):
            std::vector<CPubKey> voutTokenPubkeys;
            voutTokenPubkeys.push_back(ownerPubkey);
            voutTokenPubkeys.push_back(heirPubkey);
            
            // add opreturn 'C' and sign tx:				  // this txfee will be ignored
            std::string rawhextx = FinalizeCCTx(0, cp, mtx, myPubkey, txfee,
                                                Helper::makeClaimOpRet(tokenid, voutTokenPubkeys, fundingtxid, (myPubkey == heirPubkey) ? 1 : hasHeirSpendingBegun)); // forward isHeirSpending to the next latest tx
            
            memset(myprivkey,0,sizeof(myprivkey));
            if (!rawhextx.empty()) {
                result.push_back(Pair("result", "success"));
                result.push_back(Pair("hex", rawhextx));
            }
            else {
                std::cerr << "HeirAdd error in FinalizeCCtx" << std::endl;
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "sign error"));
            }
            
        } else {
            fprintf(stderr, "HeirClaim() cant find Heir CC inputs\n");
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "can't find heir CC funding"));
        }
    } else {
        fprintf(stderr, "HeirClaim() cant find sufficient user inputs for tx fee\n");
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "can't find sufficient user inputs to pay transaction fee"));
    }
    
    
    return result;
}

UniValue HeirClaimCaller(uint256 fundingtxid, int64_t txfee, std::string strAmount) {
    
    CPubKey ownerPubkey, heirPubkey;
    int64_t inactivityTimeSec;
    
    uint256 latesttxid, tokenid = zeroid;
    uint8_t funcId;
    std::string heirName, memo;
    uint8_t hasHeirSpendingBegun = 0;
    
    // find latest tx to see if it is a token or coin:
    if ((latesttxid = FindLatestFundingTx(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, memo, hasHeirSpendingBegun)) != zeroid) 
    {
		if (tokenid == zeroid) 
        {
            int64_t amount = 0;
            if (!ParseFixedPoint(strAmount, 8, &amount) || amount <= 0) {  // using ParseFixedPoint instead atof to avoid round errors
                UniValue result(UniValue::VOBJ);
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "invalid amount"));
                return result;
            }
			return _HeirClaim<CoinHelper>(fundingtxid, txfee, amount, latesttxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, hasHeirSpendingBegun);
		}
		else {
			int64_t amount = atoll(strAmount.c_str());
			if (amount <= 0) {
				UniValue result(UniValue::VOBJ);
				result.push_back(Pair("result", "error"));
				result.push_back(Pair("error", "invalid amount"));
				return result;
			}
			return _HeirClaim<TokenHelper>(fundingtxid, txfee, amount, latesttxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, hasHeirSpendingBegun);
		}
        
    }
    else {
        UniValue result(UniValue::VOBJ);
        
        fprintf(stderr, "HeirClaim() can't find any heir CC funding tx's\n");
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "can't find any heir CC funding transactions"));
        return result;
    }
}


/**
 * heirinfo rpc call implementation
 * returns some information about heir CC contract plan by a handle of initial fundingtxid:
 * plan name, owner and heir pubkeys, funds deposited and available, flag if spending is enabled for the heir
 * @return heir info data
 */
UniValue HeirInfo(uint256 fundingtxid)
{
    UniValue result(UniValue::VOBJ);
    
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction fundingtx;
    uint256 hashBlock;
    const bool allowSlow = false;
    
    //char markeraddr[64];
    //CCtxidaddr(markeraddr, fundingtxid);
    //SetCCunspents(unspentOutputs, markeraddr,true);
    
    // get initial funding tx and set it as initial lasttx:
    if (myGetTransaction(fundingtxid, fundingtx, hashBlock) && fundingtx.vout.size()) {
        
        CPubKey ownerPubkey, heirPubkey;
        uint256 dummyTokenid, tokenid = zeroid;  // important to clear tokenid
        std::string heirName, memo;
        int64_t inactivityTimeSec;
        const bool noLogging = false;
        uint8_t funcId;
        
        /*CScript opret = fundingtx.vout.size() > 0 ? fundingtx.vout[fundingtx.vout.size() - 1].scriptPubKey : CScript();
         uint8_t funcId = DecodeHeirEitherOpRet(opret, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, true);
         if (funcId == 0) {
         std::cerr << "HeirInfo() this fundingtx is incorrect" << std::endl;
         result.push_back(Pair("result", "error"));
         result.push_back(Pair("error", "initial tx F not found"));
         return result;
         }*/
        
        struct CCcontract_info *cp, C;
        cp = CCinit(&C, EVAL_HEIR);
        
        uint8_t hasHeirSpendingBegun = 0;
        
        uint256 latestFundingTxid = FindLatestFundingTx(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, memo, hasHeirSpendingBegun);
        
        if (latestFundingTxid != zeroid) {
            int32_t numblocks;
            uint64_t durationSec = 0;
            
            //std::cerr << "HeirInfo() latesttxid=" << latestFundingTxid.GetHex() << '\n';
            
            std::ostringstream stream;
            std::string msg;
            
			//sleep(10);

            result.push_back(Pair("fundingtxid", fundingtxid.GetHex()));
            result.push_back(Pair("name", heirName.c_str()));
            
            if (tokenid != zeroid) {	// tokens
                stream << tokenid.GetHex();
                msg = "tokenid";
                result.push_back(Pair(msg, stream.str().c_str()));
                stream.str("");
                stream.clear();
            }
            
            char hexbuf[67];
            stream << pubkey33_str(hexbuf, (uint8_t*)ownerPubkey.begin());
            result.push_back(Pair("owner", stream.str().c_str()));
            stream.str("");
            stream.clear();
            
            stream << pubkey33_str(hexbuf, (uint8_t*)heirPubkey.begin());
            result.push_back(Pair("heir", stream.str().c_str()));
            stream.str("");
            stream.clear();
            
            int64_t total;
            if (tokenid == zeroid)
                total = LifetimeHeirContractFunds<CoinHelper>(cp, fundingtxid, ownerPubkey, heirPubkey);
            else
                total = LifetimeHeirContractFunds<TokenHelper>(cp, fundingtxid, ownerPubkey, heirPubkey);
            
			msg = "type";
			if (tokenid == zeroid) {
				stream << "coins";
			}
			else {
				stream << "tokens";
			}
			result.push_back(Pair(msg, stream.str().c_str()));
			stream.str("");
			stream.clear();

			msg = "lifetime";
            if (tokenid == zeroid) {
                stream << std::fixed << std::setprecision(8) << (double)total / COIN;
            }
            else	{
                stream << total;
            }
            result.push_back(Pair(msg, stream.str().c_str()));
            stream.str("");
            stream.clear();
            
            int64_t inputs;
            if (tokenid == zeroid)
                inputs = Add1of2AddressInputs<CoinHelper>(cp, fundingtxid, mtx, ownerPubkey, heirPubkey, 0, 60); //NOTE: amount = 0 means all unspent inputs
            else
                inputs = Add1of2AddressInputs<TokenHelper>(cp, fundingtxid, mtx, ownerPubkey, heirPubkey, 0, 60);
            
			msg = "available";
            if (tokenid == zeroid) {
                stream << std::fixed << std::setprecision(8) << (double)inputs / COIN;
            }
            else	{
                stream << inputs;
            }
            result.push_back(Pair(msg, stream.str().c_str()));
            stream.str("");
            stream.clear();
            
            if (tokenid != zeroid) {
                int64_t ownerInputs = TokenHelper::addOwnerInputs(tokenid, mtx, ownerPubkey, 0, (int32_t)64);
                stream << ownerInputs;
                msg = "OwnerRemainderTokens";
                result.push_back(Pair(msg, stream.str().c_str()));
                stream.str("");
                stream.clear();
            }
            
            stream << inactivityTimeSec;
            result.push_back(Pair("InactivityTimeSetting", stream.str().c_str()));
            stream.str("");
            stream.clear();
            
            if (!hasHeirSpendingBegun) { // we do not need find duration if the spending already has begun
                durationSec = CCduration(numblocks, latestFundingTxid);
                std::cerr << "HeirInfo() duration (sec)=" << durationSec << " inactivityTime (sec)=" << inactivityTimeSec << " numblocks=" << numblocks << '\n';
            }
            
            stream << std::boolalpha << (hasHeirSpendingBegun || durationSec > inactivityTimeSec);
            result.push_back(Pair("IsHeirSpendingAllowed", stream.str().c_str()));
            stream.str("");
            stream.clear();

			// adding owner current inactivity time:
			if (!hasHeirSpendingBegun && durationSec <= inactivityTimeSec) {
				stream << durationSec;
				result.push_back(Pair("InactivityTime", stream.str().c_str()));
				stream.str("");
				stream.clear();
			}

			result.push_back(Pair("memo", memo.c_str()));

            result.push_back(Pair("result", "success"));
        }
        else {
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "could not find heir cc plan for this txid"));
        }
    }
    else {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "could not find heir cc plan for this txid (no initial tx)"));
    }
    return (result);
}

/**
 * heirlist rpc call implementation
 * @return list of heir plan handles (fundingtxid)
 */

void _HeirList(struct CCcontract_info *cp, UniValue &result)
{
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>> unspentOutputs;
    char markeraddr[64];

	GetCCaddress(cp, markeraddr, GetUnspendable(cp, NULL));
    SetCCunspents(unspentOutputs, markeraddr,true);
    
    //std::cerr << "HeirList() finding heir marker from unspendable addr=" << markeraddr << " unspentOutputs.size()=" << unspentOutputs.size() << '\n';
    
    // TODO: move marker to special cc addr to prevent checking all tokens
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>>::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++) {
        uint256 hashBlock;
        uint256 txid = it->first.txhash;
        uint256 tokenid;
        int32_t vout = (int32_t)it->first.index;
        
        //std::cerr << "HeirList() checking txid=" << txid.GetHex() << " vout=" << vout << '\n';
        
        CTransaction fundingtx;
        if (myGetTransaction(txid, fundingtx, hashBlock)) {
            CPubKey ownerPubkey, heirPubkey;
            std::string heirName, memo;
            int64_t inactivityTimeSec;
            const bool noLogging = true;
            uint256 tokenid;
            
            CScript opret = (fundingtx.vout.size() > 0) ? fundingtx.vout[fundingtx.vout.size() - 1].scriptPubKey : CScript();
            uint8_t funcId = DecodeHeirEitherOpRet(opret, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, memo, true);
            
            // note: if it is not Heir token funcId would be equal to 0
            if (funcId == 'F') {
                //result.push_back(Pair("fundingtxid kind name", txid.GetHex() + std::string(" ") + (typeid(Helper) == typeid(TokenHelper) ? std::string("token") : std::string("coin")) + std::string(" ") + heirName));
                result.push_back( txid.GetHex() );
            }
            else {
                std::cerr << "HeirList() this is not the initial F transaction=" << txid.GetHex() << std::endl;
            }
        }
        else {
            std::cerr << "HeirList() could not load transaction=" << txid.GetHex() << std::endl;
        }
    }
}


UniValue HeirList()
{
    UniValue result(UniValue::VARR);
    //result.push_back(Pair("result", "success"));
	//result.push_back(Pair("name", "Heir List"));
    
    struct CCcontract_info *cpHeir, heirC; 
    
    cpHeir = CCinit(&heirC, EVAL_HEIR);
    _HeirList(cpHeir, result);
    
    return result;
}
