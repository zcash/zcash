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


UniValue AssetOrders(uint256 refassetid, CPubKey pk, uint8_t additionalEvalCode)
{
	UniValue result(UniValue::VARR);  

    struct CCcontract_info *cpAssets, assetsC;
    struct CCcontract_info *cpTokens, tokensC;

    cpAssets = CCinit(&assetsC, EVAL_ASSETS);
    cpTokens = CCinit(&tokensC, EVAL_TOKENS);

	auto addOrders = [&](struct CCcontract_info *cp, std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it)
	{
		uint256 txid, hashBlock, assetid, assetid2;
		int64_t price;
		std::vector<uint8_t> origpubkey;
		CTransaction ordertx;
		uint8_t funcid, evalCode;
		char numstr[32], funcidstr[16], origaddr[64], origtokenaddr[64];

        txid = it->first.txhash;
        LOGSTREAM("ccassets", CCLOG_DEBUG2, stream << "addOrders() checking txid=" << txid.GetHex() << std::endl);
        if ( myGetTransaction(txid, ordertx, hashBlock) != 0 )
        {
			// for logging: funcid = DecodeAssetOpRet(vintx.vout[vintx.vout.size() - 1].scriptPubKey, evalCode, assetid, assetid2, price, origpubkey);
            if (ordertx.vout.size() > 0 && (funcid = DecodeAssetTokenOpRet(ordertx.vout[ordertx.vout.size()-1].scriptPubKey, evalCode, assetid, assetid2, price, origpubkey)) != 0)
            {
                LOGSTREAM("ccassets", CCLOG_DEBUG2, stream << "addOrders() checking ordertx.vout.size()=" << ordertx.vout.size() << " funcid=" << (char)(funcid ? funcid : ' ') << " assetid=" << assetid.GetHex() << std::endl);

                if (pk == CPubKey() && (refassetid == zeroid || assetid == refassetid)  // tokenorders
                    || pk != CPubKey() && pk == pubkey2pk(origpubkey) && (funcid == 'S' || funcid == 's'))  // mytokenorders, returns only asks (is this correct?)
                {

                    LOGSTREAM("ccassets", CCLOG_DEBUG2, stream << "addOrders() it->first.index=" << it->first.index << " ordertx.vout[it->first.index].nValue=" << ordertx.vout[it->first.index].nValue << std::endl);
                    if (ordertx.vout[it->first.index].nValue == 0) {
                        LOGSTREAM("ccassets", CCLOG_DEBUG2, stream << "addOrders() order with value=0 skipped" << std::endl);
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
                        sprintf(numstr, "%llu", (long long)ordertx.vout[it->first.index].nValue);
                        item.push_back(Pair("amount", numstr));
                        sprintf(numstr, "%llu", (long long)ordertx.vout[0].nValue);
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
                    if (price > 0)
                    {
                        if (funcid == 's' || funcid == 'S' || funcid == 'e' || funcid == 'e')
                        {
                            sprintf(numstr, "%.8f", (double)price / COIN);
                            item.push_back(Pair("totalrequired", numstr));
                            sprintf(numstr, "%.8f", (double)price / (COIN * ordertx.vout[0].nValue));
                            item.push_back(Pair("price", numstr));
                        }
                        else
                        {
                            item.push_back(Pair("totalrequired", (int64_t)price));
                            sprintf(numstr, "%.8f", (double)ordertx.vout[0].nValue / (price * COIN));
                            item.push_back(Pair("price", numstr));
                        }
                    }
                    result.push_back(item);
                    LOGSTREAM("ccassets", CCLOG_DEBUG1, stream << "addOrders() added order funcId=" << (char)(funcid ? funcid : ' ') << " it->first.index=" << it->first.index << " ordertx.vout[it->first.index].nValue=" << ordertx.vout[it->first.index].nValue << " tokenid=" << assetid.GetHex() << std::endl);
                }
            }
        }
	};

    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputsTokens, unspentOutputsDualEvalTokens, unspentOutputsCoins;

	char assetsUnspendableAddr[64];
	GetCCaddress(cpAssets, assetsUnspendableAddr, GetUnspendable(cpAssets, NULL));
	SetCCunspents(unspentOutputsCoins, assetsUnspendableAddr,true);

	char assetsTokensUnspendableAddr[64];
    std::vector<uint8_t> vopretNonfungible;
    if (refassetid != zeroid) {
        GetNonfungibleData(refassetid, vopretNonfungible);
        if (vopretNonfungible.size() > 0)
            cpAssets->additionalTokensEvalcode2 = vopretNonfungible.begin()[0];
    }
	GetTokensCCaddress(cpAssets, assetsTokensUnspendableAddr, GetUnspendable(cpAssets, NULL));
	SetCCunspents(unspentOutputsTokens, assetsTokensUnspendableAddr,true);

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

    if (additionalEvalCode != 0) {  //this would be mytokenorders
        char assetsDualEvalTokensUnspendableAddr[64];

        // try also dual eval tokenasks (and we do not need bids):
        cpAssets->additionalTokensEvalcode2 = additionalEvalCode;
        GetTokensCCaddress(cpAssets, assetsDualEvalTokensUnspendableAddr, GetUnspendable(cpAssets, NULL));
        SetCCunspents(unspentOutputsDualEvalTokens, assetsDualEvalTokensUnspendableAddr,true);

        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator itDualEvalTokens = unspentOutputsDualEvalTokens.begin();
            itDualEvalTokens != unspentOutputsDualEvalTokens.end();
            itDualEvalTokens++)
            addOrders(cpAssets, itDualEvalTokens);
    }
    return(result);
}

// not used (use TokenCreate instead)
/* std::string CreateAsset(int64_t txfee,int64_t assetsupply,std::string name,std::string description)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk; struct CCcontract_info *cp,C;
    if ( assetsupply < 0 )
    {
        fprintf(stderr,"negative assetsupply %lld\n",(long long)assetsupply);
        return("");
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( name.size() > 32 || description.size() > 4096 )
    {
        fprintf(stderr,"name.%d or description.%d is too big\n",(int32_t)name.size(),(int32_t)description.size());
        return("");
    }
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,assetsupply+2*txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,assetsupply,mypk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(cp->CChexstr) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeAssetCreateOpRet('c',Mypubkey(),name,description)));
    }
    return("");
} */
  
// not used (use TokenTransfer instead)
/* std::string AssetTransfer(int64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,int64_t total)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk; uint64_t mask; int64_t CCchange=0,inputs=0;  struct CCcontract_info *cp,C;
    if ( total < 0 )
    {
        fprintf(stderr,"negative total %lld\n",(long long)total);
        return("");
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,3) > 0 )
    {
        //n = outputs.size();
        //if ( n == amounts.size() )
        //{
        //    for (i=0; i<n; i++)
        //        total += amounts[i];
        mask = ~((1LL << mtx.vin.size()) - 1);
        if ( (inputs= AddAssetInputs(cp,mtx,mypk,assetid,total,60)) > 0 )
        {

			if (inputs < total) {   //added dimxy
				std::cerr << "AssetTransfer(): insufficient funds" << std::endl;
				return ("");
			}
            if ( inputs > total )
                CCchange = (inputs - total);
            //for (i=0; i<n; i++)
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,total,pubkey2pk(destpubkey)));
            if ( CCchange != 0 )
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,mypk));
            return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,EncodeAssetOpRet('t',assetid,zeroid,0,Mypubkey())));
        } else fprintf(stderr,"not enough CC asset inputs for %.8f\n",(double)total/COIN);
        //} else fprintf(stderr,"numoutputs.%d != numamounts.%d\n",n,(int32_t)amounts.size());
    }
    return("");
} */

// deprecated
/* std::string AssetConvert(int64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,int64_t total,int32_t evalcode)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk; int64_t CCchange=0,inputs=0;  struct CCcontract_info *cp,C;
    if ( total < 0 )
    {
        fprintf(stderr,"negative total %lld\n",(long long)total);
        return("");
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,3) > 0 )
    {
        if ( (inputs= AddAssetInputs(cp,mtx,mypk,assetid,total,60)) > 0 )
        {
            if ( inputs > total )
                CCchange = (inputs - total);
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,mypk));
            mtx.vout.push_back(MakeCC1vout(evalcode,total,pubkey2pk(destpubkey)));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeAssetOpRet('t',assetid,zeroid,0,Mypubkey())));
        } else fprintf(stderr,"not enough CC asset inputs for %.8f\n",(double)total/COIN);
    }
    return("");
} */

// rpc tokenbid implementation, locks 'bidamount' coins for the 'pricetotal' of tokens
std::string CreateBuyOffer(int64_t txfee, int64_t bidamount, uint256 assetid, int64_t pricetotal)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk; 
	struct CCcontract_info *cpAssets, C; 
	uint256 hashBlock; 
	CTransaction vintx; 
	std::vector<uint8_t> origpubkey; 
	std::string name,description;
	int64_t inputs;

	std::cerr << "CreateBuyOffer() bidamount=" << bidamount << " numtokens(pricetotal)=" << pricetotal << std::endl;

    if (bidamount < 0 || pricetotal < 0)
    {
        fprintf(stderr,"negative bidamount %lld, pricetotal %lld\n", (long long)bidamount, (long long)pricetotal);
        return("");
    }
    if (myGetTransaction(assetid, vintx, hashBlock) == 0)
    {
        fprintf(stderr,"cant find assetid\n");
        return("");
    }
    if (vintx.vout.size() > 0 && DecodeTokenCreateOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey, origpubkey, name, description) == 0)
    {
        fprintf(stderr,"assetid isnt assetcreation txid\n");
        return("");
    }

    cpAssets = CCinit(&C,EVAL_ASSETS);   // NOTE: assets here!
    if (txfee == 0)
        txfee = 10000;

    mypk = pubkey2pk(Mypubkey());

    if ((inputs = AddNormalinputs(mtx, mypk, bidamount+(2*txfee), 64)) > 0)
    {
		std::cerr << "CreateBuyOffer() inputs=" << inputs << std::endl;
		if (inputs < bidamount+txfee) {
			std::cerr << "CreateBuyOffer(): insufficient coins to make buy offer" << std::endl;
			CCerror = strprintf("insufficient coins to make buy offer");
			return ("");
		}

		CPubKey unspendableAssetsPubkey = GetUnspendable(cpAssets, 0);
        mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, bidamount, unspendableAssetsPubkey));
        mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, txfee, mypk));
		std::vector<CPubKey> voutTokenPubkeys;  // should be empty - no token vouts

        return FinalizeCCTx(0, cpAssets, mtx, mypk, txfee, 
			EncodeTokenOpRet(assetid, voutTokenPubkeys,     // TODO: actually this tx is not 'tokens', maybe it is better not to have token opret here but only asset opret.
				std::make_pair(OPRETID_ASSETSDATA, EncodeAssetOpRet('b', zeroid, pricetotal, Mypubkey()))));   // But still such token opret should not make problems because no token eval in these vouts
    }
	CCerror = strprintf("no coins found to make buy offer");
    return("");
}

// rpc tokenask implementation, locks 'askamount' tokens for the 'pricetotal' 
std::string CreateSell(int64_t txfee,int64_t askamount,uint256 assetid,int64_t pricetotal)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk; 
	uint64_t mask; 
	int64_t inputs, CCchange; 
	struct CCcontract_info *cpAssets, assetsC;
	struct CCcontract_info *cpTokens, tokensC;

	//std::cerr << "CreateSell() askamount=" << askamount << " pricetotal=" << pricetotal << std::endl;

    if (askamount < 0 || pricetotal < 0)    {
        fprintf(stderr,"negative askamount %lld, askamount %lld\n",(long long)pricetotal,(long long)askamount);
        return("");
    }

    cpAssets = CCinit(&assetsC, EVAL_ASSETS);  // NOTE: for signing
   

    if (txfee == 0)
        txfee = 10000;

    mypk = pubkey2pk(Mypubkey());
    if (AddNormalinputs(mtx, mypk, 2*txfee, 3) > 0)
    {
        std::vector<uint8_t> vopretNonfungible;
        mask = ~((1LL << mtx.vin.size()) - 1);
		// add single-eval tokens (or non-fungible tokens):
        cpTokens = CCinit(&tokensC, EVAL_TOKENS);  // NOTE: adding inputs only from EVAL_TOKENS cc
        if ((inputs = AddTokenCCInputs(cpTokens, mtx, mypk, assetid, askamount, 60, vopretNonfungible)) > 0)
        {
			if (inputs < askamount) {
				//was: askamount = inputs;
				std::cerr << "CreateSell(): insufficient tokens for ask" << std::endl;
				CCerror = strprintf("insufficient tokens for ask");
				return ("");
			}

            // if this is non-fungible tokens:
            if( !vopretNonfungible.empty() )
                // set its evalcode
                cpAssets->additionalTokensEvalcode2 = vopretNonfungible.begin()[0];

			CPubKey unspendableAssetsPubkey = GetUnspendable(cpAssets, NULL);
            mtx.vout.push_back(MakeTokensCC1vout(EVAL_ASSETS, cpAssets->additionalTokensEvalcode2, askamount, unspendableAssetsPubkey));
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, txfee, mypk));  //marker (seems, it is not for tokenorders)
            if (inputs > askamount)
                CCchange = (inputs - askamount);
            if (CCchange != 0)
                // change to single-eval or non-fungible token vout (although for non-fungible token change currently is not possible)
                mtx.vout.push_back(MakeTokensCC1vout((cpAssets->additionalTokensEvalcode2) ? cpAssets->additionalTokensEvalcode2 : EVAL_TOKENS, CCchange, mypk));	

			std::vector<CPubKey> voutTokenPubkeys;
			voutTokenPubkeys.push_back(unspendableAssetsPubkey);   

            return FinalizeCCTx(mask, cpTokens, mtx, mypk, txfee, 
                EncodeTokenOpRet(assetid, voutTokenPubkeys, 
                    std::make_pair(OPRETID_ASSETSDATA, EncodeAssetOpRet('s', zeroid, pricetotal, Mypubkey()))));
		}
		else {
			fprintf(stderr, "need some tokens to place ask\n");
		}
    }
	else {  // dimxy added 'else', because it was misleading message before
		fprintf(stderr, "need some native coins to place ask\n");
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

    if (AddNormalinputs(mtx, mypk, txfee, 3) > 0)
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

// unlocks coins
std::string CancelBuyOffer(int64_t txfee,uint256 assetid,uint256 bidtxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction vintx;	uint64_t mask;
	uint256 hashBlock; int64_t bidamount; 
	CPubKey mypk; struct CCcontract_info *cpAssets, C;
	uint8_t funcid,dummyEvalCode; uint256 dummyAssetid, dummyAssetid2; int64_t dummyPrice; std::vector<uint8_t> dummyOrigpubkey;

    cpAssets = CCinit(&C, EVAL_ASSETS);

    if (txfee == 0)
        txfee = 10000;

    mypk = pubkey2pk(Mypubkey());

    if (AddNormalinputs(mtx, mypk, txfee, 3) > 0)
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if (myGetTransaction(bidtxid, vintx, hashBlock) != 0)
        {
            std::vector<uint8_t> vopretNonfungible;
            GetNonfungibleData(assetid, vopretNonfungible);

            bidamount = vintx.vout[0].nValue;
            mtx.vin.push_back(CTxIn(bidtxid, 0, CScript()));		// coins in Assets

			if((funcid=DecodeAssetTokenOpRet(vintx.vout[vintx.vout.size() - 1].scriptPubKey, dummyEvalCode, dummyAssetid, dummyAssetid2, dummyPrice, dummyOrigpubkey))!=0)
            {
                if (funcid == 's') mtx.vin.push_back(CTxIn(bidtxid, 1, CScript()));		// spend marker if funcid='b'
                else if (funcid=='S') mtx.vin.push_back(CTxIn(bidtxid, 3, CScript()));		// spend marker if funcid='B'
            }   

            mtx.vout.push_back(CTxOut(bidamount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));

			std::vector<CPubKey> voutTokenPubkeys;  // should be empty, no token vouts 
													
            return(FinalizeCCTx(mask, cpAssets, mtx, mypk, txfee, 
				EncodeTokenOpRet(assetid, voutTokenPubkeys, 
                    std::make_pair(OPRETID_ASSETSDATA, EncodeAssetOpRet('o', zeroid, 0, Mypubkey())))));
        }
    }
    return("");
}

//unlocks tokens
std::string CancelSell(int64_t txfee,uint256 assetid,uint256 asktxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction vintx; uint64_t mask; 
	uint256 hashBlock; 	int64_t askamount; 
	CPubKey mypk; 
    struct CCcontract_info *cpTokens, *cpAssets, tokensC, assetsC;
	uint8_t funcid, dummyEvalCode; 
    uint256 dummyAssetid, dummyAssetid2; 
    int64_t dummyPrice; 
    std::vector<uint8_t> dummyOrigpubkey;

    cpAssets = CCinit(&assetsC, EVAL_ASSETS);

    if (txfee == 0)
        txfee = 10000;

    mypk = pubkey2pk(Mypubkey());

   if (AddNormalinputs(mtx, mypk, txfee, 3) > 0)
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if (myGetTransaction(asktxid, vintx, hashBlock) != 0)
        {
            std::vector<uint8_t> vopretNonfungible;
            GetNonfungibleData(assetid, vopretNonfungible);

            askamount = vintx.vout[0].nValue;
            mtx.vin.push_back(CTxIn(asktxid, 0, CScript()));

			if ((funcid=DecodeAssetTokenOpRet(vintx.vout[vintx.vout.size() - 1].scriptPubKey, dummyEvalCode, dummyAssetid, dummyAssetid2, dummyPrice, dummyOrigpubkey))!=0)
            {
                if (funcid == 's') 
                    mtx.vin.push_back(CTxIn(asktxid, 1, CScript()));		// marker if funcid='s' 
                else if (funcid=='S') 
                    mtx.vin.push_back(CTxIn(asktxid, 3, CScript()));		// marker if funcid='S' 
            }

            if (vopretNonfungible.size() > 0)
                cpAssets->additionalTokensEvalcode2 = vopretNonfungible.begin()[0];

            mtx.vout.push_back(MakeTokensCC1vout(cpAssets->additionalTokensEvalcode2 == 0 ? EVAL_TOKENS : cpAssets->additionalTokensEvalcode2, askamount, mypk));	// one-eval token vout
            mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            
			std::vector<CPubKey> voutTokenPubkeys;  
			voutTokenPubkeys.push_back(mypk);

			// this is only for unspendable addresses:
            //CCaddr2set(cpTokens, EVAL_ASSETS, mypk, myPrivkey, myCCaddr);  //do we need this? Seems FinalizeCCTx can attach to any evalcode cc addr by calling Getscriptaddress 

			uint8_t unspendableAssetsPrivkey[32];
			char unspendableAssetsAddr[64];
			// init assets 'unspendable' privkey and pubkey
			CPubKey unspendableAssetsPk = GetUnspendable(cpAssets, unspendableAssetsPrivkey);
			GetCCaddress(cpAssets, unspendableAssetsAddr, unspendableAssetsPk);

			// add additional eval-tokens unspendable assets privkey:
			CCaddr2set(cpAssets, EVAL_TOKENS, unspendableAssetsPk, unspendableAssetsPrivkey, unspendableAssetsAddr);

            return(FinalizeCCTx(mask, cpAssets, mtx, mypk, txfee, 
				EncodeTokenOpRet(assetid, voutTokenPubkeys,  
                    std::make_pair(OPRETID_ASSETSDATA, EncodeAssetOpRet('x', zeroid, 0, Mypubkey())))));
        }
    }
    return("");
}

//send tokens, receive coins:
std::string FillBuyOffer(int64_t txfee,uint256 assetid,uint256 bidtxid,int64_t fillamount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction vintx; 
	uint256 hashBlock; 
	CPubKey mypk; 
	std::vector<uint8_t> origpubkey; 
	int32_t bidvout=0; 
	uint64_t mask; 
	int64_t origprice, bidamount, paid_amount, remaining_required, inputs, CCchange=0; 
	struct CCcontract_info *cpTokens, tokensC;
	struct CCcontract_info *cpAssets, assetsC;

    if (fillamount < 0)
    {
        fprintf(stderr,"negative fillamount %lld\n", (long long)fillamount);
        return("");
    }
    cpTokens = CCinit(&tokensC, EVAL_TOKENS);
    
	if (txfee == 0)
        txfee = 10000;

    mypk = pubkey2pk(Mypubkey());

    if (AddNormalinputs(mtx, mypk, 2*txfee, 3) > 0)
    {
        mask = ~((1LL << mtx.vin.size()) - 1);
        if (myGetTransaction(bidtxid, vintx, hashBlock) != 0)
        {
            bidamount = vintx.vout[bidvout].nValue;
            SetAssetOrigpubkey(origpubkey, origprice, vintx);
          
			mtx.vin.push_back(CTxIn(bidtxid, bidvout, CScript()));					// Coins on Assets unspendable
            
            std::vector<uint8_t> vopretNonfungible;
            if ((inputs = AddTokenCCInputs(cpTokens, mtx, mypk, assetid, fillamount, 60, vopretNonfungible)) > 0)
            {
				if (inputs < fillamount) {
					std::cerr << "FillBuyOffer(): insufficient tokens to fill buy offer" << std::endl;
					CCerror = strprintf("insufficient tokens to fill buy offer");
					return ("");
				}
                
				SetBidFillamounts(paid_amount, remaining_required, bidamount, fillamount, origprice);

                uint8_t additionalTokensEvalcode2 = 0;
                if (vopretNonfungible.size() > 0)
                    additionalTokensEvalcode2 = vopretNonfungible.begin()[0];
                
				if (inputs > fillamount)
                    CCchange = (inputs - fillamount);
                
				uint8_t unspendableAssetsPrivkey[32];
				cpAssets = CCinit(&assetsC, EVAL_ASSETS); 
				CPubKey unspendableAssetsPk = GetUnspendable(cpAssets, unspendableAssetsPrivkey);

				mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, bidamount - paid_amount, unspendableAssetsPk));     // vout0 coins remainder
                mtx.vout.push_back(CTxOut(paid_amount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));		// vout1 coins to normal
                mtx.vout.push_back(MakeTokensCC1vout(additionalTokensEvalcode2 == 0 ? EVAL_TOKENS : additionalTokensEvalcode2, fillamount, pubkey2pk(origpubkey)));	// vout2 single-eval tokens sent to the originator
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, txfee, origpubkey));                                // vout3 marker to origpubkey
                
				if (CCchange != 0)
                    mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, CCchange, mypk));								// vout4 change in single-eval tokens

                fprintf(stderr,"FillBuyOffer() remaining %llu -> origpubkey\n", (long long)remaining_required);

				char unspendableAssetsAddr[64];
				cpAssets = CCinit(&assetsC, EVAL_ASSETS);
				GetCCaddress(cpAssets, unspendableAssetsAddr, unspendableAssetsPk);

				// add additional unspendable addr from Assets:
				CCaddr2set(cpTokens, EVAL_ASSETS, unspendableAssetsPk, unspendableAssetsPrivkey, unspendableAssetsAddr);

				// token vout verification pubkeys:
				std::vector<CPubKey> voutTokenPubkeys;
				voutTokenPubkeys.push_back(pubkey2pk(origpubkey));

                return(FinalizeCCTx(mask, cpTokens, mtx, mypk, txfee, 
					EncodeTokenOpRet(assetid, voutTokenPubkeys, 
                        std::make_pair(OPRETID_ASSETSDATA, EncodeAssetOpRet('B', zeroid, remaining_required, origpubkey)))));
            } else return("dont have any assets to fill bid");
        }
    }
    return("no normal coins left");
}


// send coins, receive tokens 
std::string FillSell(int64_t txfee, uint256 assetid, uint256 assetid2, uint256 asktxid, int64_t fillunits)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction vintx,filltx; 
	uint256 hashBlock; 
	CPubKey mypk; 
	std::vector<uint8_t> origpubkey; 
	double dprice; 
	uint64_t mask = 0; 
	int32_t askvout = 0; 
	int64_t received_assetoshis, total_nValue, orig_assetoshis, paid_nValue, remaining_nValue, inputs, CCchange=0; 
	//struct CCcontract_info *cpTokens, tokensC;
	struct CCcontract_info *cpAssets, assetsC;

    if (fillunits < 0)
    {
        CCerror = strprintf("negative fillunits %lld\n",(long long)fillunits);
        fprintf(stderr,"%s\n",CCerror.c_str());
        return("");
    }
    if (assetid2 != zeroid)
    {
        CCerror = "asset swaps disabled";
        fprintf(stderr,"%s\n",CCerror.c_str());
        return("");
    }

    std::vector<uint8_t> vopretNonfungible;
    uint8_t additionalTokensEvalcode2 = 0;
    GetNonfungibleData(assetid, vopretNonfungible);
    if (vopretNonfungible.size() > 0)
        additionalTokensEvalcode2 = vopretNonfungible.begin()[0];

    cpAssets = CCinit(&assetsC, EVAL_ASSETS);

    if (txfee == 0)
        txfee = 10000;

    mypk = pubkey2pk(Mypubkey());
    //if (AddNormalinputs(mtx, mypk, 2*txfee, 3) > 0)
    //{
        //mask = ~((1LL << mtx.vin.size()) - 1);
        if (myGetTransaction(asktxid, vintx, hashBlock) != 0)
        {
            orig_assetoshis = vintx.vout[askvout].nValue;
            SetAssetOrigpubkey(origpubkey, total_nValue, vintx);
            dprice = (double)total_nValue / orig_assetoshis;
            paid_nValue = dprice * fillunits;

            if (assetid2 != zeroid) {
                inputs = 0; //  = AddAssetInputs(cpAssets, mtx, mypk, assetid2, paid_nValue, 60);  // not implemented yet
            }
            else
            {
                inputs = AddNormalinputs(mtx, mypk, 2 * txfee + paid_nValue, 60);  // Better to use single AddNormalinputs() to allow payment if user has only single utxo with normal funds
                mask = ~((1LL << mtx.vin.size()) - 1);
            }
            if (inputs > 0)
            {
				if (inputs < paid_nValue) {
					std::cerr << "FillSell(): insufficient coins to fill sell" << std::endl;
					CCerror = strprintf("insufficient coins to fill sell");
					return ("");
				}

                // cc vin should be after normal vin
                mtx.vin.push_back(CTxIn(asktxid, askvout, CScript()));

				if (assetid2 != zeroid)
                    SetSwapFillamounts(received_assetoshis, remaining_nValue, orig_assetoshis, paid_nValue, total_nValue);  //not implemented correctly yet
                else 
					SetAskFillamounts(received_assetoshis, remaining_nValue, orig_assetoshis, paid_nValue, total_nValue);

                if (assetid2 != zeroid && inputs > paid_nValue)
                    CCchange = (inputs - paid_nValue);

                // vout.0 tokens remainder to unspendable cc addr:
                mtx.vout.push_back(MakeTokensCC1vout(EVAL_ASSETS, additionalTokensEvalcode2, orig_assetoshis - received_assetoshis, GetUnspendable(cpAssets, NULL))); 
                //vout.1 purchased tokens to self token single-eval or dual-eval token+nonfungible cc addr:
                mtx.vout.push_back(MakeTokensCC1vout(additionalTokensEvalcode2 == 0 ? EVAL_TOKENS : additionalTokensEvalcode2, received_assetoshis, mypk));					
                
				if (assetid2 != zeroid) {
					std::cerr << "FillSell() WARNING: asset swap not implemented yet! (paid_nValue)" << std::endl;
					// TODO: change MakeCC1vout appropriately when implementing:
					//mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, paid_nValue, origpubkey));					//vout.2 tokens... (swap is not implemented yet)
				}
				else {
					//std::cerr << "FillSell() paid_value=" << paid_nValue << " origpubkey=" << HexStr(pubkey2pk(origpubkey)) << std::endl;
					mtx.vout.push_back(CTxOut(paid_nValue, CScript() << origpubkey << OP_CHECKSIG));		//vout.2 coins to tokens seller's normal addr
				}
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,txfee,origpubkey));                              //vout.3 marker to origpubkey
                
				// not implemented
				if (CCchange != 0) {
					std::cerr << "FillSell() WARNING: asset swap not implemented yet! (CCchange)" << std::endl;
					// TODO: change MakeCC1vout appropriately when implementing:
					//mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, CCchange, mypk));							//vout.3 coins in Assets cc addr (swap not implemented)
				}

				uint8_t unspendableAssetsPrivkey[32];
				char unspendableAssetsAddr[64];
				// init assets 'unspendable' privkey and pubkey
				CPubKey unspendableAssetsPk = GetUnspendable(cpAssets, unspendableAssetsPrivkey);
				GetCCaddress(cpAssets, unspendableAssetsAddr, unspendableAssetsPk);

				// add additional eval-tokens unspendable assets privkey:
				CCaddr2set(cpAssets, EVAL_TOKENS, unspendableAssetsPk, unspendableAssetsPrivkey, unspendableAssetsAddr);

				// vout verification pubkeys:
				std::vector<CPubKey> voutTokenPubkeys;
				voutTokenPubkeys.push_back(mypk);

                cpAssets->additionalTokensEvalcode2 = additionalTokensEvalcode2;

                return(FinalizeCCTx(mask, cpAssets, mtx, mypk, txfee,
					EncodeTokenOpRet(assetid, voutTokenPubkeys, 
                        std::make_pair(OPRETID_ASSETSDATA, EncodeAssetOpRet(assetid2 != zeroid ? 'E' : 'S', assetid2, remaining_nValue, origpubkey)))));
            } else {
                CCerror = strprintf("filltx not enough utxos");
                fprintf(stderr,"%s\n", CCerror.c_str());
            }
        }
    //}
    return("");
}
