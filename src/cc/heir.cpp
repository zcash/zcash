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
#include "CCtokens.h"

#include "heir_validate.h"

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
	CNullValidator<Helper>			nullValidator(cp);

	switch (funcId) {
	case 'F': // fund tokens
		// vin validation plan:
		vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&normalInputIdentifier, &nullValidator);			// txfee vin
		vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&ccInputIdentifier, &ownerCCaddrValidator);		// check cc owner addr

		// vout validation plan:
		voutPlan.pushValidators<CValidatorBase>(0, &cc1of2ValidatorThis);												// check 1of2 addr funding
		// do not check change at this time
		// no checking for opret yet
		break;

	case 'A': // add tokens
		// vin validation plan:
		vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&normalInputIdentifier, &nullValidator);		// txfee vin 
		vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&ccInputIdentifier, &ownerCCaddrValidator);   // check cc owner addr

		// vout validation plan:
		voutPlan.pushValidators<CValidatorBase>(0, &cc1of2ValidatorThis);												// check 1of2 addr funding
																														// do not check change at this time
		voutPlan.pushValidators<CValidatorBase>(numvouts - 1, &opRetValidator);											// opreturn check, NOTE: only for C or A:
		break;

	case 'C':
		// vin validation plan:
		vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&normalInputIdentifier, &nullValidator);			// txfee vin
		vinPlan.pushValidators<CValidatorBase>((CInputIdentifierBase*)&ccInputIdentifier, &cc1of2ValidatorThis);		// cc1of2 funding addr
		
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

    uint8_t funcId;
    uint256 fundingTxidInOpret = zeroid, latestTxid = zeroid, dummyTokenid, tokenid = zeroid;
    
	CScript fundingTxOpRetScript;
	uint8_t hasHeirSpendingBegun = 0, dummyhasHeirSpendingBegun;

	int32_t heirType = NOT_HEIR;
	funcId = DecodeHeirOpRet<CoinHelper>(tx.vout[numvouts - 1].scriptPubKey, dummyTokenid, fundingTxidInOpret, dummyhasHeirSpendingBegun, true);
	if(funcId != 0)
		heirType = HEIR_COINS;
	else  {
		funcId = DecodeHeirOpRet<TokenHelper>(tx.vout[numvouts - 1].scriptPubKey, tokenid, fundingTxidInOpret, dummyhasHeirSpendingBegun, false);
		if (funcId != 0)
			heirType = HEIR_TOKENS;
	}
		
	if (heirType == NOT_HEIR)
        return eval->Invalid("invalid opreturn format");

    if (funcId != 'F') {
        if (fundingTxidInOpret == zeroid) {
            return eval->Invalid("invalid tx opreturn format: no fundingtxid present");
        }
		if (heirType == HEIR_COINS)
			latestTxid = FindLatestFundingTx<CoinHelper>(fundingTxidInOpret, dummyTokenid, fundingTxOpRetScript, hasHeirSpendingBegun);
		else
			latestTxid = FindLatestFundingTx<TokenHelper>(fundingTxidInOpret, tokenid, fundingTxOpRetScript, hasHeirSpendingBegun);

		if (latestTxid == zeroid) {
            return eval->Invalid("invalid heir transaction: no funding tx found");
        }
	}
	else {
		fundingTxOpRetScript = tx.vout[numvouts - 1].scriptPubKey;
	}

	std::cerr << "HeirValidate funcid=" << (char)funcId << std::endl;

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
		if (heirType == HEIR_TOKENS)
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
		if (heirType == HEIR_TOKENS)
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
		if (heirType == HEIR_TOKENS)
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
int64_t IsHeirFundingVout(struct CCcontract_info* cp, const CTransaction& tx, int32_t voutIndex, CPubKey ownerPubkey, CPubKey heirPubkey)
{
	char destaddr[65], heirFundingAddr[65];

	GetCCaddress1of2(cp, heirFundingAddr, ownerPubkey, heirPubkey);
	if (tx.vout[voutIndex].scriptPubKey.IsPayToCryptoCondition() != 0) {
		// NOTE: dimxy it was unsafe 'Getscriptaddress(destaddr,tx.vout[voutIndex].scriptPubKey) > 0' here:
		if (Getscriptaddress(destaddr, tx.vout[voutIndex].scriptPubKey) && strcmp(destaddr, heirFundingAddr) == 0)
			return (tx.vout[voutIndex].nValue);
		else
			std::cerr << "IsHeirFundingVout() heirFundingAddr=" << heirFundingAddr << " not equal to destaddr=" << destaddr << std::endl;
	}
	return (0);
}

// not used
bool HeirExactAmounts(struct CCcontract_info* cp, Eval* eval, const CTransaction& tx, int32_t minage, uint64_t txfee)
{
	static uint256 zerohash;
	CTransaction vinTx;
	uint256 hashBlock, activehash;
	int32_t i, numvins, numvouts;
	int64_t inputs = 0, outputs = 0, assetoshis;
	numvins = tx.vin.size();
	numvouts = tx.vout.size();
	for (i = 0; i < numvins; i++) {
		//fprintf(stderr,"HeirExactAmounts() vini.%d\n",i);
		if ((*cp->ismyvin)(tx.vin[i].scriptSig) != 0) {
			//fprintf(stderr,"HeirExactAmounts() vini.%d check mempool\n",i);
			if (eval->GetTxUnconfirmed(tx.vin[i].prevout.hash, vinTx, hashBlock) == 0)
				return eval->Invalid("cant find vinTx");
			else {
				//fprintf(stderr,"HeirExactAmounts() vini.%d check hash and vout\n",i);
				if (hashBlock == zerohash)
					return eval->Invalid("cant Heir from mempool");
				////if ( (assetoshis= IsHeirCCvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
				////    inputs += assetoshis;
			}
		}
	}
	for (i = 0; i < numvouts; i++) {
		//fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
		////if ( (assetoshis= IsHeirvout(cp,tx,i)) != 0 )
		////    outputs += assetoshis;
	}
	if (inputs != outputs + txfee) {
		fprintf(stderr, "HeirExactAmounts() inputs %llu vs outputs %llu\n", (long long)inputs, (long long)outputs);
		return eval->Invalid("mismatched inputs != outputs + txfee");
	}
	else
		return (true);
}

// makes coin initial tx opret
CScript EncodeHeirCreateOpRet(uint8_t funcid, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t inactivityTimeSec, std::string heirName)
{
	uint8_t evalcode = EVAL_HEIR;

	return CScript() << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << ownerPubkey << heirPubkey << inactivityTimeSec << heirName);
}

// makes coin additional tx opret
CScript EncodeHeirOpRet(uint8_t funcid,  uint256 fundingtxid, uint8_t hasHeirSpendingBegun)
{
	uint8_t evalcode = EVAL_HEIR;

	fundingtxid = revuint256(fundingtxid);
	return CScript() << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << fundingtxid << hasHeirSpendingBegun);
}
// makes opret for tokens while they are inside Heir contract address space - initial funding
CScript EncodeHeirTokensCreateOpRet(uint8_t heirFuncId, uint256 tokenid, std::vector<CPubKey> voutPubkeys, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t inactivityTimeSec, std::string hearName)
{
	uint8_t evalcode = EVAL_HEIR;
	uint8_t ccType = 0;
	if (voutPubkeys.size() >= 1 && voutPubkeys.size() <= 2)
		ccType = voutPubkeys.size();

	tokenid = revuint256(tokenid);
	return CScript() << OP_RETURN << 
		E_MARSHAL(ss << evalcode << (uint8_t)'t' << tokenid << ccType;					\
			if (ccType >= 1) ss << voutPubkeys[0];										\
			if (ccType == 2) ss << voutPubkeys[1];										\
			ss << heirFuncId << ownerPubkey << heirPubkey << inactivityTimeSec << hearName);
}
// makes opret for tokens while they are inside Heir contract address space - additional funding
CScript EncodeHeirTokensOpRet(uint8_t heirFuncId, uint256 tokenid, std::vector<CPubKey> voutPubkeys, uint256 fundingtxid, uint8_t hasHeirSpendingBegun)
{
	uint8_t evalcode = EVAL_HEIR;
	uint8_t ccType = 0;
	if (voutPubkeys.size() >= 1 && voutPubkeys.size() <= 2)
		ccType = voutPubkeys.size();

	tokenid = revuint256(tokenid);   // for visualization in debug logs
	fundingtxid = revuint256(fundingtxid);
	return CScript() << OP_RETURN << 
		E_MARSHAL(ss << evalcode << (uint8_t)'t' << tokenid << ccType;					\
			if (ccType >= 1) ss << voutPubkeys[0];										\
			if (ccType == 2) ss << voutPubkeys[1];										\
			ss << heirFuncId << fundingtxid << hasHeirSpendingBegun);
}

// helper for decode heir opret payload
// NOTE: Heir for coins has the same opret as Heir for tokens
uint8_t _UnmarshalOpret(std::vector<uint8_t> vopretExtra, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, uint256& fundingTxidInOpret, uint8_t &hasHeirSpendingBegun) {
	uint8_t heirFuncId = 0;
	hasHeirSpendingBegun = 0;

	bool result = E_UNMARSHAL(vopretExtra, { ss >> heirFuncId;							\
		if( heirFuncId == 'F') {														\
			ss >> ownerPubkey; ss >> heirPubkey; ss >> inactivityTime; ss >> heirName;  \
		} else {																		\
			ss >> fundingTxidInOpret >> hasHeirSpendingBegun;							\
		}																				\
	});

	if (!result /*|| assetFuncId != 't' -- any tx is ok*/)
		return (uint8_t)0;

	return heirFuncId;
}
/**
* decode opret vout for Heir contract
*/
template <class Helper> uint8_t _DecodeHeirOpRet(CScript scriptPubKey, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, uint256& fundingTxidInOpret, uint8_t &hasHeirSpendingBegun, bool noLogging)
{

	std::vector<uint8_t> vopretExtra;
	uint8_t evalCodeInOpret = 0;
	uint256 dummyTokenid;
	std::vector<CPubKey> voutPubkeysDummy;

	fundingTxidInOpret = zeroid; //to init
	tokenid = zeroid;


	if (typeid(Helper) == typeid(TokenHelper)) {  // if caller thinks it is a token

		// First - decode token opret:
		uint8_t tokenFuncId = DecodeTokenOpRet(scriptPubKey, evalCodeInOpret, tokenid, voutPubkeysDummy, vopretExtra);
		if (tokenFuncId == 0) {
			if (!noLogging) std::cerr << "DecodeHeirOpRet() warning: not heir token opret, tokenFuncId=" << (int)tokenFuncId << std::endl;
			return (uint8_t)0;
		}
	}
	else {
		std::vector<uint8_t> vopret;

		GetOpReturnData(scriptPubKey, vopret);	
		if (vopret.size() == 0) {
			if (!noLogging) std::cerr << "DecodeHeirOpRet() warning: empty opret" << std::endl;
			return (uint8_t)0;
		}
		evalCodeInOpret = vopret.begin()[0];
		vopretExtra = std::vector<uint8_t>( vopret.begin()+1, vopret.end() );  // vopretExtra = vopret + 1, get it for futher parsing
	}

	if (vopretExtra.size() > 1 && evalCodeInOpret == EVAL_HEIR) {
		// NOTE: it unmarshals for all F, A and C
		uint8_t heirFuncId = _UnmarshalOpret(vopretExtra, ownerPubkey, heirPubkey, inactivityTime, heirName, fundingTxidInOpret, hasHeirSpendingBegun);
				
		/*std::cerr << "DecodeHeirOpRet()"  
			<< " heirFuncId=" << (char)(heirFuncId ? heirFuncId : ' ')
			<< " ownerPubkey=" << HexStr(ownerPubkey)
			<< " heirPubkey=" << HexStr(heirPubkey)
			<< " heirName=" << heirName << " inactivityTime=" << inactivityTime 
			<< " hasHeirSpendingBegun=" << (int)hasHeirSpendingBegun << std::endl;*/
		

		//if (e == EVAL_HEIR && IS_CHARINSTR(funcId, "FAC"))
		if (Helper::isMyFuncId(heirFuncId)) {
			fundingTxidInOpret = revuint256(fundingTxidInOpret);
			return heirFuncId;
		}
		else	{
			if(!noLogging) std::cerr << "DecodeHeirOpRet() error: unexpected opret, heirFuncId=" << (char)(heirFuncId ? heirFuncId : ' ') << std::endl;
		}
	}
	else {
		if (!noLogging) std::cerr << "DecodeHeirOpRet() error: not a heir opret, vopretExtra.size() == 0 or not EVAL_HEIR evalcode=" << (int)evalCodeInOpret << std::endl;
	}
	return (uint8_t)0;
}

/**
* overload for 'F' opret
*/
template <class Helper> uint8_t DecodeHeirOpRet(CScript scriptPubKey, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, bool noLogging)
{
	uint256 dummytxid;
	uint8_t dummyhasHeirSpendingBegun;

	return _DecodeHeirOpRet<Helper>(scriptPubKey, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName, dummytxid, dummyhasHeirSpendingBegun, noLogging);
}

/**
* overload for A, C oprets and AddHeirContractInputs
*/
template <class Helper> uint8_t DecodeHeirOpRet(CScript scriptPubKey, uint256 &tokenid, uint256& fundingtxidInOpret, uint8_t &hasHeirSpendingBegun, bool noLogging)
{
	CPubKey dummyOwnerPubkey, dummyHeirPubkey;
	int64_t dummyInactivityTime;
	std::string dummyHeirName;

	return _DecodeHeirOpRet<Helper>(scriptPubKey, tokenid, dummyOwnerPubkey, dummyHeirPubkey, dummyInactivityTime, dummyHeirName, fundingtxidInOpret, hasHeirSpendingBegun, noLogging);
}



/**
 * find the latest funding tx: it may be the first F tx or one of A or C tx's 
 * Note: this function is also called from validation code (use non-locking calls) 
 */
template <class Helper> uint256 _FindLatestFundingTx(uint256 fundingtxid, uint8_t& funcId, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, CScript& fundingOpretScript, uint8_t &hasHeirSpendingBegun)
{
	CTransaction fundingtx;
	uint256 hashBlock;
    const bool allowSlow = false;

    //char markeraddr[64];
    //CCtxidaddr(markeraddr, fundingtxid);
    //SetCCunspents(unspentOutputs, markeraddr);

	hasHeirSpendingBegun = 0; 
	funcId = 0; 

    // get initial funding tx and set it as initial lasttx:
    if (myGetTransaction(fundingtxid, fundingtx, hashBlock) && fundingtx.vout.size()) {
        uint256 dummytxid;

        // set ownerPubkey and heirPubkey:
        if ((funcId = DecodeHeirOpRet<Helper>(fundingtx.vout[fundingtx.vout.size() - 1].scriptPubKey, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName)) != 0) {
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

    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>> unspentOutputs;
    struct CCcontract_info *cp, C;
    cp = CCinit(&C, Helper::getMyEval());
    char coinaddr[64];
    GetCCaddress1of2(cp, coinaddr, ownerPubkey, heirPubkey); // get the address of cryptocondition '1 of 2 pubkeys'
                                                             
    SetCCunspents(unspentOutputs, coinaddr);				 // get vector with tx's with unspent vouts of 1of2pubkey address:
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
        uint256 fundingTxidInOpret;

		//NOTE: maybe called from validation code:
		if (myGetTransaction(txid, regtx, hash)) {
			//std::cerr << "FindLatestFundingTx() found tx for txid=" << txid.GetHex() << " blockHeight=" << blockHeight << " maxBlockHeight=" << maxBlockHeight << '\n';

			/*{	// debug code:
				uint256 debAssetid;
				uint8_t debhasHeirSpendingBegun;
				uint8_t debfuncid = DecodeHeirOpRet<Helper>(regtx.vout[regtx.vout.size() - 1].scriptPubKey, debAssetid, fundingTxidInOpret, debhasHeirSpendingBegun, true);

				std::cerr << "FindLatestFundingTx() regtx.vout.size()=" << regtx.vout.size() << " funcId=" << (char)(debfuncid ? debfuncid : ' ') 
					<< " tokenid=" << debAssetid.GetHex() << " fundingtxidInOpret=" << fundingTxidInOpret.GetHex() << " debhasHeirSpendingBegun=" << (int)debhasHeirSpendingBegun << std::endl;
			}*/

			uint256 dummyTokenid;  // not to contaminate the tokenid from the params!
			uint8_t tmpFuncId;			
			uint8_t tmphasHeirSpendingBegun;

            if (regtx.vout.size() > 0 && 
				(tmpFuncId = DecodeHeirOpRet<Helper>(regtx.vout[regtx.vout.size() - 1].scriptPubKey, dummyTokenid, fundingTxidInOpret, tmphasHeirSpendingBegun, true)) != 0 &&
				fundingtxid == fundingTxidInOpret) {

                if (blockHeight > maxBlockHeight) {
                    maxBlockHeight = blockHeight;
                    latesttxid = txid;
					funcId = tmpFuncId;
					hasHeirSpendingBegun = tmphasHeirSpendingBegun;

                    //std::cerr << "FindLatestFundingTx() txid=" << latesttxid.GetHex() << " at blockHeight=" << maxBlockHeight 
					//	<< " opreturn type=" << (char)(funcId ? funcId : ' ') << " hasHeirSpendingBegun=" << (int)hasHeirSpendingBegun << " - set as current lasttxid" << '\n';
                }
            }
        }
    }

    return latesttxid;
}

// overload for validation code
template <class Helper> uint256 FindLatestFundingTx(uint256 fundingtxid, uint256 &tokenid, CScript& opRetScript, uint8_t &hasHeirSpendingBegun)
{
    uint8_t funcId;
    CPubKey ownerPubkey;
    CPubKey heirPubkey;
    int64_t inactivityTime;
    std::string heirName;

    return _FindLatestFundingTx<Helper>(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName, opRetScript, hasHeirSpendingBegun);
}

// overload for transaction creation code
template <class Helper> uint256 FindLatestFundingTx(uint256 fundingtxid, uint8_t& funcId, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, uint8_t &hasHeirSpendingBegun)
{
    CScript opRetScript;

    return _FindLatestFundingTx<Helper>(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName, opRetScript, hasHeirSpendingBegun);
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
    GetCCaddress1of2(cp, coinaddr, ownerPubkey, heirPubkey); // get address of cryptocondition '1 pubkey of 2 pubkeys'
    SetCCunspents(unspentOutputs, coinaddr);

    //   char markeraddr[64];
    //   CCtxidaddr(markeraddr, fundingtxid);
    //   SetCCunspents(unspentOutputs, markeraddr);
    // std::cerr << "Add1of2AddressInputs() using 1of2addr=" << coinaddr << " unspentOutputs.size()=" << unspentOutputs.size() << std::endl;

    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>>::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++) {
        uint256 txid = it->first.txhash;
        uint256 hashBlock;
        int32_t voutIndex = (int32_t)it->first.index;
        // no need to prevent dup
        // dimxy: maybe it is good to put tx's in cache?
        if (GetTransaction(txid, heirtx, hashBlock, false) != 0) {
			uint256 tokenid;
            uint256 fundingTxidInOpret;
			uint8_t dummyhasHeirSpendingBegun;
			
			uint8_t funcId = DecodeHeirOpRet<Helper>(heirtx.vout[heirtx.vout.size() - 1].scriptPubKey, tokenid, fundingTxidInOpret, dummyhasHeirSpendingBegun, true);
            if ((txid == fundingtxid || fundingTxidInOpret == fundingtxid) && 
				funcId != 0 &&
				Helper::isMyFuncId(funcId) &&     
				// (typeid(Helper) == typeid(TokenHelper) && IsHeirvout(true, cp, nullptr, tokenid, vintx, voutIndex) > 0)  && // deep validation for tokens - not used anymore
                (voutValue = IsHeirFundingVout(cp, heirtx, voutIndex, ownerPubkey, heirPubkey)) > 0 &&
                !myIsutxo_spentinmempool(txid, voutIndex)) 
			{
                //std::cerr << "Add1of2AddressInputs() voutValue=" << voutValue << " satoshis=" << it->second.satoshis << '\n';
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
    GetCCaddress1of2(cp, coinaddr, ownerPubkey, heirPubkey); // get the address of cryptocondition '1 of 2 pubkeys'

    std::vector<std::pair<CAddressIndexKey, CAmount>> addressIndexes;
    SetCCtxids(addressIndexes, coinaddr);

    //fprintf(stderr,"LifetimeHeirContractFunds() scan lifetime of %s\n",coinaddr);
    int64_t total = 0;
    for (std::vector<std::pair<CAddressIndexKey, CAmount>>::const_iterator it = addressIndexes.begin(); it != addressIndexes.end(); it++) {
        uint256 hashBlock;
        uint256 txid = it->first.txhash;
        CTransaction tx;

        if (GetTransaction(txid, tx, hashBlock, false) && tx.vout.size() > 0) {
            uint8_t funcId;
			uint256 tokenid;
            uint256 fundingTxidInOpret;
            const int32_t ivout = 0;
			uint8_t dummyhasHeirSpendingBegun;

			funcId = DecodeHeirOpRet<Helper>(tx.vout[tx.vout.size() - 1].scriptPubKey, tokenid, fundingTxidInOpret, dummyhasHeirSpendingBegun, true);

            //std::cerr << "LifetimeHeirContractFunds() found tx=" << txid.GetHex() << " vout[0].nValue=" << subtx.vout[ccVoutIdx].nValue << " opreturn=" << (char)funcId << '\n';

            if (funcId != 0 && (txid == fundingtxid || fundingTxidInOpret == fundingtxid) && Helper::isMyFuncId(funcId) && !Helper::isSpendingTx(funcId)
                /* && !myIsutxo_spentinmempool(txid, ccVoutIdx) */) // include also tx in mempool
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
template <typename Helper> std::string HeirFund(uint64_t txfee, int64_t amount, std::string heirName, CPubKey heirPubkey, int64_t inactivityTimeSec, uint256 tokenid)
{
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	struct CCcontract_info *cp, C;

    cp = CCinit(&C, Helper::getMyEval());
    if (txfee == 0)
        txfee = 10000;

    //std::cerr << "HeirFund() amount=" << amount << " txfee=" << txfee << " heirPubkey IsValid()=" << heirPubkey.IsValid() << " inactivityTime(sec)=" << inactivityTimeSec << " tokenid=" << tokenid.GetHex() << std::endl;

	if (!heirPubkey.IsValid()) {
		std::cerr << "HeirFund() heirPubkey is not valid!" << std::endl;
		return std::string("");
	}

    CPubKey myPubkey = pubkey2pk(Mypubkey());

	if (AddNormalinputs(mtx, myPubkey, txfee, 3) > 0) { // txfee for miners
		int64_t inputs, change;

		if ((inputs=Helper::addOwnerInputs(cp, tokenid, mtx, myPubkey, amount, (int32_t)64)) > 0) { // 2 x txfee: 1st for marker vout, 2nd to miners
			//mtx.vout.push_back(MakeCC1vout(EVAL_HEIR,amount,HeirCCpk));
			mtx.vout.push_back(MakeCC1of2vout(Helper::getMyEval(), amount, myPubkey, heirPubkey)); // add cryptocondition to spend amount for either pk

			// add a marker for finding all plans in HeirList()
			CPubKey HeirContractPubKey = GetUnspendable(cp, 0);
			mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(HeirContractPubKey)) << OP_CHECKSIG));   // TODO: do we need this marker?

		    // calc and add change vout:
			if (inputs > amount)
				change = (inputs - amount); //  -txfee <-- txfee pays user

			//std::cerr << "HeirFund() inputs=" << inputs << " amount=" << amount << " txfee=" << txfee << " change=" << change << '\n';

			if (change != 0) {	// vout[1]
				mtx.vout.push_back(Helper::makeUserVout(change, myPubkey));
			}

			// add 1of2 vout validation pubkeys:
			std::vector<CPubKey> voutTokenPubkeys;
			voutTokenPubkeys.push_back(myPubkey);
			voutTokenPubkeys.push_back(heirPubkey);

			// add change for txfee and opreturn vouts and sign tx:
			return (FinalizeCCTx(0, cp, mtx, myPubkey, txfee,
				// CScript() << OP_RETURN << E_MARSHAL(ss << (uint8_t)EVAL_HEIR << (uint8_t)'F' << myPubkey << heirPubkey << inactivityTimeSec << heirName)));
				Helper::makeCreateOpRet(tokenid, voutTokenPubkeys, myPubkey, heirPubkey, inactivityTimeSec, heirName)));
		}
		else  // TODO: need result return unification with heiradd and claim
			std::cerr << "HeirFund() could not find owner inputs" << std::endl;

	}
	else
		std::cerr << "HeirFund() could not find normal inputs" << std::endl;
    return std::string("");
}

// if no these callers - it could not link 
std::string HeirFundCoinCaller(uint64_t txfee, int64_t funds, std::string heirName, CPubKey heirPubkey, int64_t inactivityTimeSec, uint256 tokenid){
	return HeirFund<CoinHelper>(txfee, funds, heirName, heirPubkey, inactivityTimeSec,  tokenid);
}

std::string HeirFundTokenCaller(uint64_t txfee, int64_t funds, std::string heirName, CPubKey heirPubkey, int64_t inactivityTimeSec, uint256 tokenid) {
	return HeirFund<TokenHelper>(txfee, funds, heirName, heirPubkey, inactivityTimeSec, tokenid);
}

/**
 * heiradd rpc call implementation
 * creates tx to add more funds to cryptocondition address for spending by either funds' owner or heir
 * @return result object with raw tx or error text
 */
template <class Helper> UniValue HeirAdd(uint256 fundingtxid, uint64_t txfee, int64_t amount)
{
    UniValue result(UniValue::VOBJ); //, a(UniValue::VARR);
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey ownerPubkey, heirPubkey;
    int64_t inputs, CCchange = 0;
    int64_t inactivityTimeSec;
    struct CCcontract_info *cp, C;
    std::string rawhex;
    uint256 lasttxid, tokenid;
    std::string heirName;
    uint8_t funcId;
	uint8_t hasHeirSpendingBegun = 0;

    cp = CCinit(&C, Helper::getMyEval());

    if (txfee == 0)
        txfee = 10000;

    if ((lasttxid = FindLatestFundingTx<Helper>(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, hasHeirSpendingBegun)) != zeroid) {
        int32_t numblocks;

        CPubKey myPubkey = pubkey2pk(Mypubkey());

        // check if it is the owner
        if (myPubkey != ownerPubkey) {
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "adding funds is only allowed for the owner of this contract"));
            return result;
        }

		if (AddNormalinputs(mtx, myPubkey, txfee, 3) > 0) { // txfee for miners

			int64_t inputs, change;

			if ((inputs = Helper::addOwnerInputs(cp, tokenid, mtx, myPubkey, amount, 64)) > 0) { // TODO: why 64 max inputs?

				// we do not use markers anymore - storing data in opreturn is better
					// add marker vout:
					/* char markeraddr[64];
					CPubKey markerpubkey = CCtxidaddr(markeraddr, fundingtxid);
					mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG)); // txfee 1, txfee 2 - for miners
					std::cerr << "HeirAdd() adding markeraddr=" << markeraddr << '\n'; */

				// add cryptocondition to spend this funded amount for either pk
				mtx.vout.push_back(MakeCC1of2vout(Helper::getMyEval(), amount, ownerPubkey, heirPubkey)); // using always pubkeys from OP_RETURN in order to not mixing them up!

				if (inputs > amount)
					change = (inputs - amount); //  -txfee <-- txfee pays user

				//std::cerr << "HeirAdd() inputs=" << inputs << " amount=" << amount << " txfee=" << txfee << " change=" << change << '\n';

				if (change != 0) {																		// vout[1]
					mtx.vout.push_back(Helper::makeUserVout(change, myPubkey));
				}

				// add 1of2 vout validation pubkeys:
				std::vector<CPubKey> voutTokenPubkeys;
				voutTokenPubkeys.push_back(ownerPubkey);
				voutTokenPubkeys.push_back(heirPubkey);

				// add opreturn 'A'  and sign tx:						// this txfee ignored
				std::string rawhextx = (FinalizeCCTx(0, cp, mtx, myPubkey, txfee,
					Helper::makeAddOpRet(tokenid, voutTokenPubkeys, fundingtxid, hasHeirSpendingBegun)));

				result.push_back(Pair("result", "success"));
				result.push_back(Pair("hextx", rawhextx));
			}
			else {
				std::cerr << "HeirAdd cannot find owner inputs" << std::endl;
				result.push_back(Pair("result", "error"));
				result.push_back(Pair("error", "can't find owner inputs"));
			}
		}
		else {
			std::cerr << "HeirAdd cannot find normal inputs for tx fee" << std::endl;
			result.push_back(Pair("result", "error"));
			result.push_back(Pair("error", "can't find normal inputs for tx fee"));
		}

    } else {
        fprintf(stderr, "HeirAdd() can't find any heir CC funding tx's\n");

        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "can't find any heir CC funding transactions"));
    }

    return result;
}


UniValue HeirAddCoinCaller(uint256 fundingtxid, uint64_t txfee, int64_t amount) {
	return HeirAdd<CoinHelper>(fundingtxid,  txfee,  amount);
}
UniValue HeirAddTokenCaller(uint256 fundingtxid, uint64_t txfee, int64_t amount) {
	return HeirAdd<TokenHelper>(fundingtxid, txfee, amount);
}


/**
 * heirclaim rpc call implementation
 * creates tx to spend funds from cryptocondition address by either funds' owner or heir
 * @return result object with raw tx or error text
 */
template <typename Helper>UniValue HeirClaim(uint256 fundingtxid, uint64_t txfee, int64_t amount)
{
    UniValue result(UniValue::VOBJ); //, a(UniValue::VARR);
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	CPubKey myPubkey, ownerPubkey, heirPubkey;
    int64_t inputs, change = 0;
    int64_t inactivityTimeSec;
    struct CCcontract_info *cp, C;

    uint256 latesttxid, tokenid;
    uint8_t funcId;
    std::string heirName;
	uint8_t hasHeirSpendingBegun = 0;


    cp = CCinit(&C, Helper::getMyEval());
    if (txfee == 0)
        txfee = 10000;

    if ((latesttxid = FindLatestFundingTx<Helper>(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, hasHeirSpendingBegun)) != zeroid) {
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
				mtx.vout.push_back(Helper::makeClaimerVout(amount, myPubkey));  // vout[0]

                // calc and add change vout:
                if (inputs > amount)
					change = (inputs - amount); //  -txfee <-- txfee pays user

                //std::cerr << "HeirClaim() inputs=" << inputs << " amount=" << amount << " txfee=" << txfee << " change=" << change << '\n';

				// change to 1of2 funding addr:
                if (change != 0) {											   // vout[1]
                    mtx.vout.push_back(MakeCC1of2vout(Helper::getMyEval(), change, ownerPubkey, heirPubkey)); // using always pubkeys from OP_RETURN in order to not mixing them up!
                }

                // add marker vout:
                /*char markeraddr[64];
				CPubKey markerpubkey = CCtxidaddr(markeraddr, fundingtxid);
				// NOTE: amount = 0 is not working: causes error code: -26, error message : 64 : dust
				mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG)); // txfee 1, txfee 2 - for miners
				std::cerr << "HeirClaim() adding markeraddr=" << markeraddr << '\n'; */

                uint8_t myprivkey[32];
                char coinaddr[64];
                // set priv key addresses in CC structure:
                GetCCaddress1of2(cp, coinaddr, ownerPubkey, heirPubkey);
                Myprivkey(myprivkey);

				////fprintf(stderr,"HeirClaim() before setting unspendable CC addr2= (%s) addr3= (%s)\n", cp->unspendableaddr2, cp->unspendableaddr3);
				//CCaddr2set(cp, Helper::getMyEval(), ownerPubkey, myprivkey, coinaddr);
				//CCaddr3set(cp, Helper::getMyEval(), heirPubkey, myprivkey, coinaddr);
				////fprintf(stderr, "HeirClaim() after  setting unspendable CC addr2=(%s) addr3=(%s)\n", cp->unspendableaddr2, cp->unspendableaddr3);

				CCaddr1of2set(cp, ownerPubkey, heirPubkey, coinaddr);

				// add 1of2 vout validation pubkeys:
				std::vector<CPubKey> voutTokenPubkeys;
				voutTokenPubkeys.push_back(ownerPubkey);
				voutTokenPubkeys.push_back(heirPubkey);

                // add opreturn 'C' and sign tx:				  // this txfee will be ignored
				std::string rawhextx = FinalizeCCTx(0, cp, mtx, myPubkey, txfee,
					Helper::makeClaimOpRet(tokenid, voutTokenPubkeys, fundingtxid, (myPubkey == heirPubkey) ? 1 : hasHeirSpendingBegun)); // forward isHeirSpending to the next latest tx

                result.push_back(Pair("result", "success"));
                result.push_back(Pair("hextx", rawhextx));

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
    } else {
        fprintf(stderr, "HeirClaim() can't find any heir CC funding tx's\n");
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "can't find any heir CC funding transactions"));
    }

    return result;
}

UniValue HeirClaimCoinCaller(uint256 fundingtxid, uint64_t txfee, int64_t amount) {
	return HeirClaim<CoinHelper>(fundingtxid, txfee, amount);
}
UniValue HeirClaimTokenCaller(uint256 fundingtxid, uint64_t txfee, int64_t amount) {
	return HeirClaim<TokenHelper>(fundingtxid, txfee, amount);
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
	CPubKey ownerPubkey, heirPubkey;
    uint256 latestFundingTxid;
	uint256 dummyTokenid, tokenid;

    std::string heirName;
    uint8_t funcId;
    int64_t inactivityTimeSec;

	CTransaction fundingtx;
	uint256 hashBlock;
	const bool allowSlow = false;

	//char markeraddr[64];
	//CCtxidaddr(markeraddr, fundingtxid);
	//SetCCunspents(unspentOutputs, markeraddr);

	// get initial funding tx and set it as initial lasttx:
	if (myGetTransaction(fundingtxid, fundingtx, hashBlock) && fundingtx.vout.size()) {

		int32_t heirType = NOT_HEIR;
		const bool noLogging = true;

		if (DecodeHeirOpRet<CoinHelper>(fundingtx.vout[fundingtx.vout.size() - 1].scriptPubKey, dummyTokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, noLogging) == 'F')
			heirType = HEIR_COINS;
		else if (DecodeHeirOpRet<TokenHelper>(fundingtx.vout[fundingtx.vout.size() - 1].scriptPubKey, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, noLogging) == 'F')
			heirType = HEIR_TOKENS;
		else
		{
			std::cerr << "HeirInfo() initial tx F not found for this fundingtx" <<  std::endl;
			result.push_back(Pair("result", "error"));
			result.push_back(Pair("error", "initial tx F not found"));
			return result;
		}

		struct CCcontract_info *cp, C;
		if (heirType == HEIR_COINS)
			cp = CCinit(&C, CoinHelper::getMyEval());
		else
			cp = CCinit(&C, TokenHelper::getMyEval());

		uint8_t hasHeirSpendingBegun = 0;

		if (heirType == HEIR_COINS)
			latestFundingTxid = FindLatestFundingTx<CoinHelper>(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, hasHeirSpendingBegun);
		else
			latestFundingTxid = FindLatestFundingTx<TokenHelper>(fundingtxid, funcId, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, hasHeirSpendingBegun);

		if (latestFundingTxid != zeroid) {
			int32_t numblocks;
			uint64_t durationSec = 0;

			//std::cerr << "HeirInfo() latesttxid=" << latestFundingTxid.GetHex() << '\n';

			std::ostringstream stream;
			std::string msg;

			result.push_back(Pair("fundingtxid", fundingtxid.GetHex()));
			result.push_back(Pair("name", heirName.c_str()));

			if (heirType == HEIR_TOKENS) {
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
			if (heirType == HEIR_COINS)
				total = LifetimeHeirContractFunds<CoinHelper>(cp, fundingtxid, ownerPubkey, heirPubkey);
			else
				total = LifetimeHeirContractFunds<TokenHelper>(cp, fundingtxid, ownerPubkey, heirPubkey);

			if (heirType == HEIR_COINS) {
				msg = "funding total in coins";
				stream << (double)total / COIN;
			}
			else	{
				msg = "funding total in tokens";
				stream << total;
			}
			result.push_back(Pair(msg, stream.str().c_str()));
			stream.str("");
			stream.clear();

			int64_t inputs;
			if (heirType == HEIR_COINS)
				inputs = Add1of2AddressInputs<CoinHelper>(cp, fundingtxid, mtx, ownerPubkey, heirPubkey, 0, 60); //NOTE: amount = 0 means all unspent inputs
			else
				inputs = Add1of2AddressInputs<TokenHelper>(cp, fundingtxid, mtx, ownerPubkey, heirPubkey, 0, 60);

			if (heirType == HEIR_COINS) {
				msg = "funding available in coins";
				stream << (double)inputs / COIN;
			}
			else	{
				msg = "funding available in tokens";
				stream << inputs;
			}
			result.push_back(Pair(msg, stream.str().c_str()));
			stream.str("");
			stream.clear();

			if (heirType == HEIR_TOKENS) {
				int64_t ownerInputs = TokenHelper::addOwnerInputs(cp, tokenid, mtx, ownerPubkey, 0, (int32_t)64);
				stream << ownerInputs;
				msg = "owner funding available in tokens";
				result.push_back(Pair(msg, stream.str().c_str()));
				stream.str("");
				stream.clear();
			}

			stream << inactivityTimeSec;
			result.push_back(Pair("inactivity time setting", stream.str().c_str()));
			stream.str("");
			stream.clear();

			if (!hasHeirSpendingBegun) { // we do not need find duration if the spending already has begun
				durationSec = CCduration(numblocks, latestFundingTxid);
				std::cerr << "HeirInfo() duration=" << durationSec << " inactivityTime=" << inactivityTimeSec << " numblocks=" << numblocks << '\n';
			}

			stream << std::boolalpha << (hasHeirSpendingBegun || durationSec > inactivityTimeSec);
			result.push_back(Pair("spending allowed for the heir", stream.str().c_str()));
			stream.str("");
			stream.clear();

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

template <typename Helper>void _HeirList(struct CCcontract_info *cp, UniValue &result)
{
	std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>> unspentOutputs;
	char coinaddr[64];
	CPubKey ccPubKeyEmpty;
	GetCCaddress(cp, coinaddr, ccPubKeyEmpty);
	SetCCunspents(unspentOutputs, cp->normaladdr);

	//std::cerr << "HeirList() finding heir marker from Heir contract addr=" << cp->normaladdr << " unspentOutputs.size()=" << unspentOutputs.size() << '\n';

	// TODO: move marker to special cc addr to prevent checking all tokens
	for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>>::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++) {
		uint256 hashBlock;
		uint256 txid = it->first.txhash;
		uint256 tokenid;
		int32_t vout = (int32_t)it->first.index;

		//std::cerr << "HeirList() checking txid=" << txid.GetHex() << " vout=" << vout << '\n';

		CTransaction inittx;
		if (GetTransaction(txid, inittx, hashBlock, false) != 0 && (inittx.vout.size() - 1) > 0) {
			CPubKey ownerPubkey, heirPubkey;
			std::string heirName;
			int64_t inactivityTimeSec;
			const bool noLogging = true;

			uint8_t funcId = DecodeHeirOpRet<Helper>(inittx.vout[inittx.vout.size() - 1].scriptPubKey, tokenid, ownerPubkey, heirPubkey, inactivityTimeSec, heirName, noLogging);

			// note: if it is not Heir token funcId would be equal to 0
			if (funcId == 'F') {
				//result.push_back(Pair("fundingtxid kind name", txid.GetHex() + std::string(" ") + (typeid(Helper) == typeid(TokenHelper) ? std::string("token") : std::string("coin")) + std::string(" ") + heirName));
				result.push_back( Pair("fundingtxid", txid.GetHex()) );
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
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result", "success"));

    struct CCcontract_info *cpHeir, *cpTokens, heirC, tokenC;  // NOTE we must use a separate 'C' structure for each CCinit!
    
	cpHeir = CCinit(&heirC, EVAL_HEIR);
	cpTokens = CCinit(&tokenC, EVAL_TOKENS);

	_HeirList<CoinHelper>(cpHeir, result);
	_HeirList<TokenHelper>(cpTokens, result);
   
	return result;
}

