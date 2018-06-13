#include <gtest/gtest.h>

#include "test/data/sapling_key_components.json.h"

#include "keystore.h"
#include "random.h"
#ifdef ENABLE_WALLET
#include "wallet/crypter.h"
#endif
#include "zcash/Address.hpp"

#include "json_test_vectors.h"

#define MAKE_STRING(x) std::string((x), (x)+sizeof(x))

TEST(keystore_tests, sapling_keys) {
    // ["sk, ask, nsk, ovk, ak, nk, ivk, default_d, default_pk_d, note_v, note_r, note_cm, note_pos, note_nf"],
    UniValue sapling_keys = read_json(MAKE_STRING(json_tests::sapling_key_components));
    
    // Skipping over comments in sapling_key_components.json file
    for (size_t i = 2; i < 12; i++) {
        uint256 skSeed, ask, nsk, ovk, ak, nk, ivk;
        skSeed.SetHex(sapling_keys[i][0].getValStr());
        ask.SetHex(sapling_keys[i][1].getValStr());
        nsk.SetHex(sapling_keys[i][2].getValStr());
        ovk.SetHex(sapling_keys[i][3].getValStr());
        ak.SetHex(sapling_keys[i][4].getValStr());
        nk.SetHex(sapling_keys[i][5].getValStr());
        ivk.SetHex(sapling_keys[i][6].getValStr());
        
        libzcash::diversifier_t default_d;
        std::copy_n(ParseHex(sapling_keys[i][7].getValStr()).begin(), 11, default_d.begin());
        
        uint256 default_pk_d;
        default_pk_d.SetHex(sapling_keys[i][8].getValStr());
        
        auto sk = libzcash::SaplingSpendingKey(skSeed);
        
        // Check that expanded spending key from primitives and from sk are the same
        auto exp_sk_2 = libzcash::SaplingExpandedSpendingKey(ask, nsk, ovk);
        auto exp_sk = sk.expanded_spending_key();
        EXPECT_EQ(exp_sk, exp_sk_2);
            
        // Check that full viewing key derived from sk and expanded sk are the same
        auto full_viewing_key = sk.full_viewing_key();
        EXPECT_EQ(full_viewing_key, exp_sk.full_viewing_key());
        
        // Check that full viewing key from primitives and from sk are the same
        auto full_viewing_key_2 = libzcash::SaplingFullViewingKey(ak, nk, ovk);
        EXPECT_EQ(full_viewing_key, full_viewing_key_2);
            
        // Check that incoming viewing key from primitives and from sk are the same
        auto in_viewing_key = full_viewing_key.in_viewing_key();
        auto in_viewing_key_2 = libzcash::SaplingIncomingViewingKey(ivk);
        EXPECT_EQ(in_viewing_key, in_viewing_key_2);
        
        // Check that the default address from primitives and from sk method are the same
        auto default_addr = sk.default_address();
        auto default_addr_2 = in_viewing_key.address(default_d);
        EXPECT_EQ(default_addr, default_addr_2);
        
        auto default_addr_3 = libzcash::SaplingPaymentAddress(default_d, default_pk_d);
        EXPECT_EQ(default_addr_2, default_addr_3);
        EXPECT_EQ(default_addr, default_addr_3);
    }
}

TEST(keystore_tests, store_and_retrieve_spending_key) {
    CBasicKeyStore keyStore;
    libzcash::SproutSpendingKey skOut;

    std::set<libzcash::SproutPaymentAddress> addrs;
    keyStore.GetPaymentAddresses(addrs);
    EXPECT_EQ(0, addrs.size());

    auto sk = libzcash::SproutSpendingKey::random();
    auto addr = sk.address();

    // Sanity-check: we can't get a key we haven't added
    EXPECT_FALSE(keyStore.HaveSpendingKey(addr));
    EXPECT_FALSE(keyStore.GetSpendingKey(addr, skOut));

    keyStore.AddSpendingKey(sk);
    EXPECT_TRUE(keyStore.HaveSpendingKey(addr));
    EXPECT_TRUE(keyStore.GetSpendingKey(addr, skOut));
    EXPECT_EQ(sk, skOut);

    keyStore.GetPaymentAddresses(addrs);
    EXPECT_EQ(1, addrs.size());
    EXPECT_EQ(1, addrs.count(addr));
}

TEST(keystore_tests, store_and_retrieve_note_decryptor) {
    CBasicKeyStore keyStore;
    ZCNoteDecryption decOut;

    auto sk = libzcash::SproutSpendingKey::random();
    auto addr = sk.address();

    EXPECT_FALSE(keyStore.GetNoteDecryptor(addr, decOut));

    keyStore.AddSpendingKey(sk);
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk.receiving_key()), decOut);
}

TEST(keystore_tests, StoreAndRetrieveViewingKey) {
    CBasicKeyStore keyStore;
    libzcash::SproutViewingKey vkOut;
    libzcash::SproutSpendingKey skOut;
    ZCNoteDecryption decOut;

    auto sk = libzcash::SproutSpendingKey::random();
    auto vk = sk.viewing_key();
    auto addr = sk.address();

    // Sanity-check: we can't get a viewing key we haven't added
    EXPECT_FALSE(keyStore.HaveViewingKey(addr));
    EXPECT_FALSE(keyStore.GetViewingKey(addr, vkOut));

    // and we shouldn't have a spending key or decryptor either
    EXPECT_FALSE(keyStore.HaveSpendingKey(addr));
    EXPECT_FALSE(keyStore.GetSpendingKey(addr, skOut));
    EXPECT_FALSE(keyStore.GetNoteDecryptor(addr, decOut));

    // and we can't find it in our list of addresses
    std::set<libzcash::SproutPaymentAddress> addresses;
    keyStore.GetPaymentAddresses(addresses);
    EXPECT_FALSE(addresses.count(addr));

    keyStore.AddViewingKey(vk);
    EXPECT_TRUE(keyStore.HaveViewingKey(addr));
    EXPECT_TRUE(keyStore.GetViewingKey(addr, vkOut));
    EXPECT_EQ(vk, vkOut);

    // We should still not have the spending key...
    EXPECT_FALSE(keyStore.HaveSpendingKey(addr));
    EXPECT_FALSE(keyStore.GetSpendingKey(addr, skOut));

    // ... but we should have a decryptor
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk.receiving_key()), decOut);

    // ... and we should find it in our list of addresses
    addresses.clear();
    keyStore.GetPaymentAddresses(addresses);
    EXPECT_TRUE(addresses.count(addr));

    keyStore.RemoveViewingKey(vk);
    EXPECT_FALSE(keyStore.HaveViewingKey(addr));
    EXPECT_FALSE(keyStore.GetViewingKey(addr, vkOut));
    EXPECT_FALSE(keyStore.HaveSpendingKey(addr));
    EXPECT_FALSE(keyStore.GetSpendingKey(addr, skOut));
    addresses.clear();
    keyStore.GetPaymentAddresses(addresses);
    EXPECT_FALSE(addresses.count(addr));

    // We still have a decryptor because those are cached in memory
    // (and also we only remove viewing keys when adding a spending key)
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk.receiving_key()), decOut);
}

#ifdef ENABLE_WALLET
class TestCCryptoKeyStore : public CCryptoKeyStore
{
public:
    bool EncryptKeys(CKeyingMaterial& vMasterKeyIn) { return CCryptoKeyStore::EncryptKeys(vMasterKeyIn); }
    bool Unlock(const CKeyingMaterial& vMasterKeyIn) { return CCryptoKeyStore::Unlock(vMasterKeyIn); }
};

TEST(keystore_tests, store_and_retrieve_spending_key_in_encrypted_store) {
    TestCCryptoKeyStore keyStore;
    uint256 r {GetRandHash()};
    CKeyingMaterial vMasterKey (r.begin(), r.end());
    libzcash::SproutSpendingKey keyOut;
    ZCNoteDecryption decOut;
    std::set<libzcash::SproutPaymentAddress> addrs;

    // 1) Test adding a key to an unencrypted key store, then encrypting it
    auto sk = libzcash::SproutSpendingKey::random();
    auto addr = sk.address();
    EXPECT_FALSE(keyStore.GetNoteDecryptor(addr, decOut));

    keyStore.AddSpendingKey(sk);
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr, keyOut));
    ASSERT_EQ(sk, keyOut);
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk.receiving_key()), decOut);

    ASSERT_TRUE(keyStore.EncryptKeys(vMasterKey));
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr));
    ASSERT_FALSE(keyStore.GetSpendingKey(addr, keyOut));
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk.receiving_key()), decOut);

    // Unlocking with a random key should fail
    uint256 r2 {GetRandHash()};
    CKeyingMaterial vRandomKey (r2.begin(), r2.end());
    EXPECT_FALSE(keyStore.Unlock(vRandomKey));

    // Unlocking with a slightly-modified vMasterKey should fail
    CKeyingMaterial vModifiedKey (r.begin(), r.end());
    vModifiedKey[0] += 1;
    EXPECT_FALSE(keyStore.Unlock(vModifiedKey));

    // Unlocking with vMasterKey should succeed
    ASSERT_TRUE(keyStore.Unlock(vMasterKey));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr, keyOut));
    ASSERT_EQ(sk, keyOut);

    keyStore.GetPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());
    ASSERT_EQ(1, addrs.count(addr));

    // 2) Test adding a spending key to an already-encrypted key store
    auto sk2 = libzcash::SproutSpendingKey::random();
    auto addr2 = sk2.address();
    EXPECT_FALSE(keyStore.GetNoteDecryptor(addr2, decOut));

    keyStore.AddSpendingKey(sk2);
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr2));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr2, keyOut));
    ASSERT_EQ(sk2, keyOut);
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr2, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk2.receiving_key()), decOut);

    ASSERT_TRUE(keyStore.Lock());
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr2));
    ASSERT_FALSE(keyStore.GetSpendingKey(addr2, keyOut));
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr2, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk2.receiving_key()), decOut);

    ASSERT_TRUE(keyStore.Unlock(vMasterKey));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr2, keyOut));
    ASSERT_EQ(sk2, keyOut);
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr2, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk2.receiving_key()), decOut);

    keyStore.GetPaymentAddresses(addrs);
    ASSERT_EQ(2, addrs.size());
    ASSERT_EQ(1, addrs.count(addr));
    ASSERT_EQ(1, addrs.count(addr2));
}
#endif
