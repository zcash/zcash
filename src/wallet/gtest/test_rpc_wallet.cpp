#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "main.h"
#include "primitives/transaction.h"
#include "consensus/validation.h"
#include "transaction_builder.h"
#include "utiltest.h"
#include "gtest/utils.h"
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
            EXPECT_TRUE(string(e.what()).find("unsupported joinsplit input") != string::npos);
        }

        info.vjsin.clear();
        try {
            proxy.perform_joinsplit(info);
        } catch (const std::runtime_error & e) {
            EXPECT_TRUE(string(e.what()).find("error verifying joinsplit") != string::npos);
        }
    }

    }
    UnloadGlobalWallet();
}


// TODO: test private methods
TEST(WalletRPCTests, RPCZMergeToAddressInternals)
{
    LoadProofParameters();

    SelectParams(CBaseChainParams::TESTNET);
    LoadGlobalWallet();

    const Consensus::Params& consensusParams = Params().GetConsensus();
    KeyIO keyIO(Params());
    {
    LOCK2(cs_main, pwalletMain->cs_wallet);

    UniValue retValue;

    // Mutable tx containing contextual information we need to build tx
    // We removed the ability to create pre-Sapling Sprout proofs, so we can
    // only create Sapling-onwards transactions.
    int nHeight = consensusParams.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight;
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(consensusParams, nHeight + 1);

    // Add keys manually
    auto taddr = pwalletMain->GenerateNewKey(true).GetID();
    std::string taddr_string = keyIO.EncodeDestination(taddr);

    MergeToAddressRecipient taddr1(keyIO.DecodePaymentAddress(taddr_string).value(), "");
    auto pa = pwalletMain->GenerateNewSproutZKey();
    MergeToAddressRecipient zaddr1(pa, "DEADBEEF");

    // Insufficient funds
    {
        std::vector<MergeToAddressInputUTXO> inputs = { MergeToAddressInputUTXO{COutPoint{uint256(),0},0, CScript()} };
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_mergetoaddress(std::nullopt, mtx, inputs, {}, {}, zaddr1) );
        operation->main();
        EXPECT_TRUE(operation->isFailed());
        std::string msg = operation->getErrorMessage();
        EXPECT_TRUE(msg.find("Insufficient funds, have 0.00 and miners fee is 0.00001") != string::npos);
    }

    // get_memo_from_hex_string())
    {
        std::vector<MergeToAddressInputUTXO> inputs = { MergeToAddressInputUTXO{COutPoint{uint256(),0},100000, CScript()} };
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_mergetoaddress(std::nullopt, mtx, inputs, {}, {}, zaddr1) );
        std::shared_ptr<AsyncRPCOperation_mergetoaddress> ptr = std::dynamic_pointer_cast<AsyncRPCOperation_mergetoaddress> (operation);
        TEST_FRIEND_AsyncRPCOperation_mergetoaddress proxy(ptr);

        std::string memo = "DEADBEEF";
        std::array<unsigned char, ZC_MEMO_SIZE> array = proxy.get_memo_from_hex_string(memo);
        EXPECT_EQ(array[0], 0xDE);
        EXPECT_EQ(array[1], 0xAD);
        EXPECT_EQ(array[2], 0xBE);
        EXPECT_EQ(array[3], 0xEF);
        for (int i=4; i<ZC_MEMO_SIZE; i++) {
            EXPECT_EQ(array[i], 0x00);  // zero padding
        }

        // memo is longer than allowed
        std::vector<char> v (2 * (ZC_MEMO_SIZE+1));
        std::fill(v.begin(), v.end(), 'A');
        std::string bigmemo(v.begin(), v.end());

        try {
            proxy.get_memo_from_hex_string(bigmemo);
            FAIL() << "Should have caused an error";
        } catch (const UniValue& objError) {
            EXPECT_TRUE(find_error(objError, "too big"));
        }

        // invalid hexadecimal string
        std::fill(v.begin(), v.end(), '@'); // not a hex character
        std::string badmemo(v.begin(), v.end());

        try {
            proxy.get_memo_from_hex_string(badmemo);
            FAIL() << "Should have caused an error";
        } catch (const UniValue& objError) {
            EXPECT_TRUE(find_error(objError, "hexadecimal format"));
        }

        // odd length hexadecimal string
        std::fill(v.begin(), v.end(), 'A');
        v.resize(v.size() - 1);
        assert(v.size() % 2 == 1); // odd length
        std::string oddmemo(v.begin(), v.end());
        try {
            proxy.get_memo_from_hex_string(oddmemo);
            FAIL() << "Should have caused an error";
        } catch (const UniValue& objError) {
            EXPECT_TRUE(find_error(objError, "hexadecimal format"));
        }
    }

    // Test the perform_joinsplit methods.
    {
        // Dummy input so the operation object can be instantiated.
        std::vector<MergeToAddressInputUTXO> inputs = { MergeToAddressInputUTXO{COutPoint{uint256(),0},100000, CScript()} };
        std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_mergetoaddress(std::nullopt, mtx, inputs, {}, {}, zaddr1) );
        std::shared_ptr<AsyncRPCOperation_mergetoaddress> ptr = std::dynamic_pointer_cast<AsyncRPCOperation_mergetoaddress> (operation);
        TEST_FRIEND_AsyncRPCOperation_mergetoaddress proxy(ptr);

        // Enable test mode so tx is not sent and proofs are not generated
        static_cast<AsyncRPCOperation_mergetoaddress *>(operation.get())->testmode = true;

        MergeToAddressJSInfo info;
        std::vector<std::optional < SproutWitness>> witnesses;
        uint256 anchor;
        try {
            proxy.perform_joinsplit(info, witnesses, anchor);
            FAIL() << "Should have caused an error";
        } catch (const std::runtime_error & e) {
            EXPECT_TRUE(string(e.what()).find("anchor is null") != string::npos);
        }

        try {
            std::vector<JSOutPoint> v;
            proxy.perform_joinsplit(info, v);
            FAIL() << "Should have caused an error";
        } catch (const std::runtime_error & e) {
            EXPECT_TRUE(string(e.what()).find("anchor is null") != string::npos);
        }

        info.notes.push_back(SproutNote());
        try {
            proxy.perform_joinsplit(info);
            FAIL() << "Should have caused an error";
        } catch (const std::runtime_error & e) {
            EXPECT_TRUE(string(e.what()).find("number of notes") != string::npos);
        }

        info.notes.clear();
        info.vjsin.push_back(JSInput());
        info.vjsin.push_back(JSInput());
        info.vjsin.push_back(JSInput());
        try {
            proxy.perform_joinsplit(info);
            FAIL() << "Should have caused an error";
        } catch (const std::runtime_error & e) {
            EXPECT_TRUE(string(e.what()).find("unsupported joinsplit input") != string::npos);
        }

        info.vjsin.clear();
        try {
            proxy.perform_joinsplit(info);
            FAIL() << "Should have caused an error";
        } catch (const std::runtime_error & e) {
            EXPECT_TRUE(string(e.what()).find("error verifying joinsplit") != string::npos);
        }
    }
    }
    UnloadGlobalWallet();
}

TEST(WalletRPCTests, RPCZsendmanyTaddrToSapling)
{
    LoadProofParameters();
    SelectParams(CBaseChainParams::TESTNET);

    LoadGlobalWallet();

    RegtestActivateSapling();
    {
    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (!pwalletMain->HaveMnemonicSeed()) {
        pwalletMain->GenerateNewSeed();
    }

    UniValue retValue;

    KeyIO keyIO(Params());
    // add keys manually
    auto taddr = pwalletMain->GenerateNewKey(true).GetID();
    auto pa = pwalletMain->GenerateNewLegacySaplingZKey();

    const Consensus::Params& consensusParams = Params().GetConsensus();

    int nextBlockHeight = chainActive.Height() + 1;

    // Add a fake transaction to the wallet
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(consensusParams, nextBlockHeight);
    CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ToByteVector(taddr) << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout.push_back(CTxOut(5 * COIN, scriptPubKey));
    CWalletTx wtx(pwalletMain, mtx);
    pwalletMain->LoadWalletTx(wtx);

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = block.BuildMerkleTree();
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());
    wtx.SetMerkleBranch(block);
    pwalletMain->LoadWalletTx(wtx);

    // Context that z_sendmany requires
    auto builder = TransactionBuilder(consensusParams, nextBlockHeight, std::nullopt, pwalletMain);
    mtx = CreateNewContextualCMutableTransaction(consensusParams, nextBlockHeight);

    auto selector = pwalletMain->ZTXOSelectorForAddress(taddr, true, false).value();
    std::vector<SendManyRecipient> recipients = { SendManyRecipient(std::nullopt, pa, 1*COIN, "ABCD") };
    TransactionStrategy strategy(PrivacyPolicy::AllowRevealedSenders);
    std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_sendmany(std::move(builder), selector, recipients, 0, 0, strategy));
    std::shared_ptr<AsyncRPCOperation_sendmany> ptr = std::dynamic_pointer_cast<AsyncRPCOperation_sendmany> (operation);

    // Enable test mode so tx is not sent
    static_cast<AsyncRPCOperation_sendmany *>(operation.get())->testmode = true;

    // Generate the Sapling shielding transaction
    operation->main();
    if (!operation->isSuccess()) {
        FAIL() << operation->getErrorMessage();
    }

    // Get the transaction
    auto result = operation->getResult();
    ASSERT_TRUE(result.isObject());
    auto hexTx = result["hex"].getValStr();
    CDataStream ss(ParseHex(hexTx), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    ss >> tx;
    ASSERT_FALSE(tx.vShieldedOutput.empty());

    // We shouldn't be able to decrypt with the empty ovk
    EXPECT_FALSE(AttemptSaplingOutDecryption(
        tx.vShieldedOutput[0].outCiphertext,
        uint256(),
        tx.vShieldedOutput[0].cv,
        tx.vShieldedOutput[0].cmu,
        tx.vShieldedOutput[0].ephemeralKey));

    // We shouldn't be able to decrypt with a random ovk
    EXPECT_FALSE(AttemptSaplingOutDecryption(
        tx.vShieldedOutput[0].outCiphertext,
        random_uint256(),
        tx.vShieldedOutput[0].cv,
        tx.vShieldedOutput[0].cmu,
        tx.vShieldedOutput[0].ephemeralKey));

    auto accountKey = pwalletMain->GetLegacyAccountKey().ToAccountPubKey();
    auto ovks = accountKey.GetOVKsForShielding();
    // We should not be able to decrypt with the internal change OVK for shielding
    EXPECT_FALSE(AttemptSaplingOutDecryption(
        tx.vShieldedOutput[0].outCiphertext,
        ovks.first,
        tx.vShieldedOutput[0].cv,
        tx.vShieldedOutput[0].cmu,
        tx.vShieldedOutput[0].ephemeralKey));
    // We should be able to decrypt with the external OVK for shielding
    EXPECT_TRUE(AttemptSaplingOutDecryption(
        tx.vShieldedOutput[0].outCiphertext,
        ovks.second,
        tx.vShieldedOutput[0].cv,
        tx.vShieldedOutput[0].cmu,
        tx.vShieldedOutput[0].ephemeralKey));

    // Tear down
    chainActive.SetTip(NULL);
    mapBlockIndex.erase(blockHash);

    }
    // Revert to default
    RegtestDeactivateSapling();
    UnloadGlobalWallet();
}
