#include <chainparams.h>
#include <key_io.h>
#include <zcash/Address.hpp>

#include "utiltest.h"

#include <variant>

#include <gtest/gtest.h>

#include "json_test_vectors.h"
#include "test/data/unified_addrs.json.h"

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
            auto addr = sk.DefaultAddress();

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

TEST(Keys, EncodeAndDecodeUnified)
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
            // ["p2pkh_bytes, p2sh_bytes, sapling_raw_addr, orchard_raw_addr, unified_addr"]
            // These were added to the UA in preference order by the Python test vectors.
            if (!test[3].isNull()) {
                auto data = ParseHex(test[3].get_str());
                libzcash::UnknownReceiver r(0x03, data);
                ua.AddReceiver(r);
            }
            if (!test[2].isNull()) {
                auto data = ParseHex(test[2].get_str());
                CDataStream ss(
                    reinterpret_cast<const char*>(data.data()),
                    reinterpret_cast<const char*>(data.data() + data.size()),
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

            auto expectedBytes = ParseHex(test[4].get_str());
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
