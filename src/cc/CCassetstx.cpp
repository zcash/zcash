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

#include "CCassets.h"
#include "CCtokens.h"


UniValue AssetOrders(uint256 refassetid, CPubKey pk, uint8_t evalCodeNFT)
{
	UniValue result(UniValue::VARR);  

    struct CCcontract_info *cpAssets, assetsC;
    struct CCcontract_info *cpTokens, tokensC;

    cpAssets = CCinit(&assetsC, EVAL_ASSETS);
    cpTokens = CCinit(&tokensC, EVAL_TOKENS);

	auto addOrders = [&](struct CCcontract_info *cp, std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it)
	{
		uint256 txid, hashBlock, assetid, assetid2;
		int64_t unit_price;
		std::vector<uint8_t> origpubkey;
		CTransaction ordertx;
		uint8_t funcid, evalCode;
		char numstr[32], funcidstr[16], origaddr[KOMODO_ADDRESS_BUFSIZE], origtokenaddr[KOMODO_ADDRESS_BUFSIZE];

        txid = it->first.txhash;
        LOGSTREAM(ccassets_log, CCLOG_DEBUG2, stream << "addOrders() checking txid=" << txid.GetHex() << std::endl);
        if ( myGetTransaction(txid, ordertx, hashBlock) != 0)
        {
            if (ordertx.vout.size() > ASSETS_GLOBALADDR_VOUT && (funcid = DecodeAssetTokenOpRet(ordertx.vout.back().scriptPubKey, evalCode, assetid, assetid2, unit_price, origpubkey)) != 0)
            {
                LOGSTREAM(ccassets_log, CCLOG_DEBUG2, stream << "addOrders() checking ordertx.vout.size()=" << ordertx.vout.size() << " funcid=" << (char)(funcid ? funcid : ' ') << " assetid=" << assetid.GetHex() << std::endl);

                if (!pk.IsValid() && (refassetid == zeroid || assetid == refassetid) || // tokenorders
                    pk.IsValid() && pk == pubkey2pk(origpubkey))  // mytokenorders
                {

                    LOGSTREAM(ccassets_log, CCLOG_DEBUG2, stream << "addOrders() it->first.index=" << it->first.index << " ordertx.vout[it->first.index].nValue=" << ordertx.vout[it->first.index].nValue << std::endl);
                    if (ordertx.vout[it->first.index].nValue == 0) {
                        LOGSTREAM(ccassets_log, CCLOG_DEBUG2, stream << "addOrders() order with value=0 skipped" << std::endl);
                        return;
                    }

                    UniValue item(UniValue::VOBJ);

                    funcidstr[0] = funcid;
                    funcidstr[1] = 0;
                    item.push_back(Pair("funcid", funcidstr));
                    item.push_back(Pair("txid", txid.GetHex()));
                    item.push_back(Pair("vout", (int64_t)it->first.index));
                    if (funcid == 'b' || funcid == 'B')
                    {
                        sprintf(numstr, "%.8f", (double)ordertx.vout[it->first.index].nValue / COIN);
                        item.push_back(Pair("amount", numstr));
                        sprintf(numstr, "%.8f", (double)ordertx.vout[0].nValue / COIN);
                        item.push_back(Pair("bidamount", numstr));
                    }
                    else
                    {
                        sprintf(numstr, "%lld", (long long)ordertx.vout[it->first.index].nValue);
                        item.push_back(Pair("amount", numstr));
                        sprintf(numstr, "%lld", (long long)ordertx.vout[0].nValue);
                        item.push_back(Pair("askamount", numstr));
                    }
                    if (origpubkey.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE)
                    {
                        GetCCaddress(cp, origaddr, pubkey2pk(origpubkey));  
                        item.push_back(Pair("origaddress", origaddr));
                        GetTokensCCaddress(cpTokens, origtokenaddr, pubkey2pk(origpubkey));
                        item.push_back(Pair("origtokenaddress", origtokenaddr));
                    }
                    if (assetid != zeroid)
                        item.push_back(Pair("tokenid", assetid.GetHex()));
                    if (assetid2 != zeroid)
                        item.push_back(Pair("otherid", assetid2.GetHex()));
                    if (unit_price > 0)
                    {
                        if (funcid == 's' || funcid == 'S' || funcid == 'e' || funcid == 'E')
                        {
                            sprintf(numstr, "%.8f", (double)unit_price * ordertx.vout[ASSETS_GLOBALADDR_VOUT].nValue / COIN);
                            item.push_back(Pair("totalrequired", numstr));
                            //sprintf(numstr, "%.8f", (double)remaining_units / (COIN * ordertx.vout[0].nValue));
                            item.push_back(Pair("price", ValueFromAmount(unit_price)));
                        }
                        else
                        {
                            item.push_back(Pair("totalrequired", unit_price ? (int64_t)ordertx.vout[ASSETS_GLOBALADDR_VOUT].nValue / unit_price : 0));
                            //sprintf(numstr, "%.8f", (double)ordertx.vout[0].nValue / (remaining_units * COIN));
                            item.push_back(Pair("price", ValueFromAmount(unit_price)));
                        }
                    }
                    result.push_back(item);
                    LOGSTREAM(ccassets_log, CCLOG_DEBUG1, stream << "addOrders() added order funcId=" << (char)(funcid ? funcid : ' ') << " it->first.index=" << it->first.index << " ordertx.vout[it->first.index].nValue=" << ordertx.vout[it->first.index].nValue << " tokenid=" << assetid.GetHex() << std::endl);
                }
            }
        }
	};

    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputsTokens, unspentOutputsNFTs, unspentOutputsCoins;

	char assetsUnspendableAddr[KOMODO_ADDRESS_BUFSIZE];
	GetCCaddress(cpAssets, assetsUnspendableAddr, GetUnspendable(cpAssets, NULL));
	SetCCunspents(unspentOutputsCoins, assetsUnspendableAddr, true);

	char assetsTokensUnspendableAddr[KOMODO_ADDRESS_BUFSIZE];
    std::vector<uint8_t> vopretNFT;
    if (refassetid != zeroid) {
        GetNonfungibleData(refassetid, vopretNFT);
        if (vopretNFT.size() > 0)
            cpAssets->evalcodeNFT = vopretNFT.begin()[0];
    }
	GetTokensCCaddress(cpAssets, assetsTokensUnspendableAddr, GetUnspendable(cpAssets, NULL));
	SetCCunspents(unspentOutputsTokens, assetsTokensUnspendableAddr, true);

    // tokenbids:
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator itCoins = unspentOutputsCoins.begin();
        itCoins != unspentOutputsCoins.end();
        itCoins++)
        addOrders(cpAssets, itCoins);
    
    // tokenasks:
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator itTokens = unspentOutputsTokens.begin();
		itTokens != unspentOutputsTokens.end();
		itTokens++)
		addOrders(cpAssets, itTokens);

    if (evalCodeNFT != 0) {  //this would be mytokenorders
        char assetsNFTUnspendableAddr[KOMODO_ADDRESS_BUFSIZE];

        // try also dual eval tokenasks (and we do not need bids (why? bids are on assets global addr anyway.)):
        cpAssets->evalcodeNFT = evalCodeNFT;
        GetTokensCCaddress(cpAssets, assetsNFTUnspendableAddr, GetUnspendable(cpAssets, NULL));
        SetCCunspents(unspentOutputsNFTs, assetsNFTUnspendableAddr,true);

        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator itNFTs = unspentOutputsNFTs.begin();
            itNFTs != unspentOutputsNFTs.end();
            itNFTs++)
            addOrders(cpAssets, itNFTs);
    }
    return(result);
}

// rpc tokenbid implementation, locks 'bidamount' coins for the 'pricetotal' of tokens
UniValue CreateBuyOffer(const CPubKey &mypk, int64_t txfee, int64_t bidamount, uint256 assetid, int64_t numtokens)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	struct CCcontract_info *cpAssets, C; 
	uint256 hashBlock; 
	CTransaction vintx; 
	std::vector<uint8_t> origpubkey; 
	std::string name,description;
	int64_t inputs;
    std::vector <vscript_t> oprets;

    if (bidamount < 0 || numtokens < 0)
    {
        CCerror = "negative bidamount or numtokens";
        return("");
    }

    // check if valid token
    if (myGetTransaction(assetid, vintx, hashBlock) == 0)
    {
        CCerror = "could not find assetid\n";
        return("");
    }
    if (vintx.vout.size() == 0 || DecodeTokenCreateOpRetV1(vintx.vout.back().scriptPubKey, origpubkey, name, description, oprets) == 0)
    {
        CCerror = "assetid isn't token creation txid\n";
        return("");
    }

    cpAssets = CCinit(&C, EVAL_ASSETS);   // NOTE: assets here!
    if (txfee == 0)
        txfee = 10000;

    // use AddNormalinputsRemote to sign only with mypk
    if ((inputs = AddNormalinputsRemote(mtx, mypk, bidamount+(txfee+ASSETS_MARKER_AMOUNT), 0x10000)) > 0)   
    {
		if (inputs < bidamount+txfee) {
			CCerror = strprintf("insufficient coins to make buy offer");
			return ("");
		}

        CAmount unit_price = bidamount / numtokens;

		CPubKey unspendableAssetsPubkey = GetUnspendable(cpAssets, 0);
        mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, bidamount, unspendableAssetsPubkey));
        mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, ASSETS_MARKER_AMOUNT, mypk));

        UniValue sigData = FinalizeCCTxExt(IsRemoteRPCCall(), 0, cpAssets, mtx, mypk, txfee, 
			EncodeTokenOpRetV1(assetid, {},     // TODO: actually this tx is not 'tokens', maybe it is better not to have token opret here but only asset opret.
				{ EncodeAssetOpRet('b', zeroid, unit_price, vuint8_t(mypk.begin(), mypk.end())) } ));   // But still such token opret should not make problems because no token eval in these vouts
        return sigData;
        
    }
	CCerror = "no coins found to make buy offer";
    return("");
}

// rpc tokenask implementation, locks 'numtokens' tokens for the 'askamount' 
UniValue CreateSell(const CPubKey &mypk, int64_t txfee, int64_t numtokens, uint256 assetid, int64_t askamount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
	uint64_t mask; 
	int64_t inputs, CCchange; 
	struct CCcontract_info *cpAssets, assetsC;
	struct CCcontract_info *cpTokens, tokensC;

    if (numtokens < 0 || askamount < 0)    {
        CCerror = "negative askamount or numtokens";
        return("");
    }

    cpAssets = CCinit(&assetsC, EVAL_ASSETS);  // NOTE: for signing
   
    if (txfee == 0)
        txfee = 10000;

    if (AddNormalinputsRemote(mtx, mypk, txfee+ASSETS_MARKER_AMOUNT, 0x10000) > 0)   // use AddNormalinputsRemote to sign with mypk
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
		// add single-eval tokens (or non-fungible tokens):
        cpTokens = CCinit(&tokensC, EVAL_TOKENS);  // NOTE: adding inputs only from EVAL_TOKENS cc
        if ((inputs = AddTokenCCInputs(cpTokens, mtx, mypk, assetid, numtokens, 60)) > 0)
        {
			if (inputs < numtokens) {
				CCerror = strprintf("insufficient tokens for ask");
				return ("");
			}

            CAmount unit_price = askamount / numtokens;

            uint8_t evalcodeNFT = cpTokens->evalcodeNFT ? cpTokens->evalcodeNFT : 0;

			CPubKey unspendableAssetsPubkey = GetUnspendable(cpAssets, NULL);
            mtx.vout.push_back(MakeTokensCC1vout(EVAL_ASSETS, evalcodeNFT, numtokens, unspendableAssetsPubkey));
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, ASSETS_MARKER_AMOUNT, mypk));  //marker (seems, it is not for my tokenorders, not used yet)
            if (inputs > numtokens)
                CCchange = (inputs - numtokens);
            if (CCchange != 0)
                // change to single-eval or non-fungible token vout (although for non-fungible token change currently is not possible)
                mtx.vout.push_back(MakeTokensCC1vout(evalcodeNFT ? evalcodeNFT : EVAL_TOKENS, CCchange, mypk));	

            UniValue sigData = FinalizeCCTxExt(IsRemoteRPCCall(), mask, cpTokens, mtx, mypk, txfee, 
                EncodeTokenOpRetV1(assetid, { unspendableAssetsPubkey }, 
                    { EncodeAssetOpRet('s', zeroid, unit_price, vuint8_t(mypk.begin(), mypk.end()) ) } ));
            return sigData;
		}
		else {
            CCerror = "need some tokens to place ask";
		}
    }
	else {  
        CCerror = "need some native coins to place ask";
	}
    return("");
}

////////////////////////// NOT IMPLEMENTED YET/////////////////////////////////
std::string CreateSwap(int64_t txfee,int64_t askamount,uint256 assetid,uint256 assetid2,int64_t pricetotal)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk; uint64_t mask; int64_t inputs,CCchange; CScript opret; struct CCcontract_info *cp,C;

	////////////////////////// NOT IMPLEMENTED YET/////////////////////////////////
    fprintf(stderr,"asset swaps disabled\n");
    return("");
	////////////////////////// NOT IMPLEMENTED YET/////////////////////////////////

    if ( askamount < 0 || pricetotal < 0 )
    {
        fprintf(stderr,"negative askamount %lld, askamount %lld\n",(long long)pricetotal,(long long)askamount);
        return("");
    }
    cp = CCinit(&C, EVAL_ASSETS);

    if ( txfee == 0 )
        txfee = 10000;
	////////////////////////// NOT IMPLEMENTED YET/////////////////////////////////
    mypk = pubkey2pk(Mypubkey());

    if (AddNormalinputs(mtx, mypk, txfee, 0x10000) > 0)
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        /*if ((inputs = AddAssetInputs(cp, mtx, mypk, assetid, askamount, 60)) > 0)
        {
			////////////////////////// NOT IMPLEMENTED YET/////////////////////////////////
			if (inputs < askamount) {
				//was: askamount = inputs;
				std::cerr << "CreateSwap(): insufficient tokens for ask" << std::endl;
				CCerror = strprintf("insufficient tokens for ask");
				return ("");
			}
			////////////////////////// NOT IMPLEMENTED YET/////////////////////////////////
			CPubKey unspendablePubkey = GetUnspendable(cp, 0);
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, askamount, unspendablePubkey));

            if (inputs > askamount)
                CCchange = (inputs - askamount);
            if (CCchange != 0)
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, CCchange, mypk));

			////////////////////////// NOT IMPLEMENTED YET/////////////////////////////////
			std::vector<CPubKey> voutTokenPubkeys;  // should be empty - no token vouts

			if (assetid2 == zeroid) {
				opret = EncodeTokenOpRet(assetid, voutTokenPubkeys,
							EncodeAssetOpRet('s', zeroid, pricetotal, Mypubkey()));
			}
            else    {
                opret = EncodeTokenOpRet(assetid, voutTokenPubkeys,
							EncodeAssetOpRet('e', assetid2, pricetotal, Mypubkey()));
            } 
			////////////////////////// NOT IMPLEMENTED YET/////////////////////////////////
            return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,opret));
        } 
		else {
			fprintf(stderr, "need some assets to place ask\n");
		} */
    }
	else { // dimxy added 'else', because it was misleading message before
		fprintf(stderr,"need some native coins to place ask\n");
	}
    
    return("");
}  ////////////////////////// NOT IMPLEMENTED YET/////////////////////////////////

// unlocks coins, ends bid order
UniValue CancelBuyOffer(const CPubKey &mypk, int64_t txfee,uint256 assetid,uint256 bidtxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction vintx;	uint64_t mask;
	uint256 hashBlock; int64_t bidamount; 
	struct CCcontract_info *cpAssets, C;

    cpAssets = CCinit(&C, EVAL_ASSETS);

    if (txfee == 0)
        txfee = 10000;

    // add normal inputs only from my mypk (not from any pk in the wallet) to validate the ownership of the canceller
    if (AddNormalinputsRemote(mtx, mypk, txfee+ASSETS_MARKER_AMOUNT, 0x10000) > 0)
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if (myGetTransaction(bidtxid, vintx, hashBlock) && vintx.vout.size() > ASSETS_GLOBALADDR_VOUT)
        {
            uint8_t dummyEvalCode; uint256 dummyAssetid, dummyAssetid2; int64_t dummyPrice; std::vector<uint8_t> dummyOrigpubkey;

            //std::vector<uint8_t> vopretNonfungible;
            //GetNonfungibleData(assetid, vopretNonfungible);

            bidamount = vintx.vout[ASSETS_GLOBALADDR_VOUT].nValue;
            mtx.vin.push_back(CTxIn(bidtxid, ASSETS_GLOBALADDR_VOUT, CScript()));		// coins in Assets

            uint8_t funcid = DecodeAssetTokenOpRet(vintx.vout.back().scriptPubKey, dummyEvalCode, dummyAssetid, dummyAssetid2, dummyPrice, dummyOrigpubkey);
            if (funcid == 'b' && vintx.vout.size() > 1)
                mtx.vin.push_back(CTxIn(bidtxid, 1, CScript()));		// spend marker if funcid='b'
            else if (funcid == 'B' && vintx.vout.size() > 3)
                mtx.vin.push_back(CTxIn(bidtxid, 3, CScript()));		// spend marker if funcid='B'
            else {
                CCerror = "invalid bidtx or not enough vouts";
                return "";
            }

            mtx.vout.push_back(CTxOut(bidamount, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            mtx.vout.push_back(CTxOut(ASSETS_MARKER_AMOUNT, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));

            UniValue sigData = FinalizeCCTxExt(IsRemoteRPCCall(), mask, cpAssets, mtx, mypk, txfee,
                EncodeTokenOpRetV1(assetid, {},
                    { EncodeAssetOpRet('o', zeroid, 0, vuint8_t(mypk.begin(), mypk.end())) }));
            return sigData;
        }
        else
            CCerror = "could not load bid tx";
    }
    else
        CCerror = "could not get normal coins for txfee";
    return("");
}

//unlocks tokens, ends ask order
UniValue CancelSell(const CPubKey &mypk, int64_t txfee,uint256 assetid,uint256 asktxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction vintx; uint64_t mask; 
	uint256 hashBlock; 	int64_t askamount; 
    struct CCcontract_info *cpTokens, *cpAssets, tokensC, assetsC;

    cpAssets = CCinit(&assetsC, EVAL_ASSETS);

    if (txfee == 0)
        txfee = 10000;

    // add normal inputs only from my mypk (not from any pk in the wallet) to validate the ownership
    if (AddNormalinputsRemote(mtx, mypk, txfee+ASSETS_MARKER_AMOUNT, 0x10000) > 0)
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if (myGetTransaction(asktxid, vintx, hashBlock) != 0 && vintx.vout.size() > 0)
        {
            uint8_t dummyEvalCode; 
            uint256 dummyAssetid, dummyAssetid2; 
            int64_t dummyPrice; 
            std::vector<uint8_t> dummyOrigpubkey;

            askamount = vintx.vout[ASSETS_GLOBALADDR_VOUT].nValue;
            mtx.vin.push_back(CTxIn(asktxid, ASSETS_GLOBALADDR_VOUT, CScript()));
            
            uint8_t funcid = DecodeAssetTokenOpRet(vintx.vout.back().scriptPubKey, dummyEvalCode, dummyAssetid, dummyAssetid2, dummyPrice, dummyOrigpubkey);
            if (funcid == 's' && vintx.vout.size() > 1)
                mtx.vin.push_back(CTxIn(asktxid, 1, CScript()));		// spend marker if funcid='s'
            else if (funcid == 'S' && vintx.vout.size() > 3)
                mtx.vin.push_back(CTxIn(asktxid, 3, CScript()));		// spend marker if funcid='S'
            else {
                CCerror = "invalid ask tx or not enough vouts";
                return "";
            }

            std::vector<uint8_t> vopretNonfungible;
            GetNonfungibleData(assetid, vopretNonfungible);
            if (vopretNonfungible.size() > 0)
                cpAssets->evalcodeNFT = vopretNonfungible.begin()[0];

            mtx.vout.push_back(MakeTokensCC1vout(cpAssets->evalcodeNFT ? cpAssets->evalcodeNFT : EVAL_TOKENS, askamount, mypk));	// one-eval token vout
            mtx.vout.push_back(CTxOut(ASSETS_MARKER_AMOUNT, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));

            // this is only for unspendable addresses:
            //CCaddr2set(cpTokens, EVAL_ASSETS, mypk, myPrivkey, myCCaddr);  //do we need this? Seems FinalizeCCTx can attach to any evalcode cc addr by calling Getscriptaddress 

            uint8_t unspendableAssetsPrivkey[32];
            char unspendableAssetsAddr[KOMODO_ADDRESS_BUFSIZE];
            // init assets 'unspendable' privkey and pubkey
            CPubKey unspendableAssetsPk = GetUnspendable(cpAssets, unspendableAssetsPrivkey);
            GetCCaddress(cpAssets, unspendableAssetsAddr, unspendableAssetsPk);

            // add additional eval-tokens unspendable assets privkey:
            CCaddr2set(cpAssets, EVAL_TOKENS, unspendableAssetsPk, unspendableAssetsPrivkey, unspendableAssetsAddr);

            UniValue sigData = FinalizeCCTxExt(IsRemoteRPCCall(), mask, cpAssets, mtx, mypk, txfee,
                EncodeTokenOpRetV1(assetid, { mypk },
                    { EncodeAssetOpRet('x', zeroid, 0, vuint8_t(mypk.begin(), mypk.end())) } ));
            return sigData;
        }
        else
            CCerror = "could not get ask tx";
    }
    else
        CCerror = "could not get normal coins for txfee";
    return("");
}

//send tokens, receive coins:
UniValue FillBuyOffer(const CPubKey &mypk, int64_t txfee, uint256 assetid, uint256 bidtxid, int64_t fill_units, CAmount paid_unit_price)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction vintx; 
	uint256 hashBlock; 
	std::vector<uint8_t> origpubkey; 
	int32_t bidvout = ASSETS_GLOBALADDR_VOUT; 
	uint64_t mask; 
	int64_t orig_units, unit_price, bid_amount, paid_amount, remaining_units, inputs, CCchange=0; 
	struct CCcontract_info *cpTokens, tokensC;
	struct CCcontract_info *cpAssets, assetsC;

    if (fill_units < 0)
    {
        CCerror = "negative fill units";
        return("");
    }
    cpTokens = CCinit(&tokensC, EVAL_TOKENS);
    
	if (txfee == 0)
        txfee = 10000;

    if (AddNormalinputs(mtx, mypk, txfee+ASSETS_MARKER_AMOUNT, 0x10000, IsRemoteRPCCall()) > 0)
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if (myGetTransaction(bidtxid, vintx, hashBlock) != 0 && vintx.vout.size() > bidvout)
        {
            bid_amount = vintx.vout[bidvout].nValue;
            uint8_t funcid = SetAssetOrigpubkey(origpubkey, unit_price, vintx);  // get orig pk, orig units
            if (funcid != 'b' && funcid != 'B')  {
                CCerror = "not an bid order";
                return "";
            }
            orig_units = bid_amount / unit_price;
            if (paid_unit_price == 0)
                paid_unit_price = unit_price;
            mtx.vin.push_back(CTxIn(bidtxid, bidvout, CScript()));					// Coins on Assets unspendable

            if ((inputs = AddTokenCCInputs(cpTokens, mtx, mypk, assetid, fill_units, 60)) > 0)
            {
                if (inputs < fill_units) {
                    CCerror = strprintf("insufficient tokens to fill buy offer");
                    return ("");
                }

                SetBidFillamounts(unit_price, paid_amount, bid_amount, fill_units, orig_units, paid_unit_price);

                if (inputs > fill_units)
                    CCchange = (inputs - fill_units);

                uint8_t unspendableAssetsPrivkey[32];
                cpAssets = CCinit(&assetsC, EVAL_ASSETS);
                CPubKey unspendableAssetsPk = GetUnspendable(cpAssets, unspendableAssetsPrivkey);

                uint8_t evalcodeNFT = cpTokens->evalcodeNFT ? cpTokens->evalcodeNFT : 0;

                if (orig_units - fill_units > 0)
                    mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, bid_amount - paid_amount, unspendableAssetsPk));     // vout0 coins remainder
                else
                    mtx.vout.push_back(CTxOut(bid_amount - paid_amount, CScript() << ParseHex(HexStr(origpubkey)) << OP_CHECKSIG));     // if no more tokens to buy, send the remainder to originator
                mtx.vout.push_back(CTxOut(paid_amount, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));	// vout1 coins to mypk normal 
                mtx.vout.push_back(MakeTokensCC1vout(evalcodeNFT ? evalcodeNFT : EVAL_TOKENS, fill_units, pubkey2pk(origpubkey)));	        // vout2 single-eval tokens sent to the originator
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, ASSETS_MARKER_AMOUNT, origpubkey));                 // vout3 marker to origpubkey

                if (CCchange != 0)
                    mtx.vout.push_back(MakeTokensCC1vout(evalcodeNFT ? evalcodeNFT : EVAL_TOKENS, CCchange, mypk));		// vout4 change in single-eval tokens

                //fprintf(stderr, "%s remaining_units %lld -> origpubkey\n", __func__, (long long)remaining_units);

                char unspendableAssetsAddr[KOMODO_ADDRESS_BUFSIZE];
                cpAssets = CCinit(&assetsC, EVAL_ASSETS);
                GetCCaddress(cpAssets, unspendableAssetsAddr, unspendableAssetsPk);

                // add additional unspendable addr from Assets:
                CCaddr2set(cpTokens, EVAL_ASSETS, unspendableAssetsPk, unspendableAssetsPrivkey, unspendableAssetsAddr);

                UniValue sigData = FinalizeCCTxExt(IsRemoteRPCCall(), mask, cpTokens, mtx, mypk, txfee,
                    EncodeTokenOpRetV1(assetid, { pubkey2pk(origpubkey) },
                        { EncodeAssetOpRet('B', zeroid, unit_price, origpubkey) }));
                return sigData;
            }
            else {
                CCerror = "dont have any assets to fill bid";
                return "";
            }
        }
        else {
            CCerror = "can't load or bad bidtx";
            return "";
        }
    }
    CCerror = "no normal coins left";
    return "";
}


// send coins, receive tokens 
UniValue FillSell(const CPubKey &mypk, int64_t txfee, uint256 assetid, uint256 assetid2, uint256 asktxid, int64_t fillunits, CAmount paid_unit_price)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction vintx; 
	uint256 hashBlock; 
	std::vector<uint8_t> origpubkey; 
	//double dprice; 
	uint64_t mask = 0; 
	int32_t askvout = ASSETS_GLOBALADDR_VOUT; 
	int64_t unit_price, orig_assetoshis, paid_nValue, inputs, CCchange = 0LL; 
	struct CCcontract_info *cpAssets, assetsC;

    if (fillunits < 0)
    {
        CCerror = strprintf("negative fillunits %lld\n",(long long)fillunits);
        return("");
    }
    if (assetid2 != zeroid)
    {
        CCerror = "asset swaps disabled";
        return("");
    }

    std::vector<uint8_t> vopretNonfungible;
    uint8_t evalCodeNFT = 0;
    GetNonfungibleData(assetid, vopretNonfungible);
    if (vopretNonfungible.size() > 0)
        evalCodeNFT = vopretNonfungible.begin()[0];

    cpAssets = CCinit(&assetsC, EVAL_ASSETS);

    if (txfee == 0)
        txfee = 10000;

    if (myGetTransaction(asktxid, vintx, hashBlock) != 0 && vintx.vout.size() > askvout)
    {
        orig_assetoshis = vintx.vout[askvout].nValue;
        uint8_t funcid = SetAssetOrigpubkey(origpubkey, unit_price, vintx); // get orig pk, orig value
        if (funcid != 's' && funcid != 'S')  {
            CCerror = "not an ask order";
            return "";
        }

        //dprice = (double)orig_nValue / orig_assetoshis;
        if (paid_unit_price <= 0LL)
            paid_unit_price = unit_price;
        if (paid_unit_price <= 0LL)    {
            CCerror = "could not get unit price";
            return "";
        }
        paid_nValue = paid_unit_price * fillunits;

        if (assetid2 != zeroid) {
            inputs = 0; //  = AddAssetInputs(cpAssets, mtx, mypk, assetid2, paid_nValue, 60);  // not implemented yet
        }
        else
        {
            // Use only one AddNormalinputs() in each rpc call to allow payment if user has only single utxo with normal funds
            inputs = AddNormalinputs(mtx, mypk, txfee + ASSETS_MARKER_AMOUNT + paid_nValue, 0x10000, IsRemoteRPCCall());  
            mask = ~((1LL << mtx.vin.size()) - 1);
        }
        if (inputs > 0)
        {
			if (inputs < paid_nValue) {
				CCerror = strprintf("insufficient coins to fill sell");
				return ("");
			}

            // cc vin should be after normal vin
            mtx.vin.push_back(CTxIn(asktxid, askvout, CScript()));
            
			if (assetid2 != zeroid)
                ; // SetSwapFillamounts(orig_unit_price, fillunits, orig_assetoshis, paid_nValue, orig_nValue);  //not implemented correctly yet
            else 
				SetAskFillamounts(unit_price, fillunits, orig_assetoshis, paid_nValue);

            if (paid_nValue == 0) {
                CCerror = "ask totally filled";
                return "";
            }

            if (assetid2 != zeroid && inputs > paid_nValue)
                CCchange = (inputs - paid_nValue);

            // vout.0 tokens remainder to unspendable cc addr:
            mtx.vout.push_back(MakeTokensCC1vout(EVAL_ASSETS, evalCodeNFT, orig_assetoshis - fillunits, GetUnspendable(cpAssets, NULL)));  // token remainder on cc global addr
            //vout.1 purchased tokens to self token single-eval or dual-eval token+nonfungible cc addr:
            mtx.vout.push_back(MakeTokensCC1vout(evalCodeNFT ? evalCodeNFT : EVAL_TOKENS, fillunits, mypk));					
                
			if (assetid2 != zeroid) {
				std::cerr << __func__ << " WARNING: asset swap not implemented yet!" << std::endl;
				// TODO: change MakeCC1vout appropriately when implementing:
				//mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, paid_nValue, origpubkey));			    //vout.2 tokens... (swap is not implemented yet)
			}
			else {
				mtx.vout.push_back(CTxOut(paid_nValue, CScript() << origpubkey << OP_CHECKSIG));		//vout.2 coins to originator's normal addr
			}
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, ASSETS_MARKER_AMOUNT, origpubkey));                            //vout.3 marker to origpubkey (for my tokenorders?)
                
			// not implemented
			if (CCchange != 0) {
				std::cerr << __func__ << " WARNING: asset swap not implemented yet! (CCchange)" << std::endl;
				// TODO: change MakeCC1vout appropriately when implementing:
				//mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, CCchange, mypk));							//vout.3 coins in Assets cc addr (swap not implemented)
			}

			uint8_t unspendableAssetsPrivkey[32];
			char unspendableAssetsAddr[KOMODO_ADDRESS_BUFSIZE];
			// init assets 'unspendable' privkey and pubkey
			CPubKey unspendableAssetsPk = GetUnspendable(cpAssets, unspendableAssetsPrivkey);
			GetCCaddress(cpAssets, unspendableAssetsAddr, unspendableAssetsPk);

			// add additional eval-tokens unspendable assets privkey:
			CCaddr2set(cpAssets, EVAL_TOKENS, unspendableAssetsPk, unspendableAssetsPrivkey, unspendableAssetsAddr);

            cpAssets->evalcodeNFT = evalCodeNFT;  // set nft eval for signing

            UniValue sigData = FinalizeCCTxExt(IsRemoteRPCCall(), mask, cpAssets, mtx, mypk, txfee,
				EncodeTokenOpRetV1(assetid, { mypk }, 
                    { EncodeAssetOpRet(assetid2 != zeroid ? 'E' : 'S', assetid2, unit_price, origpubkey) } ));
            return sigData;
        } else {
            CCerror = strprintf("filltx not enough normal utxos");
            return "";
        }
    }
    CCerror = "can't get ask tx";
    return("");
}

