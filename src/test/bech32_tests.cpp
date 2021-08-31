// Copyright (c) 2017 Pieter Wuille
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "bech32.h"
#include "test/test_bitcoin.h"
#include "utilstrencodings.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(bech32_tests, BasicTestingSetup)

bool CaseInsensitiveEqual(const std::string &s1, const std::string &s2)
{
    if (s1.size() != s2.size()) return false;
    for (size_t i = 0; i < s1.size(); ++i) {
        char c1 = s1[i];
        if (c1 >= 'A' && c1 <= 'Z') c1 -= ('A' - 'a');
        char c2 = s2[i];
        if (c2 >= 'A' && c2 <= 'Z') c2 -= ('A' - 'a');
        if (c1 != c2) return false;
    }
    return true;
}

BOOST_AUTO_TEST_CASE(bip173_testvectors_valid)
{
    static const std::tuple<std::string, std::string, std::string> CASES[] = {
        {"A12UEL5L", "a", ""},
        {"a12uel5l", "a", ""},
        {
            "an83characterlonghumanreadablepartthatcontainsthenumber1andtheexcludedcharactersbio1tt5tgs",
            "an83characterlonghumanreadablepartthatcontainsthenumber1andtheexcludedcharactersbio",
            "",
        },
        {
            "an84characterslonghumanreadablepartthatcontainsthenumber1andtheexcludedcharactersbio1569pvx",
            "an84characterslonghumanreadablepartthatcontainsthenumber1andtheexcludedcharactersbio",
            "",
        },
        {"abcdef1qpzry9x8gf2tvdw0s3jn54khce6mua7lmqqqxw", "abcdef", "00443214c74254b635cf84653a56d7c675be77df"},
        {
            "11qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqc8247j",
            "1",
            "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
        },
        {
            "split1checkupstagehandshakeupstreamerranterredcaperred2y9e3w",
            "split",
            "c5f38b70305f519bf66d85fb6cf03058f3dde463ecd7918f2dc743918f2d",
        },
        {"?1ezyfcl", "?", ""},
    };
    for (const auto& [str, hrp, data] : CASES) {
        auto ret = bech32::Decode(str);
        BOOST_CHECK(!ret.first.empty());
        BOOST_CHECK_EQUAL(ret.first, hrp);

        std::vector<unsigned char> decoded;
        decoded.reserve((ret.second.size() * 5) / 8);
        auto success = ConvertBits<5, 8, false>([&](unsigned char c) { decoded.push_back(c); }, ret.second.begin(), ret.second.end());
        BOOST_CHECK(success);
        BOOST_CHECK_EQUAL(HexStr(decoded.begin(), decoded.end()), data);

        std::string recode = bech32::Encode(ret.first, ret.second);
        BOOST_CHECK(!recode.empty());
        BOOST_CHECK(CaseInsensitiveEqual(str, recode));
    }
}

BOOST_AUTO_TEST_CASE(bip173_testvectors_invalid)
{
    static const std::string CASES[] = {
        " 1nwldj5",
        "\x7f""1axkwrx",
        "\x80""1eym55h",
        "pzry9x0s0muk",
        "1pzry9x0s0muk",
        "x1b4n0q5v",
        "li1dgmt3",
        "de1lg7wt\xff",
        "A1G7SGD8",
        "10a06t8",
        "1qzzfhee",
    };
    for (const std::string& str : CASES) {
        auto ret = bech32::Decode(str);
        BOOST_CHECK(ret.first.empty());
    }
}

BOOST_AUTO_TEST_CASE(bech32_deterministic_valid)
{
    for (size_t i = 0; i < 255; i++) {
        std::vector<unsigned char> input(32, i);
        auto encoded = bech32::Encode("a", input);
        if (i < 32) {
            // Valid input
            BOOST_CHECK(!encoded.empty());
            auto ret = bech32::Decode(encoded);
            BOOST_CHECK(ret.first == "a");
            BOOST_CHECK(ret.second == input);
        } else {
            // Invalid input
            BOOST_CHECK(encoded.empty());
        }
    }

    for (size_t i = 0; i < 255; i++) {
        std::vector<unsigned char> input(43, i);
        auto encoded = bech32::Encode("a", input);
        if (i < 32) {
            // Valid input
            BOOST_CHECK(!encoded.empty());
            auto ret = bech32::Decode(encoded);
            BOOST_CHECK(ret.first == "a");
            BOOST_CHECK(ret.second == input);
        } else {
            // Invalid input
            BOOST_CHECK(encoded.empty());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
