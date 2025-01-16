// Copyright (c) 2012-2013 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "rpc/server.h"
#include "rpc/client.h"

#include "experimental_features.h"
#include "key_io.h"
#include "main.h"
#include "netbase.h"
#include "util/strencodings.h"

#include "test/test_bitcoin.h"
#include "test/test_util.h"

#include <boost/test/unit_test.hpp>

#include <univalue.h>

using namespace std;

BOOST_FIXTURE_TEST_SUITE(rpc_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(rpc_rawparams)
{
    // Test raw transaction API argument handling
    UniValue r;

    BOOST_CHECK_THROW(CallRPC("getrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction not_hex"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed not_int"), runtime_error);

    BOOST_CHECK_THROW(CallRPC("createrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction null null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction not_array"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] []"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction {} {}"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [] {}"));
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] {} extra"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [] {} 0"));
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] {} 0 0"), runtime_error); // Overwinter is not active

    BOOST_CHECK_THROW(CallRPC("decoderawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("decoderawtransaction null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("decoderawtransaction DEADBEEF"), runtime_error);
    string rawtx = "0100000001a15d57094aa7a21a28cb20b59aab8fc7d1149a3bdbcddba9c622e4f5f6a99ece010000006c493046022100f93bb0e7d8db7bd46e40132d1f8242026e045f03a0efe71bbb8e3f475e970d790221009337cd7f1f929f00cc6ff01f03729b069a7c21b59b1736ddfee5db5946c5da8c0121033b9b137ee87d5a812d6f506efdd37f0affa7ffc310711c06c7f3e097c9447c52ffffffff0100e1f505000000001976a9140389035a9225b3839e2bbf32d826a1e222031fd888ac00000000";
    BOOST_CHECK_NO_THROW(r = CallRPC(string("decoderawtransaction ")+rawtx));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "size").get_int(), 193);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "version").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "locktime").get_int(), 0);
    BOOST_CHECK_THROW(r = CallRPC(string("decoderawtransaction ")+rawtx+" extra"), runtime_error);

    BOOST_CHECK_THROW(CallRPC("signrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("signrawtransaction null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("signrawtransaction ff00"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC(string("signrawtransaction ")+rawtx));
    BOOST_CHECK_NO_THROW(CallRPC(string("signrawtransaction ")+rawtx+" null null NONE|ANYONECANPAY"));
    BOOST_CHECK_NO_THROW(CallRPC(string("signrawtransaction ")+rawtx+" [] [] NONE|ANYONECANPAY"));
    BOOST_CHECK_THROW(CallRPC(string("signrawtransaction ")+rawtx+" null null badenum"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC(string("signrawtransaction ")+rawtx+" [] [] NONE|ANYONECANPAY 5ba81b19"));
    BOOST_CHECK_THROW(CallRPC(string("signrawtransaction ")+rawtx+" [] [] ALL NONE|ANYONECANPAY 123abc"), runtime_error);

    // Only check failure cases for sendrawtransaction, there's no network to send to...
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction DEADBEEF"), runtime_error);
    BOOST_CHECK_THROW(CallRPC(string("sendrawtransaction ")+rawtx+" extra"), runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_rawsign)
{
    UniValue r;
    // input is a 1-of-2 multisig (so is output):
    string prevout =
      "[{\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\","
      "\"vout\":1,\"scriptPubKey\":\"a914b10c9df5f7edf436c697f02f1efdba4cf399615187\","
      "\"redeemScript\":\"512103debedc17b3df2badbcdd86d5feb4562b86fe182e5998abd8bcd4f122c6155b1b21027e940bb73ab8732bfdf7f9216ecefca5b94d6df834e77e108f68e66f126044c052ae\"}]";
    r = CallRPC(string("createrawtransaction ")+prevout+" "+
      "{\"t3ahmeUm2LWXPUJPx9QMheGtqTEfdDdgr7p\":11}");
    string notsigned = r.get_str();
    string privkey1 = "\"KzsXybp9jX64P5ekX1KUxRQ79Jht9uzW7LorgwE65i5rWACL6LQe\"";
    string privkey2 = "\"Kyhdf5LuKTRx4ge69ybABsiUAWjVRK4XGxAKk2FQLp2HjGMy87Z4\"";
    r = CallRPC(string("signrawtransaction ")+notsigned+" "+prevout+" "+"[]");
    BOOST_CHECK(find_value(r.get_obj(), "complete").get_bool() == false);
    r = CallRPC(string("signrawtransaction ")+notsigned+" "+prevout+" "+"["+privkey1+","+privkey2+"]");
    BOOST_CHECK(find_value(r.get_obj(), "complete").get_bool() == true);
}

BOOST_AUTO_TEST_CASE(rpc_format_monetary_values)
{
    BOOST_CHECK(ValueFromAmount(0LL).write() == "0.00000000");
    BOOST_CHECK(ValueFromAmount(1LL).write() == "0.00000001");
    BOOST_CHECK(ValueFromAmount(17622195LL).write() == "0.17622195");
    BOOST_CHECK(ValueFromAmount(50000000LL).write() == "0.50000000");
    BOOST_CHECK(ValueFromAmount(89898989LL).write() == "0.89898989");
    BOOST_CHECK(ValueFromAmount(100000000LL).write() == "1.00000000");
    BOOST_CHECK(ValueFromAmount(2099999999999990LL).write() == "20999999.99999990");
    BOOST_CHECK(ValueFromAmount(2099999999999999LL).write() == "20999999.99999999");

    BOOST_CHECK_EQUAL(ValueFromAmount(0).write(), "0.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount((COIN/10000)*123456789).write(), "12345.67890000");
    BOOST_CHECK_EQUAL(ValueFromAmount(-COIN).write(), "-1.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(-COIN/10).write(), "-0.10000000");

    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*100000000).write(), "100000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*10000000).write(), "10000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*1000000).write(), "1000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*100000).write(), "100000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*10000).write(), "10000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*1000).write(), "1000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*100).write(), "100.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN*10).write(), "10.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN).write(), "1.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/10).write(), "0.10000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/100).write(), "0.01000000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/1000).write(), "0.00100000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/10000).write(), "0.00010000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/100000).write(), "0.00001000");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/1000000).write(), "0.00000100");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/10000000).write(), "0.00000010");
    BOOST_CHECK_EQUAL(ValueFromAmount(COIN/100000000).write(), "0.00000001");
}

static UniValue ValueFromString(const std::string &str)
{
    UniValue value;
    BOOST_CHECK(value.setNumStr(str));
    return value;
}

BOOST_AUTO_TEST_CASE(rpc_parse_monetary_values)
{
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("-0.00000001")), UniValue);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0")), 0LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.00000000")), 0LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.00000001")), 1LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.17622195")), 17622195LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.5")), 50000000LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.50000000")), 50000000LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.89898989")), 89898989LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("1.00000000")), 100000000LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("20999999.9999999")), 2099999999999990LL);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("20999999.99999999")), 2099999999999999LL);

    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("1e-8")), COIN/100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.1e-7")), COIN/100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.01e-6")), COIN/100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.0000000000000000000000000000000000000000000000000000000000000000000000000001e+68")), COIN/100000000);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("10000000000000000000000000000000000000000000000000000000000000000e-64")), COIN);
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000e64")), COIN);

    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("1e-9")), UniValue); //should fail
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("0.000000019")), UniValue); //should fail
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.00000001000000")), 1LL); //should pass, cut trailing 0
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("19e-9")), UniValue); //should fail
    BOOST_CHECK_EQUAL(AmountFromValue(ValueFromString("0.19e-6")), 19); //should pass, leading 0 is present

    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("92233720368.54775808")), UniValue); //overflow error
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("1e+11")), UniValue); //overflow error
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("1e11")), UniValue); //overflow error signless
    BOOST_CHECK_THROW(AmountFromValue(ValueFromString("93e+9")), UniValue); //overflow error
}

BOOST_AUTO_TEST_CASE(json_parse_errors)
{
    // Valid
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue("1.0").value().get_real(), 1.0);
    // Valid, with leading or trailing whitespace
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue(" 1.0").value().get_real(), 1.0);
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue("1.0 ").value().get_real(), 1.0);

    BOOST_CHECK(!ParseNonRFCJSONValue(".19e-6").has_value()); //should fail, missing leading 0, therefore invalid JSON
    BOOST_CHECK_EQUAL(AmountFromValue(ParseNonRFCJSONValue("0.00000000000000000000000000000000000001e+30 ").value()), 1);
    // Invalid, initial garbage
    BOOST_CHECK(!ParseNonRFCJSONValue("[1.0").has_value());
    BOOST_CHECK(!ParseNonRFCJSONValue("a1.0").has_value());
    // Invalid, trailing garbage
    BOOST_CHECK(!ParseNonRFCJSONValue("1.0sds").has_value());
    BOOST_CHECK(!ParseNonRFCJSONValue("1.0]").has_value());
    // Bitcoin addresses should fail parsing
    BOOST_CHECK(!ParseNonRFCJSONValue("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W").has_value());
    BOOST_CHECK(!ParseNonRFCJSONValue("3J98t1WpEZ73CNmQviecrnyiWrnqRhWNL").has_value());
}

BOOST_AUTO_TEST_CASE(rpc_ban)
{
    BOOST_CHECK_NO_THROW(CallRPC(string("clearbanned")));

    UniValue r;
    BOOST_CHECK_NO_THROW(r = CallRPC(string("setban 127.0.0.0 add")));
    BOOST_CHECK_THROW(r = CallRPC(string("setban 127.0.0.0:8334")), runtime_error); //portnumber for setban not allowed
    BOOST_CHECK_NO_THROW(r = CallRPC(string("listbanned")));
    UniValue ar = r.get_array();
    UniValue o1 = ar[0].get_obj();
    UniValue adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/32");
    BOOST_CHECK_NO_THROW(CallRPC(string("setban 127.0.0.0 remove")));;
    BOOST_CHECK_NO_THROW(r = CallRPC(string("listbanned")));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0);

    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("setban 127.0.0.0/24 add 9907731200 true")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    UniValue banned_until = find_value(o1, "banned_until");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/24");
    BOOST_CHECK_EQUAL(banned_until.get_int64(), 9907731200); // absolute time check

    BOOST_CHECK_NO_THROW(CallRPC(string("clearbanned")));

    BOOST_CHECK_NO_THROW(r = CallRPC(string("setban 127.0.0.0/24 add 200")));
    BOOST_CHECK_NO_THROW(r = CallRPC(string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    banned_until = find_value(o1, "banned_until");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/24");
    int64_t now = GetTime();
    BOOST_CHECK(banned_until.get_int64() > now);
    BOOST_CHECK(banned_until.get_int64()-now <= 200);

    // must throw an exception because 127.0.0.1 is in already banned subnet range
    BOOST_CHECK_THROW(r = CallRPC(string("setban 127.0.0.1 add")), runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC(string("setban 127.0.0.0/24 remove")));;
    BOOST_CHECK_NO_THROW(r = CallRPC(string("listbanned")));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0);

    BOOST_CHECK_NO_THROW(r = CallRPC(string("setban 127.0.0.0/255.255.0.0 add")));
    BOOST_CHECK_THROW(r = CallRPC(string("setban 127.0.1.1 add")), runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC(string("clearbanned")));
    BOOST_CHECK_NO_THROW(r = CallRPC(string("listbanned")));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0);


    BOOST_CHECK_THROW(r = CallRPC(string("setban test add")), runtime_error); //invalid IP

    //IPv6 tests
    BOOST_CHECK_NO_THROW(r = CallRPC(string("setban FE80:0000:0000:0000:0202:B3FF:FE1E:8329 add")));
    BOOST_CHECK_NO_THROW(r = CallRPC(string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "fe80::202:b3ff:fe1e:8329/128");

    BOOST_CHECK_NO_THROW(CallRPC(string("clearbanned")));
    BOOST_CHECK_NO_THROW(r = CallRPC(string("setban 2001:db8::/ffff:fffc:0:0:0:0:0:0 add")));
    BOOST_CHECK_NO_THROW(r = CallRPC(string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "2001:db8::/30");

    BOOST_CHECK_NO_THROW(CallRPC(string("clearbanned")));
    BOOST_CHECK_NO_THROW(r = CallRPC(string("setban 2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/128 add")));
    BOOST_CHECK_NO_THROW(r = CallRPC(string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/128");
}


BOOST_AUTO_TEST_CASE(rpc_raw_create_overwinter_v3)
{
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

    // Sample regtest address:
    // public: tmHU5HLMu3yS8eoNvbrU1NWeJaGf6jxehru
    // private: cW1G4SxEm5rui2RQtBcSUZrERTVYPtyZXKbSi5MCwBqzbn5kqwbN

    UniValue r;
    std::string prevout =
      "[{\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\","
      "\"vout\":1}]";
    r = CallRPC(string("createrawtransaction ") + prevout + " " +
      "{\"tmHU5HLMu3yS8eoNvbrU1NWeJaGf6jxehru\":11}");
    std::string rawhex = r.get_str();
    BOOST_CHECK_NO_THROW(r = CallRPC(string("decoderawtransaction ") + rawhex));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "overwintered").get_bool(), true);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "version").get_int(), 3);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expiryheight").get_int(), 21);
    BOOST_CHECK_EQUAL(
        ParseHexToUInt32(find_value(r.get_obj(), "versiongroupid").get_str()),
        OVERWINTER_VERSION_GROUP_ID);

    // Sanity check we can deserialize the raw hex
    // 030000807082c40301f393847c97508f24b772281deea475cd3e0f719f321794e5da7cf8587e28ccb40100000000ffffffff0100ab9041000000001976a914550dc92d3ff8d1f0cb6499fddf2fe43b745330cd88ac000000000000000000
    CDataStream ss(ParseHex(rawhex), SER_DISK, PROTOCOL_VERSION);
    CTransaction tx;
    ss >> tx;
    CDataStream ss2(ParseHex(rawhex), SER_DISK, PROTOCOL_VERSION);
    CMutableTransaction mtx;
    ss2 >> mtx;
    BOOST_CHECK_EQUAL(tx.GetHash().GetHex(), CTransaction(mtx).GetHash().GetHex());

    // Revert to default
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}

BOOST_AUTO_TEST_CASE(rpc_getnetworksolps)
{
    BOOST_CHECK_NO_THROW(CallRPC("getnetworksolps"));
    BOOST_CHECK_NO_THROW(CallRPC("getnetworksolps 120"));
    BOOST_CHECK_NO_THROW(CallRPC("getnetworksolps 120 -1"));
}

// Test parameter processing (not functionality).
// These tests also ensure that src/rpc/client.cpp has the correct entries.
BOOST_AUTO_TEST_CASE(rpc_insightexplorer)
{
    CheckRPCThrows("getaddressmempool \"a\"",
        "Error: getaddressmempool is disabled. "
        "Run './" CLI_NAME " help getaddressmempool' for instructions on how to enable this feature.");
    CheckRPCThrows("getaddressutxos \"a\"",
        "Error: getaddressutxos is disabled. "
        "Run './" CLI_NAME " help getaddressutxos' for instructions on how to enable this feature.");
    CheckRPCThrows("getaddressdeltas \"a\"",
        "Error: getaddressdeltas is disabled. "
        "Run './" CLI_NAME " help getaddressdeltas' for instructions on how to enable this feature.");
    CheckRPCThrows("getaddressbalance \"a\"",
        "Error: getaddressbalance is disabled. "
        "Run './" CLI_NAME " help getaddressbalance' for instructions on how to enable this feature.");
    CheckRPCThrows("getaddresstxids \"a\"",
        "Error: getaddresstxids is disabled. "
        "Run './" CLI_NAME " help getaddresstxids' for instructions on how to enable this feature.");
    CheckRPCThrows("getspentinfo {\"a\":1}",
        "Error: getspentinfo is disabled. "
        "Run './" CLI_NAME " help getspentinfo' for instructions on how to enable this feature.");
    CheckRPCThrows("getblockdeltas \"a\"",
        "Error: getblockdeltas is disabled. "
        "Run './" CLI_NAME " help getblockdeltas' for instructions on how to enable this feature.");
    CheckRPCThrows("getblockhashes 0 0",
        "Error: getblockhashes is disabled. "
        "Run './" CLI_NAME " help getblockhashes' for instructions on how to enable this feature.");

    fExperimentalInsightExplorer = true;
    // During startup of the real system, fExperimentalInsightExplorer ("-insightexplorer")
    // automatically enables the next four, but not here, must explicitly enable.
    fAddressIndex = true;
    fSpentIndex = true;
    fTimestampIndex = true;

    // must be a legal mainnet address
    const string addr = "t1T3G72ToPuCDTiCEytrU1VUBRHsNupEBut";
    BOOST_CHECK_NO_THROW(CallRPC("getaddressmempool \"" + addr + "\""));
    BOOST_CHECK_NO_THROW(CallRPC("getaddressmempool {\"addresses\":[\"" + addr + "\"]}"));
    BOOST_CHECK_NO_THROW(CallRPC("getaddressmempool {\"addresses\":[\"" + addr + "\",\"" + addr + "\"]}"));

    BOOST_CHECK_NO_THROW(CallRPC("getaddressutxos {\"addresses\":[],\"chainInfo\":true}"));
    CheckRPCThrows("getaddressutxos {}",
        "Addresses is expected to be an array");
    CheckRPCThrows("getaddressutxos {\"addressesmisspell\":[]}",
        "Addresses is expected to be an array");
    CheckRPCThrows("getaddressutxos {\"addresses\":[],\"chainInfo\":1}",
        "JSON value is not a boolean as expected");

    BOOST_CHECK_NO_THROW(CallRPC("getaddressdeltas {\"addresses\":[]}"));
    CheckRPCThrows("getaddressdeltas {\"addresses\":[],\"start\":-2,\"chainInfo\":true}",
        "Start height must be nonnegative");
    CheckRPCThrows("getaddressdeltas {\"addresses\":[],\"end\":-2,\"chainInfo\":true}",
        "End height must be nonnegative");

    BOOST_CHECK_NO_THROW(CallRPC("getaddressbalance {\"addresses\":[]}"));

    BOOST_CHECK_NO_THROW(CallRPC("getaddresstxids {\"addresses\":[]}"));

    // transaction does not exist:
    CheckRPCThrows("getspentinfo {\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\",\"index\":0}",
        "Unable to get spent info");
    CheckRPCThrows("getspentinfo {\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\"}",
        "Invalid index, must be an integer");
    CheckRPCThrows("getspentinfo {\"txid\":\"hello\",\"index\":0}",
        "txid must be hexadecimal string (not 'hello')");

    // only the mainnet genesis block exists
    BOOST_CHECK_NO_THROW(CallRPC("getblockdeltas \"00040fe8ec8471911baa1db1266ea15dd06b4a8a5c453883c000b031973dce08\""));
    // damage the block hash (change last digit)
    CheckRPCThrows("getblockdeltas \"00040fe8ec8471911baa1db1266ea15dd06b4a8a5c453883c000b031973dce09\"",
        "Block not found");

    BOOST_CHECK_NO_THROW(CallRPC("getblockhashes 1477641360 1477641360"));
    BOOST_CHECK_NO_THROW(CallRPC("getblockhashes 1477641360 1477641360 {\"noOrphans\":true,\"logicalTimes\":true}"));
    // Unfortunately, an unknown or mangled key is ignored
    BOOST_CHECK_NO_THROW(CallRPC("getblockhashes 1477641360 1477641360 {\"AAAnoOrphans\":true,\"logicalTimes\":true}"));
    CheckRPCThrows("getblockhashes 1477641360 1477641360 {\"noOrphans\":true,\"logicalTimes\":1}",
        "JSON value is not a boolean as expected");
    CheckRPCThrows("getblockhashes 1477641360 1477641360 {\"noOrphans\":True,\"logicalTimes\":false}",
        "Error parsing JSON: {\"noOrphans\":True,\"logicalTimes\":false}");

    // revert
    fExperimentalInsightExplorer = false;
    fAddressIndex = false;
    fSpentIndex = false;
    fTimestampIndex = false;
}

BOOST_AUTO_TEST_SUITE_END()
