#include <chainparams.h>
#include <key_io.h>
#include <zcash/Address.hpp>
#include <zcash/zip32.h>

#include "utiltest.h"

#include <gtest/gtest.h>


class EncodeKeysTestFixture : public ::testing::TestWithParam<std::tuple<CBaseChainParams::Network, std::tuple<string, string, string>>>
{
    public:
        EncodeKeysTestFixture()
        {
            
            SelectParams(std::get<0>(GetParam()));
        
            std::tuple<string, string, string> keys_for_network = std::get<1>(GetParam());
            sapling_extended_spend_key = std::get<0>(keys_for_network);
            sapling_payment_address = std::get<1>(keys_for_network);
            sapling_incoming_viewing_key = std::get<2>(keys_for_network);
            master_sapling_sk = GetTestMasterSaplingSpendingKey();
        }

    protected:
        std::string sapling_extended_spend_key;
        std::string sapling_payment_address;
        std::string sapling_incoming_viewing_key;
        libzcash::SaplingExtendedSpendingKey master_sapling_sk;

};

TEST_P(EncodeKeysTestFixture, EncodeAndDecodeSapling)
{
    for (uint32_t i = 0; i < 1000; i++) {
        auto sk = master_sapling_sk.Derive(i);
        {
            std::string sk_string = EncodeSpendingKey(sk);
            EXPECT_EQ(
                sk_string.substr(0, sapling_extended_spend_key.length()),
                sapling_extended_spend_key);

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
                addr_string.substr(0, sapling_payment_address.length()),
                sapling_payment_address);

            auto paymentaddr2 = DecodePaymentAddress(addr_string);
            EXPECT_TRUE(IsValidPaymentAddress(paymentaddr2));

            ASSERT_TRUE(boost::get<libzcash::SaplingPaymentAddress>(&paymentaddr2) != nullptr);
            auto addr2 = boost::get<libzcash::SaplingPaymentAddress>(paymentaddr2);
            EXPECT_EQ(addr, addr2);
        }
        {
            auto ivk = sk.ToXFVK().fvk.in_viewing_key();
            std::string ivk_string = EncodeViewingKey(ivk);
            EXPECT_EQ(
                ivk_string.substr(0, sapling_incoming_viewing_key.length()),
                sapling_incoming_viewing_key);

            auto viewing_key = DecodeViewingKey(ivk_string);
            EXPECT_TRUE(IsValidViewingKey(viewing_key));

            auto ivk2 = boost::get<libzcash::SaplingIncomingViewingKey>(&viewing_key);
            ASSERT_TRUE(ivk2 != nullptr);
            EXPECT_EQ(*ivk2, ivk);
        }
    }
}

INSTANTIATE_TEST_CASE_P(
    Keys,
    EncodeKeysTestFixture,
    ::testing::Values(
        std::make_tuple(    
            CBaseChainParams::MAIN, 
            std::make_tuple("secret-extended-key-main", "zs", "zivks")),
        std::make_tuple(    
            CBaseChainParams::TESTNET, 
            std::make_tuple("secret-extended-key-test", "ztestsapling", "zivktestsapling")),
        std::make_tuple(    
            CBaseChainParams::REGTEST, 
            std::make_tuple("secret-extended-key-regtest", "zregtestsapling", "zivkregtestsapling"))));