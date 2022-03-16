// Copyright (c) 2013-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "rpc/server.h"
#include "rpc/client.h"

#include "fs.h"
#include "key_io.h"
#include "main.h"
#include "wallet/wallet.h"

#include "zcash/Address.hpp"

#include "asyncrpcqueue.h"
#include "asyncrpcoperation.h"
#include "wallet/asyncrpcoperation_common.h"
#include "wallet/asyncrpcoperation_mergetoaddress.h"
#include "wallet/asyncrpcoperation_sendmany.h"
#include "wallet/asyncrpcoperation_shieldcoinbase.h"

#include "init.h"
#include "utiltest.h"

#include "test/test_bitcoin.h"
#include "test/test_util.h"
#include "wallet/test/wallet_test_fixture.h"

#include <array>
#include <chrono>
#include <optional>
#include <thread>
#include <variant>

#include <fstream>
#include <unordered_set>

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/format.hpp>

#include <univalue.h>

using namespace std;

extern CWallet* pwalletMain;

namespace {

bool find_error(const UniValue& objError, const std::string& expected) {
    return find_value(objError, "message").get_str().find(expected) != string::npos;
}

/** Set the working directory for the duration of the scope. */
class PushCurrentDirectory {
public:
    PushCurrentDirectory(const std::string &new_cwd)
        : old_cwd(fs::current_path()) {
        fs::current_path(new_cwd);
    }

    ~PushCurrentDirectory() {
        fs::current_path(old_cwd);
    }
private:
    fs::path old_cwd;
};

}

static UniValue ValueFromString(const std::string &str)
{
    UniValue value;
    BOOST_CHECK(value.setNumStr(str));
    return value;
}

static SaplingPaymentAddress DefaultSaplingAddress(CWallet* pwallet) {
    auto usk = pwallet->GenerateUnifiedSpendingKeyForAccount(0);

    return usk.value()
        .ToFullViewingKey()
        .GetSaplingKey().value()
        .FindAddress(libzcash::diversifier_index_t(0)).first;
}

BOOST_FIXTURE_TEST_SUITE(rpc_wallet_tests, WalletTestingSetup)

BOOST_AUTO_TEST_CASE(rpc_addmultisig)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    rpcfn_type addmultisig = tableRPC["addmultisigaddress"]->actor;

    // old, 65-byte-long:
    const char address1Hex[] = "0434e3e09f49ea168c5bbf53f877ff4206923858aab7c7e1df25bc263978107c95e35065a27ef6f1b27222db0ec97e0e895eaca603d3ee0d4c060ce3d8a00286c8";
    // new, compressed:
    const char address2Hex[] = "0388c2037017c62240b6b72ac1a2a5f94da790596ebd06177c8572752922165cb4";

    KeyIO keyIO(Params());
    UniValue v;
    CTxDestination address;
    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(1, address1Hex), false));
    address = keyIO.DecodeDestination(v.get_str());
    BOOST_CHECK(IsValidDestination(address) && IsScriptDestination(address));

    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(1, address1Hex, address2Hex), false));
    address = keyIO.DecodeDestination(v.get_str());
    BOOST_CHECK(IsValidDestination(address) && IsScriptDestination(address));

    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(2, address1Hex, address2Hex), false));
    address = keyIO.DecodeDestination(v.get_str());
    BOOST_CHECK(IsValidDestination(address) && IsScriptDestination(address));

    BOOST_CHECK_THROW(addmultisig(createArgs(0), false), runtime_error);
    BOOST_CHECK_THROW(addmultisig(createArgs(1), false), runtime_error);
    BOOST_CHECK_THROW(addmultisig(createArgs(2, address1Hex), false), runtime_error);

    BOOST_CHECK_THROW(addmultisig(createArgs(1, ""), false), runtime_error);
    BOOST_CHECK_THROW(addmultisig(createArgs(1, "NotAValidPubkey"), false), runtime_error);

    string short1(address1Hex, address1Hex + sizeof(address1Hex) - 2); // last byte missing
    BOOST_CHECK_THROW(addmultisig(createArgs(2, short1.c_str()), false), runtime_error);

    string short2(address1Hex + 1, address1Hex + sizeof(address1Hex)); // first byte missing
    BOOST_CHECK_THROW(addmultisig(createArgs(2, short2.c_str()), false), runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_wallet)
{
    // Test RPC calls for various wallet statistics
    KeyIO keyIO(Params());
    UniValue r;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CPubKey demoPubkey = pwalletMain->GenerateNewKey(true);
    CTxDestination demoAddress(CTxDestination(demoPubkey.GetID()));
    UniValue retValue;
    string strPurpose = "receive";
    BOOST_CHECK_NO_THROW({ /*Initialize Wallet with the demo pubkey*/
        CWalletDB walletdb(pwalletMain->strWalletFile);
        pwalletMain->SetAddressBook(demoPubkey.GetID(), "", strPurpose);
    });

    CPubKey setaccountDemoPubkey = pwalletMain->GenerateNewKey(true);
    CTxDestination setaccountDemoAddress(CTxDestination(setaccountDemoPubkey.GetID()));

    /*********************************
     *                  getbalance
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getbalance"));
    BOOST_CHECK_THROW(CallRPC("getbalance " + keyIO.EncodeDestination(demoAddress)), runtime_error);

    /*********************************
     * 			listunspent
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listunspent"));
    BOOST_CHECK_THROW(CallRPC("listunspent string"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("listunspent 0 string"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("listunspent 0 1 not_array"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("listunspent 0 1 [] extra"), runtime_error);
    BOOST_CHECK_NO_THROW(r = CallRPC("listunspent 0 1 []"));
    BOOST_CHECK(r.get_array().empty());

    /*********************************
     * 		listreceivedbyaddress
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaddress"));
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaddress 0"));
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaddress not_int"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaddress 0 not_bool"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaddress 0 true"));
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaddress 0 true extra"), runtime_error);

    /*********************************
     *          listsinceblock
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listsinceblock"));

    /*********************************
     *          listtransactions
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions"));
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions *"));
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions * 20"));
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions * 20 0"));
    BOOST_CHECK_THROW(CallRPC("listtransactions " + keyIO.EncodeDestination(demoAddress) + " not_int"), runtime_error);

    /*********************************
     *          listlockunspent
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listlockunspent"));

    /*********************************
     *          listaddressgroupings
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listaddressgroupings"));

    /*********************************
     * 		walletconfirmbackup
     *********************************/
    BOOST_CHECK_THROW(CallRPC(string("walletconfirmbackup \"badmnemonic\"")), runtime_error);

    /*********************************
     * 		getrawchangeaddress
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getrawchangeaddress"));

    /*********************************
     * 		getnewaddress
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getnewaddress"));

    /*********************************
     * 	signmessage + verifymessage
     *********************************/
    BOOST_CHECK_NO_THROW(retValue = CallRPC("signmessage " + keyIO.EncodeDestination(demoAddress) + " mymessage"));
    BOOST_CHECK_THROW(CallRPC("signmessage"), runtime_error);
    /* Should throw error because this address is not loaded in the wallet */
    BOOST_CHECK_THROW(CallRPC("signmessage t1h8SqgtM3QM5e2M8EzhhT1yL2PXXtA6oqe mymessage"), runtime_error);

    /* missing arguments */
    BOOST_CHECK_THROW(CallRPC("verifymessage " + keyIO.EncodeDestination(demoAddress)), runtime_error);
    BOOST_CHECK_THROW(CallRPC("verifymessage " + keyIO.EncodeDestination(demoAddress) + " " + retValue.get_str()), runtime_error);
    /* Illegal address */
    BOOST_CHECK_THROW(CallRPC("verifymessage t1VtArtnn1dGPiD2WFfMXYXW5mHM3q1Gpg " + retValue.get_str() + " mymessage"), runtime_error);
    /* wrong address */
    BOOST_CHECK(CallRPC("verifymessage t1VtArtnn1dGPiD2WFfMXYXW5mHM3q1GpgV " + retValue.get_str() + " mymessage").get_bool() == false);
    /* Correct address and signature but wrong message */
    BOOST_CHECK(CallRPC("verifymessage " + keyIO.EncodeDestination(demoAddress) + " " + retValue.get_str() + " wrongmessage").get_bool() == false);
    /* Correct address, message and signature*/
    BOOST_CHECK(CallRPC("verifymessage " + keyIO.EncodeDestination(demoAddress) + " " + retValue.get_str() + " mymessage").get_bool() == true);

    /*********************************
     * 		listaddresses
     *********************************/
    BOOST_CHECK_NO_THROW(retValue = CallRPC("listaddresses"));
    UniValue arr = retValue.get_array();
    {
        BOOST_CHECK_EQUAL(1, arr.size());
        bool notFound = true;
        for (auto a : arr.getValues()) {
            auto source = find_value(a.get_obj(), "source");
            if (source.get_str() == "legacy_random") {
                auto t_obj = find_value(a.get_obj(), "transparent");
                auto addrs = find_value(t_obj, "addresses").get_array();
                BOOST_CHECK_EQUAL(2, addrs.size());
                for (auto addr : addrs.getValues()) {
                    notFound &= keyIO.DecodeDestination(addr.get_str()) != demoAddress;
                }
            }
        }
        BOOST_CHECK(!notFound);
    }

    /*********************************
     * 	     fundrawtransaction
     *********************************/
    BOOST_CHECK_THROW(CallRPC("fundrawtransaction 28z"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("fundrawtransaction 01000000000180969800000000001976a91450ce0a4b0ee0ddeb633da85199728b940ac3fe9488ac00000000"), runtime_error);

    /*
     * getblocksubsidy
     */
    BOOST_CHECK_THROW(CallRPC("getblocksubsidy too many args"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("getblocksubsidy -1"), runtime_error);

    BOOST_CHECK_NO_THROW(retValue = CallRPC("getblocksubsidy 50000"));
    UniValue obj = retValue.get_obj();
    BOOST_CHECK_EQUAL(find_value(obj, "miner").get_real(), 10.0);
    BOOST_CHECK_EQUAL(find_value(obj, "founders").get_real(), 2.5);
    BOOST_CHECK(!obj.exists("fundingstreams"));

    BOOST_CHECK_NO_THROW(retValue = CallRPC("getblocksubsidy 653599")); // Blossom activation - 1
    obj = retValue.get_obj();
    BOOST_CHECK_EQUAL(find_value(obj, "miner").get_real(), 10.0);
    BOOST_CHECK_EQUAL(find_value(obj, "founders").get_real(), 2.5);
    BOOST_CHECK(!obj.exists("fundingstreams"));

    BOOST_CHECK_NO_THROW(retValue = CallRPC("getblocksubsidy 653600")); // Blossom activation
    obj = retValue.get_obj();
    BOOST_CHECK_EQUAL(find_value(obj, "miner").get_real(), 5.0);
    BOOST_CHECK_EQUAL(find_value(obj, "founders").get_real(), 1.25);
    BOOST_CHECK(!obj.exists("fundingstreams"));

    BOOST_CHECK_NO_THROW(retValue = CallRPC("getblocksubsidy 1046399"));
    obj = retValue.get_obj();
    BOOST_CHECK_EQUAL(find_value(obj, "miner").get_real(), 5.0);
    BOOST_CHECK_EQUAL(find_value(obj, "founders").get_real(), 1.25);
    BOOST_CHECK(!obj.exists("fundingstreams"));

    auto check_funding_streams = [](UniValue obj, std::vector<std::string> recipients, std::vector<double> amounts, std::vector<std::string> addresses) {
        size_t n = recipients.size();
        BOOST_REQUIRE_EQUAL(amounts.size(), n);
        UniValue fundingstreams = find_value(obj, "fundingstreams");
        BOOST_CHECK_EQUAL(fundingstreams.size(), n);
        if (fundingstreams.size() != n) return;

        for (int i = 0; i < n; i++) {
            UniValue fsobj = fundingstreams[i];
            BOOST_CHECK_EQUAL(find_value(fsobj, "recipient").get_str(), recipients[i]);
            BOOST_CHECK_EQUAL(find_value(fsobj, "specification").get_str(), "https://zips.z.cash/zip-0214");
            BOOST_CHECK_EQUAL(find_value(fsobj, "value").get_real(), amounts[i]);
            BOOST_CHECK_EQUAL(find_value(fsobj, "address").get_str(), addresses[i]);
        }
    };

    bool canopyEnabled =
        Params().GetConsensus().vUpgrades[Consensus::UPGRADE_CANOPY].nActivationHeight != Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;

    // slow start + blossom activation + (pre blossom halving - blossom activation) * 2
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getblocksubsidy 1046400"));
    obj = retValue.get_obj();
    BOOST_CHECK_EQUAL(find_value(obj, "miner").get_real(), canopyEnabled ? 2.5 : 3.125);
    BOOST_CHECK_EQUAL(find_value(obj, "founders").get_real(), 0.0);
    if (canopyEnabled) {
        check_funding_streams(obj, {"Electric Coin Company", "Zcash Foundation", "Major Grants" },
                                   { 0.21875,                 0.15625,            0.25          },
                                   {
                                       "t3LmX1cxWPPPqL4TZHx42HU3U5ghbFjRiif",
                                       "t3dvVE3SQEi7kqNzwrfNePxZ1d4hUyztBA1",
                                       "t3XyYW8yBFRuMnfvm5KLGFbEVz25kckZXym"
                                   });
    }

    BOOST_CHECK_NO_THROW(retValue = CallRPC("getblocksubsidy 2726399"));
    obj = retValue.get_obj();
    BOOST_CHECK_EQUAL(find_value(obj, "miner").get_real(), canopyEnabled ? 2.5 : 3.125);
    BOOST_CHECK_EQUAL(find_value(obj, "founders").get_real(), 0.0);
    if (canopyEnabled) {
        check_funding_streams(obj, {"Electric Coin Company", "Zcash Foundation", "Major Grants" },
                                   { 0.21875,                 0.15625,            0.25          },
                                   {
                                       "t3XHAGxRP2FNfhAjxGjxbrQPYtQQjc3RCQD",
                                       "t3dvVE3SQEi7kqNzwrfNePxZ1d4hUyztBA1",
                                       "t3XyYW8yBFRuMnfvm5KLGFbEVz25kckZXym"
                                   });
    }

    BOOST_CHECK_NO_THROW(retValue = CallRPC("getblocksubsidy 2726400"));
    obj = retValue.get_obj();
    BOOST_CHECK_EQUAL(find_value(obj, "miner").get_real(), 1.5625);
    BOOST_CHECK_EQUAL(find_value(obj, "founders").get_real(), 0.0);
    BOOST_CHECK(find_value(obj, "fundingstreams").empty());

    /*
     * getblock
     */
    BOOST_CHECK_THROW(CallRPC("getblock too many args"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("getblock -1")); // negative heights relative are allowed
    BOOST_CHECK_THROW(CallRPC("getblock -2147483647"), runtime_error); // allowed, but chain tip - height < 0
    BOOST_CHECK_THROW(CallRPC("getblock 2147483647"), runtime_error); // allowed, but > height of active chain tip
    BOOST_CHECK_THROW(CallRPC("getblock 2147483648"), runtime_error); // not allowed, > int32 used for nHeight
    BOOST_CHECK_THROW(CallRPC("getblock 100badchars"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("getblock 0"));
    BOOST_CHECK_NO_THROW(CallRPC("getblock 0 0"));
    BOOST_CHECK_NO_THROW(CallRPC("getblock 0 1"));
    BOOST_CHECK_NO_THROW(CallRPC("getblock 0 2"));
    BOOST_CHECK_THROW(CallRPC("getblock 0 -1"), runtime_error); // bad verbosity
    BOOST_CHECK_THROW(CallRPC("getblock 0 3"), runtime_error); // bad verbosity

    /*
     * migration (sprout to sapling)
     */
    BOOST_CHECK_NO_THROW(CallRPC("z_setmigration true"));
    BOOST_CHECK_NO_THROW(CallRPC("z_setmigration false"));
    BOOST_CHECK_THROW(CallRPC("z_setmigration"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_setmigration nonboolean"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_setmigration 1"), runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_wallet_getbalance)
{
    SelectParams(CBaseChainParams::TESTNET);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue retValue;
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getnewaddress"));
    std::string taddr1 = retValue.get_str();

    BOOST_CHECK_THROW(CallRPC("z_getbalance too many args"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_getbalance invalidaddress"), runtime_error);
    // address does not belong to wallet
    BOOST_CHECK_THROW(CallRPC("z_getbalance tmC6YZnCUhm19dEXxh3Jb7srdBJxDawaCab"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC(std::string("z_getbalance ") + taddr1));
    // negative minconf not allowed
    BOOST_CHECK_THROW(CallRPC(std::string("z_getbalance ") + taddr1 + " -1"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC(std::string("z_getbalance ") + taddr1 + std::string(" 0")));
    // don't have the spending key
    BOOST_CHECK_THROW(CallRPC("z_getbalance tnRZ8bPq2pff3xBWhTJhNkVUkm2uhzksDeW5PvEa7aFKGT9Qi3YgTALZfjaY4jU3HLVKBtHdSXxoPoLA3naMPcHBcY88FcF 1"), runtime_error);

    BOOST_CHECK_THROW(CallRPC("z_gettotalbalance too manyargs"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_gettotalbalance -1"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("z_gettotalbalance 0"));

    BOOST_CHECK_THROW(CallRPC("z_listreceivedbyaddress too many args"), runtime_error);
    // negative minconf not allowed
    BOOST_CHECK_THROW(CallRPC("z_listreceivedbyaddress tmC6YZnCUhm19dEXxh3Jb7srdBJxDawaCab -1"), runtime_error);
    // don't have the spending key
    BOOST_CHECK_THROW(CallRPC("z_listreceivedbyaddress tnRZ8bPq2pff3xBWhTJhNkVUkm2uhzksDeW5PvEa7aFKGT9Qi3YgTALZfjaY4jU3HLVKBtHdSXxoPoLA3naMPcHBcY88FcF 1"), runtime_error);
}

/**
 * This test covers RPC command z_validateaddress
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_z_validateaddress)
{
    SelectParams(CBaseChainParams::MAIN);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue retValue;

    // Check number of args
    BOOST_CHECK_THROW(CallRPC("z_validateaddress"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_validateaddress toomany args"), runtime_error);

    // Wallet should be empty
    std::set<libzcash::SproutPaymentAddress> addrs;
    pwalletMain->GetSproutPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==0);

    // This address is not valid, it belongs to another network
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_validateaddress ztaaga95QAPyp1kSQ1hD2kguCpzyMHjxWZqaYDEkzbvo7uYQYAw2S8X4Kx98AvhhofMtQL8PAXKHuZsmhRcanavKRKmdCzk"));
    UniValue resultObj = retValue.get_obj();
    bool b = find_value(resultObj, "isvalid").get_bool();
    BOOST_CHECK_EQUAL(b, false);

    // This address is valid, but the spending key is not in this wallet
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_validateaddress zcfA19SDAKRYHLoRDoShcoz4nPohqWxuHcqg8WAxsiB2jFrrs6k7oSvst3UZvMYqpMNSRBkxBsnyjjngX5L55FxMzLKach8"));
    resultObj = retValue.get_obj();
    b = find_value(resultObj, "isvalid").get_bool();
    BOOST_CHECK_EQUAL(b, true);
    BOOST_CHECK_EQUAL(find_value(resultObj, "type").get_str(), "sprout");
    b = find_value(resultObj, "ismine").get_bool();
    BOOST_CHECK_EQUAL(b, false);

    // Let's import a spending key to the wallet and validate its payment address
    BOOST_CHECK_NO_THROW(CallRPC("z_importkey SKxoWv77WGwFnUJitQKNEcD636bL4X5Gd6wWmgaA4Q9x8jZBPJXT"));
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_validateaddress zcWsmqT4X2V4jgxbgiCzyrAfRT1vi1F4sn7M5Pkh66izzw8Uk7LBGAH3DtcSMJeUb2pi3W4SQF8LMKkU2cUuVP68yAGcomL"));
    resultObj = retValue.get_obj();
    b = find_value(resultObj, "isvalid").get_bool();
    BOOST_CHECK_EQUAL(b, true);
    BOOST_CHECK_EQUAL(find_value(resultObj, "type").get_str(), "sprout");
    b = find_value(resultObj, "ismine").get_bool();
    BOOST_CHECK_EQUAL(b, true);
    BOOST_CHECK_EQUAL(find_value(resultObj, "payingkey").get_str(), "f5bb3c888ccc9831e3f6ba06e7528e26a312eec3acc1823be8918b6a3a5e20ad");
    BOOST_CHECK_EQUAL(find_value(resultObj, "transmissionkey").get_str(), "7a58c7132446564e6b810cf895c20537b3528357dc00150a8e201f491efa9c1a");

    // This Sapling address is not valid, it belongs to another network
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_validateaddress ztestsapling1knww2nyjc62njkard0jmx7hlsj6twxmxwprn7anvrv4dc2zxanl3nemc0qx2hvplxmd2uau8gyw"));
    resultObj = retValue.get_obj();
    b = find_value(resultObj, "isvalid").get_bool();
    BOOST_CHECK_EQUAL(b, false);

    // This Sapling address is valid, but the spending key is not in this wallet
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_validateaddress zs1z7rejlpsa98s2rrrfkwmaxu53e4ue0ulcrw0h4x5g8jl04tak0d3mm47vdtahatqrlkngh9slya"));
    resultObj = retValue.get_obj();
    b = find_value(resultObj, "isvalid").get_bool();
    BOOST_CHECK_EQUAL(b, true);
    BOOST_CHECK_EQUAL(find_value(resultObj, "type").get_str(), "sapling");
    b = find_value(resultObj, "ismine").get_bool();
    BOOST_CHECK_EQUAL(b, false);
    BOOST_CHECK_EQUAL(find_value(resultObj, "diversifier").get_str(), "1787997c30e94f050c634d");
    BOOST_CHECK_EQUAL(find_value(resultObj, "diversifiedtransmissionkey").get_str(), "34ed1f60f5db5763beee1ddbb37dd5f7e541d4d4fbdcc09fbfcc6b8e949bbe9d");
}

BOOST_AUTO_TEST_CASE(rpc_wallet_z_importkey_paymentaddress) {
    SelectParams(CBaseChainParams::MAIN);
    LOCK2(cs_main, pwalletMain->cs_wallet);

    auto testAddress = [](const std::string& type, const std::string& key) {
        UniValue ret;
        BOOST_CHECK_NO_THROW(ret = CallRPC("z_importkey " + key));
        auto defaultAddr = find_value(ret, "address").get_str();
        BOOST_CHECK_EQUAL(type, find_value(ret, "type").get_str());
        BOOST_CHECK_NO_THROW(ret = CallRPC("z_validateaddress " + defaultAddr));
        ret = ret.get_obj();
        BOOST_CHECK_EQUAL(true, find_value(ret, "isvalid").get_bool());
        BOOST_CHECK_EQUAL(true, find_value(ret, "ismine").get_bool());
        BOOST_CHECK_EQUAL(type, find_value(ret, "type").get_str());
    };

    testAddress("sapling", "secret-extended-key-main1qya4wae0qqqqqqpxfq3ukywunn"
            "dtr8xf39hktp3w4z94smuu3l8wr6h4cwxklzzemtg9sk5c7tamfqs48ml6rvuvyup8"
            "ne6jz9g7l0asew0htdpjgfss29et84uvqhynjayl3laphks2wxy3c8vhqr4wrca3wl"
            "ft2fhcacqtvfwsht4t33l8ckpyr8djmzj7swlvhdhepvc3ehycf9cja335ex6rlpka"
            "8z2gzkul3mztga2ups55c3xvn9j6vpdfm5a5v60g9v3sztcpvxqhm");

    testAddress("sprout",
            "SKxoWv77WGwFnUJitQKNEcD636bL4X5Gd6wWmgaA4Q9x8jZBPJXT");
}

/*
 * This test covers RPC command z_exportwallet
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_z_exportwallet)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    // wallet should be empty
    std::set<libzcash::SproutPaymentAddress> addrs;
    pwalletMain->GetSproutPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==0);

    // wallet should have one key
    libzcash::SproutPaymentAddress addr = pwalletMain->GenerateNewSproutZKey();
    pwalletMain->GetSproutPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==1);

    // Set up paths
    fs::path tmppath = fs::temp_directory_path();
    fs::path tmpfilename = fs::unique_path("%%%%%%%%");
    fs::path exportfilepath = tmppath / tmpfilename;

    // export will fail since exportdir is not set
    BOOST_CHECK_THROW(CallRPC(string("z_exportwallet ") + tmpfilename.string()), runtime_error);

    // set exportdir
    mapArgs["-exportdir"] = tmppath.string();

    // run some tests
    BOOST_CHECK_THROW(CallRPC("z_exportwallet"), runtime_error);

    BOOST_CHECK_THROW(CallRPC("z_exportwallet toomany args"), runtime_error);

    BOOST_CHECK_THROW(CallRPC(string("z_exportwallet invalid!*/_chars.txt")), runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC(string("z_exportwallet ") + tmpfilename.string()));


    libzcash::SproutSpendingKey key;
    BOOST_CHECK(pwalletMain->GetSproutSpendingKey(addr, key));

    KeyIO keyIO(Params());
    std::string s1 = keyIO.EncodePaymentAddress(addr);
    std::string s2 = keyIO.EncodeSpendingKey(key);

    // There's no way to really delete a private key so we will read in the
    // exported wallet file and search for the spending key and payment address.

    EnsureWalletIsUnlocked();

    ifstream file;
    file.open(exportfilepath.string().c_str(), std::ios::in | std::ios::ate);
    BOOST_CHECK(file.is_open());
    bool fVerified = false;
    int64_t nFilesize = std::max((int64_t)1, (int64_t)file.tellg());
    file.seekg(0, file.beg);
    while (file.good()) {
        std::string line;
        std::getline(file, line);
        if (line.empty() || line[0] == '#')
            continue;
        if (line.find(s1) != std::string::npos && line.find(s2) != std::string::npos) {
            fVerified = true;
            break;
        }
    }
    BOOST_CHECK(fVerified);
}


/*
 * This test covers RPC command z_importwallet
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_z_importwallet)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    // error if no args
    BOOST_CHECK_THROW(CallRPC("z_importwallet"), runtime_error);

    // error if too many args
    BOOST_CHECK_THROW(CallRPC("z_importwallet toomany args"), runtime_error);

    KeyIO keyIO(Params());
    // create a random key locally
    auto testSpendingKey = libzcash::SproutSpendingKey::random();
    auto testPaymentAddress = testSpendingKey.address();
    std::string testAddr = keyIO.EncodePaymentAddress(testPaymentAddress);
    std::string testKey = keyIO.EncodeSpendingKey(testSpendingKey);

    // create test data using the random key
    std::string format_str = "# Wallet dump created by Zcash v0.11.2.0.z8-9155cc6-dirty (2016-08-11 11:37:00 -0700)\n"
            "# * Created on 2016-08-12T21:55:36Z\n"
            "# * Best block at time of backup was 0 (0de0a3851fef2d433b9b4f51d4342bdd24c5ddd793eb8fba57189f07e9235d52),\n"
            "#   mined on 2009-01-03T18:15:05Z\n"
            "\n"
            "# Zkeys\n"
            "\n"
            "%s 2016-08-12T21:55:36Z # zaddr=%s\n"
            "\n"
            "\n# End of dump";

    boost::format formatobject(format_str);
    std::string testWalletDump = (formatobject % testKey % testAddr).str();

    // write test data to file
    fs::path temp = fs::temp_directory_path() /
            fs::unique_path();
    const std::string path = temp.string();
    std::ofstream file(path);
    file << testWalletDump;
    file << std::flush;

    // wallet should currently be empty
    std::set<libzcash::SproutPaymentAddress> addrs;
    pwalletMain->GetSproutPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==0);

    // import test data from file into wallet
    BOOST_CHECK_NO_THROW(CallRPC(string("z_importwallet ") + path));

    // wallet should now have one zkey
    pwalletMain->GetSproutPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==1);

    // check that we have the spending key for the address
    auto decoded = keyIO.DecodePaymentAddress(testAddr);
    BOOST_CHECK(decoded.has_value());
    libzcash::PaymentAddress address(decoded.value());
    BOOST_ASSERT(std::holds_alternative<libzcash::SproutPaymentAddress>(address));
    auto sprout_addr = std::get<libzcash::SproutPaymentAddress>(address);
    BOOST_CHECK(pwalletMain->HaveSproutSpendingKey(sprout_addr));

    // Verify the spending key is the same as the test data
    libzcash::SproutSpendingKey k;
    BOOST_CHECK(pwalletMain->GetSproutSpendingKey(sprout_addr, k));
    BOOST_CHECK_EQUAL(testKey, keyIO.EncodeSpendingKey(k));
}

/*
 * This test covers RPC commands z_listaddresses, z_importkey, z_exportkey,
 * listaddresses, z_importviewingkey, z_exportviewingkey
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_z_importexport)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);
    UniValue retValue;
    int n1 = 1000; // number of times to import/export
    int n2 = 1000; // number of addresses to create and list

    // error if no args
    BOOST_CHECK_THROW(CallRPC("z_importkey"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_exportkey"), runtime_error);

    // error if too many args
    BOOST_CHECK_THROW(CallRPC("z_importkey way too many args"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_exportkey toomany args"), runtime_error);

    // error if invalid args
    KeyIO keyIO(Params());
    auto sk = libzcash::SproutSpendingKey::random();
    std::string prefix = std::string("z_importkey ") + keyIO.EncodeSpendingKey(sk) + " yes ";
    BOOST_CHECK_THROW(CallRPC(prefix + "-1"), runtime_error);
    BOOST_CHECK_THROW(CallRPC(prefix + "2147483647"), runtime_error); // allowed, but > height of active chain tip
    BOOST_CHECK_THROW(CallRPC(prefix + "2147483648"), runtime_error); // not allowed, > int32 used for nHeight
    BOOST_CHECK_THROW(CallRPC(prefix + "100badchars"), runtime_error);

    // wallet should currently be empty
    std::set<libzcash::SproutPaymentAddress> sproutAddrs;
    pwalletMain->GetSproutPaymentAddresses(sproutAddrs);
    BOOST_CHECK(sproutAddrs.size()==0);
    std::set<libzcash::SaplingPaymentAddress> saplingAddrs;
    pwalletMain->GetSaplingPaymentAddresses(saplingAddrs);
    BOOST_CHECK(saplingAddrs.empty());

    auto m = GetTestMasterSaplingSpendingKey();

    // verify import and export key
    for (int i = 0; i < n1; i++) {
        // create a random Sprout key locally
        auto testSpendingKey = libzcash::SproutSpendingKey::random();
        auto testPaymentAddress = testSpendingKey.address();
        std::string testAddr = keyIO.EncodePaymentAddress(testPaymentAddress);
        std::string testKey = keyIO.EncodeSpendingKey(testSpendingKey);
        BOOST_CHECK_NO_THROW(CallRPC(string("z_importkey ") + testKey));
        BOOST_CHECK_NO_THROW(retValue = CallRPC(string("z_exportkey ") + testAddr));
        BOOST_CHECK_EQUAL(retValue.get_str(), testKey);

        // create a random Sapling key locally; split between IVKs and spending keys.
        auto testSaplingSpendingKey = m.Derive(i);
        auto testSaplingPaymentAddress = testSaplingSpendingKey.ToXFVK().DefaultAddress();
        if (i % 2 == 0) {
            std::string testSaplingAddr = keyIO.EncodePaymentAddress(testSaplingPaymentAddress);
            std::string testSaplingKey = keyIO.EncodeSpendingKey(testSaplingSpendingKey);
            BOOST_CHECK_NO_THROW(CallRPC(string("z_importkey ") + testSaplingKey));
            BOOST_CHECK_NO_THROW(retValue = CallRPC(string("z_exportkey ") + testSaplingAddr));
            BOOST_CHECK_EQUAL(retValue.get_str(), testSaplingKey);
        } else {
            std::string testSaplingAddr = keyIO.EncodePaymentAddress(testSaplingPaymentAddress);
            std::string testSaplingKey = keyIO.EncodeViewingKey(testSaplingSpendingKey.ToXFVK());
            BOOST_CHECK_NO_THROW(CallRPC(string("z_importviewingkey ") + testSaplingKey));
            BOOST_CHECK_NO_THROW(retValue = CallRPC(string("z_exportviewingkey ") + testSaplingAddr));
            BOOST_CHECK_EQUAL(retValue.get_str(), testSaplingKey);
        }
    }

    // Verify we can list the keys imported
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listaddresses"));
    UniValue arr = retValue.get_array();
    BOOST_CHECK(arr.size() == n1 + (n1 / 2));

    // Verify that the keys imported are also available from listaddresses
    {
        BOOST_CHECK_NO_THROW(retValue = CallRPC("listaddresses"));
        auto listarr = retValue.get_array();
        bool sproutCountMatch = false;
        bool saplingSpendingKeyMatch = false;
        bool saplingIVKMatch = false;
        for (auto a : listarr.getValues()) {
            auto source = find_value(a.get_obj(), "source");
            if (source.get_str() == "legacy_random") {
                auto sprout_obj = find_value(a.get_obj(), "sprout").get_obj();
                auto sprout_addrs = find_value(sprout_obj, "addresses").get_array();
                sproutCountMatch = (sprout_addrs.size() == n1);
            }
            if (source.get_str() == "imported") {
                int addr_count = 0;
                for (auto sapling_obj : find_value(a.get_obj(), "sapling").get_array().getValues()) {
                    auto sapling_addrs = find_value(sapling_obj, "addresses").get_array();
                    addr_count += sapling_addrs.size();
                }
                saplingSpendingKeyMatch = (addr_count == n1 / 2);
            }
            if (source.get_str() == "imported_watchonly") {
                int addr_count = 0;
                for (auto sapling_obj : find_value(a.get_obj(), "sapling").get_array().getValues()) {
                    auto sapling_addrs = find_value(sapling_obj, "addresses").get_array();
                    addr_count += sapling_addrs.size();
                }
                saplingIVKMatch = (addr_count == n1 / 2);
            }
        }
        BOOST_CHECK(sproutCountMatch);
        BOOST_CHECK(saplingSpendingKeyMatch);
        BOOST_CHECK(saplingIVKMatch);
    }

    // Put addresses into a set
    std::unordered_set<std::string> myaddrs;
    for (UniValue element : arr.getValues()) {
        myaddrs.insert(element.get_str());
    }

    // Make new addresses for the set
    for (int i=0; i<n2; i++) {
        myaddrs.insert(keyIO.EncodePaymentAddress(pwalletMain->GenerateNewSproutZKey()));
    }

    // Verify number of addresses stored in wallet is correct
    int numAddrs = myaddrs.size();
    // sprout_addrs + sapling addrs with spend authority + new sprout addrs
    BOOST_CHECK(numAddrs == (n1 + (n1 / 2) + n2));
    pwalletMain->GetSproutPaymentAddresses(sproutAddrs);
    pwalletMain->GetSaplingPaymentAddresses(saplingAddrs);
    // here we also include the watchonly sapling addresses
    BOOST_CHECK(sproutAddrs.size() + saplingAddrs.size() == numAddrs + (n1 / 2));

    // Ask wallet to list addresses
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listaddresses"));
    arr = retValue.get_array();
    BOOST_CHECK(arr.size() == numAddrs);

    // Create a set from them
    std::unordered_set<std::string> listaddrs;
    for (UniValue element : arr.getValues()) {
        listaddrs.insert(element.get_str());
    }

    // Verify that the newly added sprout keys imported are also available from listaddresses
    {
        BOOST_CHECK_NO_THROW(retValue = CallRPC("listaddresses"));
        auto listarr = retValue.get_array();
        bool sproutCountMatch = false;
        for (auto a : listarr.getValues()) {
            auto source = find_value(a.get_obj(), "source");
            if (source.get_str() == "legacy_random") {
                auto sprout_obj = find_value(a.get_obj(), "sprout").get_obj();
                auto sprout_addrs = find_value(sprout_obj, "addresses").get_array();
                sproutCountMatch = (sprout_addrs.size() == n1 + n2);
            }
        }
        BOOST_CHECK(sproutCountMatch);
    }

    // Verify the two sets of addresses are the same
    BOOST_CHECK(listaddrs.size() == numAddrs);
    BOOST_CHECK(myaddrs == listaddrs);
}

// Check if address is of given type and spendable from our wallet.
template <typename ADDR_TYPE>
void CheckHaveAddr(const std::optional<libzcash::PaymentAddress>& addr) {
    BOOST_CHECK(addr.has_value());
    auto addr_of_type = std::get_if<ADDR_TYPE>(&(addr.value()));
    BOOST_ASSERT(addr_of_type != nullptr);

    BOOST_CHECK(pwalletMain->ZTXOSelectorForAddress(*addr_of_type, true, false).has_value());
}

BOOST_AUTO_TEST_CASE(rpc_wallet_z_getnewaddress) {
    using namespace libzcash;

    UniValue addr;

    KeyIO keyIO(Params());
    // No parameter defaults to sapling address
    addr = CallRPC("z_getnewaddress");
    CheckHaveAddr<SaplingPaymentAddress>(keyIO.DecodePaymentAddress(addr.get_str()));

    // Passing 'sapling' should also work
    addr = CallRPC("z_getnewaddress sapling");
    CheckHaveAddr<SaplingPaymentAddress>(keyIO.DecodePaymentAddress(addr.get_str()));

    // Should also support sprout
    addr = CallRPC("z_getnewaddress sprout");
    CheckHaveAddr<SproutPaymentAddress>(keyIO.DecodePaymentAddress(addr.get_str()));

    // Requesting a sprout address during IBD should fail
    bool ibd = TestSetIBD(true);
    CheckRPCThrows("z_getnewaddress sprout", "Error: Creating a Sprout address during initial block download is not supported.");
    TestSetIBD(ibd);

    // Should throw on invalid argument
    CheckRPCThrows("z_getnewaddress garbage", "Invalid address type");

    // Too many arguments will throw with the help
    BOOST_CHECK_THROW(CallRPC("z_getnewaddress many args"), runtime_error);

    {
        UniValue list;
        BOOST_CHECK_NO_THROW(list = CallRPC("listaddresses"));
        auto listarr = list.get_array();
        bool sproutCountMatch = false;
        bool saplingExtfvksMatch = false;
        bool saplingSpendAuth0 = false;
        bool saplingSpendAuth1 = false;
        bool saplingCountMismatch = true;
        for (auto a : listarr.getValues()) {
            auto source = find_value(a.get_obj(), "source");
            if (source.get_str() == "legacy_random") {
                auto sproutObj = find_value(a.get_obj(), "sprout").get_obj();
                auto sproutAddrs = find_value(sproutObj, "addresses").get_array();
                sproutCountMatch = (sproutAddrs.size() == 1);
            }
            if (source.get_str() == "legacy_hdseed") {
                auto sapling_addr_sets = find_value(a.get_obj(), "sapling").get_array();
                saplingExtfvksMatch = (sapling_addr_sets.size() == 2);

                for (auto saplingObj : sapling_addr_sets.getValues()) {
                    auto keypath = find_value(saplingObj, "zip32KeyPath").get_str();
                    saplingSpendAuth0 |= (keypath == "m/32'/133'/2147483647'/0'");
                    saplingSpendAuth1 |= (keypath == "m/32'/133'/2147483647'/1'");
                    auto saplingAddrs = find_value(saplingObj, "addresses").get_array();
                    saplingCountMismatch &= (saplingAddrs.size() != 1);
                }
            }
        }
        BOOST_CHECK(sproutCountMatch);
        BOOST_CHECK(saplingExtfvksMatch);
        BOOST_CHECK(!saplingCountMismatch);
        BOOST_CHECK(saplingSpendAuth0);
        BOOST_CHECK(saplingSpendAuth1);
    }
}

/**
 * Test Async RPC operations.
 * Tip: Create mock operations by subclassing AsyncRPCOperation.
 */

class MockSleepOperation : public AsyncRPCOperation {
public:
    std::chrono::milliseconds naptime;
    MockSleepOperation(int t=1000) {
        this->naptime = std::chrono::milliseconds(t);
    }
    virtual ~MockSleepOperation() {
    }
    virtual void main() {
        set_state(OperationStatus::EXECUTING);
        start_execution_clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(naptime));
        stop_execution_clock();
        set_result(UniValue(UniValue::VSTR, "done"));
        set_state(OperationStatus::SUCCESS);
    }
};


/*
 * Test Async RPC queue and operations.
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_async_operations)
{
    std::shared_ptr<AsyncRPCQueue> q = std::make_shared<AsyncRPCQueue>();
    BOOST_CHECK(q->getNumberOfWorkers() == 0);
    std::vector<AsyncRPCOperationId> ids = q->getAllOperationIds();
    BOOST_CHECK(ids.size()==0);

    std::shared_ptr<AsyncRPCOperation> op1 = std::make_shared<AsyncRPCOperation>();
    q->addOperation(op1);
    BOOST_CHECK(q->getOperationCount() == 1);

    OperationStatus status = op1->getState();
    BOOST_CHECK(status == OperationStatus::READY);

    AsyncRPCOperationId id1 = op1->getId();
    int64_t creationTime1 = op1->getCreationTime();

    q->addWorker();
    BOOST_CHECK(q->getNumberOfWorkers() == 1);

    // an AsyncRPCOperation doesn't do anything so will finish immediately
    std::this_thread::sleep_for(std::chrono::seconds(1));
    BOOST_CHECK(q->getOperationCount() == 0);

    // operation should be a success
    BOOST_CHECK_EQUAL(op1->isCancelled(), false);
    BOOST_CHECK_EQUAL(op1->isExecuting(), false);
    BOOST_CHECK_EQUAL(op1->isReady(), false);
    BOOST_CHECK_EQUAL(op1->isFailed(), false);
    BOOST_CHECK_EQUAL(op1->isSuccess(), true);
    BOOST_CHECK_EQUAL(op1->getError().isNull(), true);
    BOOST_CHECK_EQUAL(op1->getResult().isNull(), false);
    BOOST_CHECK_EQUAL(op1->getStateAsString(), "success");
    BOOST_CHECK_NE(op1->getStateAsString(), "executing");

    // Create a second operation which just sleeps
    std::shared_ptr<AsyncRPCOperation> op2(new MockSleepOperation(2500));
    AsyncRPCOperationId id2 = op2->getId();
    int64_t creationTime2 = op2->getCreationTime();

    // it's different from the previous operation
    BOOST_CHECK_NE(op1.get(), op2.get());
    BOOST_CHECK_NE(id1, id2);
    BOOST_CHECK_NE(creationTime1, creationTime2);

    // Only the first operation has been added to the queue
    std::vector<AsyncRPCOperationId> v = q->getAllOperationIds();
    std::set<AsyncRPCOperationId> opids(v.begin(), v.end());
    BOOST_CHECK(opids.size() == 1);
    BOOST_CHECK(opids.count(id1)==1);
    BOOST_CHECK(opids.count(id2)==0);
    std::shared_ptr<AsyncRPCOperation> p1 = q->getOperationForId(id1);
    BOOST_CHECK_EQUAL(p1.get(), op1.get());
    std::shared_ptr<AsyncRPCOperation> p2 = q->getOperationForId(id2);
    BOOST_CHECK(!p2); // null ptr as not added to queue yet

    // Add operation 2 and 3 to the queue
    q->addOperation(op2);
    std::shared_ptr<AsyncRPCOperation> op3(new MockSleepOperation(1000));
    q->addOperation(op3);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    BOOST_CHECK_EQUAL(op2->isExecuting(), true);
    op2->cancel();  // too late, already executing
    op3->cancel();
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    BOOST_CHECK_EQUAL(op2->isSuccess(), true);
    BOOST_CHECK_EQUAL(op2->isCancelled(), false);
    BOOST_CHECK_EQUAL(op3->isCancelled(), true);


    v = q->getAllOperationIds();
    std::copy( v.begin(), v.end(), std::inserter( opids, opids.end() ) );
    BOOST_CHECK(opids.size() == 3);
    BOOST_CHECK(opids.count(id1)==1);
    BOOST_CHECK(opids.count(id2)==1);
    BOOST_CHECK(opids.count(op3->getId())==1);
    q->finishAndWait();
}


// The CountOperation will increment this global
std::atomic<int64_t> gCounter(0);

class CountOperation : public AsyncRPCOperation {
public:
    CountOperation() {}
    virtual ~CountOperation() {}
    virtual void main() {
        set_state(OperationStatus::EXECUTING);
        gCounter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        set_state(OperationStatus::SUCCESS);
    }
};

// This tests the queue waiting for multiple workers to finish
BOOST_AUTO_TEST_CASE(rpc_wallet_async_operations_parallel_wait)
{
    gCounter = 0;

    std::shared_ptr<AsyncRPCQueue> q = std::make_shared<AsyncRPCQueue>();
    q->addWorker();
    q->addWorker();
    q->addWorker();
    q->addWorker();
    BOOST_CHECK(q->getNumberOfWorkers() == 4);

    int64_t numOperations = 10;     // 10 * 1000ms / 4 = 2.5 secs to finish
    for (int i=0; i<numOperations; i++) {
        std::shared_ptr<AsyncRPCOperation> op(new CountOperation());
        q->addOperation(op);
    }

    std::vector<AsyncRPCOperationId> ids = q->getAllOperationIds();
    BOOST_CHECK(ids.size()==numOperations);
    q->finishAndWait();
    BOOST_CHECK_EQUAL(q->isFinishing(), true);
    BOOST_CHECK_EQUAL(numOperations, gCounter.load());
}

// This tests the queue shutting down immediately
BOOST_AUTO_TEST_CASE(rpc_wallet_async_operations_parallel_cancel)
{
    gCounter = 0;

    std::shared_ptr<AsyncRPCQueue> q = std::make_shared<AsyncRPCQueue>();
    q->addWorker();
    q->addWorker();
    BOOST_CHECK(q->getNumberOfWorkers() == 2);

    int numOperations = 10000;  // 10000 seconds to complete
    for (int i=0; i<numOperations; i++) {
        std::shared_ptr<AsyncRPCOperation> op(new CountOperation());
        q->addOperation(op);
    }
    std::vector<AsyncRPCOperationId> ids = q->getAllOperationIds();
    BOOST_CHECK(ids.size()==numOperations);
    q->closeAndWait();

    int numSuccess = 0;
    int numCancelled = 0;
    for (auto & id : ids) {
        std::shared_ptr<AsyncRPCOperation> ptr = q->popOperationForId(id);
        if (ptr->isCancelled()) {
            numCancelled++;
        } else if (ptr->isSuccess()) {
            numSuccess++;
        }
    }

    BOOST_CHECK_EQUAL(numOperations, numSuccess+numCancelled);
    BOOST_CHECK_EQUAL(gCounter.load(), numSuccess);
    BOOST_CHECK(q->getOperationCount() == 0);
    ids = q->getAllOperationIds();
    BOOST_CHECK(ids.size()==0);
}

// This tests z_getoperationstatus, z_getoperationresult, z_listoperationids
BOOST_AUTO_TEST_CASE(rpc_z_getoperations)
{
    std::shared_ptr<AsyncRPCQueue> q = getAsyncRPCQueue();
    std::shared_ptr<AsyncRPCQueue> sharedInstance = AsyncRPCQueue::sharedInstance();
    BOOST_CHECK(q == sharedInstance);

    BOOST_CHECK_NO_THROW(CallRPC("z_getoperationstatus"));
    BOOST_CHECK_NO_THROW(CallRPC("z_getoperationstatus []"));
    BOOST_CHECK_NO_THROW(CallRPC("z_getoperationstatus [\"opid-1234\"]"));
    BOOST_CHECK_THROW(CallRPC("z_getoperationstatus [] toomanyargs"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_getoperationstatus not_an_array"), runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC("z_getoperationresult"));
    BOOST_CHECK_NO_THROW(CallRPC("z_getoperationresult []"));
    BOOST_CHECK_NO_THROW(CallRPC("z_getoperationresult [\"opid-1234\"]"));
    BOOST_CHECK_THROW(CallRPC("z_getoperationresult [] toomanyargs"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_getoperationresult not_an_array"), runtime_error);

    std::shared_ptr<AsyncRPCOperation> op1 = std::make_shared<AsyncRPCOperation>();
    q->addOperation(op1);
    std::shared_ptr<AsyncRPCOperation> op2 = std::make_shared<AsyncRPCOperation>();
    q->addOperation(op2);

    BOOST_CHECK(q->getOperationCount() == 2);
    BOOST_CHECK(q->getNumberOfWorkers() == 0);
    q->addWorker();
    BOOST_CHECK(q->getNumberOfWorkers() == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    BOOST_CHECK(q->getOperationCount() == 0);

    // Check if too many args
    BOOST_CHECK_THROW(CallRPC("z_listoperationids toomany args"), runtime_error);

    UniValue retValue;
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listoperationids"));
    BOOST_CHECK(retValue.get_array().size() == 2);

    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_getoperationstatus"));
    UniValue array = retValue.get_array();
    BOOST_CHECK(array.size() == 2);

    // idempotent
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_getoperationstatus"));
    array = retValue.get_array();
    BOOST_CHECK(array.size() == 2);

    for (UniValue v : array.getValues()) {
        UniValue obj = v.get_obj();
        UniValue id = find_value(obj, "id");

        UniValue result;
        // removes result from internal storage
        BOOST_CHECK_NO_THROW(result = CallRPC("z_getoperationresult [\"" + id.get_str() + "\"]"));
        UniValue resultArray = result.get_array();
        BOOST_CHECK(resultArray.size() == 1);

        UniValue resultObj = resultArray[0].get_obj();
        UniValue resultId = find_value(resultObj, "id");
        BOOST_CHECK_EQUAL(id.get_str(), resultId.get_str());

        // verify the operation has been removed
        BOOST_CHECK_NO_THROW(result = CallRPC("z_getoperationresult [\"" + id.get_str() + "\"]"));
        resultArray = result.get_array();
        BOOST_CHECK(resultArray.size() == 0);
    }

    // operations removed
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_getoperationstatus"));
    array = retValue.get_array();
    BOOST_CHECK(array.size() == 0);

    q->close();
}

BOOST_AUTO_TEST_CASE(rpc_z_sendmany_parameters)
{
    SelectParams(CBaseChainParams::TESTNET);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    BOOST_CHECK_THROW(CallRPC("z_sendmany"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_sendmany toofewargs"), runtime_error);
    // too many arguments:
    BOOST_CHECK_THROW(CallRPC("z_sendmany addr [] 1 0.001 true true"), runtime_error);

    // bad from address
    BOOST_CHECK_THROW(CallRPC("z_sendmany "
            "INVALIDtmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ []"), runtime_error);
    // empty amounts
    BOOST_CHECK_THROW(CallRPC("z_sendmany "
            "tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ []"), runtime_error);

    // don't have the spending key for this address
    BOOST_CHECK_THROW(CallRPC("z_sendmany "
            "tnpoQJVnYBZZqkFadj2bJJLThNCxbADGB5gSGeYTAGGrT5tejsxY9Zc1BtY8nnHmZkB"
            "UkJ1oSfbhTJhm72WiZizvkZz5aH1 []"), runtime_error);

    // duplicate address
    BOOST_CHECK_THROW(CallRPC("z_sendmany "
            "tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ "
            "[{\"address\":\"tmQP9L3s31cLsghVYf2Jb5MhKj1jRBPoeQn\",\"amount\":50.0},"
            "{\"address\":\"tmQP9L3s31cLsghVYf2Jb5MhKj1jRBPoeQn\",\"amount\":12.0}]"
            ), runtime_error);

    // invalid fee amount, cannot be negative
    BOOST_CHECK_THROW(CallRPC("z_sendmany "
            "tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ "
            "[{\"address\":\"tmQP9L3s31cLsghVYf2Jb5MhKj1jRBPoeQn\",\"amount\":50.0}] "
            "1 -0.00001"
            ), runtime_error);

    // invalid fee amount, bigger than MAX_MONEY
    BOOST_CHECK_THROW(CallRPC("z_sendmany "
            "tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ "
            "[{\"address\":\"tmQP9L3s31cLsghVYf2Jb5MhKj1jRBPoeQn\",\"amount\":50.0}] "
            "1 21000001"
            ), runtime_error);

    // fee amount is bigger than sum of outputs
    BOOST_CHECK_THROW(CallRPC("z_sendmany "
            "tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ "
            "[{\"address\":\"tmQP9L3s31cLsghVYf2Jb5MhKj1jRBPoeQn\",\"amount\":50.0}] "
            "1 50.00000001"
            ), runtime_error);

    // memo bigger than allowed length of ZC_MEMO_SIZE
    std::vector<char> v (2 * (ZC_MEMO_SIZE+1));     // x2 for hexadecimal string format
    std::fill(v.begin(),v.end(), 'A');
    std::string badmemo(v.begin(), v.end());
    auto pa = pwalletMain->GenerateNewSproutZKey();
    KeyIO keyIO(Params());
    std::string zaddr1 = keyIO.EncodePaymentAddress(pa);
    BOOST_CHECK_THROW(CallRPC(string("z_sendmany tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ ")
            + "[{\"address\":\"" + zaddr1 + "\",\"amount\":123.456}]"), runtime_error);

    // Mutable tx containing contextual information we need to build tx
    UniValue retValue = CallRPC("getblockcount");
    int nHeight = retValue.get_int();
    TransactionBuilder builder(Params().GetConsensus(), nHeight + 1, std::nullopt, pwalletMain);
}

BOOST_AUTO_TEST_CASE(asyncrpcoperation_sign_send_raw_transaction) {
    // Raw joinsplit is a zaddr->zaddr
    std::string raw = "020000000000000000000100000000000000001027000000000000183a0d4c46c369078705e39bcfebee59a978dbd210ce8de3efc9555a03fbabfd3cea16693d730c63850d7e48ccde79854c19adcb7e9dcd7b7d18805ee09083f6b16e1860729d2d4a90e2f2acd009cf78b5eb0f4a6ee4bdb64b1262d7ce9eb910c460b02022991e968d0c50ee44908e4ccccbc591d0053bcca154dd6d6fc400a29fa686af4682339832ccea362a62aeb9df0d5aa74f86a1e75ac0f48a8ccc41e0a940643c6c33e1d09223b0a46eaf47a1bb4407cfc12b1dcf83a29c0cef51e45c7876ca5b9e5bae86d92976eb3ef68f29cd29386a8be8451b50f82bf9da10c04651868655194da8f6ed3d241bb5b5ff93a3e2bbe44644544d88bcde5cc35978032ee92699c7a61fcbb395e7583f47e698c4d53ede54f956629400bf510fb5e22d03158cc10bdcaaf29e418ef18eb6480dd9c8b9e2a377809f9f32a556ef872febd0021d4ad013aa9f0b7255e98e408d302abefd33a71180b720271835b487ab309e160b06dfe51932120fb84a7ede16b20c53599a11071592109e10260f265ee60d48c62bfe24074020e9b586ce9e9356e68f2ad1a9538258234afe4b83a209f178f45202270eaeaeecaf2ce3100b2c5a714f75f35777a9ebff5ebf47059d2bbf6f3726190216468f2b152673b766225b093f3a2f827c86d6b48b42117fec1d0ac38dd7af700308dcfb02eba821612b16a2c164c47715b9b0c93900893b1aba2ea03765c94d87022db5be06ab338d1912e0936dfe87586d0a8ee49144a6cd2e306abdcb652faa3e0222739deb23154d778b50de75069a4a2cce1208cd1ced3cb4744c9888ce1c2fcd2e66dc31e62d3aa9e423d7275882525e9981f92e84ac85975b8660739407efbe1e34c2249420fde7e17db3096d5b22e83d051d01f0e6e7690dca7d168db338aadf0897fedac10de310db2b1bff762d322935dddbb60c2efb8b15d231fa17b84630371cb275c209f0c4c7d0c68b150ea5cd514122215e3f7fcfb351d69514788d67c2f3c8922581946e3a04bdf1f07f15696ca76eb95b10698bf1188fd882945c57657515889d042a6fc45d38cbc943540c4f0f6d1c45a1574c81f3e42d1eb8702328b729909adee8a5cfed7c79d54627d1fd389af941d878376f7927b9830ca659bf9ab18c5ca5192d52d02723008728d03701b8ab3e1c4a3109409ec0b13df334c7deec3523eeef4c97b5603e643de3a647b873f4c1b47fbfc6586ba66724f112e51fc93839648005043620aa3ce458e246d77977b19c53d98e3e812de006afc1a79744df236582943631d04cc02941ac4be500e4ed9fb9e3e7cc187b1c4050fad1d9d09d5fd70d5d01d615b439d8c0015d2eb10398bcdbf8c4b2bd559dbe4c288a186aed3f86f608da4d582e120c4a896e015e2241900d1daeccd05db968852677c71d752bec46de9962174b46f980e8cc603654daf8b98a3ee92dac066033954164a89568b70b1780c2ce2410b2f816dbeddb2cd463e0c8f21a52cf6427d9647a6fd4bafa8fb4cd4d47ac057b0160bee86c6b2fb8adce214c2bcdda277512200adf0eaa5d2114a2c077b009836a68ec254bfe56f51d147b9afe2ddd9cb917c0c2de19d81b7b8fd9f4574f51fa1207630dc13976f4d7587c962f761af267de71f3909a576e6bedaf6311633910d291ac292c467cc8331ef577aef7646a5d949322fa0367a49f20597a13def53136ee31610395e3e48d291fd8f58504374031fe9dcfba5e06086ebcf01a9106f6a4d6e16e19e4c5bb893f7da79419c94eca31a384be6fa1747284dee0fc3bbc8b1b860172c10b29c1594bb8c747d7fe05827358ff2160f49050001625ffe2e880bd7fc26cd0ffd89750745379a8e862816e08a5a2008043921ab6a4976064ac18f7ee37b6628bc0127d8d5ebd3548e41d8881a082d86f20b32e33094f15a0e6ea6074b08c6cd28142de94713451640a55985051f5577eb54572699d838cb34a79c8939e981c0c277d06a6e2ce69ccb74f8a691ff08f81d8b99e6a86223d29a2b7c8e7b041aba44ea678ae654277f7e91cbfa79158b989164a3d549d9f4feb0cc43169699c13e321fe3f4b94258c75d198ff9184269cd6986c55409e07528c93f64942c6c283ce3917b4bf4c3be2fe3173c8c38cccb35f1fbda0ca88b35a599c0678cb22aa8eabea8249dbd2e4f849fffe69803d299e435ebcd7df95854003d8eda17a74d98b4be0e62d45d7fe48c06a6f464a14f8e0570077cc631279092802a89823f031eef5e1028a6d6fdbd502869a731ee7d28b4d6c71b419462a30d31442d3ee444ffbcbd16d558c9000c97e949c2b1f9d6f6d8db7b9131ebd963620d3fc8595278d6f8fdf49084325373196d53e64142fa5a23eccd6ef908c4d80b8b3e6cc334b7f7012103a3682e4678e9b518163d262a39a2c1a69bf88514c52b7ccd7cc8dc80e71f7c2ec0701cff982573eb0c2c4daeb47fa0b586f4451c10d1da2e5d182b03dd067a5e971b3a6138ca6667aaf853d2ac03b80a1d5870905f2cfb6c78ec3c3719c02f973d638a0f973424a2b0f2b0023f136d60092fe15fba4bc180b9176bd0ff576e053f1af6939fe9ca256203ffaeb3e569f09774d2a6cbf91873e56651f4d6ff77e0b5374b0a1a201d7e523604e0247644544cc571d48c458a4f96f45580b";
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("rawtxn", raw);
    // Verify test mode is returning output (since no input taddrs, signed and unsigned are the same).
    std::pair<CTransaction, UniValue> txAndResult = SignSendRawTransaction(obj, std::nullopt, true);
    UniValue resultObj = txAndResult.second.get_obj();
    std::string hex = find_value(resultObj, "hex").get_str();
    BOOST_CHECK_EQUAL(hex, raw);
}

// TODO: test private methods
BOOST_AUTO_TEST_CASE(rpc_z_sendmany_internals)
{
    SelectParams(CBaseChainParams::TESTNET);
    const Consensus::Params& consensusParams = Params().GetConsensus();
    KeyIO keyIO(Params());

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue retValue;

    // Mutable tx containing contextual information we need to build tx
    // We removed the ability to create pre-Sapling Sprout proofs, so we can
    // only create Sapling-onwards transactions.
    int nHeight = consensusParams.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight;

    // add keys manually
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getnewaddress"));
    auto taddr1 = std::get<CKeyID>(keyIO.DecodePaymentAddress(retValue.get_str()).value());

    if (!pwalletMain->HaveMnemonicSeed()) {
        pwalletMain->GenerateNewSeed();
    }
    auto zaddr1 = pwalletMain->GenerateNewLegacySaplingZKey();

    // there are no utxos to spend
    {
        auto selector = pwalletMain->ZTXOSelectorForAddress(taddr1, true, false).value();
        TransactionBuilder builder(consensusParams, nHeight + 1, std::nullopt, pwalletMain);
        std::vector<SendManyRecipient> recipients = { SendManyRecipient(std::nullopt, zaddr1, 100*COIN, "DEADBEEF") };
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_sendmany(std::move(builder), selector, recipients, 1));
        operation->main();
        BOOST_CHECK(operation->isFailed());
        std::string msg = operation->getErrorMessage();
        BOOST_CHECK( msg.find("Insufficient funds") != string::npos);
    }

    // there are no unspent notes to spend
    {
        auto selector = pwalletMain->ZTXOSelectorForAddress(zaddr1, true, false).value();
        TransactionBuilder builder(consensusParams, nHeight + 1, std::nullopt, pwalletMain);
        std::vector<SendManyRecipient> recipients = { SendManyRecipient(std::nullopt, taddr1, 100*COIN, "DEADBEEF") };
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_sendmany(std::move(builder), selector, recipients, 1));
        operation->main();
        BOOST_CHECK(operation->isFailed());
        std::string msg = operation->getErrorMessage();
        BOOST_CHECK(msg.find("Insufficient funds: have 0.00") != string::npos);
    }

    // get_memo_from_hex_string())
    {
        auto selector = pwalletMain->ZTXOSelectorForAddress(zaddr1, true, false).value();
        TransactionBuilder builder(consensusParams, nHeight + 1, std::nullopt, pwalletMain);
        std::vector<SendManyRecipient> recipients = { SendManyRecipient(std::nullopt, zaddr1, 100*COIN, "DEADBEEF") };
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_sendmany(std::move(builder), selector, recipients, 1));
        std::shared_ptr<AsyncRPCOperation_sendmany> ptr = std::dynamic_pointer_cast<AsyncRPCOperation_sendmany> (operation);
        TEST_FRIEND_AsyncRPCOperation_sendmany proxy(ptr);

        std::string memo = "DEADBEEF";
        std::array<unsigned char, ZC_MEMO_SIZE> array = proxy.get_memo_from_hex_string(memo);
        BOOST_CHECK_EQUAL(array[0], 0xDE);
        BOOST_CHECK_EQUAL(array[1], 0xAD);
        BOOST_CHECK_EQUAL(array[2], 0xBE);
        BOOST_CHECK_EQUAL(array[3], 0xEF);
        for (int i=4; i<ZC_MEMO_SIZE; i++) {
            BOOST_CHECK_EQUAL(array[i], 0x00);  // zero padding
        }

        // memo is longer than allowed
        std::vector<char> v (2 * (ZC_MEMO_SIZE+1));
        std::fill(v.begin(),v.end(), 'A');
        std::string bigmemo(v.begin(), v.end());

        try {
            proxy.get_memo_from_hex_string(bigmemo);
        } catch (const UniValue& objError) {
            BOOST_CHECK( find_error(objError, "too big"));
        }

        // invalid hexadecimal string
        std::fill(v.begin(),v.end(), '@'); // not a hex character
        std::string badmemo(v.begin(), v.end());

        try {
            proxy.get_memo_from_hex_string(badmemo);
        } catch (const UniValue& objError) {
            BOOST_CHECK( find_error(objError, "hexadecimal format"));
        }

        // odd length hexadecimal string
        std::fill(v.begin(),v.end(), 'A');
        v.resize(v.size() - 1);
        assert(v.size() %2 == 1); // odd length
        std::string oddmemo(v.begin(), v.end());
        try {
            proxy.get_memo_from_hex_string(oddmemo);
        } catch (const UniValue& objError) {
            BOOST_CHECK( find_error(objError, "hexadecimal format"));
        }
    }
}


BOOST_AUTO_TEST_CASE(rpc_z_sendmany_taddr_to_sapling)
{
    RegtestActivateSapling();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->HaveMnemonicSeed()) {
        pwalletMain->GenerateNewSeed();
    }

    UniValue retValue;

    KeyIO keyIO(Params());
    // add keys manually
    auto taddr = pwalletMain->GenerateNewKey(true).GetID();
    std::string taddr1 = keyIO.EncodeDestination(taddr);
    auto pa = pwalletMain->GenerateNewLegacySaplingZKey();

    const Consensus::Params& consensusParams = Params().GetConsensus();
    retValue = CallRPC("getblockcount");
    int nextBlockHeight = retValue.get_int() + 1;

    // Add a fake transaction to the wallet
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(consensusParams, nextBlockHeight);
    CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ToByteVector(taddr) << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout.push_back(CTxOut(5 * COIN, scriptPubKey));
    CWalletTx wtx(pwalletMain, mtx);
    pwalletMain->LoadWalletTx(wtx);

    // Fake-mine the transaction
    BOOST_CHECK_EQUAL(0, chainActive.Height());
    CBlock block;
    block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    fakeIndex.nHeight = 1;
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    BOOST_CHECK(chainActive.Contains(&fakeIndex));
    BOOST_CHECK_EQUAL(1, chainActive.Height());
    wtx.SetMerkleBranch(block);
    pwalletMain->LoadWalletTx(wtx);

    // Context that z_sendmany requires
    auto builder = TransactionBuilder(consensusParams, nextBlockHeight, std::nullopt, pwalletMain);
    mtx = CreateNewContextualCMutableTransaction(consensusParams, nextBlockHeight);

    auto selector = pwalletMain->ZTXOSelectorForAddress(taddr, true, false).value();
    std::vector<SendManyRecipient> recipients = { SendManyRecipient(std::nullopt, pa, 1*COIN, "ABCD") };
    std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_sendmany(std::move(builder), selector, recipients, 0));
    std::shared_ptr<AsyncRPCOperation_sendmany> ptr = std::dynamic_pointer_cast<AsyncRPCOperation_sendmany> (operation);

    // Enable test mode so tx is not sent
    static_cast<AsyncRPCOperation_sendmany *>(operation.get())->testmode = true;

    // Generate the Sapling shielding transaction
    operation->main();
    BOOST_CHECK(operation->isSuccess());

    // Get the transaction
    auto result = operation->getResult();
    BOOST_ASSERT(result.isObject());
    auto hexTx = result["hex"].getValStr();
    CDataStream ss(ParseHex(hexTx), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    ss >> tx;
    BOOST_ASSERT(!tx.vShieldedOutput.empty());

    // We shouldn't be able to decrypt with the empty ovk
    BOOST_CHECK(!AttemptSaplingOutDecryption(
        tx.vShieldedOutput[0].outCiphertext,
        uint256(),
        tx.vShieldedOutput[0].cv,
        tx.vShieldedOutput[0].cmu,
        tx.vShieldedOutput[0].ephemeralKey));

    auto accountKey = pwalletMain->GetLegacyAccountKey().ToAccountPubKey();
    auto ovks = accountKey.GetOVKsForShielding();
    // We should not be able to decrypt with the internal change OVK for shielding
    BOOST_CHECK(!AttemptSaplingOutDecryption(
        tx.vShieldedOutput[0].outCiphertext,
        ovks.first,
        tx.vShieldedOutput[0].cv,
        tx.vShieldedOutput[0].cmu,
        tx.vShieldedOutput[0].ephemeralKey));
    // We should be able to decrypt with the external OVK for shielding
    BOOST_CHECK(AttemptSaplingOutDecryption(
        tx.vShieldedOutput[0].outCiphertext,
        ovks.second,
        tx.vShieldedOutput[0].cv,
        tx.vShieldedOutput[0].cmu,
        tx.vShieldedOutput[0].ephemeralKey));

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);
    mapArgs.erase("-developersapling");
    mapArgs.erase("-experimentalfeatures");

    // Revert to default
    RegtestDeactivateSapling();
}


/*
 * This test covers storing encrypted zkeys in the wallet.
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_encrypted_wallet_zkeys)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue retValue;
    int n = 100;

    // wallet should currently be empty
    std::set<libzcash::SproutPaymentAddress> addrs;
    pwalletMain->GetSproutPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==0);

    // create keys
    for (int i = 0; i < n; i++) {
        CallRPC("z_getnewaddress sprout");
    }

    // Verify we can list the keys imported
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listaddresses"));
    UniValue arr = retValue.get_array();
    BOOST_CHECK(arr.size() == n);

    // Verify that the wallet encryption RPC is disabled
    BOOST_CHECK_THROW(CallRPC("encryptwallet passphrase"), runtime_error);

    // Encrypt the wallet (we can't call RPC encryptwallet as that shuts down node)
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = "hello";

    PushCurrentDirectory push_dir(GetArg("-datadir","/tmp/thisshouldnothappen"));
    BOOST_CHECK(pwalletMain->EncryptWallet(strWalletPass));

    // Verify we can still list the keys imported
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listaddresses"));
    arr = retValue.get_array();
    BOOST_CHECK(arr.size() == n);

    // Try to add a new key, but we can't as the wallet is locked
    BOOST_CHECK_THROW(CallRPC("z_getnewaddress sprout"), runtime_error);

    // We can't call RPC walletpassphrase as that invokes RPCRunLater which breaks tests.
    // So we manually unlock.
    BOOST_CHECK(pwalletMain->Unlock(strWalletPass));

    // Now add a key
    BOOST_CHECK_NO_THROW(CallRPC("z_getnewaddress sprout"));

    // Verify the key has been added
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listaddresses"));
    arr = retValue.get_array();
    BOOST_CHECK(arr.size() == n+1);

    // We can't simulate over RPC the wallet closing and being reloaded
    // but there are tests for this in gtest.
}

BOOST_AUTO_TEST_CASE(rpc_wallet_encrypted_wallet_sapzkeys)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue retValue;
    int n = 100;

    if(!pwalletMain->HaveMnemonicSeed())
    {
        pwalletMain->GenerateNewSeed();
    }

    // wallet should currently be empty
    std::set<libzcash::SaplingPaymentAddress> addrs;
    pwalletMain->GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==0);

    // create keys
    for (int i = 0; i < n; i++) {
        CallRPC("z_getnewaddress sapling");
    }

    // Verify we can list the keys imported
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listaddresses"));
    UniValue arr = retValue.get_array();
    BOOST_CHECK(arr.size() == n);

    // Verify that the wallet encryption RPC is disabled
    BOOST_CHECK_THROW(CallRPC("encryptwallet passphrase"), runtime_error);

    // Encrypt the wallet (we can't call RPC encryptwallet as that shuts down node)
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = "hello";

    PushCurrentDirectory push_dir(GetArg("-datadir","/tmp/thisshouldnothappen"));
    BOOST_CHECK(pwalletMain->EncryptWallet(strWalletPass));

    // Verify we can still list the keys imported
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listaddresses"));
    arr = retValue.get_array();
    BOOST_CHECK(arr.size() == n);

    // Try to add a new key, but we can't as the wallet is locked
    BOOST_CHECK_THROW(CallRPC("z_getnewaddress sapling"), runtime_error);

    // We can't call RPC walletpassphrase as that invokes RPCRunLater which breaks tests.
    // So we manually unlock.
    BOOST_CHECK(pwalletMain->Unlock(strWalletPass));

    // Now add a key
    BOOST_CHECK_NO_THROW(CallRPC("z_getnewaddress sapling"));

    // Verify the key has been added
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listaddresses"));
    arr = retValue.get_array();
    BOOST_CHECK_EQUAL(arr.size(), n+1);

    // We can't simulate over RPC the wallet closing and being reloaded
    // but there are tests for this in gtest.
}


BOOST_AUTO_TEST_CASE(rpc_z_listunspent_parameters)
{
    SelectParams(CBaseChainParams::TESTNET);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue retValue;

    // too many args
    BOOST_CHECK_THROW(CallRPC("z_listunspent 1 2 3 4 5"), runtime_error);

    // minconf must be >= 0
    BOOST_CHECK_THROW(CallRPC("z_listunspent -1"), runtime_error);

    // maxconf must be > minconf
    BOOST_CHECK_THROW(CallRPC("z_listunspent 2 1"), runtime_error);

    // maxconf must not be out of range
    BOOST_CHECK_THROW(CallRPC("z_listunspent 1 9999999999"), runtime_error);

    // must be an array of addresses
    BOOST_CHECK_THROW(CallRPC("z_listunspent 1 999 false ztjiDe569DPNbyTE6TSdJTaSDhoXEHLGvYoUnBU1wfVNU52TEyT6berYtySkd21njAeEoh8fFJUT42kua9r8EnhBaEKqCpP"), runtime_error);

    // address must be string
    BOOST_CHECK_THROW(CallRPC("z_listunspent 1 999 false [123456]"), runtime_error);

    // no spending key
    BOOST_CHECK_THROW(CallRPC("z_listunspent 1 999 false [\"ztjiDe569DPNbyTE6TSdJTaSDhoXEHLGvYoUnBU1wfVNU52TEyT6berYtySkd21njAeEoh8fFJUT42kua9r8EnhBaEKqCpP\"]"), runtime_error);

    // allow watch only
    BOOST_CHECK_NO_THROW(CallRPC("z_listunspent 1 999 true [\"ztjiDe569DPNbyTE6TSdJTaSDhoXEHLGvYoUnBU1wfVNU52TEyT6berYtySkd21njAeEoh8fFJUT42kua9r8EnhBaEKqCpP\"]"));

    // wrong network, mainnet instead of testnet
    BOOST_CHECK_THROW(CallRPC("z_listunspent 1 999 true [\"zcMuhvq8sEkHALuSU2i4NbNQxshSAYrpCExec45ZjtivYPbuiFPwk6WHy4SvsbeZ4siy1WheuRGjtaJmoD1J8bFqNXhsG6U\"]"), runtime_error);

    // create shielded address so we have the spending key
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_getnewaddress sprout"));
    std::string myzaddr = retValue.get_str();

    // return empty array for this address
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listunspent 1 999 false [\"" + myzaddr + "\"]"));
    UniValue arr = retValue.get_array();
    BOOST_CHECK_EQUAL(0, arr.size());

    // duplicate address error
    BOOST_CHECK_THROW(CallRPC("z_listunspent 1 999 false [\"" + myzaddr + "\", \"" + myzaddr + "\"]"), runtime_error);
}


BOOST_AUTO_TEST_CASE(rpc_z_shieldcoinbase_parameters)
{
    SelectParams(CBaseChainParams::TESTNET);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    BOOST_CHECK_THROW(CallRPC("z_shieldcoinbase"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_shieldcoinbase toofewargs"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_shieldcoinbase too many args shown here"), runtime_error);

    // bad from address
    BOOST_CHECK_THROW(CallRPC("z_shieldcoinbase "
            "INVALIDtmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ tnpoQJVnYBZZqkFadj2bJJLThNCxbADGB5gSGeYTAGGrT5tejsxY9Zc1BtY8nnHmZkB"), runtime_error);

    // bad from address
    BOOST_CHECK_THROW(CallRPC("z_shieldcoinbase "
    "** tnpoQJVnYBZZqkFadj2bJJLThNCxbADGB5gSGeYTAGGrT5tejsxY9Zc1BtY8nnHmZkB"), runtime_error);

    // bad to address
    BOOST_CHECK_THROW(CallRPC("z_shieldcoinbase "
    "tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ INVALIDtnpoQJVnYBZZqkFadj2bJJLThNCxbADGB5gSGeYTAGGrT5tejsxY9Zc1BtY8nnHmZkB"), runtime_error);

    // invalid fee amount, cannot be negative
    BOOST_CHECK_THROW(CallRPC("z_shieldcoinbase "
            "tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ "
            "tnpoQJVnYBZZqkFadj2bJJLThNCxbADGB5gSGeYTAGGrT5tejsxY9Zc1BtY8nnHmZkB "
            "-0.00001"
            ), runtime_error);

    // invalid fee amount, bigger than MAX_MONEY
    BOOST_CHECK_THROW(CallRPC("z_shieldcoinbase "
            "tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ "
            "tnpoQJVnYBZZqkFadj2bJJLThNCxbADGB5gSGeYTAGGrT5tejsxY9Zc1BtY8nnHmZkB "
            "21000001"
            ), runtime_error);

    // invalid limit, must be at least 0
    BOOST_CHECK_THROW(CallRPC("z_shieldcoinbase "
    "tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ "
    "tnpoQJVnYBZZqkFadj2bJJLThNCxbADGB5gSGeYTAGGrT5tejsxY9Zc1BtY8nnHmZkB "
    "100 -1"
    ), runtime_error);

    // Mutable tx containing contextual information we need to build tx
    UniValue retValue = CallRPC("getblockcount");
    int nHeight = retValue.get_int();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), nHeight + 1);
    if (mtx.nVersion == 1) {
        mtx.nVersion = 2;
    }

    // Test constructor of AsyncRPCOperation_shieldcoinbase
    KeyIO keyIO(Params());
    auto testnetzaddr = std::get<libzcash::SproutPaymentAddress>(keyIO.DecodePaymentAddress("ztjiDe569DPNbyTE6TSdJTaSDhoXEHLGvYoUnBU1wfVNU52TEyT6berYtySkd21njAeEoh8fFJUT42kua9r8EnhBaEKqCpP").value());

    try {
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_shieldcoinbase(TransactionBuilder(), mtx, {}, testnetzaddr, -1 ));
    } catch (const UniValue& objError) {
        BOOST_CHECK( find_error(objError, "Fee is out of range"));
    }

    try {
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_shieldcoinbase(TransactionBuilder(), mtx, {}, testnetzaddr, 1));
    } catch (const UniValue& objError) {
        BOOST_CHECK( find_error(objError, "Empty inputs"));
    }
}


BOOST_AUTO_TEST_CASE(rpc_z_shieldcoinbase_internals)
{
    SelectParams(CBaseChainParams::TESTNET);
    const Consensus::Params& consensusParams = Params().GetConsensus();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Mutable tx containing contextual information we need to build tx
    // We removed the ability to create pre-Sapling Sprout proofs, so we can
    // only create Sapling-onwards transactions.
    int nHeight = consensusParams.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight;
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(consensusParams, nHeight + 1);

    // Add keys manually
    auto pa = pwalletMain->GenerateNewSproutZKey();

    // Insufficient funds
    {
        std::vector<ShieldCoinbaseUTXO> inputs = { ShieldCoinbaseUTXO{uint256(),0,0} };
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_shieldcoinbase(TransactionBuilder(), mtx, inputs, pa) );
        operation->main();
        BOOST_CHECK(operation->isFailed());
        std::string msg = operation->getErrorMessage();
        BOOST_CHECK( msg.find("Insufficient coinbase funds") != string::npos);
    }

    // Test the perform_joinsplit methods.
    {
        // Dummy input so the operation object can be instantiated.
        std::vector<ShieldCoinbaseUTXO> inputs = { ShieldCoinbaseUTXO{uint256(),0,100000} };
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_shieldcoinbase(TransactionBuilder(), mtx, inputs, pa) );
        std::shared_ptr<AsyncRPCOperation_shieldcoinbase> ptr = std::dynamic_pointer_cast<AsyncRPCOperation_shieldcoinbase> (operation);
        TEST_FRIEND_AsyncRPCOperation_shieldcoinbase proxy(ptr);
        static_cast<AsyncRPCOperation_shieldcoinbase *>(operation.get())->testmode = true;

        ShieldCoinbaseJSInfo info;
        info.vjsin.push_back(JSInput());
        info.vjsin.push_back(JSInput());
        info.vjsin.push_back(JSInput());
        try {
            proxy.perform_joinsplit(info);
        } catch (const std::runtime_error & e) {
            BOOST_CHECK( string(e.what()).find("unsupported joinsplit input")!= string::npos);
        }

        info.vjsin.clear();
        try {
            proxy.perform_joinsplit(info);
        } catch (const std::runtime_error & e) {
            BOOST_CHECK( string(e.what()).find("error verifying joinsplit")!= string::npos);
        }
    }

}

BOOST_AUTO_TEST_CASE(rpc_z_mergetoaddress_parameters)
{
    SelectParams(CBaseChainParams::TESTNET);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    BOOST_CHECK_THROW(CallRPC("z_mergetoaddress"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_mergetoaddress toofewargs"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_mergetoaddress just too many args present for this method"), runtime_error);

    std::string taddr1 = "tmRr6yJonqGK23UVhrKuyvTpF8qxQQjKigJ";
    std::string taddr2 = "tmYmhvdKqEte49iohoB9utgL1kPbGgWSdNc";
    std::string aSproutAddr = "ztVtBC7vJFXPsZC8S3hXRu51rZysoJkSe6r1t9wk56bELrV9xTK6dx5TgSCH6RTw1dRD7HuApmcY1nhuQW9QfvE4MQXRRYU";
    std::string aSaplingAddr = "ztestsapling19rnyu293v44f0kvtmszhx35lpdug574twc0lwyf4s7w0umtkrdq5nfcauxrxcyfmh3m7slemqsj";

    CheckRPCThrows("z_mergetoaddress [] " + taddr1,
        "Invalid parameter, fromaddresses array is empty.");

    // bad from address
    CheckRPCThrows("z_mergetoaddress [\"INVALID" + taddr1 + "\"] " + taddr2,
        "Unknown address format: INVALID" + taddr1);

    // bad from address
    CheckRPCThrows("z_mergetoaddress ** " + taddr2,
        "Error parsing JSON:**");

    // bad from address
    CheckRPCThrows("z_mergetoaddress [\"**\"] " + taddr2,
        "Unknown address format: **");

    // bad from address
    CheckRPCThrows("z_mergetoaddress " + taddr1 + " " + taddr2,
        "Error parsing JSON:" + taddr1);

    // bad from address
    CheckRPCThrows("z_mergetoaddress [" + taddr1 + "] " + taddr2,
        "Error parsing JSON:[" + taddr1 + "]");

    // bad to address
    CheckRPCThrows("z_mergetoaddress [\"" + taddr1 + "\"] INVALID" + taddr2,
        "Invalid parameter, unknown address format: INVALID" + taddr2);

    // duplicate address
    CheckRPCThrows("z_mergetoaddress [\"" + taddr1 + "\",\"" + taddr1 + "\"] " + taddr2,
        "Invalid parameter, duplicated address: " + taddr1);

    // invalid fee amount, cannot be negative
    CheckRPCThrows("z_mergetoaddress [\"" + taddr1 + "\"] " + taddr2 + " -0.00001",
        "Amount out of range");

    // invalid fee amount, bigger than MAX_MONEY
    CheckRPCThrows("z_mergetoaddress [\"" + taddr1 + "\"] "  + taddr2 + " 21000001",
        "Amount out of range");

    // invalid transparent limit, must be at least 0
    CheckRPCThrows("z_mergetoaddress [\"" + taddr1 + "\"] " + taddr2 + " 0.00001 -1",
        "Limit on maximum number of UTXOs cannot be negative");

    // invalid shielded limit, must be at least 0
    CheckRPCThrows("z_mergetoaddress [\"" + taddr1 + "\"] " + taddr2 + " 0.00001 100 -1",
        "Limit on maximum number of notes cannot be negative");

    CheckRPCThrows("z_mergetoaddress [\"ANY_TADDR\",\"" + taddr1 + "\"] " + taddr2,
        "Cannot specify specific taddrs when using \"ANY_TADDR\"");

    CheckRPCThrows("z_mergetoaddress [\"ANY_SPROUT\",\"" + aSproutAddr + "\"] " + taddr2,
        "Cannot specify specific zaddrs when using \"ANY_SPROUT\" or \"ANY_SAPLING\"");

    CheckRPCThrows("z_mergetoaddress [\"ANY_SAPLING\",\"" + aSaplingAddr + "\"] " + taddr2,
        "Cannot specify specific zaddrs when using \"ANY_SPROUT\" or \"ANY_SAPLING\"");

    // memo bigger than allowed length of ZC_MEMO_SIZE
    std::vector<char> v (2 * (ZC_MEMO_SIZE+1));     // x2 for hexadecimal string format
    std::fill(v.begin(),v.end(), 'A');
    std::string badmemo(v.begin(), v.end());
    CheckRPCThrows("z_mergetoaddress [\"" + taddr1 + "\"] " + aSproutAddr + " 0.00001 100 100 " + badmemo,
        "Invalid parameter, size of memo is larger than maximum allowed 512");

    // Mutable tx containing contextual information we need to build tx
    UniValue retValue = CallRPC("getblockcount");
    int nHeight = retValue.get_int();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), nHeight + 1);

    // Test constructor of AsyncRPCOperation_mergetoaddress
    KeyIO keyIO(Params());
    MergeToAddressRecipient testnetzaddr(
        keyIO.DecodePaymentAddress("ztjiDe569DPNbyTE6TSdJTaSDhoXEHLGvYoUnBU1wfVNU52TEyT6berYtySkd21njAeEoh8fFJUT42kua9r8EnhBaEKqCpP").value(),
        "testnet memo");

    try {
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_mergetoaddress(std::nullopt,  mtx, {}, {}, {}, testnetzaddr, -1 ));
        BOOST_FAIL("Should have caused an error");
    } catch (const UniValue& objError) {
        BOOST_CHECK( find_error(objError, "Fee is out of range"));
    }

    try {
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_mergetoaddress(std::nullopt, mtx, {}, {}, {}, testnetzaddr, 1));
        BOOST_FAIL("Should have caused an error");
    } catch (const UniValue& objError) {
        BOOST_CHECK( find_error(objError, "No inputs"));
    }

    std::vector<MergeToAddressInputUTXO> inputs = { MergeToAddressInputUTXO{ COutPoint{uint256(), 0}, 0, CScript()} };

    std::vector<MergeToAddressInputSproutNote> sproutNoteInputs =
        {MergeToAddressInputSproutNote{JSOutPoint(), SproutNote(), 0, SproutSpendingKey()}};
    std::vector<MergeToAddressInputSaplingNote> saplingNoteInputs =
        {MergeToAddressInputSaplingNote{SaplingOutPoint(), SaplingNote({}, uint256(), 0, uint256(), Zip212Enabled::BeforeZip212), 0, SaplingExpandedSpendingKey()}};

    // Sprout and Sapling inputs -> throw
    try {
        auto operation = new AsyncRPCOperation_mergetoaddress(std::nullopt, mtx, inputs, sproutNoteInputs, saplingNoteInputs, testnetzaddr, 1);
        BOOST_FAIL("Should have caused an error");
    } catch (const UniValue& objError) {
        BOOST_CHECK(find_error(objError, "Cannot send from both Sprout and Sapling addresses using z_mergetoaddress"));
    }
    // Sprout inputs and TransactionBuilder -> throw
    try {
        auto operation = new AsyncRPCOperation_mergetoaddress(TransactionBuilder(), mtx, inputs, sproutNoteInputs, {}, testnetzaddr, 1);
        BOOST_FAIL("Should have caused an error");
    } catch (const UniValue& objError) {
        BOOST_CHECK(find_error(objError, "Sprout notes are not supported by the TransactionBuilder"));
    }
}


// TODO: test private methods
BOOST_AUTO_TEST_CASE(rpc_z_mergetoaddress_internals)
{
    SelectParams(CBaseChainParams::TESTNET);
    const Consensus::Params& consensusParams = Params().GetConsensus();
    KeyIO keyIO(Params());

    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue retValue;

    // Mutable tx containing contextual information we need to build tx
    // We removed the ability to create pre-Sapling Sprout proofs, so we can
    // only create Sapling-onwards transactions.
    int nHeight = consensusParams.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight;
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(consensusParams, nHeight + 1);

    // Add keys manually
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getnewaddress"));
    MergeToAddressRecipient taddr1(keyIO.DecodePaymentAddress(retValue.get_str()).value(), "");
    auto pa = pwalletMain->GenerateNewSproutZKey();
    MergeToAddressRecipient zaddr1(pa, "DEADBEEF");

    // Insufficient funds
    {
        std::vector<MergeToAddressInputUTXO> inputs = { MergeToAddressInputUTXO{COutPoint{uint256(),0},0, CScript()} };
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_mergetoaddress(std::nullopt, mtx, inputs, {}, {}, zaddr1) );
        operation->main();
        BOOST_CHECK(operation->isFailed());
        std::string msg = operation->getErrorMessage();
        BOOST_CHECK( msg.find("Insufficient funds, have 0.00 and miners fee is 0.00001") != string::npos);
    }

    // get_memo_from_hex_string())
    {
        std::vector<MergeToAddressInputUTXO> inputs = { MergeToAddressInputUTXO{COutPoint{uint256(),0},100000, CScript()} };
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_mergetoaddress(std::nullopt, mtx, inputs, {}, {}, zaddr1) );
        std::shared_ptr<AsyncRPCOperation_mergetoaddress> ptr = std::dynamic_pointer_cast<AsyncRPCOperation_mergetoaddress> (operation);
        TEST_FRIEND_AsyncRPCOperation_mergetoaddress proxy(ptr);

        std::string memo = "DEADBEEF";
        std::array<unsigned char, ZC_MEMO_SIZE> array = proxy.get_memo_from_hex_string(memo);
        BOOST_CHECK_EQUAL(array[0], 0xDE);
        BOOST_CHECK_EQUAL(array[1], 0xAD);
        BOOST_CHECK_EQUAL(array[2], 0xBE);
        BOOST_CHECK_EQUAL(array[3], 0xEF);
        for (int i=4; i<ZC_MEMO_SIZE; i++) {
            BOOST_CHECK_EQUAL(array[i], 0x00);  // zero padding
        }

        // memo is longer than allowed
        std::vector<char> v (2 * (ZC_MEMO_SIZE+1));
        std::fill(v.begin(),v.end(), 'A');
        std::string bigmemo(v.begin(), v.end());

        try {
            proxy.get_memo_from_hex_string(bigmemo);
            BOOST_FAIL("Should have caused an error");
        } catch (const UniValue& objError) {
            BOOST_CHECK( find_error(objError, "too big"));
        }

        // invalid hexadecimal string
        std::fill(v.begin(),v.end(), '@'); // not a hex character
        std::string badmemo(v.begin(), v.end());

        try {
            proxy.get_memo_from_hex_string(badmemo);
            BOOST_FAIL("Should have caused an error");
        } catch (const UniValue& objError) {
            BOOST_CHECK( find_error(objError, "hexadecimal format"));
        }

        // odd length hexadecimal string
        std::fill(v.begin(),v.end(), 'A');
        v.resize(v.size() - 1);
        assert(v.size() %2 == 1); // odd length
        std::string oddmemo(v.begin(), v.end());
        try {
            proxy.get_memo_from_hex_string(oddmemo);
            BOOST_FAIL("Should have caused an error");
        } catch (const UniValue& objError) {
            BOOST_CHECK( find_error(objError, "hexadecimal format"));
        }
    }

    // Test the perform_joinsplit methods.
    {
        // Dummy input so the operation object can be instantiated.
        std::vector<MergeToAddressInputUTXO> inputs = { MergeToAddressInputUTXO{COutPoint{uint256(),0},100000, CScript()} };
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_mergetoaddress(std::nullopt, mtx, inputs, {}, {}, zaddr1) );
        std::shared_ptr<AsyncRPCOperation_mergetoaddress> ptr = std::dynamic_pointer_cast<AsyncRPCOperation_mergetoaddress> (operation);
        TEST_FRIEND_AsyncRPCOperation_mergetoaddress proxy(ptr);

        // Enable test mode so tx is not sent and proofs are not generated
        static_cast<AsyncRPCOperation_mergetoaddress *>(operation.get())->testmode = true;

        MergeToAddressJSInfo info;
        std::vector<std::optional < SproutWitness>> witnesses;
        uint256 anchor;
        try {
            proxy.perform_joinsplit(info, witnesses, anchor);
            BOOST_FAIL("Should have caused an error");
        } catch (const std::runtime_error & e) {
            BOOST_CHECK( string(e.what()).find("anchor is null")!= string::npos);
        }

        try {
            std::vector<JSOutPoint> v;
            proxy.perform_joinsplit(info, v);
            BOOST_FAIL("Should have caused an error");
        } catch (const std::runtime_error & e) {
            BOOST_CHECK( string(e.what()).find("anchor is null")!= string::npos);
        }

        info.notes.push_back(SproutNote());
        try {
            proxy.perform_joinsplit(info);
            BOOST_FAIL("Should have caused an error");
        } catch (const std::runtime_error & e) {
            BOOST_CHECK( string(e.what()).find("number of notes")!= string::npos);
        }

        info.notes.clear();
        info.vjsin.push_back(JSInput());
        info.vjsin.push_back(JSInput());
        info.vjsin.push_back(JSInput());
        try {
            proxy.perform_joinsplit(info);
            BOOST_FAIL("Should have caused an error");
        } catch (const std::runtime_error & e) {
            BOOST_CHECK( string(e.what()).find("unsupported joinsplit input")!= string::npos);
        }

        info.vjsin.clear();
        try {
            proxy.perform_joinsplit(info);
            BOOST_FAIL("Should have caused an error");
        } catch (const std::runtime_error & e) {
            BOOST_CHECK( string(e.what()).find("error verifying joinsplit")!= string::npos);
        }
    }
}

void TestWTxStatus(const Consensus::Params consensusParams, const int delta) {

    auto AddTrx = [&consensusParams]() {
        auto taddr = pwalletMain->GenerateNewKey(true).GetID();
        CMutableTransaction mtx = CreateNewContextualCMutableTransaction(consensusParams, 1);
        CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ToByteVector(taddr) << OP_EQUALVERIFY << OP_CHECKSIG;
        mtx.vout.push_back(CTxOut(5 * COIN, scriptPubKey));
        CWalletTx wtx(pwalletMain, mtx);
        pwalletMain->LoadWalletTx(wtx);
        return wtx;
    };

    vector<uint256> hashes;
    CWalletTx wtx;
    auto FakeMine = [&](const int height, bool has_trx) {
        BOOST_CHECK_EQUAL(height, chainActive.Height());
        CBlock block;
        if (has_trx) block.vtx.push_back(wtx);
        block.hashMerkleRoot = block.BuildMerkleTree();
        auto blockHash = block.GetHash();
        CBlockIndex fakeIndex {block};
        fakeIndex.nHeight = height+1;
        mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
        chainActive.SetTip(&fakeIndex);
        BOOST_CHECK(chainActive.Contains(&fakeIndex));
        BOOST_CHECK_EQUAL(height+1, chainActive.Height());

        if (has_trx) {
            wtx.SetMerkleBranch(block);
            pwalletMain->LoadWalletTx(wtx);
        }

        hashes.push_back(blockHash);
        UniValue retValue = CallRPC("gettransaction " + wtx.GetHash().GetHex());
        return retValue.get_obj();
    };

    // Add a transaction to the wallet
    wtx = AddTrx();

    // Mine blocks but never include the transaction, check status of wallet trx
    for(int i=0; i<=delta + 1; i++) {
        auto retObj = FakeMine(i, false);

        BOOST_CHECK_EQUAL(find_value(retObj, "confirmations").get_real(), -1);
        auto status = find_value(retObj, "status").get_str();
        if (i >= delta - TX_EXPIRING_SOON_THRESHOLD && i <= delta)
            BOOST_CHECK_EQUAL(status, "expiringsoon");
        else if (i >= delta - TX_EXPIRING_SOON_THRESHOLD)
            BOOST_CHECK_EQUAL(status, "expired");
        else
            BOOST_CHECK_EQUAL(status, "waiting");
    }

    // Now mine including the transaction, check status
    auto retObj = FakeMine(delta + 2, true);

    BOOST_CHECK_EQUAL(find_value(retObj, "confirmations").get_real(), 1);
    BOOST_CHECK_EQUAL(find_value(retObj, "status").get_str(), "mined");

    // Cleanup
    chainActive.SetTip(NULL);
    for (auto hash : hashes)
        mapBlockIndex.erase(hash);
}

BOOST_AUTO_TEST_CASE(rpc_gettransaction_status_sapling)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);

    TestWTxStatus(RegtestActivateSapling(), DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA);

    RegtestDeactivateSapling();
}

BOOST_AUTO_TEST_CASE(rpc_gettransaction_status_blossom)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);
    auto params = RegtestActivateBlossom(true).GetConsensus();

    TestWTxStatus(params, DEFAULT_POST_BLOSSOM_TX_EXPIRY_DELTA);

    RegtestDeactivateBlossom();
}


BOOST_AUTO_TEST_SUITE_END()
