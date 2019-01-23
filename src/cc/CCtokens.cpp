/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
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

#include "CCtokens.h"

/* TODO: correct this:
-----------------------------
 The SetTokenFillamounts() and ValidateTokenRemainder() work in tandem to calculate the vouts for a fill and to validate the vouts, respectively.
 
 This pair of functions are critical to make sure the trading is correct and is the trickiest part of the tokens contract.
 
 //vin.0: normal input
 //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
 //vin.2+: valid CC output satisfies buyoffer (*tx.vin[2])->nValue
 //vout.0: remaining amount of bid to unspendable
 //vout.1: vin.1 value to signer of vin.2
 //vout.2: vin.2 tokenoshis to original pubkey
 //vout.3: CC output for tokenoshis change (if any)
 //vout.4: normal output for change (if any)
 //vout.n-1: opreturn [EVAL_ASSETS] ['B'] [tokenid] [remaining token required] [origpubkey]
    ValidateTokenRemainder(remaining_price,tx.vout[0].nValue,nValue,tx.vout[1].nValue,tx.vout[2].nValue,totalunits);
 
 Yes, this is quite confusing...
 
 In ValudateTokenRemainder the naming convention is nValue is the coin/token with the offer on the books and "units" is what it is being paid in. The high level check is to make sure we didnt lose any coins or tokens, the harder to validate is the actual price paid as the "orderbook" is in terms of the combined nValue for the combined totalunits.
 
 We assume that the effective unit cost in the orderbook is valid and that that amount was paid and also that any remainder will be close enough in effective unit cost to not matter. At the edge cases, this will probably be not true and maybe some orders wont be practically fillable when reduced to fractional state. However, the original pubkey that created the offer can always reclaim it.
 ------------------------------
*/


// NOTE: this inital tx won't be used by other contract
// for tokens to be used there should be at least one 't' tx with other contract's custom opret
CScript EncodeTokenCreateOpRet(uint8_t funcid,std::vector<uint8_t> origpubkey,std::string name,std::string description)
{
    CScript opret; uint8_t evalcode = EVAL_TOKENS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << origpubkey << name << description);
    return(opret);
}

//  this is for other contracts which use tokens and build customized extra payloads to token's opret:
CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, CScript payload)
{
    CScript opret; 
	uint8_t tokenFuncId = 't';
	uint8_t evalCodeInOpret = EVAL_TOKENS;

    tokenid = revuint256(tokenid);

	uint8_t ccType = 0;
	if (voutPubkeys.size() >= 1 && voutPubkeys.size() <= 2)
		ccType = voutPubkeys.size();

	std::vector<uint8_t> vpayload;
	GetOpReturnData(payload, vpayload);

	opret << OP_RETURN << E_MARSHAL(ss << evalCodeInOpret << tokenFuncId << tokenid << ccType; \
		if (ccType >= 1) ss << voutPubkeys[0];				\
			if (ccType == 2) ss << voutPubkeys[1];			\
				if (vpayload.size() > 0) ss << vpayload;);
	

	// "error 64: scriptpubkey":
	// if (payload.size() > 0) 
	//	   opret += payload; 

	// error 64: scriptpubkey:
	// CScript opretPayloadNoOpcode(vpayload);
	//    return opret + opretPayloadNoOpcode;

	// how to attach payload without re-serialization: 
	// sig_aborted:
	// opret.resize(opret.size() + vpayload.size());
	// CScript::iterator it = opret.begin() + opret.size();
	// for (int i = 0; i < vpayload.size(); i++, it++)
	// 	 *it = vpayload[i];

	return opret;
}  

// overload for compatibility 
CScript EncodeTokenOpRet(uint8_t tokenFuncId, uint8_t evalCodeInOpret, uint256 tokenid, std::vector<CPubKey> voutPubkeys, CScript payload)
{
	return EncodeTokenOpRet(tokenid, voutPubkeys, payload);
}

uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey,std::vector<uint8_t> &origpubkey,std::string &name,std::string &description)
{
    std::vector<uint8_t> vopret; uint8_t dummyEvalcode, funcid, *script;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( script != 0 && vopret.size() > 2 && script[0] == EVAL_TOKENS && script[1] == 'c' )
    {
        if ( E_UNMARSHAL(vopret, ss >> dummyEvalcode; ss >> funcid; ss >> origpubkey; ss >> name; ss >> description) != 0 )
            return(funcid);
    }
    return (uint8_t)0;
}

uint8_t DecodeTokenOpRet(const CScript scriptPubKey, uint8_t &evalCode, uint256 &tokenid, std::vector<CPubKey> &voutPubkeys, std::vector<uint8_t>  &vopretExtra)
{
    std::vector<uint8_t> vopret, extra, dummyPubkey; 
	uint8_t funcId=0, *script, dummyEvalCode, dummyFuncId, ccType;
	std::string dummyName; std::string dummyDescription;
	CPubKey voutPubkey1, voutPubkey2;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
	tokenid = zeroid;

    if (script != 0 && vopret.size() > 2)
    {
		// NOTE: if parse error occures, parse might not be able to set error. It is safer to treat that it was eof if it is not set!
		bool isEof = true;

		evalCode = script[0];
		if (evalCode != EVAL_TOKENS)
			return (uint8_t)0;

        funcId = script[1];
        //fprintf(stderr,"decode.[%c]\n",funcId);

        switch( funcId )
        {
            case 'c': 
				return DecodeTokenCreateOpRet(scriptPubKey, dummyPubkey, dummyName, dummyDescription);
				//break;
            case 't':  
			//not used yet: case 'l':
				// NOTE: 'E_UNMARSHAL result==false' means 'parse error' OR 'not eof state'. Consequently, 'result==false' but 'isEof==true' means just 'parse error' 
				if (E_UNMARSHAL(vopret, ss >> dummyEvalCode; ss >> dummyFuncId; ss >> tokenid; ss >> ccType; if (ccType >= 1) ss >> voutPubkey1; if (ccType == 2) ss >> voutPubkey2;  isEof = ss.eof(); vopretExtra = std::vector<uint8_t>(ss.begin(), ss.end()))
					|| !isEof)
				{

					if (!(ccType >= 0 && ccType <= 2)) { //incorrect ccType
						std::cerr << "DecodeTokenOpRet() incorrect ccType=" << (int)ccType << " tokenid=" << revuint256(tokenid).GetHex() << std::endl;
						return (uint8_t)0;
					}

					// add verification pubkeys:
					voutPubkeys.clear();
					if (voutPubkey1.IsValid())
						voutPubkeys.push_back(voutPubkey1);
					if (voutPubkey2.IsValid())
						voutPubkeys.push_back(voutPubkey2);

					tokenid = revuint256(tokenid);
					return(funcId);
				}
				std::cerr << "DecodeTokenOpRet() bad opret format, isEof=" << isEof << " ccType=" << ccType << " tokenid=" << revuint256(tokenid).GetHex() << std::endl;
				return (uint8_t)0;

            default:
				std::cerr << "DecodeTokenOpRet() illegal funcid=" << (int)funcId << std::endl;
				return (uint8_t)0;
        }
    }
	else	{
		std::cerr << "DecodeTokenOpRet() empty opret, could not parse" << std::endl;
	}
	return (uint8_t)0;
}



// tx validation
bool TokensValidate(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn)
{
	static uint256 zero;
	CTxDestination address; CTransaction vinTx, createTx; uint256 hashBlock, tokenid, tokenid2;
	int32_t i, starti, numvins, numvouts, preventCCvins, preventCCvouts;
	int64_t remaining_price, nValue, tokenoshis, outputs, inputs, tmpprice, totalunits, ignore; 
	std::vector<uint8_t> vopretExtra, tmporigpubkey, ignorepubkey;
	uint8_t funcid, evalCodeInOpret;
	char destaddr[64], origaddr[64], CCaddr[64];
	std::vector<CPubKey> voutTokenPubkeys;

	//return true;

	numvins = tx.vin.size();
	numvouts = tx.vout.size();
	outputs = inputs = 0;
	preventCCvins = preventCCvouts = -1;

	if ((funcid = DecodeTokenOpRet(tx.vout[numvouts - 1].scriptPubKey, evalCodeInOpret, tokenid, voutTokenPubkeys, vopretExtra)) == 0)
		return eval->Invalid("TokenValidate: invalid opreturn payload");

	fprintf(stderr, "TokensValidate (%c) evalcode=0x%0x\n", funcid, cp->evalcode);

	if (eval->GetTxUnconfirmed(tokenid, createTx, hashBlock) == 0)
		return eval->Invalid("cant find token create txid");
	else if (IsCCInput(tx.vin[0].scriptSig) != 0)
		return eval->Invalid("illegal token vin0");
	else if (numvouts < 1)
		return eval->Invalid("no vouts");
	else if (funcid != 'c')
	{
		if (tokenid == zeroid)
			return eval->Invalid("illegal tokenid");
		else if (!TokensExactAmounts(true, cp, inputs, outputs, eval, tx, tokenid)) {
			if (!eval->Valid())
				return false;  //TokenExactAmounts must call eval->Invalid()!
			else
				return eval->Invalid("tokens cc inputs != cc outputs");
		}
	}


	switch (funcid)
	{
	case 'c': // create wont be called to be verified as it has no CC inputs
			  //vin.0: normal input
			  //vout.0: issuance tokenoshis to CC
			  //vout.1: normal output for change (if any)
			  //vout.n-1: opreturn EVAL_TOKENS 'c' <tokenname> <description>
		//if (evalCodeInOpret != EVAL_TOKENS)
		//	return eval->Invalid("unexpected TokenValidate for createtoken");
		//else
		return true;
		
	case 't': // transfer
			  //vin.0: normal input
			  //vin.1 .. vin.n-1: valid CC outputs
			  //vout.0 to n-2: tokenoshis output to CC
			  //vout.n-2: normal output for change (if any)
			  //vout.n-1: opreturn <other evalcode> 't' tokenid <other contract payload>
		if (inputs == 0)
			return eval->Invalid("no token inputs for transfer");

		fprintf(stderr, "token transfer preliminarily validated %.8f -> %.8f (%d %d)\n", (double)inputs / COIN, (double)outputs / COIN, preventCCvins, preventCCvouts);
		break;  // breaking to other contract validation...

	default:
		fprintf(stderr, "illegal tokens funcid.(%c)\n", funcid);
		return eval->Invalid("unexpected token funcid");
	}

	// forward validation if evalcode in opret is not EVAL_TOKENS
	// init for forwarding validation call
	//if (evalCodeInOpret != EVAL_TOKENS) {		// TODO: should we check also only allowed for tokens evalcodes, like EVAL_ASSETS, EVAL_GATEWAYS?
	//	struct CCcontract_info *cpOther = NULL, C;

	//	cpOther = CCinit(&C, evalCodeInOpret);
	//	if (cpOther)
	//		return cpOther->validate(cpOther, eval, tx, nIn);
	//	else
	//		return eval->Invalid("unsupported evalcode in opret");
	//}
	return true;
	// what does this do?
	// return(PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts));
}

// helper funcs:

// extract my vins pubkeys:
bool ExtractTokensVinPubkeys(CTransaction tx, std::vector<CPubKey> &vinPubkeys) {

	bool found = false;
	CPubKey pubkey;
	struct CCcontract_info *cpTokens, tokensC;

	cpTokens = CCinit(&tokensC, EVAL_TOKENS);

	for (int32_t i = 0; i < tx.vin.size(); i++)
	{												  // check for additional contracts which may send tokens to the Tokens contract
		if( (*cpTokens->ismyvin)(tx.vin[i].scriptSig) )
		{

			auto findEval = [](CC *cond, struct CCVisitor _) {
				bool r = false; //cc_typeId(cond) == CC_Eval && cond->codeLength == 1 && cond->code[0] == EVAL_TOKENS;

				if (cc_typeId(cond) == CC_Secp256k1) {
					*(CPubKey*)_.context = buf2pk(cond->publicKey);
					//std::cerr << "findEval found pubkey=" << HexStr(*(CPubKey*)_.context) << std::endl;
					r = true;
				}
				// false for a match, true for continue
				return r ? 0 : 1;
			};

			CC *cond = GetCryptoCondition(tx.vin[i].scriptSig);

			if (cond) {
				CCVisitor visitor = { findEval, (uint8_t*)"", 0, &pubkey };
				bool out = !cc_visit(cond, visitor);
				cc_free(cond);

				if (pubkey.IsValid()) {
					vinPubkeys.push_back(pubkey);
					found = true;
				}
			}
		}
	}
	return found;
}

// this is just for log messages indentation fur debugging recursive calls:
thread_local uint32_t tokenValIndentSize = 0;

// validates opret for token tx:
uint8_t ValidateTokenOpret(CTransaction tx, int32_t v, uint256 tokenid, std::vector<CPubKey> &voutPubkeys, std::vector<uint8_t> &vopretExtra) {

	uint256 tokenidOpret, tokenidOpret2;
	uint8_t funcid;
	uint8_t dummyEvalCode;

	// this is just for log messages indentation fur debugging recursive calls:
	std::string indentStr = std::string().append(tokenValIndentSize, '.');

	int32_t n = tx.vout.size();

	if ((funcid = DecodeTokenOpRet(tx.vout[n - 1].scriptPubKey, dummyEvalCode, tokenidOpret, voutPubkeys, vopretExtra)) == 0)
	{
		std::cerr << indentStr << "ValidateTokenOpret() DecodeTokenOpret could not parse opret for txid=" << tx.GetHash().GetHex() << std::endl;
		return(false);
	}
	else if (funcid == 'c')
	{
		if (tokenid != zeroid && tokenid == tx.GetHash() && v == 0) {
			//std::cerr << indentStr << "ValidateTokenOpret() this is the tokenbase 'c' tx, txid=" << tx.GetHash().GetHex() << " vout=" << v << " returning true" << std::endl;
			return funcid;
		}
	}
	else if (funcid == 't')  
	{
		//std::cerr << indentStr << "ValidateTokenOpret() tokenid=" << tokenid.GetHex() << " tokenIdOpret=" << tokenidOpret.GetHex() << " txid=" << tx.GetHash().GetHex() << std::endl;
		if (tokenid != zeroid && tokenid == tokenidOpret) {
			//std::cerr << indentStr << "ValidateTokenOpret() this is a transfer 't' tx, txid=" << tx.GetHash().GetHex() << " vout=" << v << " returning true" << std::endl;
			return funcid;
		}
	}
	//std::cerr << indentStr << "ValidateTokenOpret() return false funcid=" << (char)funcid << " tokenid=" << tokenid.GetHex() << " tokenIdOpret=" << tokenidOpret.GetHex() << " txid=" << tx.GetHash().GetHex() << std::endl;
	return (uint8_t)0;
}

// Checks if the vout is a really Tokens CC vout
// also checks tokenid in opret or txid if this is 'c' tx
// goDeeper is true: the func also validates amounts of the passed transaction: 
// it should be either sum(cc vins) == sum(cc vouts) or the transaction is the 'tokenbase' ('c') tx
// checkPubkeys is true: validates if the vout is token vout1 or token vout1of2. Should always be true!
int64_t IsTokensvout(bool goDeeper, bool checkPubkeys, struct CCcontract_info *cp, Eval* eval, const CTransaction& tx, int32_t v, uint256 reftokenid)
{

	// this is just for log messages indentation fur debugging recursive calls:
	std::string indentStr = std::string().append(tokenValIndentSize, '.');
	//std::cerr << indentStr << "IsTokensvout() entered for txid=" << tx.GetHash().GetHex() << " v=" << v << " for tokenid=" << reftokenid.GetHex() <<  std::endl;

	//TODO: validate cc vouts are EVAL_TOKENS!
	if (tx.vout[v].scriptPubKey.IsPayToCryptoCondition()) // maybe check address too? dimxy: possibly no, because there are too many cases with different addresses here
	{
		int32_t n = tx.vout.size();
		// just check boundaries:
		if (v >= n - 1) {  // just moved this up (dimxy)
			std::cerr << indentStr << "isTokensvout() internal err: (v >= n - 1), returning 0" << std::endl;
			return(0);
		}

		if (goDeeper) {
			//std::cerr << indentStr << "IsTokensvout() maxTokenExactAmountDepth=" << maxTokenExactAmountDepth << std::endl;
			//validate all tx
			int64_t myCCVinsAmount = 0, myCCVoutsAmount = 0;

			tokenValIndentSize++;
			// false --> because we already at the 1-st level ancestor tx and do not need to dereference ancestors of next levels
			bool isEqual = TokensExactAmounts(false, cp, myCCVinsAmount, myCCVoutsAmount, eval, tx, reftokenid);
			tokenValIndentSize--;

			if (!isEqual) {
				// if ccInputs != ccOutputs and it is not the tokenbase tx 
				// this means it is possibly a fake tx (dimxy):
				if (reftokenid != tx.GetHash()) {	// checking that this is the true tokenbase tx, by verifying that funcid=c, is done further in this function (dimxy)
					std::cerr << indentStr << "IsTokensvout() warning: for the verified tx detected a bad vintx=" << tx.GetHash().GetHex() << ": cc inputs != cc outputs and not the 'tokenbase' tx, skipping the verified tx" << std::endl;
					return 0;
				}
			}
		}

		// moved opret checking to this new reusable func (dimxy):
		std::vector<CPubKey> voutPubkeys;
		std::vector<uint8_t> vopretExtra;
		const uint8_t funcId = ValidateTokenOpret(tx, v, reftokenid, voutPubkeys, vopretExtra);
		//std::cerr << indentStr << "IsTokensvout() ValidateTokenOpret returned=" << (char)(funcId?funcId:' ') << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
		if (funcId != 0) {
			//std::cerr << indentStr << "IsTokensvout() ValidateTokenOpret returned not-null"  << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;

			if (checkPubkeys && funcId != 'c') { // verify that the vout is token's (for 'c' there is no pubkeys!):

				//std::cerr << "IsTokensvout() vopretExtra=" << HexStr(vopretExtra) << std::endl;
			
				uint8_t evalCodeInOpret;
				if (vopretExtra.size() >= 2 /*|| vopretExtra.size() != vopretExtra.begin()[0]  <-- shold we check this?*/) {
					evalCodeInOpret = vopretExtra.begin()[1];
				}
				else {
					// if payload is empty maybe it is a claim to non-payload-one-token-eval vout?
					evalCodeInOpret = EVAL_TOKENS;
				}

				// maybe this is dual-eval 1 pubkey or 1of2 pubkey vout?
				if (voutPubkeys.size() >= 1 && voutPubkeys.size() <= 2) {					
					CTxOut testDualVout;
					// check dual-eval 1 pubkey vout with the first pubkey
					testDualVout = MakeTokensCC1vout(evalCodeInOpret, tx.vout[v].nValue, voutPubkeys[0]);
					if (tx.vout[v].scriptPubKey == testDualVout.scriptPubKey) {
						//std::cerr << indentStr << "IsTokensvout() this is dual-eval token vout (i=0), eval2=" << (int)evalCodeInOpret << ", returning nValue=" << tx.vout[v].nValue << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
						return tx.vout[v].nValue;
					}
					
					if(voutPubkeys.size() == 2)	{
						// check dual eval 1of2 pubkeys vout
						testDualVout = MakeTokensCC1of2vout(evalCodeInOpret, tx.vout[v].nValue, voutPubkeys[0], voutPubkeys[1]);
						if (tx.vout[v].scriptPubKey == testDualVout.scriptPubKey) {
							//std::cerr << indentStr << "IsTokensvout() this is dual-eval token 1of2 vout, eval2=" << (int)evalCodeInOpret << ", returning nValue=" << tx.vout[v].nValue << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
							return tx.vout[v].nValue;
						}

						// check dual eval 1 pubkey vout with the second pubkey
						testDualVout = MakeTokensCC1vout(evalCodeInOpret, tx.vout[v].nValue, voutPubkeys[1]);
						if (tx.vout[v].scriptPubKey == testDualVout.scriptPubKey) {
							//std::cerr << indentStr << "IsTokensvout() this is dual-eval token vout (i=1), eval2=" << (int)evalCodeInOpret << ", returning nValue=" << tx.vout[v].nValue << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
							return tx.vout[v].nValue;
						}
					}
				

					// maybe this is claim to single-eval token?
					CTxOut testTokenVout1;
					testTokenVout1 = MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, voutPubkeys[0]);
					if (tx.vout[v].scriptPubKey == testTokenVout1.scriptPubKey) {
						//std::cerr << indentStr << "IsTokensvout() this is single-eval token vout (i=0), returning nValue=" << tx.vout[v].nValue << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
						return tx.vout[v].nValue;
					}

					if (voutPubkeys.size() == 2) {
						testTokenVout1 = MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, voutPubkeys[1]);
						if (tx.vout[v].scriptPubKey == testTokenVout1.scriptPubKey) {
							//std::cerr << indentStr << "IsTokensvout() this is single-eval token vout (i=1), returning nValue=" << tx.vout[v].nValue << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
							return tx.vout[v].nValue;
						}
					}
				}

				// maybe it is single-eval or dual-eval token change?
				std::vector<CPubKey> vinPubkeys;
				ExtractTokensVinPubkeys(tx, vinPubkeys);

				for(std::vector<CPubKey>::iterator it = vinPubkeys.begin(); it != vinPubkeys.end(); it++) {
					CTxOut testTokenVout1 = MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, *it);
					CTxOut testDualVout1 = MakeTokensCC1vout(evalCodeInOpret, tx.vout[v].nValue, *it);

					if (tx.vout[v].scriptPubKey == testTokenVout1.scriptPubKey) {
						//std::cerr << indentStr << "IsTokensvout() this is single-eval token change, returning nValue=" << tx.vout[v].nValue << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
						return tx.vout[v].nValue;
					}

					if (tx.vout[v].scriptPubKey == testDualVout1.scriptPubKey) {
						//std::cerr << indentStr << "IsTokensvout() this is dual-eval token change, vout eval2=" << (int)evalCodeInOpret << ", returning nValue=" << tx.vout[v].nValue << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
						return tx.vout[v].nValue;
					}
				}
			}
			else	{
				//std::cerr << indentStr << "IsTokensvout() returns without pubkey check value=" << tx.vout[v].nValue << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
				return tx.vout[v].nValue;
			}
		}

		//std::cerr << indentStr; fprintf(stderr,"IsTokensvout() CC vout v.%d of n=%d amount=%.8f txid=%s\n",v,n,(double)0/COIN, tx.GetHash().GetHex().c_str());
	}
	//std::cerr << indentStr; fprintf(stderr,"IsTokensvout() normal output v.%d %.8f\n",v,(double)tx.vout[v].nValue/COIN);
	return(0);
}

// compares cc inputs vs cc outputs (to prevent feeding vouts from normal inputs)
bool TokensExactAmounts(bool goDeeper, struct CCcontract_info *cp, int64_t &inputs, int64_t &outputs, Eval* eval, const CTransaction &tx, uint256 tokenid)
{
	CTransaction vinTx; 
	uint256 hashBlock; 
	int64_t tokenoshis; 

	struct CCcontract_info *cpTokens, tokensC;
	cpTokens = CCinit(&tokensC, EVAL_TOKENS);

	int32_t numvins = tx.vin.size();
	int32_t numvouts = tx.vout.size();
	inputs = outputs = 0;

	// this is just for log messages indentation for debugging recursive calls:
	std::string indentStr = std::string().append(tokenValIndentSize, '.');

	for (int32_t i = 0; i<numvins; i++)
	{												  // check for additional contracts which may send tokens to the Tokens contract
		if ((*cpTokens->ismyvin)(tx.vin[i].scriptSig) /*|| IsVinAllowed(tx.vin[i].scriptSig) != 0*/)
		{
			//std::cerr << indentStr << "TokensExactAmounts() eval is true=" << (eval != NULL) << " ismyvin=ok for_i=" << i << std::endl;
			// we are not inside the validation code -- dimxy
			if ((eval && eval->GetTxUnconfirmed(tx.vin[i].prevout.hash, vinTx, hashBlock) == 0) || (!eval && !myGetTransaction(tx.vin[i].prevout.hash, vinTx, hashBlock)))
			{
				std::cerr << indentStr << "TokensExactAmounts() cannot read vintx for i." << i << " numvins." << numvins << std::endl;
				return (!eval) ? false : eval->Invalid("always should find vin tx, but didnt");
			}
			else {
				tokenValIndentSize++;
				// validate vouts of vintx  
				//std::cerr << indentStr << "TokenExactAmounts() check vin i=" << i << " nValue=" << vinTx.vout[tx.vin[i].prevout.n].nValue << std::endl;
				tokenoshis = IsTokensvout(goDeeper, true, cpTokens, eval, vinTx, tx.vin[i].prevout.n, tokenid);
				tokenValIndentSize--;
				if (tokenoshis != 0)
				{
					std::cerr << indentStr << "TokensExactAmounts() vin i=" << i << " tokenoshis=" << tokenoshis << std::endl;
					inputs += tokenoshis;
				}
			}
		}
	}


	for (int32_t i = 0; i<numvouts; i++)
	{
		tokenValIndentSize++;
		// Note: we pass in here 'false' because we don't need to call TokenExactAmounts() recursively from IsTokensvout
		// indeed, in this case we'll be checking this tx again
		//std::cerr << indentStr << "TokenExactAmounts() check vout i=" << i << " nValue=" << tx.vout[i].nValue << std::endl;
		tokenoshis = IsTokensvout(false, true /*<--exclude non-tokens vouts*/, cpTokens, eval, tx, i, tokenid);
		tokenValIndentSize--;

		if (tokenoshis != 0)
		{
			std::cerr << indentStr << "TokensExactAmounts() vout i=" << i << " tokenoshis=" << tokenoshis << std::endl;
			outputs += tokenoshis;
		}
	}

	//std::cerr << indentStr << "TokensExactAmounts() inputs=" << inputs << " outputs=" << outputs << " for txid=" << tx.GetHash().GetHex() << std::endl;

	if (inputs != outputs) {
		if (tx.GetHash() != tokenid)
			std::cerr << indentStr << "TokenExactAmounts() found unequal token cc inputs=" << inputs << " vs cc outputs=" << outputs << " for txid=" << tx.GetHash().GetHex() << " and this is not the create tx" << std::endl;
		return false;  // do not call eval->Invalid() here!
	}
	else
		return true;
}

// add inputs from token cc addr
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 tokenid, int64_t total, int32_t maxinputs)
{
	char tokenaddr[64], destaddr[64]; 
	int64_t threshold, nValue, price, totalinputs = 0; 
	uint256 txid, hashBlock; 
	//std::vector<uint8_t> vopretExtra;
	CTransaction vintx; 
	int32_t j, vout, n = 0;
	std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

	GetTokensCCaddress(cp, tokenaddr, pk);
	SetCCunspents(unspentOutputs, tokenaddr);

	threshold = total / (maxinputs != 0 ? maxinputs : 64); // TODO: is maxinputs really could not be over 64? what if i want to calc total balance?

	for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
	{
		txid = it->first.txhash;
		vout = (int32_t)it->first.index;
		if (it->second.satoshis < threshold)
			continue;
		for (j = 0; j<mtx.vin.size(); j++)
			if (txid == mtx.vin[j].prevout.hash && vout == mtx.vin[j].prevout.n)
				break;
		if (j != mtx.vin.size())
			continue;

		if (GetTransaction(txid, vintx, hashBlock, false) != 0)
		{
			Getscriptaddress(destaddr, vintx.vout[vout].scriptPubKey);
			if (strcmp(destaddr, tokenaddr) != 0 && strcmp(destaddr, cp->unspendableCCaddr) != 0 && strcmp(destaddr, cp->unspendableaddr2) != 0)
				continue;
			//fprintf(stderr, "AddTokenCCInputs() check destaddress=%s vout amount=%.8f\n", destaddr, (double)vintx.vout[vout].nValue / COIN);

			std::vector<CPubKey> vinPubkeys;
			
			if ((nValue = IsTokensvout(true, true/*<--add only checked token uxtos */, cp, NULL, vintx, vout, tokenid)) > 0 && myIsutxo_spentinmempool(txid, vout) == 0)
			{
				if (total != 0 && maxinputs != 0)
					mtx.vin.push_back(CTxIn(txid, vout, CScript()));
				nValue = it->second.satoshis;
				totalinputs += nValue;
				std::cerr << "AddTokenInputs() adding input nValue=" << nValue  << std::endl;
				n++;
				if ((total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs))
					break;
			}
		}
	}

	//std::cerr << "AddTokenInputs() found totalinputs=" << totalinputs << std::endl;
	return(totalinputs);
}


std::string CreateToken(int64_t txfee, int64_t assetsupply, std::string name, std::string description)
{
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	CPubKey mypk; struct CCcontract_info *cp, C;
	if (assetsupply < 0)
	{
		fprintf(stderr, "negative assetsupply %lld\n", (long long)assetsupply);
		return("");
	}
	
	cp = CCinit(&C, EVAL_TOKENS);
	if (name.size() > 32 || description.size() > 4096)
	{
		fprintf(stderr, "name.%d or description.%d is too big\n", (int32_t)name.size(), (int32_t)description.size());
		return("");
	}
	if (txfee == 0)
		txfee = 10000;
	mypk = pubkey2pk(Mypubkey());

	if (AddNormalinputs(mtx, mypk, assetsupply + 2 * txfee, 64) > 0)
	{
		mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, assetsupply, mypk));
		mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(cp->CChexstr) << OP_CHECKSIG));
		return(FinalizeCCTx(0, cp, mtx, mypk, txfee, EncodeTokenCreateOpRet('c', Mypubkey(), name, description)));
	}
	return("");
}


std::string TokenTransfer(int64_t txfee, uint256 assetid, std::vector<uint8_t> destpubkey, int64_t total)
{
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	CPubKey mypk; uint64_t mask; int64_t CCchange = 0, inputs = 0;  struct CCcontract_info *cp, C;
	std::vector<uint8_t> emptyExtraOpret;

	if (total < 0)
	{
		fprintf(stderr, "negative total %lld\n", (long long)total);
		return("");
	}
	cp = CCinit(&C, EVAL_TOKENS);
	if (txfee == 0)
		txfee = 10000;
	mypk = pubkey2pk(Mypubkey());
	if (AddNormalinputs(mtx, mypk, txfee, 3) > 0)
	{
		//n = outputs.size();
		//if ( n == amounts.size() )
		//{
		//    for (i=0; i<n; i++)
		//        total += amounts[i];
		mask = ~((1LL << mtx.vin.size()) - 1);
		if ((inputs = AddTokenCCInputs(cp, mtx, mypk, assetid, total, 60)) > 0)
		{

			if (inputs < total) {   //added dimxy
				std::cerr << "AssetTransfer(): insufficient funds" << std::endl;
				return ("");
			}
			if (inputs > total)
				CCchange = (inputs - total);
			//for (i=0; i<n; i++)
			mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, total, pubkey2pk(destpubkey)));  // TODO: or MakeTokensCC1vout??
			if (CCchange != 0)
				mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, CCchange, mypk));

			std::vector<CPubKey> voutTokenPubkeys;
			voutTokenPubkeys.push_back(pubkey2pk(destpubkey));  // dest pubkey for validating vout

			return(FinalizeCCTx(mask, cp, mtx, mypk, txfee, EncodeTokenOpRet('t', EVAL_TOKENS, assetid, voutTokenPubkeys, CScript())));  // By setting EVAL_TOKENS we're getting out from assets validation code
		}
		else {
			fprintf(stderr, "not enough CC token inputs for %.8f\n", (double)total / COIN);
		}
		//} else fprintf(stderr,"numoutputs.%d != numamounts.%d\n",n,(int32_t)amounts.size());
	}
	else {
		fprintf(stderr, "not enough normal inputs for txfee\n");
	}
	return("");
}


int64_t GetTokenBalance(CPubKey pk, uint256 tokenid)
{
	uint256 hashBlock;
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	CTransaction tokentx;

	// CCerror = strprintf("obsolete, cannot return correct value without eval");
	// return 0;

	if (GetTransaction(tokenid, tokentx, hashBlock, false) == 0)
	{
		fprintf(stderr, "cant find tokenid\n");
		CCerror = strprintf("cant find tokenid");
		return 0;
	}

	struct CCcontract_info *cp, C;
	cp = CCinit(&C, EVAL_TOKENS);
	return(AddTokenCCInputs(cp, mtx, pk, tokenid, 0, 0));
}

UniValue TokenInfo(uint256 tokenid)
{
	UniValue result(UniValue::VOBJ); uint256 hashBlock; CTransaction vintx; std::vector<uint8_t> origpubkey; std::string name, description; char str[67], numstr[65];
	if (GetTransaction(tokenid, vintx, hashBlock, false) == 0)
	{
		fprintf(stderr, "cant find assetid\n");
		result.push_back(Pair("result", "error"));
		result.push_back(Pair("error", "cant find tokenid"));
		return(result);
	}
	if (vintx.vout.size() > 0 && DecodeTokenCreateOpRet(vintx.vout[vintx.vout.size() - 1].scriptPubKey, origpubkey, name, description) == 0)
	{
		fprintf(stderr, "assetid isnt token creation txid\n");
		result.push_back(Pair("result", "error"));
		result.push_back(Pair("error", "assetid isnt token creation txid"));
	}
	result.push_back(Pair("result", "success"));
	result.push_back(Pair("tokenid", uint256_str(str, tokenid)));
	result.push_back(Pair("owner", pubkey33_str(str, origpubkey.data())));
	result.push_back(Pair("name", name));
	result.push_back(Pair("supply", vintx.vout[0].nValue));
	result.push_back(Pair("description", description));
	return(result);
}

UniValue TokenList()
{
	UniValue result(UniValue::VARR);
	std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
	struct CCcontract_info *cp, C; uint256 txid, hashBlock;
	CTransaction vintx; std::vector<uint8_t> origpubkey;
	std::string name, description; char str[65];

	cp = CCinit(&C, EVAL_TOKENS);
	SetCCtxids(addressIndex, cp->normaladdr);
	for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it = addressIndex.begin(); it != addressIndex.end(); it++)
	{
		txid = it->first.txhash;
		if (GetTransaction(txid, vintx, hashBlock, false) != 0)
		{
			if (vintx.vout.size() > 0 && DecodeTokenCreateOpRet(vintx.vout[vintx.vout.size() - 1].scriptPubKey, origpubkey, name, description) != 0)
			{
				result.push_back(uint256_str(str, txid));
			}
		}
	}
	return(result);
}
