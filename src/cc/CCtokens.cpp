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

#include "CCtokens.h"
#include "old/CCtokens_v0.h"
#include "importcoin.h"

/* TODO: correct this:
  tokens cc tx creation and validation code 
*/


// helper funcs:

// extract cc token vins' pubkeys:
static bool ExtractTokensCCVinPubkeys(const CTransaction &tx, std::vector<CPubKey> &vinPubkeys) {

	bool found = false;
	CPubKey pubkey;
	struct CCcontract_info *cpTokens, tokensC;

	cpTokens = CCinit(&tokensC, EVAL_TOKENS);
    vinPubkeys.clear();

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


bool IsEqualVouts(const CTxOut &v1, const CTxOut &v2)
{
    char addr1[KOMODO_ADDRESS_BUFSIZE];
    char addr2[KOMODO_ADDRESS_BUFSIZE];
    Getscriptaddress(addr1, v1.scriptPubKey);
    Getscriptaddress(addr2, v2.scriptPubKey);
    return strcmp(addr1, addr2) == 0;
}


// this is just for log messages indentation fur debugging recursive calls:
thread_local uint32_t tokenValIndentSize = 0;

// validates opret for token tx:
static uint8_t ValidateTokenOpret(uint256 txid, const CScript &scriptPubKey, uint256 tokenid) {

	uint256 tokenidOpret = zeroid;
	uint8_t funcid;
    std::vector<CPubKey> voutPubkeysDummy;
    std::vector<vscript_t>  opretsDummy;

	// this is just for log messages indentation fur debugging recursive calls:
	std::string indentStr = std::string().append(tokenValIndentSize, '.');

    //if (tx.vout.size() == 0)
    //    return (uint8_t)0;

	if ((funcid = DecodeTokenOpRetV1(scriptPubKey, tokenidOpret, voutPubkeysDummy, opretsDummy)) == 0)
	{
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << indentStr << "could not parse opret for txid=" << txid.GetHex() << std::endl);
		return (uint8_t)0;
	}
	else if (IsTokenCreateFuncid(funcid))
	{
		if (tokenid != zeroid && tokenid == txid) {
            LOGSTREAMFN(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "this is tokenbase 'c' tx, txid=" << txid.GetHex() << " returning true" << std::endl);
			return funcid;
		}
        else {
            LOGSTREAMFN(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "not my tokenbase txid=" << txid.GetHex() << std::endl);
        }
	}
    /* 'i' not used 
    else if (funcid == 'i')
    {
        if (tokenid != zeroid && tokenid == tx.GetHash()) {
            LOGSTREAM(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "ValidateTokenOpret() this is import 'i' tx, txid=" << tx.GetHash().GetHex() << " returning true" << std::endl);
            return funcid;
        }
        else {
            LOGSTREAM(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "ValidateTokenOpret() not my import txid=" << tx.GetHash().GetHex() << std::endl);
        }
    }*/
	else if (IsTokenTransferFuncid(funcid))  
	{
		//std::cerr << indentStr << "ValidateTokenOpret() tokenid=" << tokenid.GetHex() << " tokenIdOpret=" << tokenidOpret.GetHex() << " txid=" << tx.GetHash().GetHex() << std::endl;
		if (tokenid != zeroid && tokenid == tokenidOpret) {
            LOGSTREAMFN(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "this is a transfer 't' tx, txid=" << txid.GetHex() << " returning true" << std::endl);
			return funcid;
		}
        else {
            LOGSTREAMFN(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "not my tokenid=" << tokenidOpret.GetHex() << std::endl);
        }
	}
    else {
        LOGSTREAMFN(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "not supported funcid=" << (char)funcid << " tokenIdOpret=" << tokenidOpret.GetHex() << " txid=" << txid.GetHex() << std::endl);
    }
	return (uint8_t)0;
}


// remove token->unspendablePk (it is only for marker usage)
void FilterOutTokensUnspendablePk(const std::vector<CPubKey> &sourcePubkeys, std::vector<CPubKey> &destPubkeys) {
    struct CCcontract_info *cpTokens, tokensC; 
    cpTokens = CCinit(&tokensC, EVAL_TOKENS);
    CPubKey tokensUnspendablePk = GetUnspendable(cpTokens, NULL);
    destPubkeys.clear();

    for (auto pk : sourcePubkeys)
        if (pk != tokensUnspendablePk)
            destPubkeys.push_back(pk);

}

// TODO: move it to CCutils.cpp
// get OP_DROP data:
bool MyGetCCopretV2(const CScript &scriptPubKey, CScript &opret)
{
    std::vector<std::vector<unsigned char>> vParams;
    CScript dummy; 

    if (scriptPubKey.IsPayToCryptoCondition(&dummy, vParams) != 0)
    {
        if (vParams.size() >= 1)  // allow more data after cc opret
        {
            //uint8_t version;
            //uint8_t evalCode;
            //uint8_t m, n;
            std::vector< vscript_t > vData;

            // parse vParams[0] as script
            CScript inScript(vParams[0].begin(), vParams[0].end());
            CScript::const_iterator pc = inScript.begin();
            inScript.GetPushedData(pc, vData);

            if (vData.size() > 1 && vData[0].size() == 4) // first vector is 4-byte header
            {
                //vscript_t vopret(vParams[0].begin() + 6, vParams[0].end());
                opret << OP_RETURN << vData[1];  // return vData[1] as cc opret
                return true;
            }
        }
    }
    return false;
}

// checks if any token vouts are sent to 'dead' pubkey
CAmount HasBurnedTokensvouts(const CTransaction& tx, uint256 reftokenid)
{
    uint8_t dummyEvalCode;
    uint256 tokenIdOpret;
    std::vector<CPubKey> vDeadPubkeys, voutPubkeysDummy;
    std::vector<vscript_t>  oprets;
    vscript_t vopretExtra, vopretNonfungible;

    uint8_t evalCode = EVAL_TOKENS;     // if both payloads are empty maybe it is a transfer to non-payload-one-eval-token vout like GatewaysClaim
    uint8_t evalCode2 = 0;              // will be checked if zero or not

    // test vouts for possible token use-cases:
    std::vector<std::pair<CTxOut, std::string>> testVouts;

    int32_t n = tx.vout.size();
    // just check boundaries:
    if (n == 0) {
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << "incorrect params: tx.vout.size() == 0, txid=" << tx.GetHash().GetHex() << std::endl);
        return(0);
    }

    if (DecodeTokenOpRetV1(tx.vout.back().scriptPubKey, tokenIdOpret, voutPubkeysDummy, oprets) == 0) {
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << "cannot parse opret DecodeTokenOpRet returned 0, txid=" << tx.GetHash().GetHex() << std::endl);
        return 0;
    }

    // get assets/channels/gateways token data:
    //FilterOutNonCCOprets(oprets, vopretExtra);  
    // NOTE: only 1 additional evalcode in token opret is currently supported
    if (oprets.size() > 0)
        vopretExtra = oprets[0];

    LOGSTREAMFN(cctokens_log, CCLOG_DEBUG2, stream << "vopretExtra=" << HexStr(vopretExtra) << std::endl);

    GetNonfungibleData(reftokenid, vopretNonfungible);

    if (vopretNonfungible.size() > 0)
        evalCode = vopretNonfungible.begin()[0];
    if (vopretExtra.size() > 0)
        evalCode2 = vopretExtra.begin()[0];

    if (evalCode == EVAL_TOKENS && evalCode2 != 0) {
        evalCode = evalCode2;
        evalCode2 = 0;
    }

    vDeadPubkeys.push_back(pubkey2pk(ParseHex(CC_BURNPUBKEY)));

    CAmount burnedAmount = 0;

    for (int i = 0; i < tx.vout.size(); i++)
    {
        if (tx.vout[i].scriptPubKey.IsPayToCryptoCondition())
        {
            // make all possible token vouts for dead pk:
            for (std::vector<CPubKey>::iterator it = vDeadPubkeys.begin(); it != vDeadPubkeys.end(); it++)
            {
                testVouts.push_back(std::make_pair(MakeCC1vout(EVAL_TOKENS, tx.vout[i].nValue, *it), std::string("single-eval cc1 burn pk")));
                if (evalCode != EVAL_TOKENS)
                    testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode, 0, tx.vout[i].nValue, *it), std::string("two-eval cc1 burn pk")));
                if (evalCode2 != 0) {
                    testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode, evalCode2, tx.vout[i].nValue, *it), std::string("three-eval cc1 burn pk")));
                    // also check in backward evalcode order:
                    testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode2, evalCode, tx.vout[i].nValue, *it), std::string("three-eval cc1 burn pk backward-eval")));
                }
            }

            // try all test vouts:
            for (const auto &t : testVouts) {
                if (t.first == tx.vout[i]) {
                    LOGSTREAMFN(cctokens_log, CCLOG_DEBUG1, stream << "burned amount=" << tx.vout[i].nValue << " msg=" << t.second << " evalCode=" << (int)evalCode << " evalCode2=" << (int)evalCode2 << " txid=" << tx.GetHash().GetHex() << " tokenid=" << reftokenid.GetHex() << std::endl);
                    burnedAmount += tx.vout[i].nValue;
                    break; // do not calc vout twice!
                }
            }
            LOGSTREAMFN(cctokens_log, CCLOG_DEBUG2, stream << "total burned=" << burnedAmount << " evalCode=" << (int)evalCode << " evalCode2=" << (int)evalCode2 << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl);
        }
    }

    return burnedAmount;
}

// validate spending markers from token global cc addr: this is allowed only for burned non-fungible tokens
// returns false if there is marker spending and it is prohibited
// returns true if no marker spending or it is allowed
static bool CheckMarkerSpending(struct CCcontract_info *cp, Eval *eval, const CTransaction &tx, uint256 tokenid)
{
    for (const auto &vin : tx.vin)
    {
        // validate spending from token unspendable cc addr:
        const CPubKey tokenGlobalPk = GetUnspendable(cp, NULL);
        if (cp->ismyvin(vin.scriptSig) && check_signing_pubkey(vin.scriptSig) == tokenGlobalPk)
        {
            bool allowed = false;

            if (vin.prevout.hash == tokenid)  // check if this is my marker
            {
                // calc burned amount
                CAmount burnedAmount = HasBurnedTokensvouts(tx, tokenid);
                if (burnedAmount > 0)
                {
                    vscript_t vopretNonfungible;
                    GetNonfungibleData(tokenid, vopretNonfungible);
                    if (!vopretNonfungible.empty())
                    {
                        CTransaction tokenbaseTx;
                        uint256 hashBlock;
                        if (myGetTransaction(tokenid, tokenbaseTx, hashBlock))
                        {
                            // get total supply
                            CAmount supply = 0L, output;
                            for (int v = 0; v < tokenbaseTx.vout.size() - 1; v++)
                                if ((output = IsTokensvout(false, true, cp, NULL, tokenbaseTx, v, tokenid)) > 0)
                                    supply += output;

                            if (supply == 1 && supply == burnedAmount)  // burning marker is allowed only for burned NFTs (that is with supply == 1)
                                allowed = true;
                        }
                    }
                }
            }
            if (!allowed)
                return false;
        }
    }
    return true;
}

// internal function to check if token vout is valid
// returns amount or -1 
// return also tokenid
static int64_t CheckTokensvout(bool goDeeper, bool checkPubkeys /*<--not used, always true*/, struct CCcontract_info *cp, Eval* eval, const CTransaction& tx, int32_t v, uint256 &reftokenid, std::string &errorStr)
{
	// this is just for log messages indentation fur debugging recursive calls:
	std::string indentStr = std::string().append(tokenValIndentSize, '.');
	
    LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "IsTokensvout() entered for txid=" << tx.GetHash().GetHex() << " v=" << v << " for tokenid=" << reftokenid.GetHex() <<  std::endl);

    int32_t n = tx.vout.size();
    // just check boundaries:
    if (n == 0 || v < 0 || v >= n) {  
        LOGSTREAM(cctokens_log, CCLOG_INFO, stream << indentStr << "isTokensvout() incorrect params: (n == 0 or v < 0 or v >= n)" << " v=" << v << " n=" << n << " returning error" << std::endl);
        errorStr = "out of bounds";
        return(-1);
    }

	if (tx.vout[v].scriptPubKey.IsPayToCryptoCondition()) 
	{
		/* old code recursively checking vintx
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
                    LOGSTREAM(cctokens_log, CCLOG_ERROR, stream << indentStr << "IsTokensvout() warning: for the validated tx detected a bad vintx=" << tx.GetHash().GetHex() << ": cc inputs != cc outputs and not 'tokenbase' tx, skipping the validated tx" << std::endl);
					return 0;
				}
			}
		}*/

        // instead of recursively checking tx just check that the tx has token cc vin, that is it was validated by tokens cc module
        bool hasMyccvin = false;
        for (auto const vin : tx.vin)   {
            if (cp->ismyvin(vin.scriptSig)) {
                hasMyccvin = true;
                break;
            }
        }


        CScript opret;
        bool isLastVoutOpret;
        if (MyGetCCopretV2(tx.vout[v].scriptPubKey, opret))
        {
            isLastVoutOpret = false;    
        }
        else
        {
            isLastVoutOpret = true;
            opret = tx.vout.back().scriptPubKey;
        }

        uint256 tokenIdOpret;
        std::vector<vscript_t>  oprets;
        std::vector<CPubKey> voutPubkeysInOpret;

        // token opret most important checks (tokenid == reftokenid, tokenid is non-zero, tx is 'tokenbase'):
        uint8_t funcId = DecodeTokenOpRetV1(opret, tokenIdOpret, voutPubkeysInOpret, oprets);
        if (funcId == 0)    {
            // bad opreturn
            // errorStr = "can't decode opreturn data";
            // return -1;
            return 0;  // not token vout, skip
        } 

        // basic checks:
        if (IsTokenCreateFuncid(funcId))        {
            if (hasMyccvin)       {
                errorStr = "tokenbase tx cannot have cc vins";
                return -1;
            }
            // set returned tokend to tokenbase txid:
            reftokenid = tx.GetHash();
        }
        else if (IsTokenTransferFuncid(funcId))      {
            if (!hasMyccvin)     {
                errorStr = "no token cc vin in token transaction (and not tokenbase tx)";
                return -1;
            }
            // set returned tokenid to tokenid in opreturn:
            reftokenid = tokenIdOpret;
        }
        else       {
            errorStr = "funcid not supported";
            return -1;
        }
        
        
        if (!isLastVoutOpret)  // check OP_DROP vouts:
        {            
            // get up to two eval codes from cc data:
            uint8_t evalCode1 = 0, evalCode2 = 0;
            if (oprets.size() >= 1) {
                evalCode1 = oprets[0].size() > 0 ? oprets[0][0] : 0;
                if (oprets.size() >= 2)
                    evalCode2 = oprets[1].size() > 0 ? oprets[1][0] : 0;
            }

            // get optional nft eval code:
            vscript_t vopretNonfungible;
            GetNonfungibleData(reftokenid, vopretNonfungible);
            if (vopretNonfungible.size() > 0)   {
                // shift evalcodes so the first is NFT evalcode 
                evalCode2 = evalCode1;
                evalCode1 = vopretNonfungible[0];
            }

            LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "IsTokensvout() for txid=" << tx.GetHash().GetHex() << " checking evalCode1=" << (int)evalCode1 << " evalCode2=" << (int)evalCode2 << " voutPubkeysInOpret.size()=" << voutPubkeysInOpret.size() <<  std::endl);

            if (IsTokenTransferFuncid(funcId))
            {
                // check if not sent to globalpk:
                for (const auto &pk : voutPubkeysInOpret)  {
                    if (pk == GetUnspendable(cp, NULL)) {
                        errorStr = "cannot send tokens to global pk";
                        return -1;
                    }
                }
            
                // test the vout if it is a tokens vout with or withouts other cc modules eval codes:
                if (voutPubkeysInOpret.size() == 1) 
                {
                    if (evalCode1 == 0 && evalCode2 == 0)   {
                        if (IsEqualVouts(tx.vout[v], MakeTokensCC1vout(EVAL_TOKENS, tx.vout[v].nValue, voutPubkeysInOpret[0])))
                            return tx.vout[v].nValue;
                    }
                    else if (evalCode1 != 0 && evalCode2 == 0)  {
                        if (IsEqualVouts(tx.vout[v], MakeTokensCC1vout(evalCode1, tx.vout[v].nValue, voutPubkeysInOpret[0])))
                            return tx.vout[v].nValue;
                    }
                    else if (evalCode1 != 0 && evalCode2 != 0)  {
                        if (IsEqualVouts(tx.vout[v], MakeTokensCC1vout(evalCode1, evalCode2, tx.vout[v].nValue, voutPubkeysInOpret[0])))
                            return tx.vout[v].nValue;
                    }
                    else {
                        errorStr = "evalCode1 is null"; 
                        return -1;
                    }
                }
                else if (voutPubkeysInOpret.size() == 2)
                {
                    if (evalCode1 == 0 && evalCode2 == 0)   {
                        if (IsEqualVouts(tx.vout[v], MakeTokensCC1of2vout(EVAL_TOKENS, tx.vout[v].nValue, voutPubkeysInOpret[0], voutPubkeysInOpret[1])))
                            return tx.vout[v].nValue;
                    }
                    else if (evalCode1 != 0 && evalCode2 == 0)  {
                        if (IsEqualVouts(tx.vout[v], MakeTokensCC1of2vout(evalCode1, tx.vout[v].nValue, voutPubkeysInOpret[0], voutPubkeysInOpret[1])))
                            return tx.vout[v].nValue;
                    }
                    else if (evalCode1 != 0 && evalCode2 != 0)  {
                        if (IsEqualVouts(tx.vout[v], MakeTokensCC1of2vout(evalCode1, evalCode2, tx.vout[v].nValue, voutPubkeysInOpret[0], voutPubkeysInOpret[1])))
                            return tx.vout[v].nValue;
                    }
                    else {
                        errorStr = "evalCode1 is null"; 
                        return -1;
                    }
                }
                else
                {
                    errorStr = "pubkeys size should be 1 or 2";
                    return -1;
                }
            }
            else
            {
                // funcid == 'c' 
                if (tx.IsCoinImport())   {
                    // imported coin is checked in EvalImportCoin
                    if (!IsTokenMarkerVout(tx.vout[v]))  // exclude marker
                        return tx.vout[v].nValue;
                    else
                        return 0;  
                }

                vscript_t vorigPubkey;
                std::string  dummyName, dummyDescription;
                std::vector<vscript_t>  oprets;

                if (DecodeTokenCreateOpRetV1(tx.vout.back().scriptPubKey, vorigPubkey, dummyName, dummyDescription, oprets) == 0) {
                    LOGSTREAM(cctokens_log, CCLOG_INFO, stream << indentStr << "IsTokensvout() could not decode create opret" << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl);
                    return 0;
                }

                CPubKey origPubkey = pubkey2pk(vorigPubkey);
                vuint8_t vopretNFT;
                GetOpReturnCCBlob(oprets, vopretNFT);

                // calc cc outputs for origPubkey 
                int64_t ccOutputs = 0;
                for (const auto &vout : tx.vout)
                    if (vout.scriptPubKey.IsPayToCryptoCondition())  {
                        CTxOut testvout = vopretNFT.size() == 0 ? MakeCC1vout(EVAL_TOKENS, vout.nValue, origPubkey) : MakeTokensCC1vout(vopretNFT[0], vout.nValue, origPubkey);
                        if (IsEqualVouts(vout, testvout)) 
                            ccOutputs += vout.nValue;
                    }

                int64_t normalInputs = TotalPubkeyNormalInputs(tx, origPubkey);  // calc normal inputs really signed by originator pubkey (someone not cheating with originator pubkey)
                if (normalInputs >= ccOutputs) {
                    LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "IsTokensvout() assured normalInputs >= ccOutputs" << " for tokenbase=" << reftokenid.GetHex() << std::endl);

                    // make test vout for origpubkey (either for fungible or NFT):
                    CTxOut testvout = vopretNFT.size() == 0 ? MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, origPubkey) : MakeTokensCC1vout(vopretNFT[0], tx.vout[v].nValue, origPubkey);
                    if (IsEqualVouts(tx.vout[v], testvout))    // check vout sent to orig pubkey
                        return tx.vout[v].nValue;
                    else
                        return 0;
                } 
                else {
                    LOGSTREAM(cctokens_log, CCLOG_INFO, stream << indentStr << "IsTokensvout() skipping vout not fulfilled normalInputs >= ccOutput" << " for tokenbase=" << reftokenid.GetHex() << " normalInputs=" << normalInputs << " ccOutputs=" << ccOutputs << std::endl);
                    errorStr = "tokenbase tx issued by not pubkey in opret";
                    return -1;
                }
            }
        }
        else 
        {
            // check vout with last vout OP_RETURN   

            // token opret most important checks (tokenid == reftokenid, tokenid is non-zero, tx is 'tokenbase'):
            const uint8_t funcId = ValidateTokenOpret(tx.GetHash(), tx.vout.back().scriptPubKey, reftokenid);
            if (funcId == 0) 
            {
                // bad opreturn
                errorStr = "can't decode opreturn data";
                return -1;
            }

            LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "IsTokensvout() ValidateTokenOpret returned not-null funcId=" << (char)(funcId ? funcId : ' ') << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl);

            vscript_t vopretExtra, vopretNonfungible;

            // MakeTokenCCVout functions support up to two evalcodes in vouts
            // We assume one of them could be a cc module working with tokens like assets, gateways or heir
            // another eval code could be for a cc module responsible to non-fungible token data
            uint8_t evalCodeNonfungible = 0;
            uint8_t evalCode1 = EVAL_TOKENS;     // if both payloads are empty maybe it is a transfer to non-payload-one-eval-token vout like GatewaysClaim
            uint8_t evalCode2 = 0;               // will be checked if zero or not

            // test vouts for possible token use-cases:
            std::vector<std::pair<CTxOut, std::string>> testVouts;

            uint8_t version;
            DecodeTokenOpRetV1(tx.vout.back().scriptPubKey, tokenIdOpret, voutPubkeysInOpret, oprets);
            LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << "IsTokensvout() oprets.size()=" << oprets.size() << std::endl);
            
            // get assets/channels/gateways token data in vopretExtra:
            //FilterOutNonCCOprets(oprets, vopretExtra);  
            // NOTE: only 1 additional evalcode in token opret is currently supported:
            if (oprets.size() > 0)
                vopretExtra = oprets[0];
            LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << "IsTokensvout() vopretExtra=" << HexStr(vopretExtra) << std::endl);

            // get non-fungible data
            GetNonfungibleData(reftokenid, vopretNonfungible);
            std::vector<CPubKey> voutPubkeys;
            FilterOutTokensUnspendablePk(voutPubkeysInOpret, voutPubkeys);  // cannot send tokens to token unspendable cc addr (only marker is allowed there)

            // NOTE: evalcode order in vouts is important: 
            // non-fungible-eval -> EVAL_TOKENS -> assets-eval

            if (vopretNonfungible.size() > 0)
                evalCodeNonfungible = evalCode1 = vopretNonfungible.begin()[0];
            if (vopretExtra.size() > 0)
                evalCode2 = vopretExtra.begin()[0];

            if (evalCode1 == EVAL_TOKENS && evalCode2 != 0)  {
                evalCode1 = evalCode2;   // for using MakeTokensCC1vout(evalcode,...) instead of MakeCC1vout(EVAL_TOKENS, evalcode...)
                evalCode2 = 0;
            }
            
            if (IsTokenTransferFuncid(funcId)) 
            { 
                // verify that the vout is token by constructing vouts with the pubkeys in the opret:

                // maybe this is dual-eval 1 pubkey or 1of2 pubkey vout?
                if (voutPubkeys.size() >= 1 && voutPubkeys.size() <= 2) {					
                    // check dual/three-eval 1 pubkey vout with the first pubkey
                    testVouts.push_back( std::make_pair(MakeTokensCC1vout(evalCode1, evalCode2, tx.vout[v].nValue, voutPubkeys[0]), std::string("three-eval cc1 pk[0]")) );
                    if (evalCode2 != 0) 
                        // also check in backward evalcode order
                        testVouts.push_back( std::make_pair(MakeTokensCC1vout(evalCode2, evalCode1, tx.vout[v].nValue, voutPubkeys[0]), std::string("three-eval cc1 pk[0] backward-eval")) );

                    if(voutPubkeys.size() == 2)	{
                        // check dual/three eval 1of2 pubkeys vout
                        testVouts.push_back( std::make_pair(MakeTokensCC1of2vout(evalCode1, evalCode2, tx.vout[v].nValue, voutPubkeys[0], voutPubkeys[1]), std::string("three-eval cc1of2")) );
                        // check dual/three eval 1 pubkey vout with the second pubkey
                        testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode1, evalCode2, tx.vout[v].nValue, voutPubkeys[1]), std::string("three-eval cc1 pk[1]")));
                        if (evalCode2 != 0) {
                            // also check in backward evalcode order:
                            // check dual/three eval 1of2 pubkeys vout
                            testVouts.push_back(std::make_pair(MakeTokensCC1of2vout(evalCode2, evalCode1, tx.vout[v].nValue, voutPubkeys[0], voutPubkeys[1]), std::string("three-eval cc1of2 backward-eval")));
                            // check dual/three eval 1 pubkey vout with the second pubkey
                            testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode2, evalCode1, tx.vout[v].nValue, voutPubkeys[1]), std::string("three-eval cc1 pk[1] backward-eval")));
                        }
                    }
                
                    // maybe this is like gatewayclaim to single-eval token?
                    if( evalCodeNonfungible == 0 )  // do not allow to convert non-fungible to fungible token
                        testVouts.push_back(std::make_pair(MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, voutPubkeys[0]), std::string("single-eval cc1 pk[0]")));

                    // maybe this is like FillSell for non-fungible token?
                    if( evalCode1 != 0 )
                        testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode1, tx.vout[v].nValue, voutPubkeys[0]), std::string("dual-eval-token cc1 pk[0]")));
                    if( evalCode2 != 0 )
                        testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode2, tx.vout[v].nValue, voutPubkeys[0]), std::string("dual-eval2-token cc1 pk[0]")));

                    // the same for pk[1]:
                    if (voutPubkeys.size() == 2) {
                        if (evalCodeNonfungible == 0)  // do not allow to convert non-fungible to fungible token
                            testVouts.push_back(std::make_pair(MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, voutPubkeys[1]), std::string("single-eval cc1 pk[1]")));
                        if (evalCode1 != 0)
                            testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode1, tx.vout[v].nValue, voutPubkeys[1]), std::string("dual-eval-token cc1 pk[1]")));
                        if (evalCode2 != 0)
                            testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode2, tx.vout[v].nValue, voutPubkeys[1]), std::string("dual-eval2-token cc1 pk[1]")));
                    }
                }

                if (voutPubkeys.size() > 0)  // we could pass empty pubkey array
                {
                    //special check for tx when spending from 1of2 CC address and one of pubkeys is global CC pubkey
                    struct CCcontract_info *cpEvalCode1, CEvalCode1;
                    cpEvalCode1 = CCinit(&CEvalCode1, evalCode1);
                    CPubKey pk = GetUnspendable(cpEvalCode1, 0);
                    testVouts.push_back(std::make_pair(MakeTokensCC1of2vout(evalCode1, tx.vout[v].nValue, voutPubkeys[0], pk), std::string("dual-eval1 pegscc cc1of2 pk[0] globalccpk")));
                    if (voutPubkeys.size() == 2) testVouts.push_back(std::make_pair(MakeTokensCC1of2vout(evalCode1, tx.vout[v].nValue, voutPubkeys[1], pk), std::string("dual-eval1 pegscc cc1of2 pk[1] globalccpk")));
                    if (evalCode2 != 0)
                    {
                        struct CCcontract_info *cpEvalCode2, CEvalCode2;
                        cpEvalCode2 = CCinit(&CEvalCode2, evalCode2);
                        CPubKey pk = GetUnspendable(cpEvalCode2, 0);
                        testVouts.push_back(std::make_pair(MakeTokensCC1of2vout(evalCode2, tx.vout[v].nValue, voutPubkeys[0], pk), std::string("dual-eval2 pegscc cc1of2 pk[0] globalccpk")));
                        if (voutPubkeys.size() == 2) testVouts.push_back(std::make_pair(MakeTokensCC1of2vout(evalCode2, tx.vout[v].nValue, voutPubkeys[1], pk), std::string("dual-eval2 pegscc cc1of2 pk[1] globalccpk")));
                    }
                }

                // maybe it is single-eval or dual/three-eval token change?
                std::vector<CPubKey> vinPubkeys, vinPubkeysUnfiltered;
                ExtractTokensCCVinPubkeys(tx, vinPubkeysUnfiltered);
                FilterOutTokensUnspendablePk(vinPubkeysUnfiltered, vinPubkeys);  // cannot send tokens to token unspendable cc addr (only marker is allowed there)

                for(std::vector<CPubKey>::iterator it = vinPubkeys.begin(); it != vinPubkeys.end(); it++) {
                    if (evalCodeNonfungible == 0)  // do not allow to convert non-fungible to fungible token
                        testVouts.push_back(std::make_pair(MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, *it), std::string("single-eval cc1 self vin pk")));
                    testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode1, evalCode2, tx.vout[v].nValue, *it), std::string("three-eval cc1 self vin pk")));

                    if (evalCode2 != 0) 
                        // also check in backward evalcode order:
                        testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode2, evalCode1, tx.vout[v].nValue, *it), std::string("three-eval cc1 self vin pk backward-eval")));
                }

                // try all test vouts:
                for (const auto &t : testVouts) {
                    if (t.first == tx.vout[v]) {  // test vout matches 
                        LOGSTREAM(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "IsTokensvout() valid amount=" << tx.vout[v].nValue << " msg=" << t.second << " evalCode=" << (int)evalCode1 << " evalCode2=" << (int)evalCode2 << " txid=" << tx.GetHash().GetHex() << " tokenid=" << reftokenid.GetHex() << std::endl);
                        return tx.vout[v].nValue;
                    }
                }
            }
            else	
            {  
                // funcid == 'c' 
                if (!tx.IsCoinImport())   
                {
                    vscript_t vorigPubkey;
                    std::string  dummyName, dummyDescription;
                    std::vector<vscript_t>  oprets;
                    uint8_t version;

                    if (DecodeTokenCreateOpRetV1(tx.vout.back().scriptPubKey, vorigPubkey, dummyName, dummyDescription, oprets) == 0) {
                        LOGSTREAM(cctokens_log, CCLOG_INFO, stream << indentStr << "IsTokensvout() could not decode create opret" << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl);
                        return 0;
                    }

                    CPubKey origPubkey = pubkey2pk(vorigPubkey);
                    vuint8_t vopretNFT;
                    GetOpReturnCCBlob(oprets, vopretNFT);
                    
                    // TODO: add voutPubkeys for 'c' tx

                    /* this would not work for imported tokens:
                    // for 'c' recognize the tokens only to token originator pubkey (but not to unspendable <-- closed sec violation)
                    // maybe this is like gatewayclaim to single-eval token?
                    if (evalCodeNonfungible == 0)  // do not allow to convert non-fungible to fungible token
                        testVouts.push_back(std::make_pair(MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, origPubkey), std::string("single-eval cc1 orig-pk")));
                    // maybe this is like FillSell for non-fungible token?
                    if (evalCode1 != 0)
                        testVouts.push_back(std::make_pair(MakeTokensCC1vout(evalCode1, tx.vout[v].nValue, origPubkey), std::string("dual-eval-token cc1 orig-pk")));   
                    */

                    // for tokenbase tx check that normal inputs sent from origpubkey > cc outputs 
                    // that is, tokenbase tx should be created with inputs signed by the original pubkey
                    int64_t ccOutputs = 0;
                    for (const auto &vout : tx.vout)
                        if (vout.scriptPubKey.IsPayToCryptoCondition())  {
                            CTxOut testvout = vopretNFT.size() == 0 ? MakeCC1vout(EVAL_TOKENS, vout.nValue, origPubkey) : MakeTokensCC1vout(vopretNFT[0], vout.nValue, origPubkey);
                            if (IsEqualVouts(vout, testvout)) 
                                ccOutputs += vout.nValue;
                        }

                    int64_t normalInputs = TotalPubkeyNormalInputs(tx, origPubkey);  // check if normal inputs are really signed by originator pubkey (someone not cheating with originator pubkey)
                    LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "IsTokensvout() normalInputs=" << normalInputs << " ccOutputs=" << ccOutputs << " for tokenbase=" << reftokenid.GetHex() << std::endl);

                    if (normalInputs >= ccOutputs) {
                        LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "IsTokensvout() assured normalInputs >= ccOutputs" << " for tokenbase=" << reftokenid.GetHex() << std::endl);
                        
                        // make test vout for origpubkey (either for fungible or NFT):
                        CTxOut testvout = vopretNFT.size() == 0 ? MakeCC1vout(EVAL_TOKENS, tx.vout[v].nValue, origPubkey) : MakeTokensCC1vout(vopretNFT[0], tx.vout[v].nValue, origPubkey);
                        
                        if (IsEqualVouts(tx.vout[v], testvout))    // check vout sent to orig pubkey
                            return tx.vout[v].nValue;
                        else
                            return 0; // vout is good, but do not take marker into account
                    } 
                    else {
                        LOGSTREAM(cctokens_log, CCLOG_INFO, stream << indentStr << "IsTokensvout() skipping vout not fulfilled normalInputs >= ccOutput" << " for tokenbase=" << reftokenid.GetHex() << " normalInputs=" << normalInputs << " ccOutputs=" << ccOutputs << std::endl);
                    }
                }
                else   {
                    // imported tokens are checked in the eval::ImportCoin() validation code
                    if (!IsTokenMarkerVout(tx.vout[v]))  // exclude marker
                        return tx.vout[v].nValue;
                    else
                        return 0; // vout is good, but do not take marker into account
                }
            }
            LOGSTREAM(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "IsTokensvout() no valid vouts evalCode=" << (int)evalCode1 << " evalCode2=" << (int)evalCode2 << " for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl);
        }
	}
	return(0);  // normal vout
}

// Checks if the vout is a really Tokens CC vout. 
// For this the function takes eval codes and pubkeys from the token opret and tries to construct possible token vouts
// if one of them matches to the passed vout then the passed vout is a correct token vout
// The function also checks tokenid in the opret and checks if this tx is the tokenbase tx
// If goDeeper param is true the func also validates input and output token amounts of the passed transaction: 
// it should be either sum(cc vins) == sum(cc vouts) or the transaction is the 'tokenbase' ('c' or 'C') tx
// checkPubkeys is true: validates if the vout is token vout1 or token vout1of2. Should always be true!
int64_t IsTokensvout(bool goDeeper, bool checkPubkeys /*<--not used, always true*/, struct CCcontract_info *cp, Eval* eval, const CTransaction& tx, int32_t v, uint256 reftokenid)
{
    uint256 tokenIdInOpret;
    std::string errorStr;
    CAmount retAmount = CheckTokensvout(goDeeper, checkPubkeys, cp, eval, tx, v, tokenIdInOpret, errorStr);
    if (!errorStr.empty())
        LOGSTREAMFN(cctokens_log, CCLOG_DEBUG1, stream << "error=" << errorStr << std::endl);
    if (retAmount < 0)
        return retAmount;
    if (reftokenid == tokenIdInOpret)
        return retAmount;
    return 0;
}

bool IsTokenMarkerVout(CTxOut vout) {
    struct CCcontract_info *cpTokens, CCtokens_info;
    cpTokens = CCinit(&CCtokens_info, EVAL_TOKENS);
    return vout == MakeCC1vout(EVAL_TOKENS, vout.nValue, GetUnspendable(cpTokens, NULL));
}

// compares cc inputs vs cc outputs (to prevent feeding vouts from normal inputs)
bool TokensExactAmounts(bool goDeeper, struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, std::string &errorStr)
{
	CTransaction vinTx; 
	uint256 hashBlock; 
	int64_t tokenoshis; 

	//struct CCcontract_info *cpTokens, tokensC;
	//cpTokens = CCinit(&tokensC, EVAL_TOKENS);

	int32_t numvins = tx.vin.size();
	int32_t numvouts = tx.vout.size();
	
    std::map <uint256, CAmount> mapinputs, mapoutputs;

	// this is just for log messages indentation for debugging recursive calls:
	std::string indentStr = std::string().append(tokenValIndentSize, '.');

    //LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "TokensExactAmounts() entered for txid=" << tx.GetHash().GetHex() << " for tokenid=" << reftokenid.GetHex() << std::endl);

    // pick token vouts in vin transactions and calculate input total
	for (int32_t i = 0; i<numvins; i++)
	{												  // check for additional contracts which may send tokens to the Tokens contract
		if ((*cp->ismyvin)(tx.vin[i].scriptSig) /*|| IsVinAllowed(tx.vin[i].scriptSig) != 0*/)
		{
			//std::cerr << indentStr << "TokensExactAmounts() eval is true=" << (eval != NULL) << " ismyvin=ok for_i=" << i << std::endl;
			// we are not inside the validation code -- dimxy
			if ((eval && eval->GetTxUnconfirmed(tx.vin[i].prevout.hash, vinTx, hashBlock) == 0) || (!eval && !myGetTransaction(tx.vin[i].prevout.hash, vinTx, hashBlock)))
			{
                LOGSTREAM(cctokens_log, CCLOG_ERROR, stream << indentStr << "TokensExactAmounts() cannot read vintx for i." << i << " numvins." << numvins << std::endl);
				return (!eval) ? false : eval->Invalid("could not load vin tx " + std::to_string(i));
			}
			else 
            {
                LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "TokenExactAmounts() checking vintx.vout for tx.vin[" << i << "] nValue=" << vinTx.vout[tx.vin[i].prevout.n].nValue << std::endl);

                uint256 reftokenid;
                // validate vouts of vintx  
                tokenValIndentSize++;
				tokenoshis = CheckTokensvout(goDeeper, true, cp, eval, vinTx, tx.vin[i].prevout.n, reftokenid, errorStr);
				tokenValIndentSize--;
                if (tokenoshis < 0) 
                    return false;

                // skip marker spending
                // later it will be checked if marker spending is allowed
                if (IsTokenMarkerVout(vinTx.vout[tx.vin[i].prevout.n])) {
                    LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "TokenExactAmounts() skipping marker vintx.vout for tx.vin[" << i << "] nValue=" << vinTx.vout[tx.vin[i].prevout.n].nValue << std::endl);
                    continue;
                }
                   
				if (tokenoshis != 0)
				{
                    LOGSTREAM(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "TokensExactAmounts() adding vintx.vout for tx.vin[" << i << "] tokenoshis=" << tokenoshis << std::endl);
					mapinputs[reftokenid] += tokenoshis;
				}
			}
		}
	}

    // pick token vouts in the current transaction and calculate output total
	for (int32_t i = 0; i < numvouts; i ++)  
	{
        uint256 reftokenid;
        LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "TokenExactAmounts() recursively checking tx.vout[" << i << "] nValue=" << tx.vout[i].nValue << std::endl);

        // Note: we pass in here IsTokenvout(false,...) because we don't need to call TokenExactAmounts() recursively from IsTokensvout here
        // indeed, if we pass 'true' we'll be checking this tx vout again
        tokenValIndentSize++;
		tokenoshis = CheckTokensvout(false, true, cp, eval, tx, i, reftokenid, errorStr);
		tokenValIndentSize--;
        if (tokenoshis < 0) 
            return false;

        if (IsTokenMarkerVout(tx.vout[i]))  {
            LOGSTREAM(cctokens_log, CCLOG_DEBUG2, stream << indentStr << "TokenExactAmounts() skipping marker tx.vout[" << i << "] nValue=" << tx.vout[i].nValue << std::endl);
            continue;
        }

		if (tokenoshis != 0)
		{
            LOGSTREAM(cctokens_log, CCLOG_DEBUG1, stream << indentStr << "TokensExactAmounts() adding tx.vout[" << i << "] tokenoshis=" << tokenoshis << std::endl);
			mapoutputs[reftokenid] += tokenoshis;
		}
	}

	//std::cerr << indentStr << "TokensExactAmounts() inputs=" << inputs << " outputs=" << outputs << " for txid=" << tx.GetHash().GetHex() << std::endl;

	if (mapinputs.size() > 0 && mapinputs.size() == mapoutputs.size()) 
    {
		for(auto const &m : mapinputs)  {
            if (m.second != mapoutputs[m.first])    {
                errorStr = "inputs not equal outputs for tokenid=" + m.first.GetHex();
                return false;
            }

            // check marker spending:
            if (!CheckMarkerSpending(cp, eval, tx, m.first))    {
                errorStr = "spending marker is not allowed for tokenid=" + m.first.GetHex();
                return false;
            }
        }
        return true;
	}
    errorStr = "empty token vins=" + std::to_string(mapinputs.size()) + " or vouts=" + std::to_string(mapoutputs.size());
	return false;
}


// get non-fungible data from 'tokenbase' tx (the data might be empty)
void GetNonfungibleData(uint256 tokenid, vscript_t &vopretNonfungible)
{
    CTransaction tokenbasetx;
    uint256 hashBlock;

    if (!myGetTransaction(tokenid, tokenbasetx, hashBlock)) {
        LOGSTREAM(cctokens_log, CCLOG_INFO, stream << "GetNonfungibleData() could not load token creation tx=" << tokenid.GetHex() << std::endl);
        return;
    }

    vopretNonfungible.clear();
    // check if it is non-fungible tx and get its second evalcode from non-fungible payload
    if (tokenbasetx.vout.size() > 0) {
        std::vector<uint8_t> origpubkey;
        std::string name, description;
        std::vector<vscript_t>  oprets;
        uint8_t funcid;

        if (IsTokenCreateFuncid(DecodeTokenCreateOpRetV1(tokenbasetx.vout.back().scriptPubKey, origpubkey, name, description, oprets))) {
            if (oprets.size() > 0)
                vopretNonfungible = oprets[0];
        }
    }
}

CPubKey GetTokenOriginatorPubKey(CScript scriptPubKey) {

    uint8_t funcId;
    uint256 tokenid;
    std::vector<CPubKey> voutTokenPubkeys;
    std::vector<vscript_t> oprets;

    if ((funcId = DecodeTokenOpRetV1(scriptPubKey, tokenid, voutTokenPubkeys, oprets)) != 0) {
        CTransaction tokenbasetx;
        uint256 hashBlock;

        if (myGetTransaction(tokenid, tokenbasetx, hashBlock) && tokenbasetx.vout.size() > 0) {
            vscript_t vorigpubkey;
            std::string name, desc;
            std::vector<vscript_t> oprets;
            if (DecodeTokenCreateOpRetV1(tokenbasetx.vout.back().scriptPubKey, vorigpubkey, name, desc, oprets) != 0)
                return pubkey2pk(vorigpubkey);
        }
    }
    return CPubKey(); //return invalid pubkey
}

// tx validation
// NOTE: opreturn decode v1 functions (DecodeTokenCreateOpRetV1 DecodeTokenOpRetV1) understands both old and new opreturn versions
bool TokensValidate(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn)
{
    if (!TokensIsVer1Active(eval))
        return tokensv0::TokensValidate(cp, eval, tx, nIn);

    if (strcmp(ASSETCHAINS_SYMBOL, "ROGUE") == 0 && chainActive.Height() <= 12500)
        return true;

    // check boundaries:
    if (tx.vout.size() < 1)
        return eval->Invalid("no vouts");

    std::string errorStr;
    if (!TokensExactAmounts(true, cp, eval, tx, errorStr)) 
    {
        LOGSTREAMFN(cctokens_log, CCLOG_ERROR, stream << "validation error: " << errorStr << " tx=" << HexStr(E_MARSHAL(ss << tx)) << std::endl);
		if (eval->state.IsInvalid())
			return false;  //TokenExactAmounts has already called eval->Invalid()
		else
			return eval->Invalid(errorStr); 
	}
	return true;
}

// overload for fungible tokens, adds token inputs from pubkey
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, const CPubKey &pk, uint256 tokenid, int64_t total, int32_t maxinputs, vscript_t &vopretNonfungible) 
{
    char tokenaddr[KOMODO_ADDRESS_BUFSIZE];

    GetNonfungibleData(tokenid, vopretNonfungible);
    if (vopretNonfungible.size() > 0)
        cp->evalcodeNFT = vopretNonfungible.begin()[0];  // set evalcode of NFT
    
    GetTokensCCaddress(cp, tokenaddr, pk);  // set tokenaddr, additionalTokensEvalcode2 is used if set
    return AddTokenCCInputs(cp, mtx, tokenaddr, tokenid, total, maxinputs);
} 

int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, const CPubKey &pk, uint256 tokenid, int64_t total, int32_t maxinputs) 
{
    char tokenaddr[KOMODO_ADDRESS_BUFSIZE];
    
    // check if this is a NFT
    vscript_t vopretNonfungible;
    GetNonfungibleData(tokenid, vopretNonfungible);
    if (vopretNonfungible.size() > 0)
        cp->evalcodeNFT = vopretNonfungible.begin()[0];  // set evalcode of NFT
    
    GetTokensCCaddress(cp, tokenaddr, pk);  // GetTokensCCaddress will use 'additionalTokensEvalcode2'
    return AddTokenCCInputs(cp, mtx, tokenaddr, tokenid, total, maxinputs);
} 

/*
// overload for non-fungible tokens, adds token inputs from pubkey
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, const CPubKey &pk, uint256 tokenid, int64_t total, int32_t maxinputs, bool useMempool)
{
    char tokenaddr[64];

    if (cp->additionalTokensEvalcode2 == 0)  // not set yet
    {
        // check if this is a NFT
        vscript_t vopretNonfungible;
        GetNonfungibleData(tokenid, vopretNonfungible);
        if (vopretNonfungible.size() > 0)
            cp->additionalTokensEvalcode2 = vopretNonfungible.begin()[0];  // set evalcode of NFT
    }
    GetTokensCCaddress(cp, tokenaddr, pk);  // GetTokensCCaddress will use 'additionalTokensEvalcode2'
    return AddTokenCCInputs(cp, mtx, tokenaddr, tokenid, total, maxinputs, useMempool);
}
*/

// overload, adds inputs from token cc addr and returns non-fungible opret payload if present
// also sets evalcode in cp, if needed
int64_t AddTokenCCInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, const char *tokenaddr, uint256 tokenid, int64_t total, int32_t maxinputs, bool useMempool)
{
	int64_t threshold, nValue, price, totalinputs = 0;  
	int32_t n = 0;
	std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    if (cp->evalcode != EVAL_TOKENS)
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << "warning: EVAL_TOKENS *cp is needed but used evalcode=" << (int)cp->evalcode << std::endl);
        
    if (cp->evalcodeNFT == 0)  // if not set yet (in TransferToken or this func overload)
    {
        // check if this is a NFT
        vscript_t vopretNonfungible;
        GetNonfungibleData(tokenid, vopretNonfungible); //load NFT data 
        if (vopretNonfungible.size() > 0)
            cp->evalcodeNFT = vopretNonfungible.begin()[0];  // set evalcode of NFT, for signing
    }

    //if (!useMempool)  // reserved for mempool use
	    SetCCunspents(unspentOutputs, (char*)tokenaddr, true);
    //else
    //  SetCCunspentsWithMempool(unspentOutputs, (char*)tokenaddr, true);  // add tokens in mempool too

    if (unspentOutputs.empty()) {
        LOGSTREAM(cctokens_log, CCLOG_INFO, stream << "AddTokenCCInputs() no utxos for token dual/three eval addr=" << tokenaddr << " evalcode=" << (int)cp->evalcode << " additionalTokensEvalcode2=" << (int)cp->evalcodeNFT << std::endl);
    }

	threshold = total / (maxinputs != 0 ? maxinputs : CC_MAXVINS);

	for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
	{
        CTransaction vintx;
        uint256 hashBlock;
        uint256 vintxid = it->first.txhash;
		int32_t vout = (int32_t)it->first.index;

		//if (it->second.satoshis < threshold)            // this should work also for non-fungible tokens (there should be only 1 satoshi for non-fungible token issue)
		//	continue;
        if (it->second.satoshis == 0)
            continue;

        int32_t ivin;
		for (ivin = 0; ivin < mtx.vin.size(); ivin ++)
			if (vintxid == mtx.vin[ivin].prevout.hash && vout == mtx.vin[ivin].prevout.n)
				break;
		if (ivin != mtx.vin.size()) // that is, the tx.vout is already added to mtx.vin (in some previous calls)
			continue;

		if (myGetTransaction(vintxid, vintx, hashBlock) != 0)
		{
            char destaddr[64];
			Getscriptaddress(destaddr, vintx.vout[vout].scriptPubKey);
			if (strcmp(destaddr, tokenaddr) != 0 /*&& 
                strcmp(destaddr, cp->unspendableCCaddr) != 0 &&   // TODO: check why this. Should not we add token inputs from unspendable cc addr if mypubkey is used?
                strcmp(destaddr, cp->unspendableaddr2) != 0*/)      // or the logic is to allow to spend all available tokens (what about unspendableaddr3)?
				continue;
			
            LOGSTREAM(cctokens_log, CCLOG_DEBUG1, stream << "AddTokenCCInputs() check vintx vout destaddress=" << destaddr << " amount=" << vintx.vout[vout].nValue << std::endl);

			if ((nValue = IsTokensvout(true, true/*<--add only valid token uxtos */, cp, NULL, vintx, vout, tokenid)) > 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,vintxid, vout) == 0)
			{
                /* no need in this check: we already have vopretNonfungible from tokenid opret
                //for non-fungible tokens check payload:
                if (!vopretNonfungible.empty()) {
                    vscript_t vopret;

                    // check if it is non-fungible token:
                    GetNonfungibleData(tokenid, vopret);
                    if (vopret != vopretNonfungible) {
                        LOGSTREAM(cctokens_log, CCLOG_INFO, stream << "AddTokenCCInputs() found incorrect non-fungible opret payload for vintxid=" << vintxid.GetHex() << std::endl);
                        continue;
                    }
                    // non-fungible evalCode2 cc contract should also check if there exists only one non-fungible vout with amount = 1
                } */

                
                if (total != 0 && maxinputs != 0)  // if it is not just to calc amount...
					mtx.vin.push_back(CTxIn(vintxid, vout, CScript()));

				nValue = it->second.satoshis;
				totalinputs += nValue;
                LOGSTREAM(cctokens_log, CCLOG_DEBUG1, stream << "AddTokenCCInputs() adding input nValue=" << nValue  << std::endl);
				n++;

				if ((total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs))
					break;
			}
		}
	}

	//std::cerr << "AddTokenCCInputs() found totalinputs=" << totalinputs << std::endl;
	return(totalinputs);
}



std::string CreateTokenLocal(int64_t txfee, int64_t tokensupply, std::string name, std::string description, vscript_t nonfungibleData)
{
    CPubKey nullpk = CPubKey();
    UniValue sigData = CreateTokenExt(nullpk, txfee, tokensupply, name, description, nonfungibleData, 0, false);
    return sigData[JSON_HEXTX].getValStr();
}

// returns token creation signed raw tx
// params: txfee amount, token amount, token name and description, optional NFT data, 
UniValue CreateTokenExt(const CPubKey &remotepk, int64_t txfee, int64_t tokensupply, std::string name, std::string description, vscript_t nonfungibleData, uint8_t additionalMarkerEvalCode, bool addTxInMemory)
{
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    struct CCcontract_info *cp, C;
    UniValue sigData;

	if (tokensupply < 0)	{
        CCerror = "negative tokensupply";
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << CCerror << "=" << tokensupply << std::endl);
		return NullUniValue;
	}
    if (!nonfungibleData.empty() && tokensupply != 1) {
        CCerror = "for non-fungible tokens tokensupply should be equal to 1";
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << CCerror << std::endl);
        return NullUniValue;
    }

	cp = CCinit(&C, EVAL_TOKENS);
	if (name.size() > 32 || description.size() > 4096)  // this is also checked on rpc level
	{
        LOGSTREAMFN(cctokens_log, CCLOG_DEBUG1, stream << "name len=" << name.size() << " or description len=" << description.size() << " is too big" << std::endl);
        CCerror = "name should be <= 32, description should be <= 4096";
		return NullUniValue;
	}
	if (txfee == 0)
		txfee = 10000;
	
    int32_t txfeeCount = 2;
    if (additionalMarkerEvalCode > 0)
        txfeeCount++;
    
    bool isRemote = remotepk.IsValid();
    CPubKey mypk = isRemote ? remotepk : pubkey2pk(Mypubkey());
    if (!mypk.IsFullyValid())    {
        CCerror = "mypk is not set or invalid";
        return NullUniValue;
    } 

    CAmount totalInputs;
    // always add inputs only from the mypk passed in the param to prove the token creator has the token originator pubkey
    // This what the AddNormalinputsRemote does (and it is not necessary that this is done only for nspv calls):
	if ((totalInputs = AddNormalinputsRemote(mtx, mypk, tokensupply + txfeeCount * txfee, 0x10000)) > 0)
	{
        int64_t mypkInputs = TotalPubkeyNormalInputs(mtx, mypk);  
        if (mypkInputs < tokensupply) {     // check that the token amount is really issued with mypk (because in the wallet there may be some other privkeys)
            CCerror = "some inputs signed not with mypubkey (-pubkey=pk)";
            return NullUniValue;
        }
        
        uint8_t destEvalCode = EVAL_TOKENS;
        if( nonfungibleData.size() > 0 )
            destEvalCode = nonfungibleData.begin()[0];

        // NOTE: we should prevent spending fake-tokens from this marker in IsTokenvout():
        mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, txfee, GetUnspendable(cp, NULL)));            // new marker to token cc addr, burnable and validated, vout pos now changed to 0 (from 1)
		mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, tokensupply, mypk));
		//mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(cp->CChexstr) << OP_CHECKSIG));  // old marker (non-burnable because spending could not be validated)
        //mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, txfee, GetUnspendable(cp, NULL)));          // ...moved to vout=0 for matching with rogue-game token
        if (additionalMarkerEvalCode > 0) 
        {
            // add additional marker for NFT cc evalcode:
            struct CCcontract_info *cp2, C2;
            cp2 = CCinit(&C2, additionalMarkerEvalCode);
            mtx.vout.push_back(MakeCC1vout(additionalMarkerEvalCode, txfee, GetUnspendable(cp2, NULL)));
        }

        //std::cerr << "mtx.before=" << HexStr(E_MARSHAL(ss << mtx)) << std::endl;
        //std::cerr << "mtx.hash before=" << mtx.GetHash().GetHex() << std::endl;

		sigData = FinalizeCCTxExt(isRemote, 0, cp, mtx, mypk, txfee, EncodeTokenCreateOpRetV1(vscript_t(mypk.begin(), mypk.end()), name, description, { nonfungibleData }));

        //std::cerr << "mtx.after=" << HexStr(E_MARSHAL(ss << mtx)) << std::endl;
        //std::cerr << "mtx.hash after=" << mtx.GetHash().GetHex() << std::endl;

        if (!ResultHasTx(sigData)) {
            CCerror = "couldnt finalize token tx";
            return NullUniValue;
        }
        if (addTxInMemory)
        {
            // add tx to in-mem array to use in subsequent AddNormalinputs()
            // LockUtxoInMemory::AddInMemoryTransaction(mtx);
        }
        return sigData;
	}

    CCerror = "cant find normal inputs";
    return NullUniValue;
}

// transfer tokens from mypk to another pubkey
// param additionalEvalCode2 allows transfer of dual-eval non-fungible tokens
std::string TokenTransfer(int64_t txfee, uint256 tokenid, CPubKey destpubkey, int64_t total)
{
    char tokenaddr[64];
    CPubKey mypk = pubkey2pk(Mypubkey());

    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_TOKENS);

    vscript_t vopretNonfungible;
    GetNonfungibleData(tokenid, vopretNonfungible);
    if (vopretNonfungible.size() > 0)
        cp->evalcodeNFT = vopretNonfungible.begin()[0];  // set evalcode of NFT
    GetTokensCCaddress(cp, tokenaddr, mypk);

    UniValue sigData = TokenTransferExt(CPubKey(), txfee, tokenid, tokenaddr, std::vector<std::pair<CC*, uint8_t*>>(), std::vector<CPubKey> {destpubkey}, total);
    return ResultGetTx(sigData);
}


UniValue TokenBeginTransferTx(CMutableTransaction &mtx, struct CCcontract_info *cp, const CPubKey &remotepk, CAmount txfee)
{
    bool isRemote = IS_REMOTE(remotepk);
    CPubKey mypk = isRemote ? remotepk : pubkey2pk(Mypubkey());
    if (!mypk.IsFullyValid())     {
        return MakeResultError("my pubkey not set");
    }

    if (!TokensIsVer1Active(NULL))
        return MakeResultError("tokens version 1 not active yet");

    if (txfee == 0)
		txfee = 10000;

    mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    int64_t normalInputs = AddNormalinputs(mtx, mypk, txfee, 3, isRemote);
    if (normalInputs < 0)
	{
        return MakeResultError("cannot find normal inputs");
    }
    return NullUniValue;
}

UniValue TokenAddTransferVout(CMutableTransaction &mtx, struct CCcontract_info *cp, const CPubKey &remotepk, uint256 tokenid, const char *tokenaddr, std::vector<CPubKey> destpubkeys, const std::pair<CC*, uint8_t*> &probecond, CAmount amount, bool useMempool)
{
    if (!TokensIsVer1Active(NULL))
        return MakeResultError("tokens version 1 not active yet");

    if (amount < 0)	{
        CCerror = strprintf("negative amount");
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << CCerror << "=" << amount << std::endl);
        MakeResultError("negative amount");
	}

    bool isRemote = IS_REMOTE(remotepk);
    CPubKey mypk = isRemote ? remotepk : pubkey2pk(Mypubkey());
    if (!mypk.IsFullyValid())     {
        CCerror = "mypk is not set or invalid";
        return MakeResultError("my pubkey not set");
    }

    CAmount inputs;        
    if ((inputs = AddTokenCCInputs(cp, mtx, tokenaddr, tokenid, amount, CC_MAXVINS, useMempool)) > 0)  // NOTE: AddTokenCCInputs might set cp->additionalEvalCode which is used in FinalizeCCtx!
    {
        if (inputs < amount) {   
            CCerror = strprintf("insufficient token inputs");
            LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << CCerror << std::endl);
            return MakeResultError("insufficient token inputs");
        }

        uint8_t destEvalCode = EVAL_TOKENS;
        if (cp->evalcodeNFT != 0)  // if set in AddTokenCCInputs
        {
            destEvalCode = cp->evalcodeNFT;
        }

        if (probecond.first != nullptr)
        {
            // add probe cc and kogs priv to spend from kogs global pk
            CCAddVintxCond(cp, probecond.first, probecond.second);
        }

        CScript opret = EncodeTokenOpRetV1(tokenid, destpubkeys, {});
        vscript_t vopret;
        GetOpReturnData(opret, vopret);
        std::vector<vscript_t> vData { vopret };
        if (destpubkeys.size() == 1)
            mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, amount, destpubkeys[0], &vData));  // if destEvalCode == EVAL_TOKENS then it is actually equal to MakeCC1vout(EVAL_TOKENS,...)
        else if (destpubkeys.size() == 2)
            mtx.vout.push_back(MakeTokensCC1of2vout(destEvalCode, amount, destpubkeys[0], destpubkeys[1], &vData)); 
        else
        {
            CCerror = "zero or unsupported destination pk count";
            return MakeResultError("zero or unsupported destination pubkey count");
        }

        CAmount CCchange = 0L;
        if (inputs > amount)
			CCchange = (inputs - amount);
        if (CCchange != 0)
            mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, CCchange, mypk));

        // add probe pubkeys to detect token vouts in tx 
        //std::vector<CPubKey> voutTokenPubkeys;
        //for(const auto &pk : destpubkeys)
        //    voutTokenPubkeys.push_back(pk);  // dest pubkey(s) added to opret for validating the vout as token vout (in IsTokensvout() func)
        return MakeResultSuccess("");
    }
    return MakeResultError("could not find token inputs");
}

UniValue TokenFinalizeTransferTx(CMutableTransaction &mtx, struct CCcontract_info *cp, const CPubKey &remotepk, CAmount txfee, const CScript &opret)
{
    if (!TokensIsVer1Active(NULL))
        return MakeResultError("tokens version 1 not active yet");

	uint64_t mask = ~((1LL << mtx.vin.size()) - 1);  // seems, mask is not used anymore
    bool isRemote = IS_REMOTE(remotepk);
    CPubKey mypk = isRemote ? remotepk : pubkey2pk(Mypubkey());
    if (!mypk.IsFullyValid())     {
        CCerror = "mypk is not set or invalid";
        return MakeResultError("my pubkey not set");
    }

    //if (evalcode2)
    //    cp->additionalTokensEvalcode2 = evalcode2;

    //for (const auto &p : probeconds)
    //    CCAddVintxCond(cp, p.first, p.second);

    // TODO maybe add also opret blobs form vintx
    // as now this TokenTransfer() allows to transfer only tokens (including NFTs) that are unbound to other cc
    UniValue sigData = FinalizeCCTxExt(isRemote, mask, cp, mtx, mypk, txfee, opret); 
    LOGSTREAMFN(cctokens_log, CCLOG_DEBUG1, stream << "mtx=" << HexStr(E_MARSHAL(ss << mtx)) << std::endl);
    if (ResultHasTx(sigData)) {
        // LockUtxoInMemory::AddInMemoryTransaction(mtx);  // to be able to spend mtx change
        return sigData;
    }
    else 
    {
        CCerror = "could not finalize tx";
        return MakeResultError("cannot finalize tx");;
    }
}


// token transfer extended version
// params:
// txfee - transaction fee, assumed 10000 if 0
// tokenid - token creation tx id
// tokenaddr - address where unspent token inputs to search
// probeconds - vector of pair of vintx cond and privkey (if null then global priv key will be used) to pick vintx token vouts to sign mtx vins
// destpubkeys - if size=1 then it is the dest pubkey, if size=2 then the dest address is 1of2 addr
// total - token amount to transfer
// returns: signed transfer tx in hex
UniValue TokenTransferExt(const CPubKey &remotepk, int64_t txfee, uint256 tokenid, const char *tokenaddr, std::vector<std::pair<CC*, uint8_t*>> probeconds, std::vector<CPubKey> destpubkeys, int64_t total, bool useMempool)
{
 
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	uint64_t mask; int64_t CCchange = 0, inputs = 0;  
    struct CCcontract_info *cp, C;
    
	if (total < 0)	{
        CCerror = strprintf("negative total");
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << CCerror << "=" << total << std::endl);
        return NullUniValue;
	}

	cp = CCinit(&C, EVAL_TOKENS);

	if (txfee == 0)
		txfee = 10000;

    bool isRemote = IS_REMOTE(remotepk);
    CPubKey mypk = isRemote ? remotepk : pubkey2pk(Mypubkey());
    if (!mypk.IsFullyValid())     {
        CCerror = "mypk is not set or invalid";
        return  NullUniValue;
    }

    int64_t normalInputs = AddNormalinputs(mtx, mypk, txfee, 0x10000, isRemote);
    if (normalInputs > 0)
	{
		mask = ~((1LL << mtx.vin.size()) - 1);  // seems, mask is not used anymore
        
		if ((inputs = AddTokenCCInputs(cp, mtx, tokenaddr, tokenid, total, CC_MAXVINS, useMempool)) > 0)  // NOTE: AddTokenCCInputs might set cp->additionalEvalCode which is used in FinalizeCCtx!
		{
			if (inputs < total) {   //added dimxy
                CCerror = strprintf("insufficient token inputs");
                LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << CCerror << std::endl);
				return  NullUniValue;
			}

            uint8_t destEvalCode = EVAL_TOKENS;
            if (cp->evalcodeNFT != 0)  // if set in AddTokenCCInputs
            {
                destEvalCode = cp->evalcodeNFT;
            }
            
			if (inputs > total)
				CCchange = (inputs - total);

            if (destpubkeys.size() == 1)
			    mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, total, destpubkeys[0]));  // if destEvalCode == EVAL_TOKENS then it is actually equal to MakeCC1vout(EVAL_TOKENS,...)
            else if (destpubkeys.size() == 2)
                mtx.vout.push_back(MakeTokensCC1of2vout(destEvalCode, total, destpubkeys[0], destpubkeys[1])); 
            else
            {
                CCerror = "zero or unsupported destination pk count";
                return  NullUniValue;
            }

			if (CCchange != 0)
				mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, CCchange, mypk));

            // add probe pubkeys to detect token vouts in tx 
			std::vector<CPubKey> voutTokenPubkeys;
            for(auto pk : destpubkeys)
			    voutTokenPubkeys.push_back(pk);  // dest pubkey(s) added to opret for validating the vout as token vout (in IsTokensvout() func)

            // add optional probe conds to non-usual sign vins
            for (auto p : probeconds)
                CCAddVintxCond(cp, p.first, p.second);

            // TODO maybe add also opret blobs form vintx
            // as now this TokenTransfer() allows to transfer only tokens (including NFTs) that are unbound to other cc
			UniValue sigData = FinalizeCCTxExt(isRemote, mask, cp, mtx, mypk, txfee, EncodeTokenOpRetV1(tokenid, voutTokenPubkeys, {} )); 
            if (!ResultHasTx(sigData))
                CCerror = "could not finalize tx";
            //else reserved for use in memory mtx:
            //    LockUtxoInMemory::AddInMemoryTransaction(mtx);  // to be able to spend mtx change
            return sigData;
                                                                                                                                                   
		}
		else {
            CCerror = strprintf("no token inputs");
            LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << CCerror << " for amount=" << total << std::endl);
		}
		//} else fprintf(stderr,"numoutputs.%d != numamounts.%d\n",n,(int32_t)amounts.size());
	}
	else {
        CCerror = strprintf("insufficient normal inputs for tx fee");
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << CCerror << std::endl);
	}
	return  NullUniValue;
}


// transfer token to scriptPubKey
/*
UniValue TokenTransferSpk(const CPubKey &remotepk, int64_t txfee, uint256 tokenid, const char *tokenaddr, std::vector<std::pair<CC*, uint8_t*>> probeconds, const CScript &spk, int64_t total, const std::vector<CPubKey> &voutPubkeys, bool useMempool)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    int64_t CCchange = 0, inputs = 0;

    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_TOKENS);

    if (total < 0) {
        CCerror = strprintf("negative total");
        return NullUniValue;
    }
    if (txfee == 0)
        txfee = 10000;

    bool isRemote = IS_REMOTE(remotepk);
    CPubKey mypk = isRemote ? remotepk : pubkey2pk(Mypubkey());
    if (!mypk.IsFullyValid())
    {
        CCerror = "mypk is not set or invalid";
        return NullUniValue;
    }

    if (AddNormalinputs(mtx, mypk, txfee, 3, isRemote) > 0)
    {
        if ((inputs = AddTokenCCInputs(cp, mtx, tokenaddr, tokenid, total, 60, useMempool)) > 0)
        {
            if (inputs < total) {
                CCerror = strprintf("insufficient token inputs");
                return NullUniValue;
            }

            uint8_t destEvalCode = EVAL_TOKENS;
            if (cp->additionalTokensEvalcode2 != 0)
                destEvalCode = cp->additionalTokensEvalcode2; // this is NFT

            // check if it is NFT
            //if (vopretNonfungible.size() > 0)
            //    destEvalCode = vopretNonfungible.begin()[0];

            if (inputs > total)
                CCchange = (inputs - total);
            mtx.vout.push_back(CTxOut(total, spk));
            if (CCchange != 0)
                mtx.vout.push_back(MakeTokensCC1vout(destEvalCode, CCchange, mypk));

            // add optional probe conds to non-usual sign vins
            for (auto p : probeconds)
                CCAddVintxCond(cp, p.first, p.second);

            UniValue sigData = FinalizeCCTxExt(isRemote, 0, cp, mtx, mypk, txfee, EncodeTokenOpRet(tokenid, voutPubkeys, std::make_pair((uint8_t)0, vscript_t())));
            if (!ResultHasTx(sigData))
                CCerror = "could not finalize tx";
            return sigData;
        }
        else {
            CCerror = strprintf("no token inputs");
        }
    }
    else
    {
        CCerror = "insufficient normal inputs for tx fee";
    }
    return  NullUniValue;
}
*/

int64_t GetTokenBalance(CPubKey pk, uint256 tokenid, bool usemempool)
{
	uint256 hashBlock;
	CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	CTransaction tokentx;
    uint256 tokenidInOpret;
    std::vector<CPubKey> pks;
    std::vector<vscript_t> oprets;

	// CCerror = strprintf("obsolete, cannot return correct value without eval");
	// return 0;

	if (myGetTransaction(tokenid, tokentx, hashBlock) == 0)
	{
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << "cant find tokenid" << std::endl);
		CCerror = strprintf("cant find tokenid");
		return 0;
	}

    uint8_t funcid = DecodeTokenOpRetV1(tokentx.vout.back().scriptPubKey, tokenidInOpret, pks, oprets);
    if (tokentx.vout.size() < 2 || !IsTokenCreateFuncid(funcid))
    {
        CCerror = strprintf("not a tokenid (invalid tokenbase)");
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
    CTransaction tokenbaseTx; 
    std::vector<uint8_t> origpubkey; 
    std::vector<vscript_t>  oprets;
    vscript_t vopretNonfungible;
    std::string name, description; 
    uint8_t version;

    struct CCcontract_info *cpTokens, tokensCCinfo;
    cpTokens = CCinit(&tokensCCinfo, EVAL_TOKENS);

	if( !myGetTransaction(tokenid, tokenbaseTx, hashBlock) )
	{
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << "cant find tokenid" << std::endl);
		result.push_back(Pair("result", "error"));
		result.push_back(Pair("error", "cant find tokenid"));
		return(result);
	}
    if ( KOMODO_NSPV_FULLNODE && hashBlock.IsNull()) {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "the transaction is still in mempool"));
        return(result);
    }

    uint8_t funcid = DecodeTokenCreateOpRetV1(tokenbaseTx.vout.back().scriptPubKey, origpubkey, name, description, oprets);
	if (tokenbaseTx.vout.size() > 0 && !IsTokenCreateFuncid(funcid))
	{
        LOGSTREAMFN(cctokens_log, CCLOG_INFO, stream << "passed tokenid isnt token creation txid" << std::endl);
		result.push_back(Pair("result", "error"));
		result.push_back(Pair("error", "tokenid isnt token creation txid"));
        return result;
	}
	result.push_back(Pair("result", "success"));
	result.push_back(Pair("tokenid", tokenid.GetHex()));
	result.push_back(Pair("owner", HexStr(origpubkey)));
	result.push_back(Pair("name", name));

    int64_t supply = 0, output;
    for (int v = 0; v < tokenbaseTx.vout.size(); v++)
        if ((output = IsTokensvout(false, true, cpTokens, NULL, tokenbaseTx, v, tokenid)) > 0)
            supply += output;
	result.push_back(Pair("supply", supply));
	result.push_back(Pair("description", description));

    //GetOpretBlob(oprets, OPRETID_NONFUNGIBLEDATA, vopretNonfungible);
    if (oprets.size() > 0)
        vopretNonfungible = oprets[0];
    if( !vopretNonfungible.empty() )    
        result.push_back(Pair("data", HexStr(vopretNonfungible)));

    if (tokenbaseTx.IsCoinImport()) { // if imported token
        ImportProof proof;
        CTransaction burnTx;
        std::vector<CTxOut> payouts;
        CTxDestination importaddress;

        std::string sourceSymbol = "can't decode";
        std::string sourceTokenId = "can't decode";

        if (UnmarshalImportTx(tokenbaseTx, proof, burnTx, payouts))
        {
            // extract op_return to get burn source chain.
            std::vector<uint8_t> burnOpret;
            std::string targetSymbol;
            uint32_t targetCCid;
            uint256 payoutsHash;
            std::vector<uint8_t> rawproof;
            if (UnmarshalBurnTx(burnTx, targetSymbol, &targetCCid, payoutsHash, rawproof)) {
                if (rawproof.size() > 0) {
                    CTransaction tokenbasetx;
                    E_UNMARSHAL(rawproof, ss >> sourceSymbol;
                    if (!ss.eof())
                        ss >> tokenbasetx);
                    
                    if (!tokenbasetx.IsNull())
                        sourceTokenId = tokenbasetx.GetHash().GetHex();
                }
            }
        }
        result.push_back(Pair("IsImported", "yes"));
        result.push_back(Pair("sourceChain", sourceSymbol));
        result.push_back(Pair("sourceTokenId", sourceTokenId));
    }

	return result;
}

UniValue TokenList()
{
	UniValue result(UniValue::VARR);
	std::vector<uint256> txids;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > addressIndexCCMarker;

	struct CCcontract_info *cp, C; uint256 txid, hashBlock;
	CTransaction vintx; std::vector<uint8_t> origpubkey;
	std::string name, description;

	cp = CCinit(&C, EVAL_TOKENS);

    auto addTokenId = [&](uint256 txid) {
        if (myGetTransaction(txid, vintx, hashBlock) != 0) {
            if (vintx.vout.size() > 0 && DecodeTokenCreateOpRetV1(vintx.vout[vintx.vout.size() - 1].scriptPubKey, origpubkey, name, description) != 0) {
                result.push_back(txid.GetHex());
            }
        }
    };

	SetCCtxids(txids, cp->normaladdr, false, cp->evalcode, 0, zeroid, 'c');                      // find by old normal addr marker
   	for (std::vector<uint256>::const_iterator it = txids.begin(); it != txids.end(); it++) 	{
        addTokenId(*it);
	}

    SetCCunspents(addressIndexCCMarker, cp->unspendableCCaddr, true);    // find by burnable validated cc addr marker
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = addressIndexCCMarker.begin(); it != addressIndexCCMarker.end(); it++) {
        addTokenId(it->first.txhash);
    }

	return(result);
}
