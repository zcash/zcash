#include <gtest/gtest.h>

#include "fs.h"
#include "zcash/Address.hpp"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "util.h"

/**
 * This test covers Sapling methods on CWallet
 * GenerateNewLegacySaplingZKey()
 * AddSaplingZKey()
 * AddSaplingPaymentAddress()
 * LoadSaplingZKey()
 * LoadSaplingIncomingViewingKey()
 * LoadSaplingZKeyMetadata()
 */

TEST(WalletZkeysTest, StoreAndLoadSaplingZkeys) {

    SelectParams(CBaseChainParams::MAIN);

    CWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    // wallet should be empty
    std::set<libzcash::SaplingPaymentAddress> addrs;
    wallet.GetSaplingPaymentAddresses(addrs);
    ASSERT_EQ(0, addrs.size());

    // No HD seed in the wallet
    EXPECT_ANY_THROW(wallet.GenerateNewLegacySaplingZKey());

    // Add a random seed to the wallet.
    wallet.GenerateNewSeed();
    auto seed = wallet.GetMnemonicSeed().value();

    // Now this call succeeds
    auto address = wallet.GenerateNewLegacySaplingZKey();

    // wallet should have one key
    wallet.GetSaplingPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());

    // verify wallet has incoming viewing key for the address
    ASSERT_TRUE(wallet.HaveSaplingIncomingViewingKey(address));

    // manually add new spending key to wallet
    auto m = libzcash::SaplingExtendedSpendingKey::Master(seed);
    auto sk = m.Derive(0);
    ASSERT_TRUE(wallet.AddSaplingZKey(sk));

    // verify wallet did add it
    auto extfvk = sk.ToXFVK();
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(extfvk));

    // verify spending key stored correctly
    libzcash::SaplingExtendedSpendingKey keyOut;
    wallet.GetSaplingSpendingKey(extfvk, keyOut);
    ASSERT_EQ(sk, keyOut);

    // verify there are two keys
    wallet.GetSaplingPaymentAddresses(addrs);
    EXPECT_EQ(2, addrs.size());
    EXPECT_EQ(1, addrs.count(address));
    EXPECT_EQ(1, addrs.count(sk.ToXFVK().DefaultAddress()));

    // Find a diversified address that does not use the same diversifier as the default address.
    // By starting our search at `10` we ensure there's no more than a 2^-10 chance that we
    // collide with the default diversifier.
    libzcash::diversifier_index_t j(10);
    auto dpa = sk.ToXFVK().FindAddress(j).first;

    // verify wallet only has the default address
    EXPECT_TRUE(wallet.HaveSaplingIncomingViewingKey(sk.ToXFVK().DefaultAddress()));
    EXPECT_FALSE(wallet.HaveSaplingIncomingViewingKey(dpa));

    // manually add a diversified address
    auto ivk = extfvk.fvk.in_viewing_key();
    EXPECT_TRUE(wallet.AddSaplingPaymentAddress(ivk, dpa));

    // verify wallet did add it
    EXPECT_TRUE(wallet.HaveSaplingIncomingViewingKey(sk.ToXFVK().DefaultAddress()));
    EXPECT_TRUE(wallet.HaveSaplingIncomingViewingKey(dpa));

    // Load a third key into the wallet
    auto sk2 = m.Derive(1);
    ASSERT_TRUE(wallet.LoadSaplingZKey(sk2));

    // attach metadata to this third key
    auto ivk2 = sk2.expsk.full_viewing_key().in_viewing_key();
    int64_t now = GetTime();
    CKeyMetadata meta(now);
    wallet.LoadSaplingZKeyMetadata(ivk2, meta);

    // check metadata is the same
    ASSERT_EQ(wallet.mapSaplingZKeyMetadata[ivk2].nCreateTime, now);

    // Load a diversified address for the third key into the wallet
    auto dpa2 = sk2.ToXFVK().FindAddress(j).first;
    EXPECT_TRUE(wallet.HaveSaplingIncomingViewingKey(sk2.ToXFVK().DefaultAddress()));
    EXPECT_FALSE(wallet.HaveSaplingIncomingViewingKey(dpa2));
    EXPECT_TRUE(wallet.LoadSaplingPaymentAddress(dpa2, ivk2));
    EXPECT_TRUE(wallet.HaveSaplingIncomingViewingKey(dpa2));
}

/**
 * This test covers methods on CWallet
 * GenerateNewSproutZKey()
 * AddSproutZKey()
 * LoadZKey()
 * LoadZKeyMetadata()
 */
TEST(WalletZkeysTest, StoreAndLoadZkeys) {
    SelectParams(CBaseChainParams::MAIN);

    CWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    // wallet should be empty
    std::set<libzcash::SproutPaymentAddress> addrs;
    wallet.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(0, addrs.size());

    // wallet should have one key
    auto addr = wallet.GenerateNewSproutZKey();
    wallet.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());

    // verify wallet has spending key for the address
    ASSERT_TRUE(wallet.HaveSproutSpendingKey(addr));

    // manually add new spending key to wallet
    auto sk = libzcash::SproutSpendingKey::random();
    ASSERT_TRUE(wallet.AddSproutZKey(sk));

    // verify wallet did add it
    addr = sk.address();
    ASSERT_TRUE(wallet.HaveSproutSpendingKey(addr));

    // verify spending key stored correctly
    libzcash::SproutSpendingKey keyOut;
    wallet.GetSproutSpendingKey(addr, keyOut);
    ASSERT_EQ(sk, keyOut);

    // verify there are two keys
    wallet.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(2, addrs.size());
    ASSERT_EQ(1, addrs.count(addr));

    // Load a third key into the wallet
    sk = libzcash::SproutSpendingKey::random();
    ASSERT_TRUE(wallet.LoadZKey(sk));

    // attach metadata to this third key
    addr = sk.address();
    int64_t now = GetTime();
    CKeyMetadata meta(now);
    wallet.LoadZKeyMetadata(addr, meta);

    // check metadata is the same
    CKeyMetadata m= wallet.mapSproutZKeyMetadata[addr];
    ASSERT_EQ(m.nCreateTime, now);
}

/**
 * This test covers methods on CWallet
 * AddSproutViewingKey()
 * RemoveSproutViewingKey()
 * LoadSproutViewingKey()
 */
TEST(WalletZkeysTest, StoreAndLoadViewingKeys) {
    SelectParams(CBaseChainParams::MAIN);

    CWallet wallet(Params());
    LOCK(wallet.cs_wallet);

    // wallet should be empty
    std::set<libzcash::SproutPaymentAddress> addrs;
    wallet.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(0, addrs.size());

    // manually add new viewing key to wallet
    auto sk = libzcash::SproutSpendingKey::random();
    auto vk = sk.viewing_key();
    ASSERT_TRUE(wallet.AddSproutViewingKey(vk));

    // verify wallet did add it
    auto addr = sk.address();
    ASSERT_TRUE(wallet.HaveSproutViewingKey(addr));
    // and that we don't have the corresponding spending key
    ASSERT_FALSE(wallet.HaveSproutSpendingKey(addr));

    // verify viewing key stored correctly
    libzcash::SproutViewingKey vkOut;
    wallet.GetSproutViewingKey(addr, vkOut);
    ASSERT_EQ(vk, vkOut);

    // Load a second viewing key into the wallet
    auto sk2 = libzcash::SproutSpendingKey::random();
    ASSERT_TRUE(wallet.LoadSproutViewingKey(sk2.viewing_key()));

    // verify wallet did add it
    auto addr2 = sk2.address();
    ASSERT_TRUE(wallet.HaveSproutViewingKey(addr2));
    ASSERT_FALSE(wallet.HaveSproutSpendingKey(addr2));

    // Remove the first viewing key
    ASSERT_TRUE(wallet.RemoveSproutViewingKey(vk));
    ASSERT_FALSE(wallet.HaveSproutViewingKey(addr));
    ASSERT_TRUE(wallet.HaveSproutViewingKey(addr2));
}

/**
 * This test covers methods on CWalletDB
 * WriteZKey()
 */
TEST(WalletZkeysTest, WriteZkeyDirectToDb) {
    SelectParams(CBaseChainParams::TESTNET);

    // Get temporary and unique path for file.
    // Note: / operator to append paths
    fs::path pathTemp = fs::temp_directory_path() / fs::unique_path();
    fs::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();

    bool fFirstRun;
    CWallet wallet(Params(), "wallet.dat");
    LOCK(wallet.cs_wallet);
    ASSERT_EQ(DB_LOAD_OK, wallet.LoadWallet(fFirstRun));

    // No default CPubKey set
    ASSERT_TRUE(fFirstRun);

    // wallet should be empty
    std::set<libzcash::SproutPaymentAddress> addrs;
    wallet.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(0, addrs.size());

    // Add random key to the wallet
    auto paymentAddress = wallet.GenerateNewSproutZKey();

    // wallet should have one key
    wallet.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());

    // create random key and add it to database directly, bypassing wallet
    auto sk = libzcash::SproutSpendingKey::random();
    auto addr = sk.address();
    int64_t now = GetTime();
    CKeyMetadata meta(now);
    CWalletDB db("wallet.dat");
    db.WriteZKey(addr, sk, meta);

    // wallet should not be aware of key
    ASSERT_FALSE(wallet.HaveSproutSpendingKey(addr));

    // wallet sees one key
    wallet.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());

    // wallet should have default metadata for addr with null createtime
    CKeyMetadata m = wallet.mapSproutZKeyMetadata[addr];
    ASSERT_EQ(m.nCreateTime, 0);
    ASSERT_NE(m.nCreateTime, now);

    // load the wallet again
    ASSERT_EQ(DB_LOAD_OK, wallet.LoadWallet(fFirstRun));

    // wallet can now see the spending key
    ASSERT_TRUE(wallet.HaveSproutSpendingKey(addr));

    // check key is the same
    libzcash::SproutSpendingKey keyOut;
    wallet.GetSproutSpendingKey(addr, keyOut);
    ASSERT_EQ(sk, keyOut);

    // wallet should have two keys
    wallet.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(2, addrs.size());

    // check metadata is now the same
    m = wallet.mapSproutZKeyMetadata[addr];
    ASSERT_EQ(m.nCreateTime, now);
}

/**
 * This test covers methods on CWalletDB
 * WriteSproutViewingKey()
 */
TEST(WalletZkeysTest, WriteViewingKeyDirectToDB) {
    SelectParams(CBaseChainParams::TESTNET);

    // Get temporary and unique path for file.
    // Note: / operator to append paths
    fs::path pathTemp = fs::temp_directory_path() / fs::unique_path();
    fs::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();

    bool fFirstRun;
    CWallet wallet(Params(), "wallet-vkey.dat");
    LOCK(wallet.cs_wallet);
    ASSERT_EQ(DB_LOAD_OK, wallet.LoadWallet(fFirstRun));

    // No default CPubKey set
    ASSERT_TRUE(fFirstRun);

    // create random viewing key and add it to database directly, bypassing wallet
    auto sk = libzcash::SproutSpendingKey::random();
    auto vk = sk.viewing_key();
    auto addr = sk.address();
    int64_t now = GetTime();
    CKeyMetadata meta(now);
    CWalletDB db("wallet-vkey.dat");
    db.WriteSproutViewingKey(vk);

    // wallet should not be aware of viewing key
    ASSERT_FALSE(wallet.HaveSproutViewingKey(addr));

    // load the wallet again
    ASSERT_EQ(DB_LOAD_OK, wallet.LoadWallet(fFirstRun));

    // wallet can now see the viewing key
    ASSERT_TRUE(wallet.HaveSproutViewingKey(addr));

    // check key is the same
    libzcash::SproutViewingKey vkOut;
    wallet.GetSproutViewingKey(addr, vkOut);
    ASSERT_EQ(vk, vkOut);
}



/**
 * This test covers methods on CWalletDB to load/save crypted z keys.
 */
TEST(WalletZkeysTest, WriteCryptedzkeyDirectToDb) {

    SelectParams(CBaseChainParams::TESTNET);

    // Get temporary and unique path for file.
    // Note: / operator to append paths
    fs::path pathTemp = fs::temp_directory_path() / fs::unique_path();
    fs::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();

    bool fFirstRun;
    CWallet wallet(Params(), "wallet_crypted.dat");
    LOCK(wallet.cs_wallet);
    ASSERT_EQ(DB_LOAD_OK, wallet.LoadWallet(fFirstRun));

    // No default CPubKey set
    ASSERT_TRUE(fFirstRun);

    // wallet should be empty
    std::set<libzcash::SproutPaymentAddress> addrs;
    wallet.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(0, addrs.size());

    // Add random key to the wallet
    auto paymentAddress = wallet.GenerateNewSproutZKey();

    // wallet should have one key
    wallet.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());

    // encrypt wallet
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = "hello";
    ASSERT_TRUE(wallet.EncryptWallet(strWalletPass));

    // adding a new key will fail as the wallet is locked
    EXPECT_ANY_THROW(wallet.GenerateNewSproutZKey());

    // unlock wallet and then add
    wallet.Unlock(strWalletPass);
    auto paymentAddress2 = wallet.GenerateNewSproutZKey();

    // Create a new wallet from the existing wallet path
    CWallet wallet2(Params(), "wallet_crypted.dat");
    ASSERT_EQ(DB_LOAD_OK, wallet2.LoadWallet(fFirstRun));

    // Confirm it's not the same as the other wallet
    ASSERT_TRUE(&wallet != &wallet2);

    // wallet should have two keys
    wallet2.GetSproutPaymentAddresses(addrs);
    ASSERT_EQ(2, addrs.size());

    // check we have entries for our payment addresses
    ASSERT_TRUE(addrs.count(paymentAddress));
    ASSERT_TRUE(addrs.count(paymentAddress2));

    // spending key is crypted, so we can't extract valid payment address
    libzcash::SproutSpendingKey keyOut;
    wallet2.GetSproutSpendingKey(paymentAddress, keyOut);
    ASSERT_FALSE(paymentAddress == keyOut.address());

    // unlock wallet to get spending keys and verify payment addresses
    wallet2.Unlock(strWalletPass);

    wallet2.GetSproutSpendingKey(paymentAddress, keyOut);
    ASSERT_EQ(paymentAddress, keyOut.address());

    wallet2.GetSproutSpendingKey(paymentAddress2, keyOut);
    ASSERT_EQ(paymentAddress2, keyOut.address());
}

/**
 * This test covers methods on CWalletDB to load/save crypted sapling z keys.
 */
TEST(WalletZkeysTest, WriteCryptedSaplingZkeyDirectToDb) {
    SelectParams(CBaseChainParams::TESTNET);

    // Get temporary and unique path for file.
    // Note: / operator to append paths
    fs::path pathTemp = fs::temp_directory_path() / fs::unique_path();
    fs::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();

    bool fFirstRun;
    CWallet wallet(Params(), "wallet_crypted_sapling.dat");
    LOCK(wallet.cs_wallet);
    ASSERT_EQ(DB_LOAD_OK, wallet.LoadWallet(fFirstRun));

     // No default CPubKey set
    ASSERT_TRUE(fFirstRun);

    ASSERT_FALSE(wallet.HaveMnemonicSeed());

    // Add a mnemonic seed to the wallet.
    wallet.GenerateNewSeed();
    ASSERT_TRUE(wallet.HaveMnemonicSeed());

    // wallet should be empty
    std::set<libzcash::SaplingPaymentAddress> addrs;
    wallet.GetSaplingPaymentAddresses(addrs);
    ASSERT_EQ(0, addrs.size());

    // Add random key to the wallet
    auto address = wallet.GenerateNewLegacySaplingZKey();

    // wallet should have one key
    wallet.GetSaplingPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());

    // Generate a diversified address different to the default
    // If we can't get an early diversified address, we are very unlucky
    libzcash::SaplingExtendedSpendingKey extsk;
    EXPECT_TRUE(wallet.GetSaplingExtendedSpendingKey(address, extsk));
    libzcash::diversifier_index_t j(10);
    auto dpa = extsk.ToXFVK().FindAddress(j).first;

    // Add diversified address to the wallet
    auto ivk = extsk.expsk.full_viewing_key().in_viewing_key();
    EXPECT_TRUE(wallet.AddSaplingPaymentAddress(ivk, dpa));

    // encrypt wallet
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = "hello";
    ASSERT_TRUE(wallet.EncryptWallet(strWalletPass));

    // adding a new key will fail as the wallet is locked
    EXPECT_ANY_THROW(wallet.GenerateNewLegacySaplingZKey());

    // unlock wallet and then add
    wallet.Unlock(strWalletPass);
    auto address2 = wallet.GenerateNewLegacySaplingZKey();

    // flush the wallet to prevent race conditions
    wallet.Flush();

    // Create a new wallet from the existing wallet path
    CWallet wallet2(Params(), "wallet_crypted_sapling.dat");
    ASSERT_EQ(DB_LOAD_OK, wallet2.LoadWallet(fFirstRun));

    // Confirm it's not the same as the other wallet
    ASSERT_TRUE(&wallet != &wallet2);
    ASSERT_TRUE(wallet2.HaveMnemonicSeed());

    // wallet should have three addresses
    wallet2.GetSaplingPaymentAddresses(addrs);
    ASSERT_EQ(3, addrs.size());

    //check we have entries for our payment addresses
    ASSERT_TRUE(addrs.count(address));
    ASSERT_TRUE(addrs.count(address2));
    ASSERT_TRUE(addrs.count(dpa));

    // spending key is crypted, so we can't extract valid payment address
    libzcash::SaplingExtendedSpendingKey keyOut;
    EXPECT_FALSE(wallet2.GetSaplingExtendedSpendingKey(address, keyOut));

    // address -> ivk mapping is not crypted
    libzcash::SaplingIncomingViewingKey ivkOut;
    EXPECT_TRUE(wallet2.GetSaplingIncomingViewingKey(dpa, ivkOut));
    EXPECT_EQ(ivk, ivkOut);

    // unlock wallet to get spending keys and verify payment addresses
    wallet2.Unlock(strWalletPass);

    EXPECT_TRUE(wallet2.GetSaplingExtendedSpendingKey(address, keyOut));
    ASSERT_EQ(address, keyOut.ToXFVK().DefaultAddress());

    EXPECT_TRUE(wallet2.GetSaplingExtendedSpendingKey(address2, keyOut));
    ASSERT_EQ(address2, keyOut.ToXFVK().DefaultAddress());
}
