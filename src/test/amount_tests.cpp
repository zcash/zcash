// Copyright (c) 2024 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "amount.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

#include <limits>
#include <string>

BOOST_FIXTURE_TEST_SUITE(amount_tests, BasicTestingSetup)

// ---------------------------------------------------------------------------
// MoneyRange tests
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(money_range_valid)
{
    BOOST_CHECK(MoneyRange(0));
    BOOST_CHECK(MoneyRange(1));
    BOOST_CHECK(MoneyRange(COIN));
    BOOST_CHECK(MoneyRange(MAX_MONEY));
    BOOST_CHECK(MoneyRange(MAX_MONEY / 2));
    BOOST_CHECK(MoneyRange(LEGACY_DEFAULT_FEE));
}

BOOST_AUTO_TEST_CASE(money_range_invalid)
{
    BOOST_CHECK(!MoneyRange(-1));
    BOOST_CHECK(!MoneyRange(-COIN));
    BOOST_CHECK(!MoneyRange(MAX_MONEY + 1));
    BOOST_CHECK(!MoneyRange(std::numeric_limits<CAmount>::max()));
    BOOST_CHECK(!MoneyRange(std::numeric_limits<CAmount>::min()));
}

// ---------------------------------------------------------------------------
// CFeeRate constructor tests
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(fee_rate_default_constructor)
{
    CFeeRate rate;
    BOOST_CHECK_EQUAL(rate.GetFeePerK(), 0);
    BOOST_CHECK_EQUAL(rate.GetFee(0), 0);
    BOOST_CHECK_EQUAL(rate.GetFee(100), 0);
}

BOOST_AUTO_TEST_CASE(fee_rate_explicit_constructor)
{
    CFeeRate rate(1000);
    BOOST_CHECK_EQUAL(rate.GetFeePerK(), 1000);
}

BOOST_AUTO_TEST_CASE(fee_rate_copy_constructor)
{
    CFeeRate original(5000);
    CFeeRate copy(original);
    BOOST_CHECK_EQUAL(copy.GetFeePerK(), 5000);
    BOOST_CHECK(copy == original);
}

BOOST_AUTO_TEST_CASE(fee_rate_from_fee_and_size)
{
    // 10000 zatoshis paid for 1000 bytes => 10000 per K
    CFeeRate rate(10000, 1000);
    BOOST_CHECK_EQUAL(rate.GetFeePerK(), 10000);

    // 5000 zatoshis paid for 500 bytes => 10000 per K
    CFeeRate rate2(5000, 500);
    BOOST_CHECK_EQUAL(rate2.GetFeePerK(), 10000);

    // Zero size should yield zero rate (no division by zero)
    CFeeRate rate3(10000, 0);
    BOOST_CHECK_EQUAL(rate3.GetFeePerK(), 0);
}

BOOST_AUTO_TEST_CASE(fee_rate_from_fee_and_size_overflow_protection)
{
    // Very large fee should be clamped and not overflow
    CAmount largeFee = std::numeric_limits<int64_t>::max() / 500;
    CFeeRate rate(largeFee, 1);
    // Should not be negative (overflow check)
    BOOST_CHECK(rate.GetFeePerK() >= 0);
}

// ---------------------------------------------------------------------------
// CFeeRate::GetFee tests
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(fee_rate_get_fee_basic)
{
    CFeeRate rate(1000); // 1000 zatoshis per 1000 bytes
    BOOST_CHECK_EQUAL(rate.GetFee(0), 0);
    BOOST_CHECK_EQUAL(rate.GetFee(1000), 1000);
    BOOST_CHECK_EQUAL(rate.GetFee(2000), 2000);
    BOOST_CHECK_EQUAL(rate.GetFee(500), 500);
}

BOOST_AUTO_TEST_CASE(fee_rate_get_fee_minimum)
{
    // When fee would round to 0 but rate is non-zero, minimum fee = rate
    CFeeRate rate(1); // 1 zatoshi per 1000 bytes
    BOOST_CHECK_EQUAL(rate.GetFee(1), 1); // Would be 0, so returns nSatoshisPerK
    BOOST_CHECK_EQUAL(rate.GetFee(999), 1); // Still rounds to 0, returns minimum
    BOOST_CHECK_EQUAL(rate.GetFee(1000), 1); // Exactly 1
}

BOOST_AUTO_TEST_CASE(fee_rate_get_fee_zero_rate)
{
    CFeeRate rate(0);
    BOOST_CHECK_EQUAL(rate.GetFee(0), 0);
    BOOST_CHECK_EQUAL(rate.GetFee(1000), 0);
    BOOST_CHECK_EQUAL(rate.GetFee(999999), 0);
}

// ---------------------------------------------------------------------------
// CFeeRate::GetFeeForRelay tests
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(fee_rate_get_fee_for_relay_capped)
{
    // High fee rate: relay fee should be capped at LEGACY_DEFAULT_FEE
    CFeeRate highRate(100000); // 100000 per K
    CAmount relayFee = highRate.GetFeeForRelay(10000);
    BOOST_CHECK_EQUAL(relayFee, LEGACY_DEFAULT_FEE);
}

BOOST_AUTO_TEST_CASE(fee_rate_get_fee_for_relay_low)
{
    // Low fee rate: relay fee should be the actual fee (below cap)
    CFeeRate lowRate(100); // 100 per K
    CAmount relayFee = lowRate.GetFeeForRelay(1000);
    BOOST_CHECK_EQUAL(relayFee, 100);
    BOOST_CHECK(relayFee <= LEGACY_DEFAULT_FEE);
}

BOOST_AUTO_TEST_CASE(fee_rate_get_fee_for_relay_zero)
{
    CFeeRate zeroRate(0);
    BOOST_CHECK_EQUAL(zeroRate.GetFeeForRelay(1000), 0);
}

// ---------------------------------------------------------------------------
// CFeeRate comparison operators
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(fee_rate_comparison_operators)
{
    CFeeRate low(100);
    CFeeRate mid(500);
    CFeeRate high(1000);
    CFeeRate midCopy(500);

    // Less than
    BOOST_CHECK(low < mid);
    BOOST_CHECK(mid < high);
    BOOST_CHECK(!(high < low));
    BOOST_CHECK(!(mid < midCopy));

    // Greater than
    BOOST_CHECK(high > mid);
    BOOST_CHECK(mid > low);
    BOOST_CHECK(!(low > high));
    BOOST_CHECK(!(mid > midCopy));

    // Equal
    BOOST_CHECK(mid == midCopy);
    BOOST_CHECK(!(low == high));

    // Less than or equal
    BOOST_CHECK(low <= mid);
    BOOST_CHECK(mid <= midCopy);
    BOOST_CHECK(!(high <= low));

    // Greater than or equal
    BOOST_CHECK(high >= mid);
    BOOST_CHECK(mid >= midCopy);
    BOOST_CHECK(!(low >= high));
}

// ---------------------------------------------------------------------------
// CFeeRate compound assignment
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(fee_rate_compound_addition)
{
    CFeeRate rate(100);
    CFeeRate increment(200);
    rate += increment;
    BOOST_CHECK_EQUAL(rate.GetFeePerK(), 300);

    // Adding zero
    CFeeRate zero;
    rate += zero;
    BOOST_CHECK_EQUAL(rate.GetFeePerK(), 300);
}

// ---------------------------------------------------------------------------
// CFeeRate::ToString tests
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(fee_rate_to_string)
{
    CFeeRate rate(0);
    std::string str = rate.ToString();
    BOOST_CHECK(str.find("ZEC") != std::string::npos);
    BOOST_CHECK(str.find("per 1000 bytes") != std::string::npos);

    CFeeRate oneZec(COIN);
    str = oneZec.ToString();
    BOOST_CHECK(str.find("1.00000000") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Constant sanity checks
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(amount_constants)
{
    BOOST_CHECK_EQUAL(COIN, 100000000LL);
    BOOST_CHECK_EQUAL(CENT, 1000000LL);
    BOOST_CHECK_EQUAL(MAX_MONEY, 21000000LL * COIN);
    BOOST_CHECK_EQUAL(LEGACY_DEFAULT_FEE, 1000);
    BOOST_CHECK_EQUAL(CURRENCY_UNIT, "ZEC");
    BOOST_CHECK_EQUAL(MINOR_CURRENCY_UNIT, "zatoshis");
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(fee_rate_negative_rate)
{
    // Negative fee rates should be representable (used in comparisons)
    CFeeRate negative(-100);
    CFeeRate positive(100);
    BOOST_CHECK(negative < positive);
    BOOST_CHECK_EQUAL(negative.GetFee(1000), -100);
}

BOOST_AUTO_TEST_CASE(money_range_boundary)
{
    BOOST_CHECK(MoneyRange(MAX_MONEY));
    BOOST_CHECK(!MoneyRange(MAX_MONEY + 1));
    BOOST_CHECK(MoneyRange(0));
    BOOST_CHECK(!MoneyRange(-1));
}

// ---------------------------------------------------------------------------
// ClampToMoneyRange tests
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(clamp_to_money_range)
{
    BOOST_CHECK_EQUAL(ClampToMoneyRange(0), 0);
    BOOST_CHECK_EQUAL(ClampToMoneyRange(COIN), COIN);
    BOOST_CHECK_EQUAL(ClampToMoneyRange(MAX_MONEY), MAX_MONEY);
    BOOST_CHECK_EQUAL(ClampToMoneyRange(MAX_MONEY + 1), MAX_MONEY);
    BOOST_CHECK_EQUAL(ClampToMoneyRange(-1), 0);
    BOOST_CHECK_EQUAL(ClampToMoneyRange(-COIN), 0);
    BOOST_CHECK_EQUAL(ClampToMoneyRange(std::numeric_limits<CAmount>::max()), MAX_MONEY);
    BOOST_CHECK_EQUAL(ClampToMoneyRange(std::numeric_limits<CAmount>::min()), 0);
}

// ---------------------------------------------------------------------------
// SafeMoneyAdd tests
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(safe_money_add_valid)
{
    CAmount result;
    BOOST_CHECK(SafeMoneyAdd(0, 0, result));
    BOOST_CHECK_EQUAL(result, 0);

    BOOST_CHECK(SafeMoneyAdd(COIN, COIN, result));
    BOOST_CHECK_EQUAL(result, 2 * COIN);

    BOOST_CHECK(SafeMoneyAdd(0, MAX_MONEY, result));
    BOOST_CHECK_EQUAL(result, MAX_MONEY);

    BOOST_CHECK(SafeMoneyAdd(MAX_MONEY, 0, result));
    BOOST_CHECK_EQUAL(result, MAX_MONEY);
}

BOOST_AUTO_TEST_CASE(safe_money_add_overflow)
{
    CAmount result;
    // Sum exceeds MAX_MONEY
    BOOST_CHECK(!SafeMoneyAdd(MAX_MONEY, 1, result));
    BOOST_CHECK_EQUAL(result, 0);

    // Arithmetic overflow
    BOOST_CHECK(!SafeMoneyAdd(std::numeric_limits<CAmount>::max(), 1, result));
    BOOST_CHECK_EQUAL(result, 0);

    // Negative values
    BOOST_CHECK(!SafeMoneyAdd(-1, 0, result));
    BOOST_CHECK_EQUAL(result, 0);
}

BOOST_AUTO_TEST_SUITE_END()
