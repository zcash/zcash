// Copyright (c) 2013-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"
#include "rpcclient.h"

#include "base58.h"
#include "main.h"
#include "wallet/wallet.h"

#include "test/test_bitcoin.h"

#include "zcash/Address.hpp"

#include <fstream>
#include <unordered_set>

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/format.hpp>

using namespace std;
using namespace json_spirit;

extern Array createArgs(int nRequired, const char* address1 = NULL, const char* address2 = NULL);
extern Value CallRPC(string args);

extern CWallet* pwalletMain;

BOOST_FIXTURE_TEST_SUITE(rpc_wallet_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(rpc_addmultisig)
{
    LOCK(pwalletMain->cs_wallet);

    rpcfn_type addmultisig = tableRPC["addmultisigaddress"]->actor;

    // old, 65-byte-long:
    const char address1Hex[] = "0434e3e09f49ea168c5bbf53f877ff4206923858aab7c7e1df25bc263978107c95e35065a27ef6f1b27222db0ec97e0e895eaca603d3ee0d4c060ce3d8a00286c8";
    // new, compressed:
    const char address2Hex[] = "0388c2037017c62240b6b72ac1a2a5f94da790596ebd06177c8572752922165cb4";

    Value v;
    CBitcoinAddress address;
    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(1, address1Hex), false));
    address.SetString(v.get_str());
    BOOST_CHECK(address.IsValid() && address.IsScript());

    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(1, address1Hex, address2Hex), false));
    address.SetString(v.get_str());
    BOOST_CHECK(address.IsValid() && address.IsScript());

    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(2, address1Hex, address2Hex), false));
    address.SetString(v.get_str());
    BOOST_CHECK(address.IsValid() && address.IsScript());

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
    Value r;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CPubKey demoPubkey = pwalletMain->GenerateNewKey();
    CBitcoinAddress demoAddress = CBitcoinAddress(CTxDestination(demoPubkey.GetID()));
    Value retValue;
    string strAccount = "walletDemoAccount";
    string strPurpose = "receive";
    BOOST_CHECK_NO_THROW({ /*Initialize Wallet with an account */
        CWalletDB walletdb(pwalletMain->strWalletFile);
        CAccount account;
        account.vchPubKey = demoPubkey;
        pwalletMain->SetAddressBook(account.vchPubKey.GetID(), strAccount, strPurpose);
        walletdb.WriteAccount(strAccount, account);
    });

    CPubKey setaccountDemoPubkey = pwalletMain->GenerateNewKey();
    CBitcoinAddress setaccountDemoAddress = CBitcoinAddress(CTxDestination(setaccountDemoPubkey.GetID()));

    /*********************************
     * 			setaccount
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("setaccount " + setaccountDemoAddress.ToString() + " nullaccount"));
    /* 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ is not owned by the test wallet. */
    BOOST_CHECK_THROW(CallRPC("setaccount 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ nullaccount"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("setaccount"), runtime_error);
    /* 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4X (33 chars) is an illegal address (should be 34 chars) */
    BOOST_CHECK_THROW(CallRPC("setaccount 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4X nullaccount"), runtime_error);


    /*********************************
     *                  getbalance
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getbalance"));
    BOOST_CHECK_NO_THROW(CallRPC("getbalance " + demoAddress.ToString()));

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
     * 		listreceivedbyaccount
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaccount"));
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaccount 0"));
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaccount not_int"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaccount 0 not_bool"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaccount 0 true"));
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaccount 0 true extra"), runtime_error);

    /*********************************
     *          listsinceblock
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listsinceblock"));

    /*********************************
     *          listtransactions
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions"));
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions " + demoAddress.ToString()));
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions " + demoAddress.ToString() + " 20"));
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions " + demoAddress.ToString() + " 20 0"));
    BOOST_CHECK_THROW(CallRPC("listtransactions " + demoAddress.ToString() + " not_int"), runtime_error);

    /*********************************
     *          listlockunspent
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listlockunspent"));

    /*********************************
     *          listaccounts
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listaccounts"));

    /*********************************
     *          listaddressgroupings
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listaddressgroupings"));

    /*********************************
     * 		getrawchangeaddress
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getrawchangeaddress"));

    /*********************************
     * 		getnewaddress
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getnewaddress"));
    BOOST_CHECK_NO_THROW(CallRPC("getnewaddress getnewaddress_demoaccount"));

    /*********************************
     * 		getaccountaddress
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getaccountaddress \"\""));
    BOOST_CHECK_NO_THROW(CallRPC("getaccountaddress accountThatDoesntExists")); // Should generate a new account
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getaccountaddress " + strAccount));
    BOOST_CHECK(CBitcoinAddress(retValue.get_str()).Get() == demoAddress.Get());

    /*********************************
     * 			getaccount
     *********************************/
    BOOST_CHECK_THROW(CallRPC("getaccount"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("getaccount " + demoAddress.ToString()));

    /*********************************
     * 	signmessage + verifymessage
     *********************************/
    BOOST_CHECK_NO_THROW(retValue = CallRPC("signmessage " + demoAddress.ToString() + " mymessage"));
    BOOST_CHECK_THROW(CallRPC("signmessage"), runtime_error);
    /* Should throw error because this address is not loaded in the wallet */
    BOOST_CHECK_THROW(CallRPC("signmessage 1QFqqMUD55ZV3PJEJZtaKCsQmjLT6JkjvJ mymessage"), runtime_error);

    /* missing arguments */
    BOOST_CHECK_THROW(CallRPC("verifymessage " + demoAddress.ToString()), runtime_error);
    BOOST_CHECK_THROW(CallRPC("verifymessage " + demoAddress.ToString() + " " + retValue.get_str()), runtime_error);
    /* Illegal address */
    BOOST_CHECK_THROW(CallRPC("verifymessage 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4X " + retValue.get_str() + " mymessage"), runtime_error);
    /* wrong address */
    BOOST_CHECK(CallRPC("verifymessage 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ " + retValue.get_str() + " mymessage").get_bool() == false);
    /* Correct address and signature but wrong message */
    BOOST_CHECK(CallRPC("verifymessage " + demoAddress.ToString() + " " + retValue.get_str() + " wrongmessage").get_bool() == false);
    /* Correct address, message and signature*/
    BOOST_CHECK(CallRPC("verifymessage " + demoAddress.ToString() + " " + retValue.get_str() + " mymessage").get_bool() == true);

    /*********************************
     * 		getaddressesbyaccount
     *********************************/
    BOOST_CHECK_THROW(CallRPC("getaddressesbyaccount"), runtime_error);
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getaddressesbyaccount " + strAccount));
    Array arr = retValue.get_array();
    BOOST_CHECK(arr.size() > 0);
    BOOST_CHECK(CBitcoinAddress(arr[0].get_str()).Get() == demoAddress.Get());
}

/*
 * This test covers RPC command z_exportwallet
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_z_exportwallet)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);
    
    // wallet should by empty
    std::set<libzcash::PaymentAddress> addrs;
    pwalletMain->GetPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==0);

    // wallet should have one key
    CZCPaymentAddress paymentAddress = pwalletMain->GenerateNewZKey();
    pwalletMain->GetPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==1);
    
    BOOST_CHECK_THROW(CallRPC("z_exportwallet"), runtime_error);
    
   
    boost::filesystem::path temp = boost::filesystem::temp_directory_path() /
            boost::filesystem::unique_path();
    const std::string path = temp.native();

    BOOST_CHECK_NO_THROW(CallRPC(string("z_exportwallet ") + path));

    auto addr = paymentAddress.Get();
    libzcash::SpendingKey key;
    BOOST_CHECK(pwalletMain->GetSpendingKey(addr, key));

    std::string s1 = paymentAddress.ToString();
    std::string s2 = CZCSpendingKey(key).ToString();
    
    // There's no way to really delete a private key so we will read in the
    // exported wallet file and search for the spending key and payment address.
    
    EnsureWalletIsUnlocked();

    ifstream file;
    file.open(path.c_str(), std::ios::in | std::ios::ate);
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

    // create a random key locally
    auto testSpendingKey = libzcash::SpendingKey::random();
    auto testPaymentAddress = testSpendingKey.address();
    std::string testAddr = CZCPaymentAddress(testPaymentAddress).ToString();
    std::string testKey = CZCSpendingKey(testSpendingKey).ToString();

//    std::cout << "testAddr = " << testAddr << std::endl;
//    std::cout << "testKey = " << testKey << std::endl;
    
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

//    std::cout << "testWalletDump =\n" << testWalletDump << std::endl;
    
    // write test data to file
    boost::filesystem::path temp = boost::filesystem::temp_directory_path() /
            boost::filesystem::unique_path();
    const std::string path = temp.native();
    std::ofstream file(path);
    file << testWalletDump;
    file << std::flush;

    // wallet should currently be empty
    std::set<libzcash::PaymentAddress> addrs;
    pwalletMain->GetPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==0);
    
    // import test data from file into wallet
    BOOST_CHECK_NO_THROW(CallRPC(string("z_importwallet ") + path));
        
    // wallet should now have one zkey
    pwalletMain->GetPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==1);
    
    // check that we have the spending key for the address
    CZCPaymentAddress address(testAddr);
    auto addr = address.Get();
    BOOST_CHECK(pwalletMain->HaveSpendingKey(addr));
    
    // Verify the spending keys is the same as the test data
    libzcash::SpendingKey k;
    BOOST_CHECK(pwalletMain->GetSpendingKey(addr, k));
    CZCSpendingKey spendingkey(k);
    BOOST_CHECK_EQUAL(testKey, spendingkey.ToString());
    
//    std::cout << "address = " << address.ToString() << std::endl;
//    std::cout << "spendingkey = " << spendingkey.ToString() << std::endl;
}


/*
 * This test covers RPC command z_listaddresses, z_importkey, z_exportkey
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_z_importexport)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);
    Value retValue;
    int n1 = 1000; // number of times to import/export
    int n2 = 1000; // number of addresses to create and list
   
    // error if no args
    BOOST_CHECK_THROW(CallRPC("z_importkey"), runtime_error);   
    BOOST_CHECK_THROW(CallRPC("z_exportkey"), runtime_error);   

    // wallet should currently be empty
    std::set<libzcash::PaymentAddress> addrs;
    pwalletMain->GetPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==0);

    // verify import and export key
    for (int i = 0; i < n1; i++) {
        // create a random key locally
        auto testSpendingKey = libzcash::SpendingKey::random();
        auto testPaymentAddress = testSpendingKey.address();
        std::string testAddr = CZCPaymentAddress(testPaymentAddress).ToString();
        std::string testKey = CZCSpendingKey(testSpendingKey).ToString();
        BOOST_CHECK_NO_THROW(CallRPC(string("z_importkey ") + testKey));
        BOOST_CHECK_NO_THROW(retValue = CallRPC(string("z_exportkey ") + testAddr));
        BOOST_CHECK_EQUAL(retValue.get_str(), testKey);
    }

    // Verify we can list the keys imported
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listaddresses"));
    Array arr = retValue.get_array();
    BOOST_CHECK(arr.size() == n1);

    // Put addresses into a set
    std::unordered_set<std::string> myaddrs;
    for (Value element : arr) {
        myaddrs.insert(element.get_str());
    }
    
    // Make new addresses for the set
    for (int i=0; i<n2; i++) {
        myaddrs.insert((pwalletMain->GenerateNewZKey()).ToString());
    }

    // Verify number of addresses stored in wallet is n1+n2
    int numAddrs = myaddrs.size();
    BOOST_CHECK(numAddrs == n1+n2);
    pwalletMain->GetPaymentAddresses(addrs);
    BOOST_CHECK(addrs.size()==numAddrs);  
    
    // Ask wallet to list addresses
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listaddresses"));
    arr = retValue.get_array();
    BOOST_CHECK(arr.size() == numAddrs);
  
    // Create a set form them
    std::unordered_set<std::string> listaddrs;
    for (Value element : arr) {
        listaddrs.insert(element.get_str());
    }
    
    // Verify the two sets of addresses are the same
    BOOST_CHECK(listaddrs.size() == numAddrs);
    BOOST_CHECK(myaddrs == listaddrs);

    // Add one more address
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_getnewaddress"));
    std::string newaddress = retValue.get_str();
    CZCPaymentAddress pa(newaddress);
    auto newAddr = pa.Get();
    BOOST_CHECK(pwalletMain->HaveSpendingKey(newAddr));
}
        

BOOST_AUTO_TEST_SUITE_END()