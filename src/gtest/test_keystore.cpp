#include <gtest/gtest.h>

#include "keystore.h"
#include "random.h"
#include "wallet/crypter.h"
#include "zcash/Address.hpp"

TEST(keystore_tests, store_and_retrieve_spending_key) {
    CBasicKeyStore keyStore;
    libzcash::SpendingKey skOut;

    std::set<libzcash::PaymentAddress> addrs;
    keyStore.GetPaymentAddresses(addrs);
    EXPECT_EQ(0, addrs.size());

    auto sk = libzcash::SpendingKey::random();
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

    auto sk = libzcash::SpendingKey::random();
    auto addr = sk.address();

    EXPECT_FALSE(keyStore.GetNoteDecryptor(addr, decOut));

    keyStore.AddSpendingKey(sk);
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk.viewing_key()), decOut);
}

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
    libzcash::SpendingKey keyOut;
    std::set<libzcash::PaymentAddress> addrs;

    // 1) Test adding a key to an unencrypted key store, then encrypting it
    auto sk = libzcash::SpendingKey::random();
    auto addr = sk.address();
    keyStore.AddSpendingKey(sk);
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr, keyOut));
    ASSERT_EQ(sk, keyOut);

    ASSERT_TRUE(keyStore.EncryptKeys(vMasterKey));
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr));
    ASSERT_FALSE(keyStore.GetSpendingKey(addr, keyOut));

    ASSERT_TRUE(keyStore.Unlock(vMasterKey));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr, keyOut));
    ASSERT_EQ(sk, keyOut);

    keyStore.GetPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());
    ASSERT_EQ(1, addrs.count(addr));

    // 2) Test adding a spending key to an already-encrypted key store
    auto sk2 = libzcash::SpendingKey::random();
    auto addr2 = sk2.address();
    keyStore.AddSpendingKey(sk2);
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr2));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr2, keyOut));
    ASSERT_EQ(sk2, keyOut);

    ASSERT_TRUE(keyStore.Lock());
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr2));
    ASSERT_FALSE(keyStore.GetSpendingKey(addr2, keyOut));

    ASSERT_TRUE(keyStore.Unlock(vMasterKey));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr2, keyOut));
    ASSERT_EQ(sk2, keyOut);

    keyStore.GetPaymentAddresses(addrs);
    ASSERT_EQ(2, addrs.size());
    ASSERT_EQ(1, addrs.count(addr));
    ASSERT_EQ(1, addrs.count(addr2));
}
