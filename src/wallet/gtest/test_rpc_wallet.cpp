#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "main.h"
#include "primitives/transaction.h"
#include "consensus/validation.h"
#include "transaction_builder.h"
#include "utiltest.h"
#include "wallet/asyncrpcoperation_mergetoaddress.h"
#include "wallet/asyncrpcoperation_shieldcoinbase.h"
#include "wallet/asyncrpcoperation_sendmany.h"
#include "zcash/JoinSplit.hpp"

#include <librustzcash.h>
#include <rust/ed25519.h>

bool find_error(const UniValue& objError, const std::string& expected) {
    return find_value(objError, "message").get_str().find(expected) != string::npos;
}

TEST(WalletRPCTests,ZShieldCoinbaseInternals)
{
    LoadProofParameters();

    SelectParams(CBaseChainParams::TESTNET);
    const Consensus::Params& consensusParams = Params().GetConsensus();

    // we need to use pwalletMain because of AsyncRPCOperation_shieldcoinbase
    LoadGlobalWallet();
    {
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
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_shieldcoinbase(TransactionBuilder(), mtx, inputs, pa) );
        operation->main();
        EXPECT_TRUE(operation->isFailed());
        std::string msg = operation->getErrorMessage();
        EXPECT_TRUE(msg.find("Insufficient coinbase funds") != string::npos);
    }

    // Test the perform_joinsplit methods.
    {
        // Dummy input so the operation object can be instantiated.
        std::vector<ShieldCoinbaseUTXO> inputs = { ShieldCoinbaseUTXO{uint256(),0,100000} };
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_shieldcoinbase(TransactionBuilder(), mtx, inputs, pa) );
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
            EXPECT_TRUE(string(e.what()).find("unsupported joinsplit input")!= string::npos);
        }

        info.vjsin.clear();
        try {
            proxy.perform_joinsplit(info);
        } catch (const std::runtime_error & e) {
            EXPECT_TRUE(string(e.what()).find("error verifying joinsplit")!= string::npos);
        }
    }

    }
    UnloadGlobalWallet();
}
