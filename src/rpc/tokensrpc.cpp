/******************************************************************************
 * Copyright  2014-2019 The SuperNET Developers.                             *
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

#include <stdint.h>
#include <string.h>
#include <numeric>
#include "univalue.h"
#include "amount.h"
#include "rpc/server.h"
#include "rpc/protocol.h"

#include "../wallet/crypter.h"
#include "../wallet/rpcwallet.h"

#include "sync_ext.h"

#include "../cc/CCinclude.h"
#include "../cc/CCtokens.h"
#include "../cc/CCassets.h"


using namespace std;

UniValue assetsaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
	struct CCcontract_info *cp, C; std::vector<unsigned char> pubkey;
	cp = CCinit(&C, EVAL_ASSETS);
	if (fHelp || params.size() > 1)
		throw runtime_error("assetsaddress [pubkey]\n");
	if (ensure_CCrequirements(cp->evalcode) < 0)
		throw runtime_error(CC_REQUIREMENTS_MSG);
	if (params.size() == 1)
		pubkey = ParseHex(params[0].get_str().c_str());
	return(CCaddress(cp, (char *)"Assets", pubkey));
}

UniValue tokenaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp,C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C,EVAL_TOKENS);
    if ( fHelp || params.size() > 1 )
        throw runtime_error("tokenaddress [pubkey]\n");
    if ( ensure_CCrequirements(cp->evalcode) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if ( params.size() == 1 )
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp,(char *)"Tokens", pubkey));
}

UniValue tokenlist(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 tokenid;
    if ( fHelp || params.size() > 0 )
        throw runtime_error("tokenlist\n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    return(TokenList());
}

UniValue tokeninfo(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 tokenid;
    if ( fHelp || params.size() != 1 )
        throw runtime_error("tokeninfo tokenid\n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    return(TokenInfo(tokenid));
}

UniValue tokenorders(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 tokenid;
    uint8_t evalcodeNFT = 0;

    if ( fHelp || params.size() > 2 )
        throw runtime_error("tokenorders [tokenid|'*'] [evalcode]\n"
                            "returns token orders for the tokenid or all available token orders if tokenid is not set\n"
                            "returns also NFT ask orders if NFT evalcode is set\n" "\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
	if (params.size() >= 1) 
    {
        if (params[0].get_str() != "*")
        {
		    tokenid = Parseuint256((char *)params[0].get_str().c_str());
		    if (tokenid == zeroid) 
			    throw runtime_error("incorrect tokenid\n");
        }
    }
    if (params.size() == 2)
        evalcodeNFT = strtol(params[1].get_str().c_str(), NULL, 0);  // supports also 0xEE-like values

    if (TokensIsVer1Active(NULL))
        return AssetOrders(tokenid, CPubKey(), evalcodeNFT);
    else
        return tokensv0::AssetOrders(tokenid, CPubKey(), evalcodeNFT);
}


UniValue mytokenorders(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    uint256 tokenid;
    if (fHelp || params.size() > 1)
        throw runtime_error("mytokenorders [evalcode]\n"
                            "returns all the token orders for mypubkey\n"
                            "if evalcode is set then returns mypubkey token orders for non-fungible tokens with this evalcode\n" "\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    uint8_t evalcodeNFT = 0;
    if (params.size() == 1)
        evalcodeNFT = strtol(params[0].get_str().c_str(), NULL, 0);  // supports also 0xEE-like values
    
    if (TokensIsVer1Active(NULL))
        return AssetOrders(zeroid, Mypubkey(), evalcodeNFT);
    else
        return tokensv0::AssetOrders(zeroid, Mypubkey(), evalcodeNFT);

}

UniValue tokenbalance(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); uint256 tokenid; uint64_t balance; std::vector<unsigned char> pubkey; struct CCcontract_info *cp,C;
	CCerror.clear();

    if ( fHelp || params.size() < 1 || params.size() > 2 )
        throw runtime_error("tokenbalance tokenid [pubkey]\n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    
	LOCK(cs_main);

    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    if ( params.size() == 2 )
        pubkey = ParseHex(params[1].get_str().c_str());
    else 
		pubkey = Mypubkey();

    balance = GetTokenBalance(pubkey2pk(pubkey),tokenid);

	if (CCerror.empty()) {
		char destaddr[64];

		result.push_back(Pair("result", "success"));
        cp = CCinit(&C,EVAL_TOKENS);
		if (GetCCaddress(cp, destaddr, pubkey2pk(pubkey)) != 0)
			result.push_back(Pair("CCaddress", destaddr));

		result.push_back(Pair("tokenid", params[0].get_str()));
		result.push_back(Pair("balance", (int64_t)balance));
	}
	else {
		ERR_RESULT(CCerror);
	}

    return(result);
}

UniValue tokencreate(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ);
    std::string name, description, hextx; 
    std::vector<uint8_t> nonfungibleData;
    int64_t supply; // changed from uin64_t to int64_t for this 'if ( supply <= 0 )' to work as expected

    CCerror.clear();

    if ( fHelp || params.size() > 4 || params.size() < 2 )
        throw runtime_error("tokencreate name supply [description][data]\n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);

    name = params[0].get_str();
    if (name.size() == 0 || name.size() > 32)   {
        ERR_RESULT("Token name must not be empty and up to 32 characters");
        return(result);
    }

    supply = AmountFromValue(params[1]);   
    if (supply <= 0)    {
        ERR_RESULT("Token supply must be positive");
        return(result);
    }
    
    if (params.size() >= 3)     {
        description = params[2].get_str();
        if (description.size() > 4096)   {
            ERR_RESULT("Token description must be <= 4096 characters");
            return(result);
        }
    }
    
    if (params.size() == 4)    {
        nonfungibleData = ParseHex(params[3].get_str());
        if (nonfungibleData.size() > IGUANA_MAXSCRIPTSIZE) // opret limit
        {
            ERR_RESULT("Non-fungible data size must be <= " + std::to_string(IGUANA_MAXSCRIPTSIZE));
            return(result);
        }
        if( nonfungibleData.empty() ) {
            ERR_RESULT("Non-fungible data incorrect");
            return(result);
        }
    }

    hextx = CreateTokenLocal(0, supply, name, description, nonfungibleData);
    RETURN_IF_ERROR(CCerror);

    if( hextx.size() > 0 )     {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hextx));
    } 
    else
        result = MakeResultError("could not create token");
    return(result);
}

UniValue tokentransfer(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); 
    std::string hex; 
    int64_t amount; 
    uint256 tokenid;
    
    CCerror.clear();

    if ( fHelp || params.size() < 3)
        throw runtime_error("tokentransfer tokenid destpubkey amount \n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    std::vector<unsigned char> pubkey(ParseHex(params[1].get_str().c_str()));
	amount = atoll(params[2].get_str().c_str()); 
    if( tokenid == zeroid )    {
        ERR_RESULT("invalid tokenid");
        return(result);
    }
    if( amount <= 0 )    {
        ERR_RESULT("amount must be positive");
        return(result);
    }

    hex = TokenTransfer(0, tokenid, pubkey, amount);
    RETURN_IF_ERROR(CCerror);
    if (hex.size() > 0)
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    }
    else
        result = MakeResultError("could not transfer token");
    return(result);
}

UniValue tokentransfermany(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ); 
    std::string hex; 
    CAmount amount; 
    uint256 tokenid;
    const CAmount txfee = 10000;
    
    CCerror.clear();

    if ( fHelp || params.size() < 3)
        throw runtime_error("tokentransfermany tokenid1,tokenid2,... destpubkey amount \n");
    if ( ensure_CCrequirements(EVAL_TOKENS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);

    std::vector<uint256> tokenids;
    int i = 0;
    for (; i < params.size() - 2; i ++)
    {
        uint256 tokenid = Parseuint256((char *)params[i].get_str().c_str());
        if( tokenid == zeroid )    {
            result = MakeResultError("invalid tokenid");
            return(result);
        }
        tokenids.push_back(tokenid);
    }
    CPubKey destpk = pubkey2pk(ParseHex(params[i++].get_str().c_str()));
	if (destpk.size() != CPubKey::COMPRESSED_PUBLIC_KEY_SIZE) {
        result = MakeResultError("invalid destpubkey");
        return(result);
    }    
    amount = atoll(params[i].get_str().c_str()); 
    if( amount <= 0 )    {
        result = MakeResultError("amount must be positive");
        return(result);
    }        
    
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);

    CPubKey mypk = remotepk.IsValid() ? remotepk : pubkey2pk(Mypubkey());

    CMutableTransaction mtx;
    struct CCcontract_info *cpTokens, CTokens;
    cpTokens = CCinit(&CTokens, EVAL_TOKENS);

    UniValue beginResult = TokenBeginTransferTx(mtx, cpTokens, remotepk, txfee);
    if (ResultIsError(beginResult)) 
        return beginResult;
    
    for (const auto &tokenid : tokenids)
    {
        vuint8_t vnftData;
        GetNonfungibleData(tokenid, vnftData);
        CC* probeCond;
        if (vnftData.size() > 0)
            probeCond = MakeTokensCCcond1(vnftData[0], mypk);
        else
            probeCond = MakeCCcond1(EVAL_TOKENS, mypk);

        uint8_t mypriv[32];
        Myprivkey(mypriv);
        
        char tokenaddr[KOMODO_ADDRESS_BUFSIZE];
        cpTokens->evalcodeNFT = vnftData.size() > 0 ? vnftData[0] : 0;
        GetTokensCCaddress(cpTokens, tokenaddr, mypk);

        UniValue addtxResult = TokenAddTransferVout(mtx, cpTokens, destpk, tokenid, tokenaddr, { destpk }, {probeCond, mypriv}, amount, false);
        cc_free(probeCond);
        memset(mypriv, '\0', sizeof(mypriv));
        if (ResultIsError(addtxResult)) 
            return MakeResultError( ResultGetError(addtxResult) + " " + tokenid.GetHex() );
    }
    UniValue sigData = TokenFinalizeTransferTx(mtx, cpTokens, remotepk, txfee, CScript());
    RETURN_IF_ERROR(CCerror);
    if (ResultHasTx(sigData) > 0)
        result = sigData;
    else
        result = MakeResultError("could not transfer token:" + ResultGetError(sigData) );
    return(result);
}

UniValue tokenconvert(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string hex; int32_t evalcode; int64_t amount; uint256 tokenid;
    CCerror.clear();
    if ( fHelp || params.size() != 4 )
        throw runtime_error("tokenconvert evalcode tokenid pubkey amount\n");
    if ( ensure_CCrequirements(EVAL_ASSETS) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    evalcode = atoi(params[0].get_str().c_str());
    tokenid = Parseuint256((char *)params[1].get_str().c_str());
    std::vector<unsigned char> pubkey(ParseHex(params[2].get_str().c_str()));
    //amount = atol(params[3].get_str().c_str());
	amount = atoll(params[3].get_str().c_str()); // dimxy changed to prevent loss of significance
    if ( tokenid == zeroid )
    {
        ERR_RESULT("invalid tokenid");
        return(result);
    }
    if ( amount <= 0 )
    {
        ERR_RESULT("amount must be positive");
        return(result);
    }

	ERR_RESULT("deprecated");
	return(result);

/*    hex = AssetConvert(0,tokenid,pubkey,amount,evalcode);
    if (amount > 0) {
        if ( hex.size() > 0 )
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else ERR_RESULT("couldnt convert tokens");
    } else {
        ERR_RESULT("amount must be positive");
    }
    return(result); */
}

UniValue tokenbid(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); int64_t bidamount,numtokens; std::string hex; uint256 tokenid;

    CCerror.clear();
    if ( fHelp || params.size() != 3 )
        throw runtime_error("tokenbid numtokens tokenid price\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);

    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);

	numtokens = atoll(params[0].get_str().c_str());  
    tokenid = Parseuint256((char *)params[1].get_str().c_str());
    CAmount price = AmountFromValue(params[2]);
    bidamount = (price * numtokens);
    if (price <= 0)
    {
        ERR_RESULT("price must be positive");
        return(result);
    }
    if (tokenid == zeroid)
    {
        ERR_RESULT("invalid tokenid");
        return(result);
    }
    if (bidamount <= 0)
    {
        ERR_RESULT("bid amount must be positive");
        return(result);
    }
    if (TokensIsVer1Active(NULL))
        hex = CreateBuyOffer(0, bidamount, tokenid, numtokens);
    else
        hex = tokensv0::CreateBuyOffer(0, bidamount, tokenid, numtokens);
    RETURN_IF_ERROR(CCerror);

    if (price > 0 && numtokens > 0) 
    {
        if (hex.size() > 0)
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } 
        else 
            ERR_RESULT("couldnt create bid");
    } else {
        ERR_RESULT("price and numtokens must be positive");
    }
    return(result);
}

UniValue tokencancelbid(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string hex; int32_t i; uint256 tokenid,bidtxid;
    CCerror.clear();
    if (fHelp || params.size() != 2)
        throw runtime_error("tokencancelbid tokenid bidtxid\n");

    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);

    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);

    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    bidtxid = Parseuint256((char *)params[1].get_str().c_str());
    if ( tokenid == zeroid || bidtxid == zeroid )
    {
        result.push_back(Pair("error", "invalid parameter"));
        return(result);
    }

    if (TokensIsVer1Active(NULL))
        hex = CancelBuyOffer(0,tokenid,bidtxid);
    else
        hex = tokensv0::CancelBuyOffer(0,tokenid,bidtxid);
    RETURN_IF_ERROR(CCerror);
    if (hex.size() > 0)
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } 
    else 
        ERR_RESULT("couldnt cancel bid");
    return(result);
}

UniValue tokenfillbid(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); 
    int64_t fillamount; 
    std::string hex; 
    uint256 tokenid,bidtxid;

    CCerror.clear();

    if (fHelp || params.size() != 3 && params.size() != 4)
        throw runtime_error("tokenfillbid tokenid bidtxid fillamount [unit_price]\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);

    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    bidtxid = Parseuint256((char *)params[1].get_str().c_str());
    fillamount = atoll(params[2].get_str().c_str());		
    if (fillamount <= 0)
    {
        ERR_RESULT("fillamount must be positive");
        return(result);
    }
    if (tokenid == zeroid || bidtxid == zeroid)
    {
        ERR_RESULT("must provide tokenid and bidtxid");
        return(result);
    }
    CAmount unit_price = 0LL;
    if (params.size() == 4)
	    unit_price = AmountFromValue(params[3].get_str().c_str());

    if (TokensIsVer1Active(NULL))	 
        hex = FillBuyOffer(0, tokenid, bidtxid, fillamount, unit_price);
    else
        hex = tokensv0::FillBuyOffer(0, tokenid, bidtxid, fillamount);

    RETURN_IF_ERROR(CCerror);
    if (hex.size() > 0)
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } 
    else 
        ERR_RESULT("couldnt fill bid");
    return(result);
}

UniValue tokenask(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); 
    int64_t askamount, numtokens; 
    std::string hex; 
    uint256 tokenid;

    CCerror.clear();
    if (fHelp || params.size() != 3)
        throw runtime_error("tokenask numtokens tokenid price\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);

	numtokens = atoll(params[0].get_str().c_str());			
    tokenid = Parseuint256((char *)params[1].get_str().c_str());
    CAmount price = AmountFromValue(params[2]);
    askamount = (price * numtokens);
	//std::cerr << std::boolalpha << "tokenask(): (tokenid == zeroid) is "  << (tokenid == zeroid) << " (numtokens <= 0) is " << (numtokens <= 0) << " (price <= 0) is " << (price <= 0) << " (askamount <= 0) is " << (askamount <= 0) << std::endl;
    if (tokenid == zeroid || numtokens <= 0 || price <= 0 || askamount <= 0)
    {
        ERR_RESULT("invalid parameter");
        return(result);
    }
    if (TokensIsVer1Active(NULL))	 
        hex = CreateSell(0, numtokens, tokenid, askamount);
    else
        hex = tokensv0::CreateSell(0, numtokens, tokenid, askamount);
    RETURN_IF_ERROR(CCerror);
    if (hex.size() > 0)
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } 
    else 
        ERR_RESULT("couldnt create ask");
    
    return(result);
}

UniValue tokenswapask(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    static uint256 zeroid;
    UniValue result(UniValue::VOBJ); int64_t askamount,numtokens; std::string hex; double price; uint256 tokenid,otherid;

    CCerror.clear();
    if (fHelp || params.size() != 4)
        throw runtime_error("tokenswapask numtokens tokenid otherid price\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);

    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);

    throw runtime_error("tokenswapask not supported\n");

    //numtokens = atoi(params[0].get_str().c_str());
	numtokens = atoll(params[0].get_str().c_str());			// dimxy changed to prevent loss of significance
    tokenid = Parseuint256((char *)params[1].get_str().c_str());
    otherid = Parseuint256((char *)params[2].get_str().c_str());
    price = atof(params[3].get_str().c_str());
    askamount = (price * numtokens);
    hex = CreateSwap(0,numtokens,tokenid,otherid,askamount);
    RETURN_IF_ERROR(CCerror);
    if (price > 0 && numtokens > 0) {
        if ( hex.size() > 0 )
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else ERR_RESULT("couldnt create swap");
    } else {
        ERR_RESULT("price and numtokens must be positive");
    }
    return(result);
}

UniValue tokencancelask(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); std::string hex; int32_t i; uint256 tokenid,asktxid;

    CCerror.clear();
    if (fHelp || params.size() != 2)
        throw runtime_error("tokencancelask tokenid asktxid\n");

    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);

    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    asktxid = Parseuint256((char *)params[1].get_str().c_str());
    if (tokenid == zeroid || asktxid == zeroid)
    {
        result.push_back(Pair("error", "invalid parameter"));
        return(result);
    }
    if (TokensIsVer1Active(NULL))	 
        hex = CancelSell(0, tokenid, asktxid);
    else
        hex = tokensv0::CancelSell(0, tokenid, asktxid);

    RETURN_IF_ERROR(CCerror);
    if (hex.size() > 0)
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", hex));
    } 
    else 
        ERR_RESULT("couldnt cancel ask");
    return(result);
}

UniValue tokenfillask(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    UniValue result(UniValue::VOBJ); 
    int64_t fillunits; 
    std::string hex; 
    uint256 tokenid,asktxid;

    if (fHelp || params.size() != 3 && params.size() != 4)
        throw runtime_error("tokenfillask tokenid asktxid fillunits [unitprice]\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0 || ensure_CCrequirements(EVAL_TOKENS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);

    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);

    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    asktxid = Parseuint256((char *)params[1].get_str().c_str());
	fillunits = atoll(params[2].get_str().c_str());	 
    if (fillunits <= 0)
    {
        ERR_RESULT("fillunits must be positive");
        return(result);
    }
    if (tokenid == zeroid || asktxid == zeroid)
    {
        result.push_back(Pair("error", "invalid parameter"));
        return(result);
    }
    CAmount unit_price = 0LL;
    if (params.size() == 4)
	    unit_price = AmountFromValue(params[3].get_str().c_str());	 

    if (TokensIsVer1Active(NULL))	 
        hex = FillSell(0, tokenid, zeroid, asktxid, fillunits, unit_price);
    else
        hex = tokensv0::FillSell(0, tokenid, zeroid, asktxid, fillunits);
    RETURN_IF_ERROR(CCerror);
    if (fillunits > 0) 
    {
        if (hex.size() > 0) {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else {
            ERR_RESULT("couldnt fill ask");
        }
    } else {
        ERR_RESULT("fillunits must be positive");
    }
    return(result);
}

UniValue tokenfillswap(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    static uint256 zeroid;
    UniValue result(UniValue::VOBJ); 
    int64_t fillunits; std::string hex; uint256 tokenid,otherid,asktxid;

    CCerror.clear();
    if (fHelp || params.size() != 4 && params.size() != 5)
        throw runtime_error("tokenfillswap tokenid otherid asktxid fillunits [unitprice]\n");
    if (ensure_CCrequirements(EVAL_ASSETS) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
        
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);

    throw runtime_error("tokenfillswap not supported\n");

    tokenid = Parseuint256((char *)params[0].get_str().c_str());
    otherid = Parseuint256((char *)params[1].get_str().c_str());
    asktxid = Parseuint256((char *)params[2].get_str().c_str());
    //fillunits = atol(params[3].get_str().c_str());
	fillunits = atoll(params[3].get_str().c_str());  // dimxy changed to prevent loss of significance
    CAmount unit_price = 0LL;
    if (params.size() == 5)
	    unit_price = AmountFromValue(params[4].get_str().c_str());

    hex = FillSell(0,tokenid,otherid,asktxid,fillunits, unit_price);
    RETURN_IF_ERROR(CCerror);
    if (fillunits > 0) {
        if ( hex.size() > 0 ) {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", hex));
        } else ERR_RESULT("couldnt fill bid");
    } else {
        ERR_RESULT("fillunits must be positive");
    }
    return(result);
}


static const CRPCCommand commands[] =
{ //  category              name                actor (function)        okSafeMode
  //  -------------- ------------------------  -----------------------  ----------
  // Marmara
     // tokens & assets
	{ "tokens",       "assetsaddress",     &assetsaddress,      true },
    { "tokens",       "tokeninfo",        &tokeninfo,         true },
    { "tokens",       "tokenlist",        &tokenlist,         true },
    { "tokens",       "tokenorders",      &tokenorders,       true },
    { "tokens",       "mytokenorders",    &mytokenorders,     true },
    { "tokens",       "tokenaddress",     &tokenaddress,      true },
    { "tokens",       "tokenbalance",     &tokenbalance,      true },
    { "tokens",       "tokencreate",      &tokencreate,       true },
    { "tokens",       "tokentransfer",    &tokentransfer,     true },
    { "tokens",       "tokentransfermany",    &tokentransfermany,     true },
    { "tokens",       "tokenbid",         &tokenbid,          true },
    { "tokens",       "tokencancelbid",   &tokencancelbid,    true },
    { "tokens",       "tokenfillbid",     &tokenfillbid,      true },
    { "tokens",       "tokenask",         &tokenask,          true },
    //{ "tokens",       "tokenswapask",     &tokenswapask,      true },
    { "tokens",       "tokencancelask",   &tokencancelask,    true },
    { "tokens",       "tokenfillask",     &tokenfillask,      true },
    //{ "tokens",       "tokenfillswap",    &tokenfillswap,     true },
    { "tokens",       "tokenconvert", &tokenconvert, true },
};

void RegisterTokensRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
