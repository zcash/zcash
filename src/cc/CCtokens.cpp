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
CScript EncodeTokenOpRet(uint8_t tokenFuncId, uint8_t evalCodeInOpret, uint256 tokenid, std::vector<uint8_t> payload)
{
    CScript opret; 
	//uint8_t evalcode = EVAL_TOKENS;
    tokenid = revuint256(tokenid);
	//uint8_t tokenFuncId = (isTransferrable) ? (uint8_t)'t' : (uint8_t)'l';

    opret << OP_RETURN << E_MARSHAL(ss << evalCodeInOpret << tokenFuncId << tokenid << payload);
    return(opret);
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

uint8_t DecodeTokenOpRet(const CScript scriptPubKey, uint8_t &evalCode, uint256 &tokenid, std::vector<uint8_t>  &vopretExtra)
{
    std::vector<uint8_t> vopret, extra, dummyPubkey; 
	uint8_t funcid=0, *script, e, dummyFuncId;
	std::string dummyName; std::string dummyDescription;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
	tokenid = zeroid;

    if (script != 0 /*enable all evals: && script[0] == EVAL_TOKENS*/)
    {
		bool isEof = true;
		evalCode = script[0];
        funcid = script[1];
        //fprintf(stderr,"decode.[%c]\n",funcid);
        switch ( funcid )
        {
            case 'c': 
				return DecodeTokenCreateOpRet(scriptPubKey, dummyPubkey, dummyName, dummyDescription);
				//break;
            case 't':  
			//not used yet: case 'l':
				if (E_UNMARSHAL(vopret, ss >> e; ss >> dummyFuncId; ss >> tokenid; isEof = ss.eof(); vopretExtra = std::vector<uint8_t>(ss.begin(), ss.end())) || !isEof)
				{
					tokenid = revuint256(tokenid);
					return(funcid);
				}
				std::cerr << "DecodeTokenOpRet() isEof=" << isEof << std::endl;
				fprintf(stderr, "DecodeTokenOpRet() bad opret format\n");  // this may be just check, no error logging
				return (uint8_t)0;

            default:
                fprintf(stderr, "DecodeTokenOpRet() illegal funcid.%02x\n", funcid);
				return (uint8_t)0;
        }
    }
	return (uint8_t)0;
}



// tx validation
bool TokensValidate(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn)
{
	static uint256 zero;
	CTxDestination address; CTransaction vinTx, createTx; uint256 hashBlock, tokenid, tokenid2;
	int32_t i, starti, numvins, numvouts, preventCCvins, preventCCvouts;
	int64_t remaining_price, nValue, tokenoshis, outputs, inputs, tmpprice, totalunits, ignore; std::vector<uint8_t> origpubkey, tmporigpubkey, ignorepubkey;
	uint8_t funcid, evalCodeInOpret;
	char destaddr[64], origaddr[64], CCaddr[64];

	numvins = tx.vin.size();
	numvouts = tx.vout.size();
	outputs = inputs = 0;
	preventCCvins = preventCCvouts = -1;

	if ((funcid = DecodeTokenOpRet(tx.vout[numvouts - 1].scriptPubKey, evalCodeInOpret, tokenid, origpubkey)) == 0)
		return eval->Invalid("TokenValidate: invalid opreturn payload");

	fprintf(stderr, "TokensValidate (%c)\n", funcid);

	if (eval->GetTxUnconfirmed(tokenid, createTx, hashBlock) == 0)
		return eval->Invalid("cant find token create txid");
	else if (IsCCInput(tx.vin[0].scriptSig) != 0)
		return eval->Invalid("illegal token vin0");  // why? (dimxy)
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

	// init for forwarding validation call
	struct CCcontract_info *cpOther = NULL, C;
	if (evalCodeInOpret != EVAL_TOKENS)
		cpOther = CCinit(&C, evalCodeInOpret);

	switch (funcid)
	{
	case 'c': // create wont be called to be verified as it has no CC inputs
			  //vin.0: normal input
			  //vout.0: issuance tokenoshis to CC
			  //vout.1: normal output for change (if any)
			  //vout.n-1: opreturn EVAL_TOKENS 'c' <tokenname> <description>
		if (evalCodeInOpret != EVAL_TOKENS)
			return eval->Invalid("unexpected TokenValidate for createtoken");
		else
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
	if (cpOther)
		return cpOther->validate(cpOther, eval, tx, nIn);
	else
		return eval->Invalid("unsupported evalcode in opret");

	// what does this do?
	// return(PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts));
}



// this is just for log messages indentation fur debugging recursive calls:
thread_local uint32_t tokenValIndentSize = 0;

// validates opret for token tx:
bool ValidateTokenOpret(CTransaction tx, int32_t v, uint256 tokenid, std::vector<uint8_t> &vopretExtra) {

	uint256 tokenidOpret, tokenidOpret2;
	uint8_t funcid, evalCode;

	// this is just for log messages indentation fur debugging recursive calls:
	std::string indentStr = std::string().append(tokenValIndentSize, '.');

	int32_t n = tx.vout.size();

	if ((funcid = DecodeTokenOpRet(tx.vout[n - 1].scriptPubKey, evalCode, tokenidOpret, vopretExtra)) == 0)
	{
		std::cerr << indentStr << "ValidateTokenOpret() DecodeOpret returned null for n-1=" << n - 1 << " txid=" << tx.GetHash().GetHex() << std::endl;
		return(false);
	}
	else if (funcid == 'c')
	{
		if (tokenid != zeroid && tokenid == tx.GetHash() && v == 0) {
			//std::cerr << indentStr << "ValidateTokenOpret() this is the tokenbase 'c' tx, txid=" << tx.GetHash().GetHex() << " vout=" << v << " returning true" << std::endl;
			return(true);
		}
	}
	else if (funcid == 't')  
	{
		//std::cerr << indentStr << "ValidateTokenOpret() tokenid=" << tokenid.GetHex() << " tokenIdOpret=" << tokenidOpret.GetHex() << " txid=" << tx.GetHash().GetHex() << std::endl;
		if (tokenid != zeroid && tokenid == tokenidOpret) {
			//std::cerr << indentStr << "ValidateTokenOpret() this is a transfer 't' tx, txid=" << tx.GetHash().GetHex() << " vout=" << v << " returning true" << std::endl;
			return(true);
		}
	}
	//std::cerr << indentStr << "ValidateTokenOpret() return false funcid=" << (char)funcid << " tokenid=" << tokenid.GetHex() << " tokenIdOpret=" << tokenidOpret.GetHex() << " txid=" << tx.GetHash().GetHex() << std::endl;
	return false;
}


// Checks if the vout is a really Tokens CC vout
// compareTotals == true, the func also validates the passed transaction itself: 
// it should be either sum(cc vins) == sum(cc vouts) or the transaction is the 'tokenbase' ('c') tx
int64_t IsTokensvout(bool compareTotals, struct CCcontract_info *cp, Eval* eval, std::vector<uint8_t> &vopretExtra, const CTransaction& tx, int32_t v, uint256 reftokenid)
{

	// this is just for log messages indentation fur debugging recursive calls:
	std::string indentStr = std::string().append(tokenValIndentSize, '.');
	//std::cerr << indentStr << "IsTokensvout() entered for txid=" << tx.GetHash().GetHex() << " v=" << v << " for tokenid=" << reftokenid.GetHex() <<  std::endl;

	//TODO: validate cc vouts are EVAL_TOKENS!
	if (tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0) // maybe check address too? dimxy: possibly no, because there are too many cases with different addresses here
	{
		int32_t n = tx.vout.size();
		// just check boundaries:
		if (v >= n - 1) {  // just moved this up (dimxy)
			std::cerr << indentStr << "isTokensvout() internal err: (v >= n - 1), returning 0" << std::endl;
			return(0);
		}

		if (compareTotals) {
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
		const bool valOpret = ValidateTokenOpret(tx, v, reftokenid, vopretExtra);
		//std::cerr << indentStr << "IsTokensvout() ValidateTokenOpret returned=" << std::boolalpha << valOpret << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
		if (valOpret) {
			//std::cerr << indentStr << "IsTokensvout() ValidateTokenOpret returned true, returning nValue=" << tx.vout[v].nValue << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl;
			return tx.vout[v].nValue;
		}

		//std::cerr << indentStr; fprintf(stderr,"IsTokensvout() CC vout v.%d of n=%d amount=%.8f txid=%s\n",v,n,(double)0/COIN, tx.GetHash().GetHex().c_str());
	}
	//std::cerr << indentStr; fprintf(stderr,"IsTokensvout() normal output v.%d %.8f\n",v,(double)tx.vout[v].nValue/COIN);
	return(0);
}

// compares cc inputs vs cc outputs (to prevent feeding vouts from normal inputs)
bool TokensExactAmounts(bool compareTotals, struct CCcontract_info *cpTokens, int64_t &inputs, int64_t &outputs, Eval* eval, const CTransaction &tx, uint256 tokenid)
{
	CTransaction vinTx; uint256 hashBlock, id, id2; int32_t flag; int64_t tokenoshis; std::vector<uint8_t> tmporigpubkey; int64_t tmpprice;
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
				tokenoshis = IsTokensvout(compareTotals, cpTokens, eval, tmporigpubkey, vinTx, tx.vin[i].prevout.n, tokenid);
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
		// Note: we pass in here 'false' because we don't need to call TokenExactAmounts() recursively from IsTokenvout
		// indeed, in this case we'll be checking this tx again
		tokenoshis = IsTokensvout(false, cpTokens, eval, tmporigpubkey, tx, i, tokenid);
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
			std::cerr << indentStr << "TokenExactAmounts() found unequal inputs=" << inputs << " vs outputs=" << outputs << " for txid=" << tx.GetHash().GetHex() << " and this is not create tx" << std::endl;
		return false;  // do not call eval->Invalid() here!
	}
	else
		return true;
}

// add inputs from token cc addr
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey pk, uint256 tokenid, int64_t total, int32_t maxinputs)
{
	char coinaddr[64], destaddr[64]; 
	int64_t threshold, nValue, price, totalinputs = 0; 
	uint256 txid, hashBlock; 
	std::vector<uint8_t> vopretExtra;
	CTransaction vintx; 
	int32_t j, vout, n = 0;
	std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

	GetCCaddress(cp, coinaddr, pk);
	SetCCunspents(unspentOutputs, coinaddr);

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
			if (strcmp(destaddr, coinaddr) != 0 && strcmp(destaddr, cp->unspendableCCaddr) != 0 && strcmp(destaddr, cp->unspendableaddr2) != 0)
				continue;
			fprintf(stderr, "AddTokenCCInputs() check destaddress=%s vout amount=%.8f\n", destaddr, (double)vintx.vout[vout].nValue / COIN);
			if ((nValue = IsTokensvout(true, cp, NULL, vopretExtra, vintx, vout, tokenid)) > 0 && myIsutxo_spentinmempool(txid, vout) == 0)
			{
				if (total != 0 && maxinputs != 0)
					mtx.vin.push_back(CTxIn(txid, vout, CScript()));
				nValue = it->second.satoshis;
				totalinputs += nValue;
				//std::cerr << "AddTokenInputs() adding input nValue=" << nValue  << std::endl;
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
			mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, total, pubkey2pk(destpubkey)));
			if (CCchange != 0)
				mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, CCchange, mypk));
			return(FinalizeCCTx(mask, cp, mtx, mypk, txfee, EncodeTokenOpRet('t', EVAL_TOKENS, assetid, emptyExtraOpret)));  // By setting EVA_TOKENS we're getting out from assets validation code
		}
		else fprintf(stderr, "not enough CC asset inputs for %.8f\n", (double)total / COIN);
		//} else fprintf(stderr,"numoutputs.%d != numamounts.%d\n",n,(int32_t)amounts.size());
	}
	return("");
}