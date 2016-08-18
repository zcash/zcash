#include <gtest/gtest.h>

#include "keystore.h"
#include "zcash/Address.hpp"

TEST(keystore_tests, store_and_retrieve_spending_key) {
    CBasicKeyStore keyStore;
    libzcash::SpendingKey skOut;

    std::set<libzcash::PaymentAddress> addrs;
    keyStore.GetPaymentAddresses(addrs);
    ASSERT_EQ(0, addrs.size());

    auto sk = libzcash::SpendingKey::random();
    auto addr = sk.address();

    // Sanity-check: we can't get a key we haven't added
    ASSERT_FALSE(keyStore.HaveSpendingKey(addr));
    ASSERT_FALSE(keyStore.GetSpendingKey(addr, skOut));

    keyStore.AddSpendingKey(sk);
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr));
    ASSERT_TRUE(keyStore.GetSpendingKey(addr, skOut));
    ASSERT_EQ(sk, skOut);

    keyStore.GetPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());
    ASSERT_EQ(1, addrs.count(addr));
}
