/**
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "algebra/fields/bigint.hpp"

using namespace libsnark;

void test_bigint()
{
    static_assert(GMP_NUMB_BITS == 64, "GMP limb not 64-bit");

    const char *b1_decimal = "76749407";
    const char *b2_decimal = "435020359732196472065729437602";
    const char *b3_decimal = "33387554642372758038536799358397002014";
    const char *b2_binary = "0000000000000000000000000000010101111101101000000110100001011010"
                            "1101101010001001000001101000101000100110011001110001111110100010";

    bigint<1> b0 = bigint<1>(UINT64_C(0));
    bigint<1> b1 = bigint<1>(b1_decimal);
    bigint<2> b2 = bigint<2>(b2_decimal);

    EXPECT_EQ(b0.as_uint64(), UINT64_C(0));
    EXPECT_TRUE(b0.is_zero());
    EXPECT_EQ(b1.as_uint64(), UINT64_C(76749407));
    EXPECT_FALSE(b1.is_zero());
    EXPECT_EQ(b2.as_uint64(), UINT64_C(15747124762497195938));
    EXPECT_FALSE(b2.is_zero());
    EXPECT_NE(b0, b1);
    EXPECT_FALSE(b0 == b1);

    EXPECT_EQ(b2.max_bits(), 128u);
    EXPECT_EQ(b2.num_bits(), 99u);
    for (size_t i = 0; i < 128; i++) {
        assert(b2.test_bit(i) == (b2_binary[127-i] == '1'));
    }

    bigint<3> b3 = b2 * b1;

    assert(b3 == bigint<3>(b3_decimal));
    assert(!(b3.is_zero()));

    bigint<3> b3a { b3 };
    assert(b3a == bigint<3>(b3_decimal));
    assert(b3a == b3);
    assert(!(b3a.is_zero()));

    mpz_t m3;
    mpz_init(m3);
    b3.to_mpz(m3);
    bigint<3> b3b { m3 };
    assert(b3b == b3);

    bigint<2> quotient;
    bigint<2> remainder;
    bigint<3>::div_qr(quotient, remainder, b3, b2);
    EXPECT_LT(quotient.num_bits(), static_cast<size_t>(GMP_NUMB_BITS));
    EXPECT_EQ(quotient.as_uint64(), b1.as_uint64());
    bigint<1> b1inc = bigint<1>("76749408");
    bigint<1> b1a = quotient.shorten(b1inc, "test");
    assert(b1a == b1);
    assert(remainder.is_zero());
    remainder.limit(b2, "test");

    try {
        (void)(quotient.shorten(b1, "test"));
        assert(false);
    } catch (std::domain_error) {}
    try {
        remainder.limit(remainder, "test");
        assert(false);
    } catch (std::domain_error) {}

    bigint<1> br = bigint<1>("42");
    b3 += br;
    assert(b3 != b3a);
    assert(b3 > b3a);
    assert(!(b3a > b3));

    bigint<3>::div_qr(quotient, remainder, b3, b2);
    EXPECT_LT(quotient.num_bits(), static_cast<size_t>(GMP_NUMB_BITS));
    EXPECT_EQ(quotient.as_uint64(), b1.as_uint64());
    EXPECT_LT(remainder.num_bits(), static_cast<size_t>(GMP_NUMB_BITS));
    EXPECT_EQ(remainder.as_uint64(), 42u);

    b3a.clear();
    EXPECT_TRUE(b3a.is_zero());
    EXPECT_EQ(b3a.num_bits(), 0u);
    EXPECT_FALSE(b3.is_zero());

    bigint<4> bx = bigint<4>().randomize();
    bigint<4> by = bigint<4>().randomize();
    assert(!(bx == by));

    // TODO: test serialization
}

int main(void)
{
    test_bigint();
    return 0;
}

