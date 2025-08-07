// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "util/system.h"

#include "clientversion.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "util/strencodings.h"
#include "util/moneystr.h"
#include "test/test_bitcoin.h"

#include <stdint.h>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_FIXTURE_TEST_SUITE(util_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(util_criticalsection)
{
    CCriticalSection cs;

    do {
        LOCK(cs);
        break;

        BOOST_ERROR("break was swallowed!");
    } while(0);

    do {
        TRY_LOCK(cs, lockTest);
        if (lockTest)
            break;

        BOOST_ERROR("break was swallowed!");
    } while(0);
}

static const unsigned char ParseHex_expected[65] = {
    0x04, 0x67, 0x8a, 0xfd, 0xb0, 0xfe, 0x55, 0x48, 0x27, 0x19, 0x67, 0xf1, 0xa6, 0x71, 0x30, 0xb7,
    0x10, 0x5c, 0xd6, 0xa8, 0x28, 0xe0, 0x39, 0x09, 0xa6, 0x79, 0x62, 0xe0, 0xea, 0x1f, 0x61, 0xde,
    0xb6, 0x49, 0xf6, 0xbc, 0x3f, 0x4c, 0xef, 0x38, 0xc4, 0xf3, 0x55, 0x04, 0xe5, 0x1e, 0xc1, 0x12,
    0xde, 0x5c, 0x38, 0x4d, 0xf7, 0xba, 0x0b, 0x8d, 0x57, 0x8a, 0x4c, 0x70, 0x2b, 0x6b, 0xf1, 0x1d,
    0x5f
};
BOOST_AUTO_TEST_CASE(util_ParseHex)
{
    std::vector<unsigned char> result;
    std::vector<unsigned char> expected(ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected));
    // Basic test vector
    result = ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");
    BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());

    // Spaces between bytes must be supported
    result = ParseHex("12 34 56 78");
    BOOST_CHECK(result.size() == 4 && result[0] == 0x12 && result[1] == 0x34 && result[2] == 0x56 && result[3] == 0x78);

    // Stop parsing at invalid value
    result = ParseHex("1234 invalid 1234");
    BOOST_CHECK(result.size() == 2 && result[0] == 0x12 && result[1] == 0x34);
}

BOOST_AUTO_TEST_CASE(util_HexStr)
{
    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected)),
        "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected + 5, true),
        "04 67 8a fd b0");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected, true),
        "");

    std::vector<unsigned char> ParseHex_vec(ParseHex_expected, ParseHex_expected + 5);

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_vec, true),
        "04 67 8a fd b0");
}


BOOST_AUTO_TEST_CASE(util_DateTimeStrFormat)
{
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%Y-%m-%d %H:%M:%S", 0), "1970-01-01 00:00:00");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%Y-%m-%d %H:%M:%S", 0x7FFFFFFF), "2038-01-19 03:14:07");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%Y-%m-%d %H:%M:%S", 1317425777), "2011-09-30 23:36:17");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%Y-%m-%d %H:%M", 1317425777), "2011-09-30 23:36");
    BOOST_CHECK_EQUAL(DateTimeStrFormat("%a, %d %b %Y %H:%M:%S +0000", 1317425777), "Fri, 30 Sep 2011 23:36:17 +0000");
}

BOOST_AUTO_TEST_CASE(util_ParseParameters)
{
    const char *argv_test[] = {"-ignored", "-a", "-b", "-ccc=argument", "-ccc=multiple", "f", "-d=e"};

    ParseParameters(0, (char**)argv_test);
    BOOST_CHECK(mapArgs.empty() && mapMultiArgs.empty());

    ParseParameters(1, (char**)argv_test);
    BOOST_CHECK(mapArgs.empty() && mapMultiArgs.empty());

    ParseParameters(5, (char**)argv_test);
    // expectation: -ignored is ignored (program name argument),
    // -a, -b and -ccc end up in map, -d ignored because it is after
    // a non-option argument (non-GNU option parsing)
    BOOST_CHECK(mapArgs.size() == 3 && mapMultiArgs.size() == 3);
    BOOST_CHECK(mapArgs.count("-a") && mapArgs.count("-b") && mapArgs.count("-ccc")
                && !mapArgs.count("f") && !mapArgs.count("-d"));
    BOOST_CHECK(mapMultiArgs.count("-a") && mapMultiArgs.count("-b") && mapMultiArgs.count("-ccc")
                && !mapMultiArgs.count("f") && !mapMultiArgs.count("-d"));

    BOOST_CHECK(mapArgs["-a"] == "" && mapArgs["-ccc"] == "multiple");
    BOOST_CHECK(mapMultiArgs["-ccc"].size() == 2);
}

BOOST_AUTO_TEST_CASE(util_GetArg)
{
    mapArgs.clear();
    mapArgs["strtest1"] = "string...";
    // strtest2 undefined on purpose
    mapArgs["inttest1"] = "12345";
    mapArgs["inttest2"] = "81985529216486895";
    // inttest3 undefined on purpose
    mapArgs["booltest1"] = "";
    // booltest2 undefined on purpose
    mapArgs["booltest3"] = "0";
    mapArgs["booltest4"] = "1";

    BOOST_CHECK_EQUAL(GetArg("strtest1", "default"), "string...");
    BOOST_CHECK_EQUAL(GetArg("strtest2", "default"), "default");
    BOOST_CHECK_EQUAL(GetArg("inttest1", -1), 12345);
    BOOST_CHECK_EQUAL(GetArg("inttest2", -1), 81985529216486895LL);
    BOOST_CHECK_EQUAL(GetArg("inttest3", -1), -1);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest1", false), true);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest2", false), false);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest3", false), false);
    BOOST_CHECK_EQUAL(GetBoolArg("booltest4", false), true);
}

BOOST_AUTO_TEST_CASE(util_FormatMoney)
{
    BOOST_CHECK_EQUAL(FormatMoney(0), "0.00");
    BOOST_CHECK_EQUAL(FormatMoney((COIN/10000)*123456789), "12345.6789");
    BOOST_CHECK_EQUAL(FormatMoney(-COIN), "-1.00");

    BOOST_CHECK_EQUAL(FormatMoney(COIN*100000000), "100000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10000000), "10000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*1000000), "1000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*100000), "100000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10000), "10000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*1000), "1000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*100), "100.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10), "10.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN), "1.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10), "0.10");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100), "0.01");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/1000), "0.001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10000), "0.0001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100000), "0.00001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/1000000), "0.000001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10000000), "0.0000001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100000000), "0.00000001");
}

BOOST_AUTO_TEST_CASE(util_ParseMoney)
{
    CAmount ret = 0;
    BOOST_CHECK(ParseMoney("0.0", ret));
    BOOST_CHECK_EQUAL(ret, 0);

    BOOST_CHECK(ParseMoney("12345.6789", ret));
    BOOST_CHECK_EQUAL(ret, (COIN/10000)*123456789);

    BOOST_CHECK(ParseMoney("100000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100000000);
    BOOST_CHECK(ParseMoney("10000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10000000);
    BOOST_CHECK(ParseMoney("1000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*1000000);
    BOOST_CHECK(ParseMoney("100000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100000);
    BOOST_CHECK(ParseMoney("10000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10000);
    BOOST_CHECK(ParseMoney("1000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*1000);
    BOOST_CHECK(ParseMoney("100.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100);
    BOOST_CHECK(ParseMoney("10.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10);
    BOOST_CHECK(ParseMoney("1.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN);
    BOOST_CHECK(ParseMoney("0.1", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10);
    BOOST_CHECK(ParseMoney("0.01", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100);
    BOOST_CHECK(ParseMoney("0.001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/1000);
    BOOST_CHECK(ParseMoney("0.0001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10000);
    BOOST_CHECK(ParseMoney("0.00001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100000);
    BOOST_CHECK(ParseMoney("0.000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/1000000);
    BOOST_CHECK(ParseMoney("0.0000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10000000);
    BOOST_CHECK(ParseMoney("0.00000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100000000);

    // Attempted 63 bit overflow should fail
    BOOST_CHECK(!ParseMoney("92233720368.54775808", ret));
}

BOOST_AUTO_TEST_CASE(util_IsHex)
{
    BOOST_CHECK(IsHex("00"));
    BOOST_CHECK(IsHex("00112233445566778899aabbccddeeffAABBCCDDEEFF"));
    BOOST_CHECK(IsHex("ff"));
    BOOST_CHECK(IsHex("FF"));

    BOOST_CHECK(!IsHex(""));
    BOOST_CHECK(!IsHex("0"));
    BOOST_CHECK(!IsHex("a"));
    BOOST_CHECK(!IsHex("eleven"));
    BOOST_CHECK(!IsHex("00xx00"));
    BOOST_CHECK(!IsHex("0x0000"));
}

BOOST_AUTO_TEST_CASE(util_seed_insecure_rand)
{
    SeedInsecureRand(true);
    for (int mod=2;mod<11;mod++)
    {
        int mask = 1;
        // Really rough binomal confidence approximation.
        int err = 30*10000./mod*sqrt((1./mod*(1-1./mod))/10000.);
        //mask is 2^ceil(log2(mod))-1
        while(mask<mod-1)mask=(mask<<1)+1;

        int count = 0;
        //How often does it get a zero from the uniform range [0,mod)?
        for (int i = 0; i < 10000; i++) {
            uint32_t rval;
            do{
                rval=InsecureRand32()&mask;
            }while(rval>=(uint32_t)mod);
            count += rval==0;
        }
        BOOST_CHECK(count<=10000/mod+err);
        BOOST_CHECK(count>=10000/mod-err);
    }
}

BOOST_AUTO_TEST_CASE(util_TimingResistantEqual)
{
    BOOST_CHECK(TimingResistantEqual(std::string(""), std::string("")));
    BOOST_CHECK(!TimingResistantEqual(std::string("abc"), std::string("")));
    BOOST_CHECK(!TimingResistantEqual(std::string(""), std::string("abc")));
    BOOST_CHECK(!TimingResistantEqual(std::string("a"), std::string("aa")));
    BOOST_CHECK(!TimingResistantEqual(std::string("aa"), std::string("a")));
    BOOST_CHECK(TimingResistantEqual(std::string("abc"), std::string("abc")));
    BOOST_CHECK(!TimingResistantEqual(std::string("abc"), std::string("aba")));
}

/* Test strprintf formatting directives.
 * Put a string before and after to ensure sanity of element sizes on stack. */
#define B "check_prefix"
#define E "check_postfix"
BOOST_AUTO_TEST_CASE(strprintf_numbers)
{
    int64_t s64t = -9223372036854775807LL; /* signed 64 bit test value */
    uint64_t u64t = 18446744073709551615ULL; /* unsigned 64 bit test value */
    BOOST_CHECK(strprintf("%s %d %s", B, s64t, E) == B" -9223372036854775807 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, u64t, E) == B" 18446744073709551615 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, u64t, E) == B" ffffffffffffffff " E);

    size_t st = 12345678; /* unsigned size_t test value */
    ssize_t sst = -12345678; /* signed size_t test value */
    BOOST_CHECK(strprintf("%s %d %s", B, sst, E) == B" -12345678 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, st, E) == B" 12345678 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, st, E) == B" bc614e " E);

    ptrdiff_t pt = 87654321; /* positive ptrdiff_t test value */
    ptrdiff_t spt = -87654321; /* negative ptrdiff_t test value */
    BOOST_CHECK(strprintf("%s %d %s", B, spt, E) == B" -87654321 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, pt, E) == B" 87654321 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, pt, E) == B" 5397fb1 " E);
}
#undef B
#undef E

/* Check for mingw/wine issue #3494
 * Remove this test before time.ctime(0xffffffff) == 'Sun Feb  7 07:28:15 2106'
 */
BOOST_AUTO_TEST_CASE(gettime)
{
    BOOST_CHECK((GetTime() & ~0xFFFFFFFFLL) == 0);
}

BOOST_AUTO_TEST_CASE(test_ParseInt32)
{
    int32_t n;
    // Valid values
    BOOST_CHECK(ParseInt32("1234", NULL));
    BOOST_CHECK(ParseInt32("0", &n) && n == 0);
    BOOST_CHECK(ParseInt32("1234", &n) && n == 1234);
    BOOST_CHECK(ParseInt32("01234", &n) && n == 1234); // no octal
    BOOST_CHECK(ParseInt32("2147483647", &n) && n == 2147483647);
    BOOST_CHECK(ParseInt32("-2147483648", &n) && n == (-2147483647 - 1)); // (-2147483647 - 1) equals INT_MIN
    BOOST_CHECK(ParseInt32("-1234", &n) && n == -1234);
    // Invalid values
    BOOST_CHECK(!ParseInt32("", &n));
    BOOST_CHECK(!ParseInt32(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseInt32("1 ", &n));
    BOOST_CHECK(!ParseInt32("1a", &n));
    BOOST_CHECK(!ParseInt32("aap", &n));
    BOOST_CHECK(!ParseInt32("0x1", &n)); // no hex
    BOOST_CHECK(!ParseInt32("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseInt32(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseInt32("-2147483649", NULL));
    BOOST_CHECK(!ParseInt32("2147483648", NULL));
    BOOST_CHECK(!ParseInt32("-32482348723847471234", NULL));
    BOOST_CHECK(!ParseInt32("32482348723847471234", NULL));
}

BOOST_AUTO_TEST_CASE(test_ParseInt64)
{
    int64_t n;
    // Valid values
    BOOST_CHECK(ParseInt64("1234", NULL));
    BOOST_CHECK(ParseInt64("0", &n) && n == 0LL);
    BOOST_CHECK(ParseInt64("1234", &n) && n == 1234LL);
    BOOST_CHECK(ParseInt64("01234", &n) && n == 1234LL); // no octal
    BOOST_CHECK(ParseInt64("2147483647", &n) && n == 2147483647LL);
    BOOST_CHECK(ParseInt64("-2147483648", &n) && n == -2147483648LL);
    BOOST_CHECK(ParseInt64("9223372036854775807", &n) && n == (int64_t)9223372036854775807);
    BOOST_CHECK(ParseInt64("-9223372036854775808", &n) && n == (int64_t)-9223372036854775807-1);
    BOOST_CHECK(ParseInt64("-1234", &n) && n == -1234LL);
    // Invalid values
    BOOST_CHECK(!ParseInt64("", &n));
    BOOST_CHECK(!ParseInt64(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseInt64("1 ", &n));
    BOOST_CHECK(!ParseInt64("1a", &n));
    BOOST_CHECK(!ParseInt64("aap", &n));
    BOOST_CHECK(!ParseInt64("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseInt64(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseInt64("-9223372036854775809", NULL));
    BOOST_CHECK(!ParseInt64("9223372036854775808", NULL));
    BOOST_CHECK(!ParseInt64("-32482348723847471234", NULL));
    BOOST_CHECK(!ParseInt64("32482348723847471234", NULL));
}

BOOST_AUTO_TEST_CASE(test_ParseUInt32)
{
    uint32_t n;
    // Valid values
    BOOST_CHECK(ParseUInt32("1234", NULL));
    BOOST_CHECK(ParseUInt32("0", &n) && n == 0);
    BOOST_CHECK(ParseUInt32("1234", &n) && n == 1234);
    BOOST_CHECK(ParseUInt32("01234", &n) && n == 1234); // no octal
    BOOST_CHECK(ParseUInt32("2147483647", &n) && n == 2147483647);
    BOOST_CHECK(ParseUInt32("2147483648", &n) && n == (uint32_t)2147483648);
    BOOST_CHECK(ParseUInt32("4294967295", &n) && n == (uint32_t)4294967295);
    // Invalid values
    BOOST_CHECK(!ParseUInt32("", &n));
    BOOST_CHECK(!ParseUInt32(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseUInt32(" -1", &n));
    BOOST_CHECK(!ParseUInt32("1 ", &n));
    BOOST_CHECK(!ParseUInt32("1a", &n));
    BOOST_CHECK(!ParseUInt32("aap", &n));
    BOOST_CHECK(!ParseUInt32("0x1", &n)); // no hex
    BOOST_CHECK(!ParseUInt32("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseUInt32(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseUInt32("-2147483648", &n));
    BOOST_CHECK(!ParseUInt32("4294967296", &n));
    BOOST_CHECK(!ParseUInt32("-1234", &n));
    BOOST_CHECK(!ParseUInt32("-32482348723847471234", NULL));
    BOOST_CHECK(!ParseUInt32("32482348723847471234", NULL));
}

BOOST_AUTO_TEST_CASE(test_ParseUInt64)
{
    uint64_t n;
    // Valid values
    BOOST_CHECK(ParseUInt64("1234", NULL));
    BOOST_CHECK(ParseUInt64("0", &n) && n == 0LL);
    BOOST_CHECK(ParseUInt64("1234", &n) && n == 1234LL);
    BOOST_CHECK(ParseUInt64("01234", &n) && n == 1234LL); // no octal
    BOOST_CHECK(ParseUInt64("2147483647", &n) && n == 2147483647LL);
    BOOST_CHECK(ParseUInt64("9223372036854775807", &n) && n == 9223372036854775807ULL);
    BOOST_CHECK(ParseUInt64("9223372036854775808", &n) && n == 9223372036854775808ULL);
    BOOST_CHECK(ParseUInt64("18446744073709551615", &n) && n == 18446744073709551615ULL);
    // Invalid values
    BOOST_CHECK(!ParseUInt64("", &n));
    BOOST_CHECK(!ParseUInt64(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseUInt64(" -1", &n));
    BOOST_CHECK(!ParseUInt64("1 ", &n));
    BOOST_CHECK(!ParseUInt64("1a", &n));
    BOOST_CHECK(!ParseUInt64("aap", &n));
    BOOST_CHECK(!ParseUInt64("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseUInt64(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseUInt64("-9223372036854775809", NULL));
    BOOST_CHECK(!ParseUInt64("18446744073709551616", NULL));
    BOOST_CHECK(!ParseUInt64("-32482348723847471234", NULL));
    BOOST_CHECK(!ParseUInt64("-2147483648", &n));
    BOOST_CHECK(!ParseUInt64("-9223372036854775808", &n));
    BOOST_CHECK(!ParseUInt64("-1234", &n));
}

BOOST_AUTO_TEST_CASE(test_ParseDouble)
{
    double n;
    // Valid values
    BOOST_CHECK(ParseDouble("1234", NULL));
    BOOST_CHECK(ParseDouble("0", &n) && n == 0.0);
    BOOST_CHECK(ParseDouble("1234", &n) && n == 1234.0);
    BOOST_CHECK(ParseDouble("01234", &n) && n == 1234.0); // no octal
    BOOST_CHECK(ParseDouble("2147483647", &n) && n == 2147483647.0);
    BOOST_CHECK(ParseDouble("-2147483648", &n) && n == -2147483648.0);
    BOOST_CHECK(ParseDouble("-1234", &n) && n == -1234.0);
    BOOST_CHECK(ParseDouble("1e6", &n) && n == 1e6);
    BOOST_CHECK(ParseDouble("-1e6", &n) && n == -1e6);
    // Invalid values
    BOOST_CHECK(!ParseDouble("", &n));
    BOOST_CHECK(!ParseDouble(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseDouble("1 ", &n));
    BOOST_CHECK(!ParseDouble("1a", &n));
    BOOST_CHECK(!ParseDouble("aap", &n));
    BOOST_CHECK(!ParseDouble("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseDouble(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseDouble("-1e10000", NULL));
    BOOST_CHECK(!ParseDouble("1e10000", NULL));
}

BOOST_AUTO_TEST_CASE(test_FormatParagraph)
{
    BOOST_CHECK_EQUAL(FormatParagraph("", 79, 0), "");
    BOOST_CHECK_EQUAL(FormatParagraph("test", 79, 0), "test");
    BOOST_CHECK_EQUAL(FormatParagraph(" test", 79, 0), "test");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 79, 0), "test test");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 4, 0), "test\ntest");
    BOOST_CHECK_EQUAL(FormatParagraph("testerde test ", 4, 0), "testerde\ntest");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 4, 4), "test\n    test");
    BOOST_CHECK_EQUAL(FormatParagraph("This is a very long test string. This is a second sentence in the very long test string."), "This is a very long test string. This is a second sentence in the very long\ntest string.");
}

BOOST_AUTO_TEST_CASE(test_FormatSubVersion)
{
    std::vector<std::string> comments;
    comments.push_back(std::string("comment1"));
    std::vector<std::string> comments2;
    comments2.push_back(std::string("comment1"));
    comments2.push_back(SanitizeString(std::string("Comment2; .,_?@; !\"#$%&'()*+-/<=>[]\\^`{|}~"), SAFE_CHARS_UA_COMMENT)); // Semicolon is discouraged but not forbidden by BIP-0014
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99900, std::vector<std::string>()), std::string("/Test:0.9.99-beta1/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99924, std::vector<std::string>()), std::string("/Test:0.9.99-beta25/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99925, std::vector<std::string>()), std::string("/Test:0.9.99-rc1/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99949, std::vector<std::string>()), std::string("/Test:0.9.99-rc25/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99950, std::vector<std::string>()), std::string("/Test:0.9.99/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99951, std::vector<std::string>()), std::string("/Test:0.9.99-1/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99999, std::vector<std::string>()), std::string("/Test:0.9.99-49/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99900, comments),  std::string("/Test:0.9.99-beta1(comment1)/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99950, comments),  std::string("/Test:0.9.99(comment1)/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99900, comments2), std::string("/Test:0.9.99-beta1(comment1; Comment2; .,_?@; )/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99950, comments2), std::string("/Test:0.9.99(comment1; Comment2; .,_?@; )/"));

    // bug https://github.com/zcash/zcash/issues/4375
    BOOST_CHECK_EQUAL(SanitizeString(std::string("MagicBean:2.1.1-1")), "MagicBean:2.1.11");
    // fixed by adding new rule https://github.com/zcash/zcash/pull/4444
    BOOST_CHECK_EQUAL(SanitizeString(std::string("MagicBean:2.1.1-1"), SAFE_CHARS_SUBVERSION), "MagicBean:2.1.1-1");
}

BOOST_AUTO_TEST_CASE(test_ParseFixedPoint)
{
    int64_t amount = 0;
    BOOST_CHECK(ParseFixedPoint("0", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 0LL);
    BOOST_CHECK(ParseFixedPoint("1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 100000000LL);
    BOOST_CHECK(ParseFixedPoint("0.0", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 0LL);
    BOOST_CHECK(ParseFixedPoint("-0.1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -10000000LL);
    BOOST_CHECK(ParseFixedPoint("1.1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 110000000LL);
    BOOST_CHECK(ParseFixedPoint("1.10000000000000000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 110000000LL);
    BOOST_CHECK(ParseFixedPoint("1.1e1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 1100000000LL);
    BOOST_CHECK(ParseFixedPoint("1.1e-1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 11000000LL);
    BOOST_CHECK(ParseFixedPoint("1000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 100000000000LL);
    BOOST_CHECK(ParseFixedPoint("-1000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -100000000000LL);
    BOOST_CHECK(ParseFixedPoint("0.00000001", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 1LL);
    BOOST_CHECK(ParseFixedPoint("0.0000000100000000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 1LL);
    BOOST_CHECK(ParseFixedPoint("-0.00000001", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -1LL);
    BOOST_CHECK(ParseFixedPoint("1000000000.00000001", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 100000000000000001LL);
    BOOST_CHECK(ParseFixedPoint("9999999999.99999999", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 999999999999999999LL);
    BOOST_CHECK(ParseFixedPoint("-9999999999.99999999", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -999999999999999999LL);

    BOOST_CHECK(!ParseFixedPoint("", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("a-1000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-a1000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-1000a", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-01000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("00.1", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint(".1", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("--0.1", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("0.000000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-0.000000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("0.00000001000000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-10000000000.00000000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("10000000000.00000000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-10000000000.00000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("10000000000.00000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-10000000000.00000009", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("10000000000.00000009", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-99999999999.99999999", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("99999909999.09999999", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("92233720368.54775807", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("92233720368.54775808", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-92233720368.54775808", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-92233720368.54775809", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("1.1e", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("1.1e-", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("1.", 8, &amount));
}

BOOST_AUTO_TEST_CASE(test_ParseArbitraryInt)
{
    // Negation not allowed.
    BOOST_CHECK(!ParseArbitraryInt("-1"));

    // no legal digits
    BOOST_CHECK(!ParseArbitraryInt(""));
    BOOST_CHECK(!ParseArbitraryInt(" "));

    // Hex not allowed (only decimal).
    BOOST_CHECK(!ParseArbitraryInt("ab"));
    BOOST_CHECK(!ParseArbitraryInt("0xab"));

    // Decimal point not allowed.
    BOOST_CHECK(!ParseArbitraryInt("1."));

    std::optional<std::vector<unsigned char>> v;

    // simple success case
    v = ParseArbitraryInt("1");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 1);
    BOOST_CHECK_EQUAL((*v)[0], 1);

    // Leading and trailing whitespace (spaces and tabs) is allowed.
    v = ParseArbitraryInt(" 1");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 1);
    BOOST_CHECK_EQUAL((*v)[0], 1);
    v = ParseArbitraryInt("2 ");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 1);
    BOOST_CHECK_EQUAL((*v)[0], 2);
    v = ParseArbitraryInt(" \t3 ");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 1);
    BOOST_CHECK_EQUAL((*v)[0], 3);

    // Leading zeros have no effect, does not mean octal
    v = ParseArbitraryInt("010\t");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 1);
    BOOST_CHECK_EQUAL((*v)[0], 10);

    v = ParseArbitraryInt("                 255\t");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 1);
    BOOST_CHECK_EQUAL((*v)[0], 255);

    v = ParseArbitraryInt("\t 256");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 2);
    BOOST_CHECK_EQUAL((*v)[0], 0);
    BOOST_CHECK_EQUAL((*v)[1], 1);

    v = ParseArbitraryInt("257 \t");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 2);
    BOOST_CHECK_EQUAL((*v)[0], 1);
    BOOST_CHECK_EQUAL((*v)[1], 1);

    v = ParseArbitraryInt("65535");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 2);
    BOOST_CHECK_EQUAL((*v)[0], 255);
    BOOST_CHECK_EQUAL((*v)[1], 255);

    v = ParseArbitraryInt("65536");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 3);
    BOOST_CHECK_EQUAL((*v)[0], 0);
    BOOST_CHECK_EQUAL((*v)[1], 0);
    BOOST_CHECK_EQUAL((*v)[2], 1);

    // This decimal string came from:
    // $ echo 16i 102030405060708090A0B0C0D0E0F0F1F2F3F4F5F6F7 p | dc
    v = ParseArbitraryInt("6033354224708459019450009057293028077350189222196983");
    BOOST_CHECK(v.has_value());
    BOOST_CHECK_EQUAL(v->size(), 22);
    BOOST_CHECK_EQUAL((*v)[0], 0xf7);
    BOOST_CHECK_EQUAL((*v)[1], 0xf6);
    BOOST_CHECK_EQUAL((*v)[2], 0xf5);
    BOOST_CHECK_EQUAL((*v)[3], 0xf4);
    BOOST_CHECK_EQUAL((*v)[4], 0xf3);
    BOOST_CHECK_EQUAL((*v)[5], 0xf2);
    BOOST_CHECK_EQUAL((*v)[6], 0xf1);
    BOOST_CHECK_EQUAL((*v)[7], 0xf0);
    BOOST_CHECK_EQUAL((*v)[8], 0xe0);
    BOOST_CHECK_EQUAL((*v)[9], 0xd0);
    BOOST_CHECK_EQUAL((*v)[10], 0xc0);
    BOOST_CHECK_EQUAL((*v)[11], 0xb0);
    BOOST_CHECK_EQUAL((*v)[12], 0xa0);
    BOOST_CHECK_EQUAL((*v)[13], 0x90);
    BOOST_CHECK_EQUAL((*v)[14], 0x80);
    BOOST_CHECK_EQUAL((*v)[15], 0x70);
    BOOST_CHECK_EQUAL((*v)[16], 0x60);
    BOOST_CHECK_EQUAL((*v)[17], 0x50);
    BOOST_CHECK_EQUAL((*v)[18], 0x40);
    BOOST_CHECK_EQUAL((*v)[19], 0x30);
    BOOST_CHECK_EQUAL((*v)[20], 0x20);
    BOOST_CHECK_EQUAL((*v)[21], 0x10);
}

BOOST_AUTO_TEST_CASE(test_ArbitraryIntStr)
{
    BOOST_CHECK_EQUAL(ArbitraryIntStr({}), "0");
    BOOST_CHECK_EQUAL(ArbitraryIntStr({0}), "0");
    BOOST_CHECK_EQUAL(ArbitraryIntStr({0, 0}), "0");
    BOOST_CHECK_EQUAL(ArbitraryIntStr({0, 0, 0}), "0");
    BOOST_CHECK_EQUAL(ArbitraryIntStr({0, 0, 0, 0}), "0");

    BOOST_CHECK_EQUAL(ArbitraryIntStr({1}), "1");
    BOOST_CHECK_EQUAL(ArbitraryIntStr({2}), "2");
    BOOST_CHECK_EQUAL(ArbitraryIntStr({10}), "10");
    BOOST_CHECK_EQUAL(ArbitraryIntStr({100}), "100");

    BOOST_CHECK_EQUAL(ArbitraryIntStr({0xff}), "255");
    BOOST_CHECK_EQUAL(ArbitraryIntStr({0x00, 0x01}), "256");
    BOOST_CHECK_EQUAL(ArbitraryIntStr({0xff, 0x01}), "511");
    BOOST_CHECK_EQUAL(ArbitraryIntStr({0x00, 0x02}), "512");

    BOOST_CHECK_EQUAL(
        ArbitraryIntStr({0xff, 0xff, 0xff, 0xff}),
        "4294967295");
    BOOST_CHECK_EQUAL(
        ArbitraryIntStr({0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}),
        "309485009821345068724781055");
}

BOOST_AUTO_TEST_SUITE_END()
