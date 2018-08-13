#include <chainparams.h>
#include <key_io.h>
#include <zcash/Address.hpp>

#include <gtest/gtest.h>

TEST(Keys, DISABLED_EncodeAndDecodeSapling)
{
    SelectParams(CBaseChainParams::MAIN);

    for (size_t i = 0; i < 1000; i++) {
        auto sk = libzcash::SaplingSpendingKey::random();
        {
            std::string sk_string = EncodeSpendingKey(sk);
            EXPECT_EQ(
                sk_string.substr(0, 24),
                Params().Bech32HRP(CChainParams::SAPLING_SPENDING_KEY));

            auto spendingkey2 = DecodeSpendingKey(sk_string);
            EXPECT_TRUE(IsValidSpendingKey(spendingkey2));

            ASSERT_TRUE(boost::get<libzcash::SaplingSpendingKey>(&spendingkey2) != nullptr);
            auto sk2 = boost::get<libzcash::SaplingSpendingKey>(spendingkey2);
            EXPECT_EQ(sk, sk2);
        }
        {
            auto addr = sk.default_address();

            std::string addr_string = EncodePaymentAddress(addr);
            EXPECT_EQ(
                addr_string.substr(0, 2),
                Params().Bech32HRP(CChainParams::SAPLING_PAYMENT_ADDRESS));

            auto paymentaddr2 = DecodePaymentAddress(addr_string);
            EXPECT_TRUE(IsValidPaymentAddress(paymentaddr2));

            ASSERT_TRUE(boost::get<libzcash::SaplingPaymentAddress>(&paymentaddr2) != nullptr);
            auto addr2 = boost::get<libzcash::SaplingPaymentAddress>(paymentaddr2);
            EXPECT_EQ(addr, addr2);
        }
    }
}
