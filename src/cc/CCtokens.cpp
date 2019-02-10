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
CScript EncodeTokenCreateOpRet(uint8_t funcid, std::vector<uint8_t> origpubkey, std::string name, std::string description, std::vector<uint8_t> vopretNonfungible )
{
    CScript opret; 
    uint8_t evalcode = EVAL_TOKENS;
    funcid = 'c'; // override the param

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << origpubkey << name << description; \
        if(!vopretNonfungible.empty()) ss << vopretNonfungible );
    return(opret);
}

//  this is for other contracts which use tokens and build customized extra payloads to token's opret:
CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, CScript payload)
{
    std::vector<uint8_t> vpayloadNonfungibleEmpty;
    return EncodeTokenOpRet(tokenid, voutPubkeys, vpayloadNonfungibleEmpty, payload);
}

CScript EncodeTokenOpRet(uint256 tokenid, std::vector<CPubKey> voutPubkeys, std::vector<uint8_t> vpayloadNonfungible, CScript payload)
{
    CScript opret; 
	uint8_t tokenFuncId = 't';
	uint8_t evalCodeInOpret = EVAL_TOKENS;

    tokenid = revuint256(tokenid);

	uint8_t ccType = 0;
	if (voutPubkeys.size() >= 0 && voutPubkeys.size() <= 2)
		ccType = voutPubkeys.size();
    else {
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "EncodeTokenOpRet voutPubkeys.size()=" << voutPubkeys.size() << " not supported" << std::endl);
    }

	std::vector<uint8_t> vpayload;
	GetOpReturnData(payload, vpayload);

    opret << OP_RETURN << E_MARSHAL(ss << evalCodeInOpret << tokenFuncId << tokenid << ccType; \
        if (ccType >= 1) ss << voutPubkeys[0];				                \
        if (ccType == 2) ss << voutPubkeys[1];			                    \
        if (vpayloadNonfungible.size() > 0) ss << vpayloadNonfungible;      \
        if (vpayload.size() > 0) ss << vpayload);

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

// overload for fungible:
uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey, std::vector<uint8_t> &origpubkey, std::string &name, std::string &description) {
    std::vector<uint8_t>  vopretNonfungibleDummy;
    return DecodeTokenCreateOpRet(scriptPubKey, origpubkey, name, description, vopretNonfungibleDummy);
}

uint8_t DecodeTokenCreateOpRet(const CScript &scriptPubKey,std::vector<uint8_t> &origpubkey,std::string &name,std::string &description, std::vector<uint8_t>  &vopretNonfungible)
{
    std::vector<uint8_t> vopret; uint8_t dummyEvalcode, funcid, *script;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();

    if ( script != 0 && vopret.size() > 2 && script[0] == EVAL_TOKENS && script[1] == 'c' )
    {
        if( E_UNMARSHAL(vopret, ss >> dummyEvalcode; ss >> funcid; ss >> origpubkey; ss >> name; ss >> description;    \
            // we suppose in 'c' opret it might be only non-fungible payload and not any assets/heir/etc payloads
            if(!ss.eof()) ss >> vopretNonfungible ) )
            return(funcid);
    }
    return (uint8_t)0;
}

// overload for compatibility allows only usual fungible tokens:
// warning: it makes vopret marshalling to CScript because this is what caller would expect
uint8_t DecodeTokenOpRet(const CScript scriptPubKey, uint8_t &evalCode, uint256 &tokenid, std::vector<CPubKey> &voutPubkeys, std::vector<uint8_t>  &vopretExtra) {
    std::vector<uint8_t>  vopret1, vopret2;
    uint8_t funcId = DecodeTokenOpRet(scriptPubKey, evalCode, tokenid, voutPubkeys, vopret1, vopret2);

    CScript opretExtra;
    vopretExtra.clear();

    // make marshalling for compatibility
    // callers of this func expect length of full array at the beginning (and they will make 'vopretStripped' from vopretExtra)
    if (vopret2.empty())
        opretExtra << OP_RETURN << E_MARSHAL(ss << vopret1);  // if first opret (or no oprets)
    else
        opretExtra << OP_RETURN << E_MARSHAL(ss << vopret2);  // if both oprets present, return assets/heir/gateways/... opret (dump non-fungible opret) 

    GetOpReturnData(opretExtra, vopretExtra);
    return funcId;
} 

uint8_t DecodeTokenOpRet(const CScript scriptPubKey, uint8_t &evalCodeTokens, uint256 &tokenid, std::vector<CPubKey> &voutPubkeys, std::vector<uint8_t>  &vopret1, std::vector<uint8_t>  &vopret2)
{
    std::vector<uint8_t> vopret, extra, dummyPubkey; 
	uint8_t funcId=0, *script, dummyEvalCode, dummyFuncId, ccType;
	std::string dummyName; std::string dummyDescription;
	CPubKey voutPubkey1, voutPubkey2;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
	tokenid = zeroid;

    if (script != NULL && vopret.size() > 2)
    {
		// NOTE: if parse error occures, parse might not be able to set error. It is safer to treat that it was eof if it is not set!
		bool isEof = true;

		evalCodeTokens = script[0];
		if (evalCodeTokens != EVAL_TOKENS)
			return (uint8_t)0;

        funcId = script[1];
        LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << "DecodeTokenOpRet decoded funcId=" << (char)(funcId?funcId:' '));

        switch( funcId )
        {
            case 'c': 
				return DecodeTokenCreateOpRet(scriptPubKey, dummyPubkey, dummyName, dummyDescription, vopret1);
				//break;
            case 't':  
			//not used yet: case 'l':
				// NOTE: 'E_UNMARSHAL result==false' means 'parse error' OR 'not eof state'. Consequently, 'result==false' but 'isEof==true' means just 'parse error' 
                if (E_UNMARSHAL(vopret, ss >> dummyEvalCode; ss >> dummyFuncId; ss >> tokenid; ss >> ccType;    \
                    if (ccType >= 1) ss >> voutPubkey1;                                                         \
                    if (ccType == 2) ss >> voutPubkey2;                                                         \
                    isEof = ss.eof();                                                                           \
                    if (!isEof) ss >> vopret1;                                                                  \
                    isEof = ss.eof();                                                                           \
                    if (!isEof) { ss >> vopret2; }                                                              \
                    // if something else remains -> bad format
                    isEof = ss.eof()) || !isEof)
				{

					if (!(ccType >= 0 && ccType <= 2)) { //incorrect ccType
                        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenOpRet() incorrect ccType=" << (int)ccType << " tokenid=" << revuint256(tokenid).GetHex() << std::endl);
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
                LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenOpRet() bad opret format, isEof=" << isEof << " ccType=" << ccType << " tokenid=" << revuint256(tokenid).GetHex() << std::endl);
				return (uint8_t)0;

            default:
                LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenOpRet() illegal funcid=" << (int)funcId << std::endl);
				return (uint8_t)0;
        }
    }
	else	{
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "DecodeTokenOpRet() empty opret, could not parse" << std::endl);
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
	std::vector<uint8_t> vopret1, vopret2, tmporigpubkey, ignorepubkey;
	uint8_t funcid, evalCodeInOpret;
	char destaddr[64], origaddr[64], CCaddr[64];
	std::vector<CPubKey> voutTokenPubkeys;

	//return true;

	numvins = tx.vin.size();
	numvouts = tx.vout.size();
	outputs = inputs = 0;
	preventCCvins = preventCCvouts = -1;

	if ((funcid = DecodeTokenOpRet(tx.vout[numvouts - 1].scriptPubKey, evalCodeInOpret, tokenid, voutTokenPubkeys, vopret1, vopret2)) == 0)
		return eval->Invalid("TokenValidate: invalid opreturn payload");

    LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "TokensValidate funcId=" << (char)(funcid?funcid:' ') << " evalcode=" << cp->evalcode << std::endl);

    if (eval->GetTxUnconfirmed(tokenid, createTx, hashBlock) == 0)
    //if (myGetTransaction(tokenid, createTx, hashBlock) == 0)
    {
        fprintf(stderr,"tokenid.%s\n",tokenid.GetHex());
		return eval->Invalid("cant find token create txid");
    }
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

    // non-fungible tokens validation:
    std::vector<uint8_t> vopretNonfungible;
    GetNonfungibleData(tokenid, vopretNonfungible);
    if (vopretNonfungible.size() > 0 && vopretNonfungible != vopret1)  // assuming tx vopretNonfungible in vopret1
        return eval->Invalid("incorrect or empty non-fungible data");


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

        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "token transfer preliminarily validated inputs=" << inputs << "->outputs=" << outputs << " preventCCvins=" << preventCCvins<< " preventCCvouts=" << preventCCvouts << std::endl);
		break;  // breaking to other contract validation...

	default:
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "illegal tokens funcid=" << (char)(funcid?funcid:' ') << std::endl);
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

// extract cc token vins' pubkeys:
bool ExtractTokensVinPubkeys(CTransaction tx, std::vector<CPubKey> &vinPubkeys) {

	bool found = false;
	CPubKey pubkey;
	struct CCcontract_info *cpTokens, tokensC;

	cpTokens = CCinit(&tokensC, EVAL_TOKENS);

	for (int32_t i = 0; i < tx.vin.size(); i++)
	{	
        // check for cc token vins:
		if( (*cpTokens->ismyvin)(tx.vin[i].scriptSig) )
		{

			auto findEval = [](CC *cond, struct CCVisitor _) {
				bool r = false; 

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
uint8_t ValidateTokenOpret(CTransaction tx, uint256 tokenid) {

	uint256 tokenidOpret = zeroid;
	uint8_t funcid;
	uint8_t dummyEvalCode;
    std::vector<CPubKey> voutPubkeysDummy;
    std::vector<uint8_t> vopretExtraDummy;

	// this is just for log messages indentation fur debugging recursive calls:
	std::string indentStr = std::string().append(tokenValIndentSize, '.');

    if (tx.vout.size() == 0)
        return (uint8_t)0;

	if ((funcid = DecodeTokenOpRet(tx.vout.back().scriptPubKey, dummyEvalCode, tokenidOpret, voutPubkeysDummy, vopretExtraDummy)) == 0)
	{
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << indentStr << "ValidateTokenOpret() DecodeTokenOpret could not parse opret for txid=" << tx.GetHash().GetHex() << std::endl);
		return (uint8_t)0;
	}
	else if (funcid == 'c')
	{
		if (tokenid != zeroid && tokenid == tx.GetHash()) {
            LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << indentStr << "ValidateTokenOpret() this is the tokenbase 'c' tx, txid=" << tx.GetHash().GetHex() << " returning true" << std::endl);
			return funcid;
		}
        else {
            LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << indentStr << "ValidateTokenOpret() not my tokenbase txid=" << tx.GetHash().GetHex() << std::endl);
        }
	}
	else if (funcid == 't')  
	{
		//std::cerr << indentStr << "ValidateTokenOpret() tokenid=" << tokenid.GetHex() << " tokenIdOpret=" << tokenidOpret.GetHex() << " txid=" << tx.GetHash().GetHex() << std::endl;
		if (tokenid != zeroid && tokenid == tokenidOpret) {
            LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << indentStr << "ValidateTokenOpret() this is a transfer 't' tx, txid=" << tx.GetHash().GetHex() << " returning true" << std::endl);
			return funcid;
		}
        else {
            LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << indentStr << "ValidateTokenOpret() not my tokenid=" << tokenidOpret.GetHex() << std::endl);
        }
	}
    else {
        LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << indentStr << "ValidateTokenOpret() not supported funcid=" << (char)funcid << " tokenIdOpret=" << tokenidOpret.GetHex() << " txid=" << tx.GetHash().GetHex() << std::endl);
    }
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
	
    LOGSTREAM((char *)"cctokens", CCLOG_DEBUG2, stream << indentStr << "IsTokensvout() entered for txid=" << tx.GetHash().GetHex() << " v=" << v << " for tokenid=" << reftokenid.GetHex() <<  std::endl);

    int32_t n = tx.vout.size();
    // just check boundaries:
    if (n == 0 || v < 0 || v >= n-1) {  
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << indentStr << "isTokensvout() incorrect params: (n == 0 or v < 0 or v >= n-1)" << " v=" << v << " n=" << n << " returning 0" << std::endl);
        return(0);
    }

	//TODO: validate cc vouts are EVAL_TOKENS!
	if (tx.vout[v].scriptPubKey.IsPayToCryptoCondition()) // maybe check address too? dimxy: possibly no, because there are too many cases with different addresses here
	{
		if (goDeeper) {
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
                    LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << indentStr << "IsTokensvout() warning: for the verified tx detected a bad vintx=" << tx.GetHash().GetHex() << ": cc inputs != cc outputs and not the 'tokenbase' tx, skipping the verified tx" << std::endl);
					return 0;
				}
			}
		}

		// token opret most important checks (tokenid == reftokenid, tokenid is non-zero, tx is 'tokenbase'):
		const uint8_t funcId = ValidateTokenOpret(tx, reftokenid);
		//std::cerr << indentStr << "IsTokensvout() ValidateTokenOpret returned=" << (char)(funcId?funcId:' ') << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
		if (funcId != 0) {
            LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << indentStr << "IsTokensvout() ValidateTokenOpret returned not-null funcId=" << (char)(funcId?funcId:' ') << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl);

            uint8_t dummyEvalCode;
            uint256 tokenIdOpret;
            std::vector<CPubKey> voutPubkeys;
            std::vector<uint8_t> vopret1;
            std::vector<uint8_t> vopret2;
            DecodeTokenOpRet(tx.vout.back().scriptPubKey, dummyEvalCode, tokenIdOpret, voutPubkeys, vopret1, vopret2);

			if (checkPubkeys && funcId != 'c') { // for 'c' there is no pubkeys
                // verify that the vout is token by constructing vouts with the pubkeys in the opret:

                LOGSTREAM((char *)"cctokens", CCLOG_DEBUG2, stream << "IsTokensvout() vopret1=" << HexStr(vopret1) << std::endl);
                LOGSTREAM((char *)"cctokens", CCLOG_DEBUG2, stream << "IsTokensvout() vopret2=" << HexStr(vopret2) << std::endl);

				uint8_t evalCode = EVAL_TOKENS;     // if both payloads are empty maybe it is a transfer to non-payload-one-eval-token vout like GatewaysClaim
                uint8_t evalCode2 = 0;              // will be checked if zero or not
               
                // NOTE: evalcode order in vouts is important: 
                // non-fungible-eval -> EVAL_TOKENS -> assets-eval
                if (vopret1.size() > 0) {
                    evalCode = vopret1.begin()[0];   
                }
                if (vopret2.size() > 0) {
                    evalCode2 = vopret2.begin()[0];   
                }

                // checking vouts for possible token use-cases:
                std::vector<std::pair<CTxOut, std::string>> testVouts;
				// maybe this is dual-eval 1 pubkey or 1of2 pubkey vout?
				if (voutPubkeys.size() >= 1 && voutPubkeys.size() <= 2) {					
					// check dual/three-eval 1 pubkey vout with the first pubkey
                    testVouts.push_back( std::make_pair(MakeTokensCC1vout(evalCode, evalCode2, tx.vout[v].nValue, voutPubkeys[0]), std::string("three-eval cc1 pk[0]")) );
                    if (evalCode2 != 0) 
                        // also check in backward evalcode order
                        testVouts.push_back( std::make_pair(MakeTokensCC1vout(evalCode2, evalCode, tx.vout[v].nValue, voutPubkeys[0]), std::string("three-eval cc1 pk[0] backward-eval")) );

					if(voutPubkeys.size() == 2)	{
						// check dual/three eval 1of2 pubkeys vout
						testVouts.push_back( std::make_pair(MakeTokensCC1of2vout(evalCode, evalCode2, tx.vout[v].nValue, voutPubkeys[0], voutPubkeys[1]), std::string("three-eval cc1of2")) );
                        // check dual/three eval 1 pubkey vout with the second pubkey
						testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode, evalCode2, tx.vout[v].nValue, voutPubkeys[1]), std::string("three-eval cc1 pk[1]")));
                        if (evalCode2 != 0) {
                            // also check in backward evalcode order:
                            // check dual/three eval 1of2 pubkeys vout
                            testVouts.push_back(std::make_pair(MakeTokensCC1of2vout(evalCode2, evalCode, tx.vout[v].nValue, voutPubkeys[0], voutPubkeys[1]), std::string("three-eval cc1of2 backward-eval")));
                            // check dual/three eval 1 pubkey vout with the second pubkey
                            testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode2, evalCode, tx.vout[v].nValue, voutPubkeys[1]), std::string("three-eval cc1 pk[1] backward-eval")));
                        }
					}
				

					// maybe this is like gatewayclaim to single-eval token?
                    testVouts.push_back(std::make_pair(MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, voutPubkeys[0]), std::string("single-eval cc1 pk[0]")));
                    // maybe this is like FillSell for non-fungible token?
                    if( evalCode != 0 )
                        testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode, tx.vout[v].nValue, voutPubkeys[0]), std::string("dual-eval-token cc1 pk[0]")));
                    if( evalCode2 != 0 )
                        testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode2, tx.vout[v].nValue, voutPubkeys[0]), std::string("dual-eval2-token cc1 pk[0]")));

					if (voutPubkeys.size() == 2) {
                        // the same for pk[1]:
                        testVouts.push_back(std::make_pair(MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, voutPubkeys[1]), std::string("single-eval cc1 pk[1]")));
                        if (evalCode != 0)
                            testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode, tx.vout[v].nValue, voutPubkeys[1]), std::string("dual-eval-token cc1 pk[1]")));
                        if (evalCode2 != 0)
                            testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode2, tx.vout[v].nValue, voutPubkeys[1]), std::string("dual-eval2-token cc1 pk[1]")));
					}
				}

				// maybe it is single-eval or dual/three-eval token change?
				std::vector<CPubKey> vinPubkeys;
				ExtractTokensVinPubkeys(tx, vinPubkeys);
				for(std::vector<CPubKey>::iterator it = vinPubkeys.begin(); it != vinPubkeys.end(); it++) {
                    testVouts.push_back(std::make_pair(MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, *it), std::string("single-eval cc1 self vin pk")));
                    testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode, evalCode2, tx.vout[v].nValue, *it), std::string("three-eval cc1 self vin pk")));

                    if (evalCode2 != 0) 
                        // also check in backward evalcode order:
                        testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode2, evalCode, tx.vout[v].nValue, *it), std::string("three-eval cc1 self vin pk backward-eval")));
				}

                // try all test vouts:
                for (auto t : testVouts) {
                    if (t.first == tx.vout[v]) {
                        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << indentStr << "IsTokensvout() valid amount=" << tx.vout[v].nValue << " msg=" << t.second << " evalCode=" << (int)evalCode << " evalCode2=" << (int)evalCode2 << " txid=" << tx.GetHash().GetHex() << " tokenid=" << reftokenid.GetHex() << std::endl);
                        return tx.vout[v].nValue;
                    }
                }
                LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << indentStr << "IsTokensvout() no valid vouts evalCode=" << (int)evalCode << " evalCode2=" << (int)evalCode2 << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl);
			}
			else	{
                LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << indentStr << "IsTokensvout() returns without pubkey check value=" << tx.vout[v].nValue << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl);
				return tx.vout[v].nValue;
			}
		}

		//std::cerr << indentStr; fprintf(stderr,"IsTokensvout() CC vout v.%d of n=%d amount=%.8f txid=%s\n",v,n,(double)0/COIN, tx.GetHash().GetHex().c_str());
	}
	//std::cerr << indentStr; fprintf(stderr,"IsTokensvout() normal output v.%d %.8f\n",v,(double)tx.vout[v].nValue/COIN);
	return(0);
}

// compares cc inputs vs cc outputs (to prevent feeding vouts from normal inputs)
bool TokensExactAmounts(bool goDeeper, struct CCcontract_info *cp, int64_t &inputs, int64_t &outputs, Eval* eval, const CTransaction &tx, uint256 reftokenid)
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

    LOGSTREAM((char *)"cctokens", CCLOG_DEBUG2, stream << indentStr << "TokensExactAmounts() entered for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl);

	for (int32_t i = 0; i<numvins; i++)
	{												  // check for additional contracts which may send tokens to the Tokens contract
		if ((*cpTokens->ismyvin)(tx.vin[i].scriptSig) /*|| IsVinAllowed(tx.vin[i].scriptSig) != 0*/)
		{
			//std::cerr << indentStr << "TokensExactAmounts() eval is true=" << (eval != NULL) << " ismyvin=ok for_i=" << i << std::endl;
			// we are not inside the validation code -- dimxy
			if ((eval && eval->GetTxUnconfirmed(tx.vin[i].prevout.hash, vinTx, hashBlock) == 0) || (!eval && !myGetTransaction(tx.vin[i].prevout.hash, vinTx, hashBlock)))
			{
                LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << indentStr << "TokensExactAmounts() cannot read vintx for i." << i << " numvins." << numvins << std::endl);
				return (!eval) ? false : eval->Invalid("always should find vin tx, but didnt");
			}
			else {
                LOGSTREAM((char *)"cctokens", CCLOG_DEBUG2, stream << indentStr << "TokenExactAmounts() checking vintx.vout for tx.vin[" << i << "] nValue=" << vinTx.vout[tx.vin[i].prevout.n].nValue << std::endl);

                // validate vouts of vintx  
                tokenValIndentSize++;
				tokenoshis = IsTokensvout(goDeeper, true, cpTokens, eval, vinTx, tx.vin[i].prevout.n, reftokenid);
				tokenValIndentSize--;
				if (tokenoshis != 0)
				{
                    LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << indentStr << "TokensExactAmounts() adding vintx.vout for tx.vin[" << i << "] tokenoshis=" << tokenoshis << std::endl);
					inputs += tokenoshis;
				}
			}
		}
	}

	for (int32_t i = 0; i < numvouts-1; i ++)  // 'numvouts-1' <-- do not check opret
	{
        LOGSTREAM((char *)"cctokens", CCLOG_DEBUG2, stream << indentStr << "TokenExactAmounts() recursively checking tx.vout[" << i << "] nValue=" << tx.vout[i].nValue << std::endl);

        // Note: we pass in here IsTokenvout(false,...) because we don't need to call TokenExactAmounts() recursively from IsTokensvout here
        // indeed, if we pass 'true' we'll be checking this tx vout again
        tokenValIndentSize++;
		tokenoshis = IsTokensvout(false /*<--do not recursion here*/, true /*<--exclude non-tokens vouts*/, cpTokens, eval, tx, i, reftokenid);
		tokenValIndentSize--;

		if (tokenoshis != 0)
		{
            LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << indentStr << "TokensExactAmounts() adding tx.vout[" << i << "] tokenoshis=" << tokenoshis << std::endl);
			outputs += tokenoshis;
		}
	}

	//std::cerr << indentStr << "TokensExactAmounts() inputs=" << inputs << " outputs=" << outputs << " for txid=" << tx.GetHash().GetHex() << std::endl;

	if (inputs != outputs) {
		if (tx.GetHash() != reftokenid)
            LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << indentStr << "TokenExactAmounts() found unequal token cc inputs=" << inputs << " vs cc outputs=" << outputs << " for txid=" << tx.GetHash().GetHex() << " and this is not the create tx" << std::endl);
		return false;  // do not call eval->Invalid() here!
	}
	else
		return true;
}

// get non-fungible data from 'tokenbase' tx (the data might be empty)
void GetNonfungibleData(uint256 tokenid, std::vector<uint8_t> &vopretNonfungible)
{
    CTransaction tokenbasetx;
    uint256 hashBlock;

    if (!myGetTransaction(tokenid, tokenbasetx, hashBlock)) {
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "SetNonfungibleEvalCode() cound not load token creation tx=" << tokenid.GetHex() << std::endl);
        return;
    }

    vopretNonfungible.clear();
    // check if it is non-fungible tx and get its second evalcode from non-fungible payload
    if (tokenbasetx.vout.size() > 0) {
        uint8_t dummyEvalCode;
        uint256 tokenIdOpret;
        std::vector<CPubKey> voutPubkeys;
        std::vector<uint8_t> vopretExtra;
        DecodeTokenOpRet(tokenbasetx.vout.back().scriptPubKey, dummyEvalCode, tokenIdOpret, voutPubkeys, vopretNonfungible, vopretExtra);
    }
}


// overload, adds inputs from token cc addr
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 tokenid, int64_t total, int32_t maxinputs) {
    std::vector<uint8_t> vopretNonfungibleDummy;
    return AddTokenCCInputs(cp, mtx, pk, tokenid, total, maxinputs, vopretNonfungibleDummy);
}

// adds inputs from token cc addr and returns non-fungible opret payload if present
// also sets evalcode in cp, if needed
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 tokenid, int64_t total, int32_t maxinputs, std::vector<uint8_t> &vopretNonfungible)
{
	char tokenaddr[64], destaddr[64]; 
	int64_t threshold, nValue, price, totalinputs = 0;  
	int32_t n = 0;
	std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    GetNonfungibleData(tokenid, vopretNonfungible);
    if (vopretNonfungible.size() > 0)
        cp->additionalTokensEvalcode2 = vopretNonfungible.begin()[0];

	GetTokensCCaddress(cp, tokenaddr, pk);
	SetCCunspents(unspentOutputs, tokenaddr);

    if (unspentOutputs.empty()) {
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "AddTokenCCInputs() no utxos for token dual/three eval addr=" << tokenaddr << " evalcode=" << (int)cp->evalcode << " additionalTokensEvalcode2=" << (int)cp->additionalTokensEvalcode2 << std::endl);
    }

	threshold = total / (maxinputs != 0 ? maxinputs : 64); // TODO: maxinputs really could not be over 64? what if i want to calc total balance for all available uxtos?
                                                           // maybe it is better to add all uxtos if maxinputs == 0

	for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
	{
        CTransaction vintx;
        uint256 hashBlock;
        uint256 vintxid = it->first.txhash;
		int32_t vout = (int32_t)it->first.index;

		if (it->second.satoshis < threshold)            // this should work also for non-fungible tokens (there should be only 1 satoshi for non-fungible token issue)
			continue;

        int32_t ivin;
		for (ivin = 0; ivin < mtx.vin.size(); ivin ++)
			if (vintxid == mtx.vin[ivin].prevout.hash && vout == mtx.vin[ivin].prevout.n)
				break;
		if (ivin != mtx.vin.size()) // that is, the tx.vout is already added to mtx.vin (in some previous calls)
			continue;

		if (GetTransaction(vintxid, vintx, hashBlock, false) != 0)
		{
			Getscriptaddress(destaddr, vintx.vout[vout].scriptPubKey);
			if (strcmp(destaddr, tokenaddr) != 0 && 
                strcmp(destaddr, cp->unspendableCCaddr) != 0 &&   // TODO: check why this. Should not we add token inputs from unspendable cc addr if mypubkey is used?
                strcmp(destaddr, cp->unspendableaddr2) != 0)      // or the logic is to allow to spend all available tokens (what about unspendableaddr3)?
				continue;
			
            LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << "AddTokenCCInputs() check vintx vout destaddress=" << destaddr << " amount=" << vintx.vout[vout].nValue << std::endl);

			if ((nValue = IsTokensvout(true, true/*<--add only valid token uxtos */, cp, NULL, vintx, vout, tokenid)) > 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,vintxid, vout) == 0)
			{
				//for non-fungible tokens check payload:
                if (!vopretNonfungible.empty()) {
                    std::vector<uint8_t> vopret;

                    // check if it is non-fungible token:
                    GetNonfungibleData(tokenid, vopret);
                    if (vopret != vopretNonfungible) {
                        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "AddTokenCCInputs() found incorrect non-fungible opret payload for vintxid=" << vintxid.GetHex() << std::endl);
                        continue;
                    }

                    // non-fungible evalCode2 cc contract should also check if there exists only one non-fungible vout with amount = 1
                }

                
                if (total != 0 && maxinputs != 0)  // if it is not just to calc amount...
					mtx.vin.push_back(CTxIn(vintxid, vout, CScript()));

				nValue = it->second.satoshis;
				totalinputs += nValue;
                LOGSTREAM((char *)"cctokens", CCLOG_DEBUG1, stream << "AddTokenCCInputs() adding input nValue=" << nValue  << std::endl);
				n++;

				if ((total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs))
					break;
			}
		}
	}

	//std::cerr << "AddTokenCCInputs() found totalinputs=" << totalinputs << std::endl;
	return(totalinputs);
}


std::string CreateToken(int64_t txfee, int64_t tokensupply, std::string name, std::string description, std::vector<uint8_t> nonfungibleData)
{
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	CPubKey mypk; struct CCcontract_info *cp, C;
	if (tokensupply < 0)
	{
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "CreateToken() negative tokensupply=" << tokensupply << std::endl);
		return std::string("");
	}
    if (!nonfungibleData.empty() && tokensupply != 1) {
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "CreateToken() for non-fungible tokens tokensupply should be equal to 1" << std::endl);
        CCerror = "for non-fungible tokens tokensupply should be equal to 1";
        return std::string("");
    }

	
	cp = CCinit(&C, EVAL_TOKENS);
	if (name.size() > 32 || description.size() > 4096)  // this is also checked on rpc level
	{
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "name of=" << name.size() << " or description of=" << description.size() << " is too big" << std::endl);
        CCerror = "name should be < 32, description should be < 4096";
		return("");
	}
	if (txfee == 0)
		txfee = 10000;
	mypk = pubkey2pk(Mypubkey());

	if (AddNormalinputs(mtx, mypk, tokensupply + 2 * txfee, 64) > 0)
	{
        uint8_t destEvalCode = EVAL_TOKENS;
        if( nonfungibleData.size() > 0 )
            destEvalCode = nonfungibleData.begin()[0];

		mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, tokensupply, mypk));
		mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(cp->CChexstr) << OP_CHECKSIG));
		return(FinalizeCCTx(0, cp, mtx, mypk, txfee, EncodeTokenCreateOpRet('c', Mypubkey(), name, description, nonfungibleData)));
	}

    LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "cant find normal inputs" << std::endl);
    CCerror = "cant find normal inputs";
    return std::string("");
}

// transfer tokens to another pubkey
// param additionalEvalCode allows transfer of dual-eval non-fungible tokens
std::string TokenTransfer(int64_t txfee, uint256 tokenid, std::vector<uint8_t> destpubkey, int64_t total)
{
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	CPubKey mypk; uint64_t mask; int64_t CCchange = 0, inputs = 0;  struct CCcontract_info *cp, C;
	std::vector<uint8_t> vopretNonfungible;

	if (total < 0)	{
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "negative total=" << total << std::endl);
		return("");
	}

	cp = CCinit(&C, EVAL_TOKENS);

	if (txfee == 0)
		txfee = 10000;
	mypk = pubkey2pk(Mypubkey());
	if (AddNormalinputs(mtx, mypk, txfee, 3) > 0)
	{
		mask = ~((1LL << mtx.vin.size()) - 1);  // seems, mask is not used anymore
        
		if ((inputs = AddTokenCCInputs(cp, mtx, mypk, tokenid, total, 60, vopretNonfungible)) > 0)  // NOTE: AddTokenCCInputs might set cp->additionalEvalCode which is used in FinalizeCCtx!
		{
			if (inputs < total) {   //added dimxy
                LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "TokenTransfer(): insufficient token funds" << std::endl);
                CCerror = strprintf("insufficient token inputs");
				return std::string("");
			}

            uint8_t destEvalCode = EVAL_TOKENS;
            if (vopretNonfungible.size() > 0) 
                destEvalCode = vopretNonfungible.begin()[0];
            
			if (inputs > total)
				CCchange = (inputs - total);
			mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, total, pubkey2pk(destpubkey)));  // if destEvalCode == EVAL_TOKENS then it is actually MakeCC1vout(EVAL_TOKENS,...)
			if (CCchange != 0)
				mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, CCchange, mypk));

			std::vector<CPubKey> voutTokenPubkeys;
			voutTokenPubkeys.push_back(pubkey2pk(destpubkey));  // dest pubkey for validating vout

			return(FinalizeCCTx(mask, cp, mtx, mypk, txfee, EncodeTokenOpRet(tokenid, voutTokenPubkeys, vopretNonfungible, CScript()))); 
		}
		else {
            LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "not enough CC token inputs for amount=" << total << std::endl);
            CCerror = strprintf("no token inputs");
		}
		//} else fprintf(stderr,"numoutputs.%d != numamounts.%d\n",n,(int32_t)amounts.size());
	}
	else {
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "not enough normal inputs for txfee" << std::endl);
        CCerror = strprintf("insufficient normal inputs");
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
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "cant find tokenid" << std::endl);
		CCerror = strprintf("cant find tokenid");
		return 0;
	}

	struct CCcontract_info *cp, C;
	cp = CCinit(&C, EVAL_TOKENS);
	return(AddTokenCCInputs(cp, mtx, pk, tokenid, 0, 0));
}

UniValue TokenInfo(uint256 tokenid)
{
	UniValue result(UniValue::VOBJ); 
    uint256 hashBlock; 
    CTransaction vintx; 
    std::vector<uint8_t> origpubkey; 
    std::vector<uint8_t> vopretNonfungible;
    std::string name, description; 

	if (GetTransaction(tokenid, vintx, hashBlock, false) == 0)
	{
		fprintf(stderr, "TokenInfo() cant find tokenid\n");
		result.push_back(Pair("result", "error"));
		result.push_back(Pair("error", "cant find tokenid"));
		return(result);
	}
	if (vintx.vout.size() > 0 && DecodeTokenCreateOpRet(vintx.vout[vintx.vout.size() - 1].scriptPubKey, origpubkey, name, description, vopretNonfungible) == 0)
	{
        LOGSTREAM((char *)"cctokens", CCLOG_INFO, stream << "TokenInfo() passed tokenid isnt token creation txid" << std::endl);
		result.push_back(Pair("result", "error"));
		result.push_back(Pair("error", "tokenid isnt token creation txid"));
	}
	result.push_back(Pair("result", "success"));
	result.push_back(Pair("tokenid", tokenid.GetHex()));
	result.push_back(Pair("owner", HexStr(origpubkey)));
	result.push_back(Pair("name", name));
	result.push_back(Pair("supply", vintx.vout[0].nValue));
	result.push_back(Pair("description", description));
    if( !vopretNonfungible.empty() )    
        result.push_back(Pair("data", HexStr(vopretNonfungible)));

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
