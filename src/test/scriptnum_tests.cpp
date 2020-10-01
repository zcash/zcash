// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "scriptnum10.h"
#include "script/script.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>
#include <limits.h>
#include <stdint.h>

BOOST_FIXTURE_TEST_SUITE(scriptnum_tests, BasicTestingSetup)

static const int64_t values[] = \
{ 0, 1, CHAR_MIN, CHAR_MAX, UCHAR_MAX, SHRT_MIN, USHRT_MAX, INT_MIN, INT_MAX, UINT_MAX, INT64_MIN, INT64_MAX };
static const int64_t offsets[] = { 1, 0x79, 0x80, 0x81, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000};


static bool addition_in_range(int64_t a, int64_t b) {
    // intentionally excludes cases where the result would be INT64_MIN
    return b > 0 ? (a <= INT64_MAX - b) : (a > INT64_MIN - b);
}

static bool subtraction_in_range(int64_t a, int64_t b) {
    // intentionally excludes cases where the result would be INT64_MIN
    return b >= 0 ? (a > INT64_MIN + b) : (a <= INT64_MAX + b);
}

static bool value_in_range(int64_t a) {
    return a != INT64_MIN;
}

static bool verify(const CScriptNum10& bignum, const CScriptNum& scriptnum)
{
    return bignum.getvch() == scriptnum.getvch() && bignum.getint() == scriptnum.getint();
}

static void CheckCreateVch(int64_t num)
{
    CScriptNum10 bignum(num);
    CScriptNum scriptnum(num);
    BOOST_CHECK(verify(bignum, scriptnum));

    CScriptNum10 bignum2(bignum.getvch(), false);
    std::vector<unsigned char> scriptnum_vch = scriptnum.getvch();
    // The 9-byte case is exercised by the 'intmin' test.
    BOOST_CHECK(scriptnum_vch.size() <= 8);
    CScriptNum scriptnum2(scriptnum_vch, false);
    BOOST_CHECK(verify(bignum2, scriptnum2));

    CScriptNum10 bignum3(scriptnum2.getvch(), false);
    CScriptNum scriptnum3(bignum2.getvch(), false);
    BOOST_CHECK(verify(bignum3, scriptnum3));
}

static void CheckCreateInt(int64_t num)
{
    CScriptNum10 bignum(num);
    CScriptNum scriptnum(num);
    BOOST_CHECK(verify(bignum, scriptnum));
    BOOST_CHECK(verify(CScriptNum10(bignum.getint()), CScriptNum(scriptnum.getint())));
    BOOST_CHECK(verify(CScriptNum10(scriptnum.getint()), CScriptNum(bignum.getint())));
    BOOST_CHECK(verify(CScriptNum10(CScriptNum10(scriptnum.getint()).getint()), CScriptNum(CScriptNum(bignum.getint()).getint())));
}


static void CheckAdd(int64_t num1, int64_t num2)
{
    const CScriptNum10 bignum1(num1);
    const CScriptNum10 bignum2(num2);
    const CScriptNum scriptnum1(num1);
    const CScriptNum scriptnum2(num2);
    CScriptNum10 bignum3(num1);
    CScriptNum10 bignum4(num1);
    CScriptNum scriptnum3(num1);
    CScriptNum scriptnum4(num1);

    if (addition_in_range(num1, num2))
    {
        BOOST_CHECK(verify(bignum1 + bignum2, scriptnum1 + scriptnum2));
        BOOST_CHECK(verify(bignum1 + bignum2, scriptnum1 + num2));
        BOOST_CHECK(verify(bignum1 + bignum2, scriptnum2 + num1));
    }
}

static void CheckNegate(int64_t num)
{
    const CScriptNum10 bignum(num);
    const CScriptNum scriptnum(num);

    BOOST_CHECK(verify(-bignum, -scriptnum));
}

static void CheckSubtract(int64_t num1, int64_t num2)
{
    const CScriptNum10 bignum1(num1);
    const CScriptNum10 bignum2(num2);
    const CScriptNum scriptnum1(num1);
    const CScriptNum scriptnum2(num2);
    bool invalid = false;

    if (subtraction_in_range(num1, num2))
    {
        BOOST_CHECK(verify(bignum1 - bignum2, scriptnum1 - scriptnum2));
        BOOST_CHECK(verify(bignum1 - bignum2, scriptnum1 - num2));
    }

    if (subtraction_in_range(num2, num1))
    {
        BOOST_CHECK(verify(bignum2 - bignum1, scriptnum2 - scriptnum1));
        BOOST_CHECK(verify(bignum2 - bignum1, scriptnum2 - num1));
    }
}

static void CheckCompare(int64_t num1, int64_t num2)
{
    const CScriptNum10 bignum1(num1);
    const CScriptNum10 bignum2(num2);
    const CScriptNum scriptnum1(num1);
    const CScriptNum scriptnum2(num2);

    BOOST_CHECK((bignum1 == bignum1) == (scriptnum1 == scriptnum1));
    BOOST_CHECK((bignum1 != bignum1) == (scriptnum1 != scriptnum1));
    BOOST_CHECK((bignum1 <  bignum1) == (scriptnum1 <  scriptnum1));
    BOOST_CHECK((bignum1 >  bignum1) == (scriptnum1 >  scriptnum1));
    BOOST_CHECK((bignum1 >= bignum1) == (scriptnum1 >= scriptnum1));
    BOOST_CHECK((bignum1 <= bignum1) == (scriptnum1 <= scriptnum1));

    BOOST_CHECK((bignum1 == bignum1) == (scriptnum1 == num1));
    BOOST_CHECK((bignum1 != bignum1) == (scriptnum1 != num1));
    BOOST_CHECK((bignum1 <  bignum1) == (scriptnum1 <  num1));
    BOOST_CHECK((bignum1 >  bignum1) == (scriptnum1 >  num1));
    BOOST_CHECK((bignum1 >= bignum1) == (scriptnum1 >= num1));
    BOOST_CHECK((bignum1 <= bignum1) == (scriptnum1 <= num1));

    BOOST_CHECK((bignum1 == bignum2) == (scriptnum1 == scriptnum2));
    BOOST_CHECK((bignum1 != bignum2) == (scriptnum1 != scriptnum2));
    BOOST_CHECK((bignum1 <  bignum2) == (scriptnum1 <  scriptnum2));
    BOOST_CHECK((bignum1 >  bignum2) == (scriptnum1 >  scriptnum2));
    BOOST_CHECK((bignum1 >= bignum2) == (scriptnum1 >= scriptnum2));
    BOOST_CHECK((bignum1 <= bignum2) == (scriptnum1 <= scriptnum2));

    BOOST_CHECK((bignum1 == bignum2) == (scriptnum1 == num2));
    BOOST_CHECK((bignum1 != bignum2) == (scriptnum1 != num2));
    BOOST_CHECK((bignum1 < bignum2)  == (scriptnum1 < num2));
    BOOST_CHECK((bignum1 > bignum2)  == (scriptnum1 > num2));
    BOOST_CHECK((bignum1 >= bignum2) == (scriptnum1 >= num2));
    BOOST_CHECK((bignum1 <= bignum2) == (scriptnum1 <= num2));
}

static void RunCreate(int64_t num)
{
    CheckCreateInt(num);
    CScriptNum scriptnum(num);
    if (scriptnum.getvch().size() <= CScriptNum::nDefaultMaxNumSize)
    {
        CheckCreateVch(num);
    } else {
        BOOST_CHECK_THROW(CheckCreateVch(num), scriptnum10_error);
    }
}

static void RunOperators(int64_t num1, int64_t num2)
{
    CheckAdd(num1, num2);
    CheckSubtract(num1, num2);
    CheckNegate(num1);
    CheckCompare(num1, num2);
}

BOOST_AUTO_TEST_CASE(creation)
{
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        for (size_t j = 0; j < sizeof(offsets) / sizeof(offsets[0]); ++j)
        {
            if (value_in_range(values[i])) {
                RunCreate(values[i]);
            }
            if (addition_in_range(values[i], offsets[j])) {
                RunCreate(values[i] + offsets[j]);
            }
            if (subtraction_in_range(values[i], offsets[j])) {
                RunCreate(values[i] - offsets[j]);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(operators)
{
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        for (size_t j = 0; j < sizeof(values) / sizeof(values[0]); ++j)
        {
            if (value_in_range(values[i]))
            {
                RunOperators(values[i], values[i]);
                RunOperators(values[i], -values[i]);
            }
            if (value_in_range(values[i]) && value_in_range(values[j]))
            {
               RunOperators(values[i], values[j]);
               RunOperators(values[i], -values[j]);
            }
            if (addition_in_range(values[i], values[j]) && value_in_range(values[j]))
            {
                RunOperators(values[i] + values[j], values[j]);
                RunOperators(values[i] + values[j], -values[j]);
                RunOperators(values[i] + values[j], values[i] + values[j]);
            }
            if (subtraction_in_range(values[i], values[j]) && value_in_range(values[j]))
            {
                RunOperators(values[i] - values[j], values[j]);
                RunOperators(values[i] - values[j], -values[j]);
                RunOperators(values[i] - values[j], values[i] - values[j]);
            }
            if (addition_in_range(values[i], values[j]) && subtraction_in_range(values[i], values[j]))
            {
                RunOperators(values[i] + values[j], values[i] - values[j]);
                RunOperators(values[i] - values[j], values[i] + values[j]);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(intmin)
{
    // INT64_MIN encodes to the buggy encoding.
    const CScriptNum sn(INT64_MIN);
    std::vector<unsigned char> buggy_int64_min_encoding = {0, 0, 0, 0, 0, 0, 0, 128, 128};
    BOOST_CHECK(sn.getvch() == buggy_int64_min_encoding);

    // The buggy INT64_MIN encoding decodes correctly.
    const CScriptNum sn2(buggy_int64_min_encoding, true, 9);
    BOOST_CHECK(sn2 == INT64_MIN);
    BOOST_CHECK(sn2.getvch() == buggy_int64_min_encoding);
    // getint() saturates at the min/max value of the int type
    BOOST_CHECK((sn2.getint()) == std::numeric_limits<int>::min());

    // Should throw for any other 9+ byte encoding.
    std::vector<unsigned char> invalid_nine_bytes = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    BOOST_CHECK_THROW (CScriptNum sn3(invalid_nine_bytes, false, 9), scriptnum_error);
}

BOOST_AUTO_TEST_SUITE_END()
