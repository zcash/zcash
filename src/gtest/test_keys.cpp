#include <chainparams.h>
#include <key_io.h>
#include <zcash/Address.hpp>
#include <zcash/zip32.h>
#include "consensus/upgrades.h"

#include <gtest/gtest.h>

TEST(Keys, EncodeAndDecodeSapling)
{
    SelectParams(CBaseChainParams::MAIN);

    std::vector<unsigned char, secure_allocator<unsigned char>> rawSeed(32);
    HDSeed seed(rawSeed);
    auto m = libzcash::SaplingExtendedSpendingKey::Master(seed);

    for (uint32_t i = 0; i < 1000; i++) {
        auto sk = m.Derive(i);
        {
            std::string sk_string = EncodeSpendingKey(sk);
            EXPECT_EQ(
                sk_string.substr(0, 24),
                Params().Bech32HRP(CChainParams::SAPLING_EXTENDED_SPEND_KEY));

            auto spendingkey2 = DecodeSpendingKey(sk_string);
            EXPECT_TRUE(IsValidSpendingKey(spendingkey2));

            ASSERT_TRUE(boost::get<libzcash::SaplingExtendedSpendingKey>(&spendingkey2) != nullptr);
            auto sk2 = boost::get<libzcash::SaplingExtendedSpendingKey>(spendingkey2);
            EXPECT_EQ(sk, sk2);
        }
        {
            auto addr = sk.DefaultAddress();

            std::string addr_string = EncodePaymentAddress(addr);
            EXPECT_EQ(
                addr_string.substr(0, 2),
                Params().Bech32HRP(CChainParams::SAPLING_PAYMENT_ADDRESS));

            auto paymentaddr2 = DecodePaymentAddress(addr_string);
            EXPECT_TRUE(IsValidPaymentAddress(paymentaddr2, SAPLING_BRANCH_ID));

            ASSERT_TRUE(boost::get<libzcash::SaplingPaymentAddress>(&paymentaddr2) != nullptr);
            auto addr2 = boost::get<libzcash::SaplingPaymentAddress>(paymentaddr2);
            EXPECT_EQ(addr, addr2);
        }
        {
            auto ivk = sk.ToXFVK().fvk.in_viewing_key();
            std::string ivk_string = EncodeViewingKey(ivk);
            EXPECT_EQ(
                ivk_string.substr(0, 5),
                Params().Bech32HRP(CChainParams::SAPLING_INCOMING_VIEWING_KEY));

            auto viewing_key = DecodeViewingKey(ivk_string);
            EXPECT_TRUE(IsValidViewingKey(viewing_key));

            auto ivk2 = boost::get<libzcash::SaplingIncomingViewingKey>(&viewing_key);
            ASSERT_TRUE(ivk2 != nullptr);
            EXPECT_EQ(*ivk2, ivk);
        }
    }
}
