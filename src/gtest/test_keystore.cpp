#include <gtest/gtest.h>

#include "keystore.h"
#include "zcash/Address.hpp"

TEST(keystore_tests, store_and_retrieve_spending_key) {
    CBasicKeyStore keyStore;

    std::set<libzcash::PaymentAddress> addrs;
    keyStore.GetPaymentAddresses(addrs);
    ASSERT_EQ(0, addrs.size());

    auto sk = libzcash::SpendingKey::random();
    keyStore.AddSpendingKey(sk);

    auto addr = sk.address();
    ASSERT_TRUE(keyStore.HaveSpendingKey(addr));

    libzcash::SpendingKey keyOut;
    keyStore.GetSpendingKey(addr, keyOut);
    ASSERT_EQ(sk, keyOut);

    keyStore.GetPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());
    ASSERT_EQ(1, addrs.count(addr));
}
