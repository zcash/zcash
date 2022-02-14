#include <chainparams.h>
#include <key_io.h>
#include <zcash/Address.hpp>

#include "utiltest.h"

#include <variant>

#include <gtest/gtest.h>

#include "json_test_vectors.h"
#include "test/data/unified_addrs.json.h"
#include "test/data/unified_full_viewing_keys.json.h"

TEST(Keys, EncodeAndDecodeSapling)
{
    SelectParams(CBaseChainParams::MAIN);
    KeyIO keyIO(Params());

    auto m = GetTestMasterSaplingSpendingKey();

    for (uint32_t i = 0; i < 1000; i++) {
        auto sk = m.Derive(i);
        {
            std::string sk_string = keyIO.EncodeSpendingKey(sk);
            EXPECT_EQ(
                sk_string.substr(0, 24),
                Params().Bech32HRP(CChainParams::SAPLING_EXTENDED_SPEND_KEY));

            auto spendingkey2 = keyIO.DecodeSpendingKey(sk_string);
            EXPECT_TRUE(spendingkey2.has_value());

            auto sk2 = std::get_if<libzcash::SaplingExtendedSpendingKey>(&spendingkey2.value());
            EXPECT_NE(sk2, nullptr);
            EXPECT_EQ(sk, *sk2);
        }
        {
            auto extfvk = sk.ToXFVK();
            std::string vk_string = keyIO.EncodeViewingKey(extfvk);
            EXPECT_EQ(
                vk_string.substr(0, 7),
                Params().Bech32HRP(CChainParams::SAPLING_EXTENDED_FVK));

            auto viewingkey2 = keyIO.DecodeViewingKey(vk_string);
            EXPECT_TRUE(viewingkey2.has_value());

            auto extfvk2 = std::get_if<libzcash::SaplingExtendedFullViewingKey>(&viewingkey2.value());
            EXPECT_NE(extfvk2, nullptr);
            EXPECT_EQ(extfvk, *extfvk2);
        }
        {
            auto addr = sk.ToXFVK().DefaultAddress();

            std::string addr_string = keyIO.EncodePaymentAddress(addr);
            EXPECT_EQ(
                addr_string.substr(0, 2),
                Params().Bech32HRP(CChainParams::SAPLING_PAYMENT_ADDRESS));

            auto paymentaddr2 = keyIO.DecodePaymentAddress(addr_string);
            EXPECT_TRUE(paymentaddr2.has_value());

            auto addr2 = std::get_if<libzcash::SaplingPaymentAddress>(&paymentaddr2.value());
            EXPECT_NE(addr2, nullptr);
            EXPECT_EQ(addr, *addr2);
        }
    }
}

#define MAKE_STRING(x) std::string((x), (x) + sizeof(x))

namespace libzcash {
    class ReceiverToString {
    public:
        ReceiverToString() {}

        std::string operator()(const SaplingPaymentAddress &zaddr) const {
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << zaddr;
            return tfm::format("Sapling(%s)", HexStr(ss.begin(), ss.end()));
        }

        std::string operator()(const CScriptID &p2sh) const {
            return tfm::format("P2SH(%s)", p2sh.GetHex());
        }

        std::string operator()(const CKeyID &p2pkh) const {
            return tfm::format("P2PKH(%s)", p2pkh.GetHex());
        }

        std::string operator()(const UnknownReceiver &unknown) const {
            return tfm::format(
                "Unknown(%x, %s)",
                unknown.typecode,
                HexStr(unknown.data.begin(), unknown.data.end()));
        }
    };

    void PrintTo(const Receiver& receiver, std::ostream* os) {
        *os << std::visit(ReceiverToString(), receiver);
    }
    void PrintTo(const UnifiedAddress& ua, std::ostream* os) {
        *os << "UnifiedAddress(" << testing::PrintToString(ua.GetReceiversAsParsed()) << ")";
    }
}

TEST(Keys, EncodeAndDecodeUnifiedAddresses)
{
    SelectParams(CBaseChainParams::MAIN);
    KeyIO keyIO(Params());

    UniValue ua_tests = read_json(MAKE_STRING(json_tests::unified_addrs));

    for (size_t idx = 0; idx < ua_tests.size(); idx++) {
        UniValue test = ua_tests[idx];
        std::string strTest = test.write();
        if (test.size() < 1) // Allow for extra stuff (useful for comments)
        {
            FAIL() << "Bad test: " << strTest;
            continue;
        }
        if (test.size() == 1) continue; // comment

        try {
            libzcash::UnifiedAddress ua;
            // ["p2pkh_bytes, p2sh_bytes, sapling_raw_addr, orchard_raw_addr, unknown_typecode, unknown_bytes, unified_addr, root_seed, account, diversifier_index"],
            // These were added to the UA in preference order by the Python test vectors.
            if (!test[3].isNull()) {
                auto data = ParseHex(test[3].get_str());
                libzcash::UnknownReceiver r(0x03, data);
                ua.AddReceiver(r);
            }
            if (!test[2].isNull()) {
                auto data = ParseHex(test[2].get_str());
                CDataStream ss(
                    data,
                    SER_NETWORK,
                    PROTOCOL_VERSION);
                libzcash::SaplingPaymentAddress r;
                ss >> r;
                ua.AddReceiver(r);
            }
            if (!test[1].isNull()) {
                CScriptID r(ParseHex(test[1].get_str()));
                ua.AddReceiver(r);
            }
            if (!test[0].isNull()) {
                CKeyID r(ParseHex(test[0].get_str()));
                ua.AddReceiver(r);
            }

            auto expectedBytes = ParseHex(test[6].get_str());
            std::string expected(expectedBytes.begin(), expectedBytes.end());

            auto decoded = keyIO.DecodePaymentAddress(expected);
            EXPECT_TRUE(decoded.has_value());
            auto ua_ptr = std::get_if<libzcash::UnifiedAddress>(&decoded.value());
            EXPECT_NE(ua_ptr, nullptr);
            EXPECT_EQ(*ua_ptr, ua);

            auto encoded = keyIO.EncodePaymentAddress(ua);
            EXPECT_EQ(encoded, expected);
        } catch (const std::exception& ex) {
            FAIL() << "Bad test, couldn't deserialize data: " << strTest << ": " << ex.what();
        } catch (...) {
            FAIL() << "Bad test, couldn't deserialize data: " << strTest;
        }
    }
}

TEST(Keys, DeriveUnifiedFullViewingKeys)
{
    SelectParams(CBaseChainParams::MAIN);
    KeyIO keyIO(Params());

    UniValue ua_tests = read_json(MAKE_STRING(json_tests::unified_full_viewing_keys));

    for (size_t idx = 0; idx < ua_tests.size(); idx++) {
        UniValue test = ua_tests[idx];

        std::string strTest = test.write();
        if (test.size() < 1) // Allow for extra stuff (useful for comments)
        {
            FAIL() << "Bad test: " << strTest;
            continue;
        }
        if (test.size() == 1) continue; // comment

        try {
            auto seed_hex = test[6].get_str();
            auto seed_data = ParseHex(seed_hex);
            RawHDSeed raw_seed(seed_data.begin(), seed_data.end());
            ASSERT_EQ(HexStr(raw_seed), seed_hex);

            HDSeed hdseed(raw_seed);
            auto usk = libzcash::ZcashdUnifiedSpendingKey::ForAccount(
                    hdseed,
                    133, //ZCASH_MAIN_COINTYPE
                    test[7].get_int());
            ASSERT_TRUE(usk.has_value());
            auto ufvk = usk->ToFullViewingKey();

            if (!test[0].isNull()) {
                auto expectedHex = test[0].get_str();

                // Ensure that the serialized transparent account key matches the test data.
                auto tkey = ufvk.GetTransparentKey().value();
                CDataStream ssEncode(SER_NETWORK, PROTOCOL_VERSION);
                ssEncode << tkey.GetChainablePubKey();
                EXPECT_EQ(ssEncode.size(), 65);
                auto tkeyHex = HexStr(ssEncode.begin(), ssEncode.end());
                EXPECT_EQ(expectedHex, tkeyHex);

                // Ensure that parsing the test data derives the correct account pubkey.
                auto data = ParseHex(expectedHex);
                ASSERT_EQ(data.size(), 65);
                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                auto expected = libzcash::transparent::AccountPubKey(CChainablePubKey::Read(ss));
                EXPECT_EQ(expected, tkey);
            }
            if (!test[1].isNull()) {
                auto expectedHex = test[1].get_str();

                // Ensure that the serialized Sapling dfvk matches the test data.
                auto saplingKey = ufvk.GetSaplingKey().value();
                CDataStream ssEncode(SER_NETWORK, PROTOCOL_VERSION);
                ssEncode << saplingKey;
                EXPECT_EQ(ssEncode.size(), 128);
                auto skeyHex = HexStr(ssEncode.begin(), ssEncode.end());
                EXPECT_EQ(expectedHex, skeyHex);

                // Ensure that parsing the test data derives the correct dfvk
                auto data = ParseHex(expectedHex);
                ASSERT_EQ(data.size(), 128);
                CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
                auto key = libzcash::SaplingDiversifiableFullViewingKey::Read(ss);
                EXPECT_EQ(key, saplingKey);
            }
            // Enable the following after Orchard keys are supported.
            //{
            //    auto fvk_data = ParseHex(test[5].get_str());
            //    std::string expected(fvk_data.begin(), fvk_data.end());
            //    EXPECT_EQ(expected, ufvk.Encode(Params(CBaseChainParams::MAIN)));
            //}
        } catch (const std::exception& ex) {
            FAIL() << "Bad test, couldn't deserialize data: " << strTest << ": " << ex.what();
        } catch (...) {
            FAIL() << "Bad test, couldn't deserialize data: " << strTest;
        }
    }
}


TEST(Keys, EncodeAndDecodeUnifiedFullViewingKeys)
{
    SelectParams(CBaseChainParams::MAIN);
    KeyIO keyIO(Params());

    UniValue ua_tests = read_json(MAKE_STRING(json_tests::unified_full_viewing_keys));

    for (size_t idx = 0; idx < ua_tests.size(); idx++) {
        UniValue test = ua_tests[idx];
        std::string strTest = test.write();
        if (test.size() < 1) // Allow for extra stuff (useful for comments)
        {
            FAIL() << "Bad test: " << strTest;
            continue;
        }
        if (test.size() == 1) continue; // comment

        libzcash::UnifiedFullViewingKeyBuilder builder;
        // ["t_key_bytes, sapling_fvk_bytes, orchard_fvk_bytes, unknown_fvk_typecode, unknown_fvk_bytes, unified_fvk"]
        // These were added to the UA in preference order by the Python test vectors.
        if (!test[0].isNull()) {
            auto data = ParseHex(test[0].get_str());
            ASSERT_EQ(data.size(), 65);
            CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
            auto decoded = CChainablePubKey::Read(ss);
            ASSERT_TRUE(builder.AddTransparentKey(decoded));
        }
        if (!test[1].isNull()) {
            auto data = ParseHex(test[1].get_str());
            ASSERT_EQ(data.size(), 128);
            CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
            auto key = libzcash::SaplingDiversifiableFullViewingKey::Read(ss);
            ASSERT_TRUE(builder.AddSaplingKey(key));
        }

        // Orchard keys and unknown items are not yet supported; instead,
        // we just test that we're able to parse the unified key string
        // and that the constituent items match the elements; if no Sapling
        // key is present then UFVK construction would fail because it might
        // presume the UFVK to be transparent-only.
        if (test[1].isNull())
            continue;

        auto built = builder.build();
        ASSERT_TRUE(built.has_value());

        auto keystrBytes = ParseHex(test[5].get_str());
        std::string keystr(keystrBytes.begin(), keystrBytes.end());

        auto decoded = libzcash::UnifiedFullViewingKey::Decode(keystr, Params());
        ASSERT_TRUE(decoded.has_value());

        EXPECT_EQ(decoded.value().GetTransparentKey(), built.value().GetTransparentKey());
        EXPECT_EQ(decoded.value().GetSaplingKey(), built.value().GetSaplingKey());
    }
}
