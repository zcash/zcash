#include <gtest/gtest.h>

#include "keystore.h"
#include "random.h"
#ifdef ENABLE_WALLET
#include "wallet/crypter.h"
#endif
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
    libzcash::SpendingKey keyOut;
    ZCNoteDecryption decOut;
    std::set<libzcash::PaymentAddress> addrs;

    // 1) Test adding a key to an unencrypted key store, then encrypting it
    auto sk = libzcash::SpendingKey::random();
    auto addr = sk.address();
    EXPECT_FALSE(keyStore.GetNoteDecryptor(addr, decOut));

    keyStore.AddSpendingKey(sk);
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr, keyOut));
    ASSERT_EQ(sk, keyOut);
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk.viewing_key()), decOut);

    ASSERT_TRUE(keyStore.EncryptKeys(vMasterKey));
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr));
    ASSERT_FALSE(keyStore.GetSpendingKey(addr, keyOut));
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk.viewing_key()), decOut);

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
    auto sk2 = libzcash::SpendingKey::random();
    auto addr2 = sk2.address();
    EXPECT_FALSE(keyStore.GetNoteDecryptor(addr2, decOut));

    keyStore.AddSpendingKey(sk2);
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr2));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr2, keyOut));
    ASSERT_EQ(sk2, keyOut);
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr2, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk2.viewing_key()), decOut);

    ASSERT_TRUE(keyStore.Lock());
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr2));
    ASSERT_FALSE(keyStore.GetSpendingKey(addr2, keyOut));
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr2, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk2.viewing_key()), decOut);

    ASSERT_TRUE(keyStore.Unlock(vMasterKey));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr2, keyOut));
    ASSERT_EQ(sk2, keyOut);
    EXPECT_TRUE(keyStore.GetNoteDecryptor(addr2, decOut));
    EXPECT_EQ(ZCNoteDecryption(sk2.viewing_key()), decOut);

    keyStore.GetPaymentAddresses(addrs);
    ASSERT_EQ(2, addrs.size());
    ASSERT_EQ(1, addrs.count(addr));
    ASSERT_EQ(1, addrs.count(addr2));
}
#endif
