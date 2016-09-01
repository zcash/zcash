#include <gtest/gtest.h>

#include "keystore.h"
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
