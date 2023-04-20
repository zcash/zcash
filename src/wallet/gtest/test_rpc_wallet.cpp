#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "main.h"
#include "primitives/transaction.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "transaction_builder.h"
#include "util/test.h"
#include "gtest/utils.h"
#include "wallet/asyncrpcoperation_shieldcoinbase.h"
#include "wallet/asyncrpcoperation_sendmany.h"
#include "wallet/memo.h"
#include "zcash/JoinSplit.hpp"

#include <librustzcash.h>
#include <rust/ed25519.h>

namespace {

bool find_error(const UniValue& objError, const std::string& expected) {
    return find_value(objError, "message").get_str().find(expected) != string::npos;
}

CWalletTx FakeWalletTx() {
    CMutableTransaction mtx;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 1;
    return CWalletTx(nullptr, mtx);
}

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
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(consensusParams, nHeight + 1, false);

    // Add keys manually
    auto taddr = pwalletMain->GenerateNewKey(true).GetID();
    std::string taddr_string = keyIO.EncodeDestination(taddr);

    NetAmountRecipient taddr1(keyIO.DecodePaymentAddress(taddr_string).value(), Memo());
    auto sproutKey = pwalletMain->GenerateNewSproutZKey();
    NetAmountRecipient zaddr1(sproutKey, Memo());

    auto saplingKey = pwalletMain->GenerateNewLegacySaplingZKey();
    NetAmountRecipient zaddr2(saplingKey, Memo());

    WalletTxBuilder builder(Params(), minRelayTxFee);
    auto selector = CWallet::LegacyTransparentZTXOSelector(
            true,
            TransparentCoinbasePolicy::Disallow);
    TransactionStrategy strategy(PrivacyPolicy::AllowRevealedSenders);

    SpendableInputs inputs;
    auto wtx = FakeWalletTx();
    inputs.utxos.emplace_back(COutput(&wtx, 0, 100, true));

    // Can’t send to Sprout
    builder.PrepareTransaction(
            *pwalletMain,
            selector,
            inputs,
            zaddr1,
            chainActive,
            strategy,
            0,
            1)
        .map_error([](const auto& err) {
            EXPECT_TRUE(examine(err, match {
                [](const AddressResolutionError& are) {
                    return are == AddressResolutionError::SproutRecipientsNotSupported;
                },
                [](const auto&) { return false; },
            }));
        })
        .map([](const auto&) { EXPECT_TRUE(false); });

    // Insufficient funds
    builder.PrepareTransaction(
            *pwalletMain,
            selector,
            inputs,
            zaddr2,
            chainActive,
            strategy,
            std::nullopt,
            1)
        .map_error([](const auto& err) {
            EXPECT_TRUE(examine(err, match {
                [](const InvalidFundsError& ife) {
                    return std::holds_alternative<InsufficientFundsError>(ife.reason);
                },
                [](const auto&) { return false; },
            }));
        })
        .map([](const auto&) { EXPECT_TRUE(false); });
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
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(consensusParams, nextBlockHeight, false);
    CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ToByteVector(taddr) << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout.push_back(CTxOut(5 * COIN, scriptPubKey));
    CWalletTx wtx(pwalletMain, mtx);
    pwalletMain->LoadWalletTx(wtx);

    // Fake-mine the transaction
    EXPECT_EQ(-1, chainActive.Height());
    CBlock block;
    block.vtx.push_back(wtx);
    block.hashMerkleRoot = BlockMerkleRoot(block);
    auto blockHash = block.GetHash();
    CBlockIndex fakeIndex {block};
    mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
    chainActive.SetTip(&fakeIndex);
    EXPECT_TRUE(chainActive.Contains(&fakeIndex));
    EXPECT_EQ(0, chainActive.Height());
    wtx.SetMerkleBranch(block);
    pwalletMain->LoadWalletTx(wtx);

    // Context that z_sendmany requires
    auto builder = WalletTxBuilder(Params(), minRelayTxFee);
    mtx = CreateNewContextualCMutableTransaction(consensusParams, nextBlockHeight, false);

    // we need AllowFullyTransparent because the transaction will result
    // in transparent change as a consequence of sending from a legacy taddr
    TransactionStrategy strategy(PrivacyPolicy::AllowFullyTransparent);
    auto selector = pwalletMain->ZTXOSelectorForAddress(
            taddr,
            true,
            TransparentCoinbasePolicy::Disallow,
            false).value();
    std::vector<Payment> recipients = { Payment(pa, 1*COIN, Memo::FromHexOrThrow("ABCD")) };
    std::shared_ptr<AsyncRPCOperation> operation(new AsyncRPCOperation_sendmany(std::move(builder), selector, recipients, 0, 0, strategy, std::nullopt));
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
