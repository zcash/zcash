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

#include "rpcserver.h"
#include "asyncrpcqueue.h"
#include "asyncrpcoperation.h"
#include "wallet/asyncrpcoperation_sendmany.h"
#include "rpcprotocol.h"

#include <chrono>
#include <thread>

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

    /*
     * getblocksubsidy
     */
    BOOST_CHECK_THROW(CallRPC("getblocksubsidy too many args"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("getblocksubsidy -1"), runtime_error);
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getblocksubsidy 50000"));
    Object obj = retValue.get_obj();
    BOOST_CHECK(find_value(obj, "miner") == 10.0);
    BOOST_CHECK(find_value(obj, "founders") == 2.5);
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getblocksubsidy 1000000"));
    obj = retValue.get_obj();
    BOOST_CHECK(find_value(obj, "miner") == 6.25);
    BOOST_CHECK(find_value(obj, "founders") == 0.0);
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getblocksubsidy 2000000"));
    obj = retValue.get_obj();
    BOOST_CHECK(find_value(obj, "miner") == 3.125);
    BOOST_CHECK(find_value(obj, "founders") == 0.0);
}

/*
 * This test covers RPC command z_exportwallet
 */
BOOST_AUTO_TEST_CASE(rpc_wallet_z_exportwallet)
{
    LOCK2(cs_main, pwalletMain->cs_wallet);
    
    // wallet should be empty
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
    
    // Verify the spending key is the same as the test data
    libzcash::SpendingKey k;
    BOOST_CHECK(pwalletMain->GetSpendingKey(addr, k));
    CZCSpendingKey spendingkey(k);
    BOOST_CHECK_EQUAL(testKey, spendingkey.ToString());
}


/*
 * This test covers RPC commands z_listaddresses, z_importkey, z_exportkey
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
  
    // Create a set from them
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
        set_result(Value("done"));
        set_state(OperationStatus::SUCCESS);
    }
};


/*
 * Test Aysnc RPC queue and operations.
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
    BOOST_CHECK(op1->getError() == Value::null );
    BOOST_CHECK(op1->getResult().is_null() == false );
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
    
    // the shared counter should equal the number of successful operations.
    BOOST_CHECK_NE(numOperations, gCounter.load());

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
    
    Value retValue;
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_listoperationids"));
    BOOST_CHECK(retValue.get_array().size() == 2);

    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_getoperationstatus"));
    Array array = retValue.get_array();
    BOOST_CHECK(array.size() == 2);

    // idempotent
    BOOST_CHECK_NO_THROW(retValue = CallRPC("z_getoperationstatus"));
    array = retValue.get_array();
    BOOST_CHECK(array.size() == 2);   
    
    for (Value v : array) {
        Object obj = v.get_obj();
        Value id = find_value(obj, "id");
        
        Value result;
        // removes result from internal storage
        BOOST_CHECK_NO_THROW(result = CallRPC("z_getoperationresult [\"" + id.get_str() + "\"]"));
        Array resultArray = result.get_array();
        BOOST_CHECK(resultArray.size() == 1);
        
        Object resultObj = resultArray.front().get_obj();
        Value resultId = find_value(resultObj, "id");
        BOOST_CHECK_EQUAL(id.get_str(), resultId.get_str());
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

    LOCK(pwalletMain->cs_wallet);

    BOOST_CHECK_THROW(CallRPC("z_sendmany"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_sendmany toofewargs"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("z_sendmany too many args here"), runtime_error);

    // bad from address
    BOOST_CHECK_THROW(CallRPC("z_sendmany INVALIDmwehwBzEHJTB5hiyxjmVkuFu9CHD2Cojjs []"), runtime_error);
    // empty amounts
    BOOST_CHECK_THROW(CallRPC("z_sendmany mwehwBzEHJTB5hiyxjmVkuFu9CHD2Cojjs []"), runtime_error);

    // don't have the spending key for this address
    BOOST_CHECK_THROW(CallRPC("z_sendmany tnpoQJVnYBZZqkFadj2bJJLThNCxbADGB5gSGeYTAGGrT5tejsxY9Zc1BtY8nnHmZkBUkJ1oSfbhTJhm72WiZizvkZz5aH1 []"), runtime_error);

    // duplicate address
    BOOST_CHECK_THROW(CallRPC("z_sendmany mwehwBzEHJTB5hiyxjmVkuFu9CHD2Cojjs [{\"address\":\"mvBkHw3UTeV2ivipmSA6uo8yjN4DqZ5KoG\", \"amount\":50.0}, {\"address\":\"mvBkHw3UTeV2ivipmSA6uo8yjN4DqZ5KoG\", \"amount\":12.0} ]"), runtime_error);

    // memo bigger than allowed length of ZC_MEMO_SIZE
    std::vector<char> v (ZC_MEMO_SIZE*2);
    std::fill(v.begin(),v.end(), 0xFF);
    std::string badmemo(v.begin(), v.end());
    CZCPaymentAddress pa = pwalletMain->GenerateNewZKey();
    std::string zaddr1 = pa.ToString();
    BOOST_CHECK_THROW(CallRPC(string("z_sendmany mwehwBzEHJTB5hiyxjmVkuFu9CHD2Cojjs ") + "[{\"address\":\"" + zaddr1 + "\", \"amount\":123.456}]"), runtime_error);
    
    // Test constructor of AsyncRPCOperation_sendmany 
    try {
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_sendmany("",{}, {}, -1));
    } catch (const Object& objError) {
        BOOST_CHECK( find_value(objError, "message").get_str().find("Minconf cannot be negative") != string::npos);
    }

    try {
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_sendmany("",{}, {}, 1));
    } catch (const Object& objError) {
        BOOST_CHECK( find_value(objError, "message").get_str().find("From address parameter missing")!= string::npos);
    }

    try {
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_sendmany("mwehwBzEHJTB5hiyxjmVkuFu9CHD2Cojjs", {}, {}, 1) );
    } catch (const Object& objError) {
        BOOST_CHECK( find_value(objError, "message").get_str().find("No recipients")!= string::npos);
    }

    try {
        std::vector<SendManyRecipient> recipients = { SendManyRecipient("dummy",1.0, "") };
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_sendmany("INVALID", recipients, {}, 1) );
    } catch (const Object& objError) {
        BOOST_CHECK( find_value(objError, "message").get_str().find("payment address is invalid")!= string::npos);
    }

    // This test is for testnet addresses which begin with 't' not 'z'.
    try {
        std::vector<SendManyRecipient> recipients = { SendManyRecipient("dummy",1.0, "") };
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_sendmany("zcMuhvq8sEkHALuSU2i4NbNQxshSAYrpCExec45ZjtivYPbuiFPwk6WHy4SvsbeZ4siy1WheuRGjtaJmoD1J8bFqNXhsG6U", recipients, {}, 1) );
    } catch (const Object& objError) {
        BOOST_CHECK( find_value(objError, "message").get_str().find("payment address is for wrong network type")!= string::npos);
    }

    // Note: The following will crash as a google test because AsyncRPCOperation_sendmany
    // invokes a method on pwalletMain, which is undefined in the google test environment.
    try {
        std::vector<SendManyRecipient> recipients = { SendManyRecipient("dummy",1.0, "") };
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_sendmany("tnpoQJVnYBZZqkFadj2bJJLThNCxbADGB5gSGeYTAGGrT5tejsxY9Zc1BtY8nnHmZkBUkJ1oSfbhTJhm72WiZizvkZz5aH1", recipients, {}, 1) );
    } catch (const Object& objError) {
        BOOST_CHECK( find_value(objError, "message").get_str().find("no spending key found for zaddr")!= string::npos);
    }
}

// TODO: test private methods
BOOST_AUTO_TEST_CASE(rpc_z_sendmany_internals)
{
    SelectParams(CBaseChainParams::TESTNET);

    LOCK(pwalletMain->cs_wallet);

    Value retValue;
 
    // add keys manually
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getnewaddress"));
    std::string taddr1 = retValue.get_str();
    CZCPaymentAddress pa = pwalletMain->GenerateNewZKey();
    std::string zaddr1 = pa.ToString();
    
    // there are no utxos to spend
    {
        std::vector<SendManyRecipient> recipients = { SendManyRecipient(zaddr1,100.0, "DEADBEEF") };
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_sendmany(taddr1, {}, recipients, 1) );
        operation->main();
        BOOST_CHECK(operation->isFailed());
        std::string msg = operation->getErrorMessage();
        BOOST_CHECK( msg.find("Insufficient funds, no UTXOs found") != string::npos);
    }
    
    // there are no unspent notes to spend
    {
        std::vector<SendManyRecipient> recipients = { SendManyRecipient(taddr1,100.0, "DEADBEEF") };
        std::shared_ptr<AsyncRPCOperation> operation( new AsyncRPCOperation_sendmany(zaddr1, recipients, {}, 1) );
        operation->main();
        BOOST_CHECK(operation->isFailed());
        std::string msg = operation->getErrorMessage();
        BOOST_CHECK( msg.find("Insufficient funds, no unspent notes") != string::npos);
    }

}


BOOST_AUTO_TEST_SUITE_END()
