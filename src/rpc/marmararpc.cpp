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

#include "cc/CCinclude.h"
#include "cc/CCMarmara.h"

using namespace std;

UniValue marmaraaddress(const UniValue& params, bool fHelp, const CPubKey& mypk)
{
    struct CCcontract_info *cp, C; std::vector<unsigned char> pubkey;
    cp = CCinit(&C, EVAL_MARMARA);
    if (fHelp || params.size() > 1)
        throw runtime_error("Marmaraaddress [pubkey]\n");
    if (ensure_CCrequirements(cp->evalcode) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    if (params.size() == 1)
        pubkey = ParseHex(params[0].get_str().c_str());
    return(CCaddress(cp, (char *)"Marmara", pubkey));
}

UniValue marmara_poolpayout(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    int32_t firstheight; double perc; char *jsonstr;
    if ( fHelp || params.size() != 3 )
    {
        // marmarapoolpayout 0.5 2 '[["024131032ed90941e714db8e6dd176fe5a86c9d873d279edecf005c06f773da686",1000],["02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92",100]]';
        //marmarapoolpayout 0 2 '[["024131032ed90941e714db8e6dd176fe5a86c9d873d279edecf005c06f773da686",1000]]'
        throw runtime_error("marmarapoolpayout perc firstheight \"[[\\\"pubkey\\\":shares], ...]\"\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    CONDITIONAL_LOCK2(cs_main, pwalletMain->cs_wallet, !remotepk.IsValid());
#endif 

    perc = atof(params[0].get_str().c_str()) / 100.;
    firstheight = atol(params[1].get_str().c_str());
    jsonstr = (char *)params[2].get_str().c_str();
    return "not implemented";
    //return(MarmaraPoolPayout(0,firstheight,perc,jsonstr)); // [[pk0, shares0], [pk1, shares1], ...]
}

UniValue marmara_receive(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ), jsonParams(UniValue::VOBJ);
    uint256 batontxid; std::vector<uint8_t> senderpub; int64_t amount = 0; int32_t matures = 0; std::string currency;

    if (fHelp || (params.size() != 5 && params.size() != 3))
    {
        // automatic flag -> lsb of matures
        
        throw runtime_error(
            "marmarareceive senderpk amount currency matures '{\"avalcount\":\"n\"}'\n"
            "marmarareceive senderpk batontxid '{\"avalcount\":\"n\"}'\n"
            "creates requesttx for issuer or endorser.\nFor the first call batontxid should be empty.\n"
            "the value of 'matures' is relative block number from the current height\n" "\n");
    }
    if (ensure_CCrequirements(EVAL_MARMARA) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    CONDITIONAL_LOCK2(cs_main, pwalletMain->cs_wallet, !remotepk.IsValid());
#endif 
    
    memset(&batontxid, 0, sizeof(batontxid));
    senderpub = ParseHex(params[0].get_str().c_str());
    if (senderpub.size() != 33)
    {
        ERR_RESULT("invalid sender pubkey");
        return result;
    }

    int njson;
    if (params.size() == 5)
    {
        amount = AmountFromValue(params[1]);
        currency = params[2].get_str();
        matures = chainActive.LastTip()->GetHeight() + atol(params[3].get_str().c_str()) + 1;  // if no baton (first call) then matures value is relative
        njson = 4;
    }
    else
    {
        batontxid = Parseuint256((char *)params[1].get_str().c_str());
        if (batontxid.IsNull())
            throw runtime_error("incorrect batontxid\n");
        njson = 2;
    }

    // parse json:
    if (params[njson].getType() == UniValue::VOBJ)       // as json in {...}
        jsonParams = params[njson].get_obj();
    else if (params[njson].getType() == UniValue::VSTR)  // as json in quoted string '{...}'
        jsonParams.read(params[njson].get_str().c_str());
    if (jsonParams.getType() != UniValue::VOBJ || jsonParams.empty())
        throw runtime_error("last parameter must be object\n");
    std::cerr << __func__ << " test output optParams=" << jsonParams.write(0, 0) << std::endl; 
    // TODO: check allowed params
    int32_t avalcount = 0;
    std::vector<std::string> keys = jsonParams.getKeys();
    std::vector<std::string>::iterator iter = std::find(keys.begin(), keys.end(), "avalcount");
    if (iter != keys.end()) {
        avalcount = atoi(jsonParams[iter - keys.begin()].get_str().c_str());
        //std::cerr << __func__ << " test output avalcount=" << avalcount << std::endl;
    }

    result = MarmaraReceive(remotepk, 0, pubkey2pk(senderpub), amount, currency, matures, avalcount, batontxid, true);
    return result;
}

UniValue marmara_issue(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ), jsonParams(UniValue::VOBJ); 
    uint256 requesttxid; 
    std::vector<uint8_t> receiverpub; 

    if (fHelp || params.size() != 3)
    {
        throw runtime_error("marmaraissue receiverpk '{\"avalcount\":\"n\", \"autosettlement\":\"true\"|\"false\", \"autoinsurance\":\"true\"|\"false\", \"disputeexpires\":\"offset\", \"EscrowOn\":\"true\"|\"false\", \"BlockageAmount\":\"amount\" }' requesttxid\n");
    }
    if( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    CONDITIONAL_LOCK2(cs_main, pwalletMain->cs_wallet, !remotepk.IsValid());
#endif    

    receiverpub = ParseHex(params[0].get_str().c_str());
    if (receiverpub.size() != CPubKey::COMPRESSED_PUBLIC_KEY_SIZE)
    {
        ERR_RESULT("invalid receiver pubkey");
        return result;
    }

    // parse json params:
    if (params[1].getType() == UniValue::VOBJ)
        jsonParams = params[1].get_obj();
    else if (params[1].getType() == UniValue::VSTR)  // json in quoted string '{...}'
        jsonParams.read(params[1].get_str().c_str());
    if (jsonParams.getType() != UniValue::VOBJ || jsonParams.empty())
        throw runtime_error("parameter 2 must be object\n");
    //std::cerr << __func__ << " test output optParams=" << jsonParams.write(0, 0) << std::endl;
    
    // TODO: check only allowed params present
    struct SMarmaraOptParams optParams;
    std::vector<std::string> keys = jsonParams.getKeys();
    std::vector<std::string>::iterator iter;
        
    iter = std::find(keys.begin(), keys.end(), "avalcount");
    if (iter != keys.end()) {
        optParams.avalCount = atoi(jsonParams[iter - keys.begin()].get_str().c_str());
        //std::cerr << __func__ << " test output avalcount=" << optParams.avalCount << std::endl;
    }
    iter = std::find(keys.begin(), keys.end(), "autosettlement");
    if (iter != keys.end()) {
        std::string value = jsonParams[iter - keys.begin()].get_str();
        optParams.autoSettlement = std::equal(value.begin(), value.end(), "true", [](char c1, char c2) {return std::toupper(c1) == std::toupper(c2);});
        //std::cerr << __func__ << " test output autosettlement=" << optParams.autoSettlement << std::endl;
    }
    iter = std::find(keys.begin(), keys.end(), "autoinsurance");
    if (iter != keys.end()) {
        std::string value = jsonParams[iter - keys.begin()].get_str();
        optParams.autoInsurance = std::equal(value.begin(), value.end(), "true", [](char c1, char c2) {return std::toupper(c1) == std::toupper(c2);});
        //std::cerr << __func__ << " test output autoinsurance=" << optParams.autoInsurance << std::endl;
    }
    iter = std::find(keys.begin(), keys.end(), "disputeexpires");
    if (iter != keys.end()) {
        std::string value = jsonParams[iter - keys.begin()].get_str();
        optParams.disputeExpiresOffset = atoi(jsonParams[iter - keys.begin()].get_str().c_str());
        //std::cerr << __func__ << " test output disputeexpiresoffset=" << optParams.disputeExpiresOffset << std::endl;
    }
    iter = std::find(keys.begin(), keys.end(), "EscrowOn");
    if (iter != keys.end()) {
        std::string value = jsonParams[iter - keys.begin()].get_str();
        optParams.escrowOn = std::equal(value.begin(), value.end(), "true", [](char c1, char c2) {return std::toupper(c1) == std::toupper(c2);});
        //std::cerr << __func__ << " test output EscrowOn=" << optParams.escrowOn << std::endl;
    }
    iter = std::find(keys.begin(), keys.end(), "BlockageAmount");
    if (iter != keys.end()) {
        optParams.blockageAmount = atoll(jsonParams[iter - keys.begin()].get_str().c_str());
        //std::cerr << __func__ << " test output BlockageAmount=" << optParams.blockageAmount << std::endl;
    }

    requesttxid = Parseuint256((char *)params[2].get_str().c_str());
    if (requesttxid.IsNull())
        throw runtime_error("incorrect requesttxid\n");

    result = MarmaraIssue(remotepk, 0, MARMARA_ISSUE, pubkey2pk(receiverpub), optParams, requesttxid, zeroid);
    return result;
}

UniValue marmara_transfer(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ), jsonParams(UniValue::VOBJ); 
    uint256 requesttxid, batontxid; 
    std::vector<uint8_t> receiverpub;
    std::vector<uint256> creditloop;

    if (fHelp || params.size() != 3)
    {
        throw runtime_error("marmaratransfer receiverpk '{\"avalcount\":\"n\"}' requesttxid\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    receiverpub = ParseHex(params[0].get_str().c_str());
    if (receiverpub.size() != CPubKey::COMPRESSED_PUBLIC_KEY_SIZE)
    {
        ERR_RESULT("invalid receiver pubkey");
        return result;
    }

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    CONDITIONAL_LOCK2(cs_main, pwalletMain->cs_wallet, !remotepk.IsValid());
#endif 
    
    // parse json params:
    if (params[1].getType() == UniValue::VOBJ)
        jsonParams = params[1].get_obj();
    else if (params[1].getType() == UniValue::VSTR)  // json in quoted string '{...}'
        jsonParams.read(params[1].get_str().c_str());
    if (jsonParams.getType() != UniValue::VOBJ || jsonParams.empty())
        throw runtime_error("parameter 2 must be object\n");
    //std::cerr << __func__ << " test output optParams=" << jsonParams.write(0, 0) << std::endl;
    // TODO: check only allowed params present
    struct SMarmaraOptParams optParams;
    std::vector<std::string> keys = jsonParams.getKeys();
    std::vector<std::string>::iterator iter;

    iter = std::find(keys.begin(), keys.end(), "avalcount");
    if (iter != keys.end()) {
        optParams.avalCount = atoi(jsonParams[iter - keys.begin()].get_str().c_str());
        //std::cerr << __func__ << " test output avalcount=" << optParams.avalCount << std::endl;
    }

    requesttxid = Parseuint256((char *)params[2].get_str().c_str());
    if (requesttxid.IsNull())
        throw runtime_error("incorrect requesttxid\n");

    // find the baton for transfer call:
    if (MarmaraGetbatontxid(creditloop, batontxid, requesttxid) < 0)
        throw runtime_error("couldnt find batontxid\n");

    result = MarmaraIssue(remotepk, 0, MARMARA_TRANSFER, pubkey2pk(receiverpub), optParams, requesttxid, batontxid);
    return result;
}

UniValue marmara_info(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ); 
    CPubKey pk; 
    std::vector<uint8_t> vpk; 
    int64_t minamount,maxamount; 
    int32_t firstheight,lastheight; 
    std::string currency;

    if ( fHelp || params.size() < 4 || params.size() > 6 )
    {
        throw runtime_error("marmarainfo firstheight lastheight minamount maxamount [pk currency]\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);
    /*
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    */
    firstheight = atol(params[0].get_str().c_str());
    lastheight = atol(params[1].get_str().c_str());
    minamount = atof(params[2].get_str().c_str()) * COIN;
    maxamount = atof(params[3].get_str().c_str()) * COIN;
    if (params.size() >= 5) {
        vpk = ParseHex(params[4].get_str().c_str());
        if (vpk.size() != CPubKey::COMPRESSED_PUBLIC_KEY_SIZE)
        {
            ERR_RESULT("invalid pubkey parameter");
            return result;
        }
        pk = pubkey2pk(vpk);
    }
    if ( params.size() == 6 )
    {
        currency = params[5].get_str();
    }

    /*if (!pk.IsValid()) {
        // if pk param not set then use mypk or remote pk
        if (remotepk.IsValid())
            pk = remotepk;
        else
            pk = pubkey2pk(Mypubkey());
    }*/

    result = MarmaraInfo(pk, firstheight, lastheight, minamount, maxamount, currency);
    return(result);
}

UniValue marmara_creditloop(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ); uint256 txid;
    if ( fHelp || params.size() != 1 )
    {
        // marmaracreditloop 010ff7f9256cefe3b5dee3d72c0eeae9fc6f34884e6f32ffe5b60916df54a9be
        throw runtime_error("marmaracreditloop txid\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    CONDITIONAL_LOCK2(cs_main, pwalletMain->cs_wallet, !remotepk.IsValid());
#endif 

    txid = Parseuint256((char *)params[0].get_str().c_str());
    result = MarmaraCreditloop(remotepk, txid);
    return(result);
}

UniValue marmara_settlement(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ); uint256 batontxid;
    if ( fHelp || params.size() != 1 )
    {
        // marmarasettlement 010ff7f9256cefe3b5dee3d72c0eeae9fc6f34884e6f32ffe5b60916df54a9be
        // marmarasettlement ff3e259869196f3da9b5ea3f9e088a76c4fc063cf36ab586b652e121d441a603
        throw runtime_error("marmarasettlement batontxid (discontinued)\n");
    }
    if ( ensure_CCrequirements(EVAL_MARMARA) < 0 )
        throw runtime_error(CC_REQUIREMENTS_MSG);

    throw runtime_error("marmarasettlement is discontinued\n");

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    CONDITIONAL_LOCK2(cs_main, pwalletMain->cs_wallet, !remotepk.IsValid());
#endif 

    /* batontxid = Parseuint256((char *)params[0].get_str().c_str());
    CTransaction tx;
    result = MarmaraSettlement(0,batontxid, tx); */
    return(result);
}

UniValue marmara_lock(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ); int64_t amount; int32_t height;
    if ( fHelp || (params.size() < 1 || params.size() > 2))
    {
        throw runtime_error("marmaralock amount [pubkey]\n" 
                            "converts normal coins to activated coins\n" "\n");
    }
    if (ensure_CCrequirements(EVAL_MARMARA) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    
#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    CONDITIONAL_LOCK2(cs_main, pwalletMain->cs_wallet, !remotepk.IsValid());
#endif   

    amount = atof(params[0].get_str().c_str()) * COIN + 0.00000000499999;

    CPubKey destPk; // created empty
    if (params.size() == 2) {
        vuint8_t vpubkey = ParseHex(params[1].get_str().c_str());
        destPk = pubkey2pk(vpubkey);
    }

    result = MarmaraLock(remotepk, 0, amount, destPk);
    return result;
}

// generate new activated address and output its segid
UniValue marmara_newaddress(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ); 
    if (fHelp || params.size() != 0)
    {
        throw runtime_error("marmaranewaddress\n");
    }
    if (ensure_CCrequirements(EVAL_MARMARA) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);
    
    std::string strAccount;

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    LOCK2(cs_main, pwalletMain->cs_wallet);
    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newPubKey;
    if (!pwalletMain->GetKeyFromPool(newPubKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newPubKey.GetID();

    pwalletMain->SetAddressBook(keyID, strAccount, "receive");

    result = MarmaraNewActivatedAddress(newPubKey);
#else
    result.push_back(std::make_pair("result", "error"));
    result.push_back(std::make_pair("error", "wallet unavailable"));
#endif 
    return result;
}

// marmaralock64 rpc impl, create 64 activated addresses for each segid and add amount / 64
UniValue marmara_lock64(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ);
    CCerror.clear();
    if (fHelp || params.size() != 2)
    {
        throw runtime_error("marmaralock64 amount num\n"
                            "generates 64 activated addresses in the wallet and distributes 'amount' in coins on the addresses creating 'num' utxos on each address\n" "\n");
    }
    if (ensure_CCrequirements(EVAL_MARMARA) < 0)
        throw runtime_error(CC_REQUIREMENTS_MSG);

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();
    //if (!pwalletMain->IsLocked())
    //    pwalletMain->TopUpKeyPool();

    CAmount amount = (CAmount)(atof(params[0].get_str().c_str()) * (double)COIN);
    if (amount <= 0)
        throw runtime_error("amount should be > 0\n");

    int32_t nutxos = atoi(params[1].get_str().c_str());
    if (nutxos <= 0)
        throw runtime_error("num should be > 0\n");

    std::string hextx = MarmaraLock64(pwalletMain, amount, nutxos);
    RETURN_IF_ERROR(CCerror);

    result.push_back(std::make_pair("result", "success"));
    result.push_back(std::make_pair("hextx", hextx));
#else
    result.push_back(std::make_pair("result", "error"));
    result.push_back(std::make_pair("error", "wallet unavailable"));
#endif
    return result;
}

// marmaralistactivated rpc impl, lists activated addresses in the wallet and return amounts on these addresses
UniValue marmara_listactivatedaddresses(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ);
    if (fHelp || params.size() != 0)
    {
        throw runtime_error("marmaralistactivatedaddresses\n"
            "list activated addresses in the wallet and returns amount on the addresses\n" "\n");
    }

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");
    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureWalletIsUnlocked();

    result = MarmaraListActivatedAddresses(pwalletMain);    
#else
    result.push_back(std::make_pair("result", "error"));
    result.push_back(std::make_pair("error", "wallet unavailable"));
#endif
    return result;
}

// marmarareleaseactivatedcoins rpc impl, collects activated utxos in the wallet and sends the amount to the address param
UniValue marmara_releaseactivatedcoins(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ);
    CCerror.clear();
    if (fHelp || params.size() != 1)
    {
        throw runtime_error("marmarareleaseactivatedcoins address\n"
            "collects activated utxos in the wallet and sends the amount to the normal 'address'\n" "\n");
    }

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");

    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureWalletIsUnlocked();

    std::string dest = params[0].get_str();
    std::string hextx = MarmaraReleaseActivatedCoins(pwalletMain, dest);
    RETURN_IF_ERROR(CCerror);

    result.push_back(std::make_pair("result", "success"));
    result.push_back(std::make_pair(JSON_HEXTX, hextx));
#else
    result.push_back(std::make_pair("result", "error"));
    result.push_back(std::make_pair("error", "wallet unavailable"));
#endif
    return result;
}

// marmaraposstat rpc impl, return PoS statistics
UniValue marmara_posstat(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    CCerror.clear();
    if (fHelp || params.size() != 2)
    {
        throw runtime_error("marmaraposstat begin-height end-height\n"
            "returns PoS statistics for the marmara chain from begin-height to end-height block.\n"
            "If begin-height is 0 the statistics is collected from the beginning of the chain\n"
            "If end-height is 0 the statistics is collected to the last block of the chain\n" "\n");
    }

    int32_t beginHeight = atoi(params[0].get_str().c_str());
    if (beginHeight < 0 || beginHeight > chainActive.Height())
        throw runtime_error("begin-height out of block range\n");
    int32_t endHeight = atoi(params[1].get_str().c_str());
    if (endHeight < 0 || endHeight > chainActive.Height())
        throw runtime_error("end-height out of block range\n");

    UniValue result = MarmaraPoSStat(beginHeight, endHeight);
    RETURN_IF_ERROR(CCerror);
    return result;
}

// marmaraposstat rpc impl, return PoS statistics
UniValue marmara_unlock(const UniValue& params, bool fHelp, const CPubKey& remotepk)
{
    UniValue result(UniValue::VOBJ);
    CCerror.clear();
    if (fHelp || params.size() != 1)
    {
        throw runtime_error("marmaraunlock satoshis\n"
            "unlocks activated coins on my pubkey and sends coins to normal address.\n" "\n");
    }

#ifdef ENABLE_WALLET
    if (!EnsureWalletIsAvailable(false))
        throw runtime_error("wallet is required");

    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    CAmount sat = atoll(params[0].get_str().c_str());
    result = MarmaraUnlockActivatedCoins(sat);
    RETURN_IF_ERROR(CCerror);
#else
    result.push_back(std::make_pair("result", "error"));
    result.push_back(std::make_pair("error", "wallet unavailable"));
#endif
    return result;
}


static const CRPCCommand commands[] =
{ //  category              name                actor (function)        okSafeMode
  //  -------------- ------------------------  -----------------------  ----------
  // Marmara
    { "marmara",       "marmaraaddress",   &marmaraaddress,      true },
    { "marmara",       "marmarapoolpayout",   &marmara_poolpayout,      true },
    { "marmara",       "marmarareceive",   &marmara_receive,      true },
    { "marmara",       "marmaraissue",   &marmara_issue,      true },
    { "marmara",       "marmaratransfer",   &marmara_transfer,      true },
    { "marmara",       "marmarainfo",   &marmara_info,      true },
    { "marmara",       "marmaracreditloop",   &marmara_creditloop,      true },
    { "marmara",       "marmarasettlement",   &marmara_settlement,      true },
    { "marmara",       "marmaralock",   &marmara_lock,      true },
    { "marmara",       "marmaranewaddress",   &marmara_newaddress,      true },
    { "marmara",       "marmaralock64",   &marmara_lock64,      true },
    { "marmara",       "marmaralistactivatedaddresses",   &marmara_listactivatedaddresses,      true },
    { "marmara",       "marmarareleaseactivatedcoins",   &marmara_releaseactivatedcoins,      true },
    { "marmara",       "marmaraposstat",   &marmara_posstat,      true },
    { "marmara",       "marmaraunlock",   &marmara_unlock,      true },
};

void RegisterMarmaraRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
