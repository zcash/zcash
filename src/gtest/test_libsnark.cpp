#include <gtest/gtest.h>

#include "libsnark/algebra/fields/bigint.hpp"
#include "libsnark/algebra/curves/alt_bn128/alt_bn128_pp.hpp"
#include "libsnark/reductions/r1cs_to_qap/r1cs_to_qap.hpp"
#include "libsnark/relations/constraint_satisfaction_problems/r1cs/examples/r1cs_examples.hpp"
#include "zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "zk_proof_systems/ppzksnark/r1cs_ppzksnark/examples/run_r1cs_ppzksnark.hpp"
using namespace libsnark;

TEST(libsnark, bigint)
{
    static_assert(ULONG_MAX == 0xFFFFFFFFFFFFFFFFul, "unsigned long not 64-bit");
    static_assert(GMP_NUMB_BITS == 64, "GMP limb not 64-bit");

    const char *b1_decimal = "76749407";
    const char *b2_decimal = "435020359732196472065729437602";
    const char *b3_decimal = "33387554642372758038536799358397002014";
    const char *b2_binary = "0000000000000000000000000000010101111101101000000110100001011010"
                            "1101101010001001000001101000101000100110011001110001111110100010";

    bigint<1> b0 = bigint<1>(0ul);
    bigint<1> b1 = bigint<1>(b1_decimal);
    bigint<2> b2 = bigint<2>(b2_decimal);

    ASSERT_TRUE(b0.as_ulong() == 0ul);
    ASSERT_TRUE(b0.is_zero());
    ASSERT_TRUE(b1.as_ulong() == 76749407ul);
    ASSERT_TRUE(!(b1.is_zero()));
    ASSERT_TRUE(b2.as_ulong() == 15747124762497195938ul);
    ASSERT_TRUE(!(b2.is_zero()));
    ASSERT_TRUE(b0 != b1);
    ASSERT_TRUE(!(b0 == b1));

    ASSERT_TRUE(b2.max_bits() == 128);
    ASSERT_TRUE(b2.num_bits() == 99);
    for (size_t i = 0; i < 128; i++) {
        ASSERT_TRUE(b2.test_bit(i) == (b2_binary[127-i] == '1'));
    }

    bigint<3> b3 = b2 * b1;

    ASSERT_TRUE(b3 == bigint<3>(b3_decimal));
    ASSERT_TRUE(!(b3.is_zero()));

    bigint<3> b3a { b3 };
    ASSERT_TRUE(b3a == bigint<3>(b3_decimal));
    ASSERT_TRUE(b3a == b3);
    ASSERT_TRUE(!(b3a.is_zero()));

    mpz_t m3;
    mpz_init(m3);
    b3.to_mpz(m3);
    bigint<3> b3b { m3 };
    ASSERT_TRUE(b3b == b3);

    bigint<2> quotient;
    bigint<2> remainder;
    bigint<3>::div_qr(quotient, remainder, b3, b2);
    ASSERT_TRUE(quotient.num_bits() < GMP_NUMB_BITS);
    ASSERT_TRUE(quotient.as_ulong() == b1.as_ulong());
    bigint<1> b1inc = bigint<1>("76749408");
    bigint<1> b1a = quotient.shorten(b1inc, "test");
    ASSERT_TRUE(b1a == b1);
    ASSERT_TRUE(remainder.is_zero());
    remainder.limit(b2, "test");

    try {
        (void)(quotient.shorten(b1, "test"));
        ASSERT_TRUE(false);
    } catch (std::domain_error) {}
    try {
        remainder.limit(remainder, "test");
        ASSERT_TRUE(false);
    } catch (std::domain_error) {}

    bigint<1> br = bigint<1>("42");
    b3 += br;
    ASSERT_TRUE(b3 != b3a);
    ASSERT_TRUE(b3 > b3a);
    ASSERT_TRUE(!(b3a > b3));

    bigint<3>::div_qr(quotient, remainder, b3, b2);
    ASSERT_TRUE(quotient.num_bits() < GMP_NUMB_BITS);
    ASSERT_TRUE(quotient.as_ulong() == b1.as_ulong());
    ASSERT_TRUE(remainder.num_bits() < GMP_NUMB_BITS);
    ASSERT_TRUE(remainder.as_ulong() == 42);

    b3a.clear();
    ASSERT_TRUE(b3a.is_zero());
    ASSERT_TRUE(b3a.num_bits() == 0);
    ASSERT_TRUE(!(b3.is_zero()));

    bigint<4> bx = bigint<4>().randomize();
    bigint<4> by = bigint<4>().randomize();
    ASSERT_TRUE(!(bx == by));

    // TODO: test serialization
}

namespace {
    template<typename FieldT>
    void test_field()
    {
        bigint<1> rand1 = bigint<1>("76749407");
        bigint<1> rand2 = bigint<1>("44410867");
        bigint<1> randsum = bigint<1>("121160274");

        FieldT zero = FieldT::zero();
        FieldT one = FieldT::one();
        FieldT a = FieldT::random_element();
        FieldT a_ser;
        a_ser = reserialize<FieldT>(a);
        ASSERT_TRUE(a_ser == a);

        FieldT b = FieldT::random_element();
        FieldT c = FieldT::random_element();
        FieldT d = FieldT::random_element();

        ASSERT_TRUE(a != zero);
        ASSERT_TRUE(a != one);

        ASSERT_TRUE(a * a == a.squared());
        ASSERT_TRUE((a + b).squared() == a.squared() + a*b + b*a + b.squared());
        ASSERT_TRUE((a + b)*(c + d) == a*c + a*d + b*c + b*d);
        ASSERT_TRUE(a - b == a + (-b));
        ASSERT_TRUE(a - b == (-b) + a);

        ASSERT_TRUE((a ^ rand1) * (a ^ rand2) == (a^randsum));

        ASSERT_TRUE(a * a.inverse() == one);
        ASSERT_TRUE((a + b) * c.inverse() == a * c.inverse() + (b.inverse() * c).inverse());

    }

    template<typename FieldT>
    void test_sqrt()
    {
        for (size_t i = 0; i < 100; ++i)
        {
            FieldT a = FieldT::random_element();
            FieldT asq = a.squared();
            ASSERT_TRUE(asq.sqrt() == a || asq.sqrt() == -a);
        }
    }

    template<typename FieldT>
    void test_two_squarings()
    {
        FieldT a = FieldT::random_element();
        ASSERT_TRUE(a.squared() == a * a);
        ASSERT_TRUE(a.squared() == a.squared_complex());
        ASSERT_TRUE(a.squared() == a.squared_karatsuba());
    }

    template<typename FieldT>
    void test_Frobenius()
    {
        FieldT a = FieldT::random_element();
        ASSERT_TRUE(a.Frobenius_map(0) == a);
        FieldT a_q = a ^ FieldT::base_field_char();
        for (size_t power = 1; power < 10; ++power)
        {
            const FieldT a_qi = a.Frobenius_map(power);
            ASSERT_TRUE(a_qi == a_q);

            a_q = a_q ^ FieldT::base_field_char();
        }
    }

    template<typename FieldT>
    void test_unitary_inverse()
    {
        ASSERT_TRUE(FieldT::extension_degree() % 2 == 0);
        FieldT a = FieldT::random_element();
        FieldT aqcubed_minus1 = a.Frobenius_map(FieldT::extension_degree()/2) * a.inverse();
        ASSERT_TRUE(aqcubed_minus1.inverse() == aqcubed_minus1.unitary_inverse());
    }

    template<typename ppT>
    void test_all_fields()
    {
        test_field<Fr<ppT> >();
        test_field<Fq<ppT> >();
        test_field<Fqe<ppT> >();
        test_field<Fqk<ppT> >();

        test_sqrt<Fr<ppT> >();
        test_sqrt<Fq<ppT> >();
        test_sqrt<Fqe<ppT> >();

        test_Frobenius<Fqe<ppT> >();
        test_Frobenius<Fqk<ppT> >();

        test_unitary_inverse<Fqk<ppT> >();
    }

    TEST(libsnark, fields)
    {
        test_field<alt_bn128_Fq6>();
        test_Frobenius<alt_bn128_Fq6>();
        test_all_fields<alt_bn128_pp>();
    }
}

namespace {
    template<typename GroupT>
    void test_mixed_add()
    {
        GroupT base, el, result;

        base = GroupT::zero();
        el = GroupT::zero();
        el.to_special();
        result = base.mixed_add(el);
        ASSERT_TRUE(result == base + el);

        base = GroupT::zero();
        el = GroupT::random_element();
        el.to_special();
        result = base.mixed_add(el);
        ASSERT_TRUE(result == base + el);

        base = GroupT::random_element();
        el = GroupT::zero();
        el.to_special();
        result = base.mixed_add(el);
        ASSERT_TRUE(result == base + el);

        base = GroupT::random_element();
        el = GroupT::random_element();
        el.to_special();
        result = base.mixed_add(el);
        ASSERT_TRUE(result == base + el);

        base = GroupT::random_element();
        el = base;
        el.to_special();
        result = base.mixed_add(el);
        ASSERT_TRUE(result == base.dbl());
    }

    template<typename GroupT>
    void test_group()
    {
        bigint<1> rand1 = bigint<1>("76749407");
        bigint<1> rand2 = bigint<1>("44410867");
        bigint<1> randsum = bigint<1>("121160274");

        GroupT zero = GroupT::zero();
        ASSERT_TRUE(zero == zero);
        GroupT one = GroupT::one();
        ASSERT_TRUE(one == one);
        GroupT two = bigint<1>(2l) * GroupT::one();
        ASSERT_TRUE(two == two);
        GroupT five = bigint<1>(5l) * GroupT::one();

        GroupT three = bigint<1>(3l) * GroupT::one();
        GroupT four = bigint<1>(4l) * GroupT::one();

        ASSERT_TRUE(two+five == three+four);

        GroupT a = GroupT::random_element();
        GroupT b = GroupT::random_element();

        ASSERT_TRUE(one != zero);
        ASSERT_TRUE(a != zero);
        ASSERT_TRUE(a != one);

        ASSERT_TRUE(b != zero);
        ASSERT_TRUE(b != one);

        ASSERT_TRUE(a.dbl() == a + a);
        ASSERT_TRUE(b.dbl() == b + b);
        ASSERT_TRUE(one.add(two) == three);
        ASSERT_TRUE(two.add(one) == three);
        ASSERT_TRUE(a + b == b + a);
        ASSERT_TRUE(a - a == zero);
        ASSERT_TRUE(a - b == a + (-b));
        ASSERT_TRUE(a - b == (-b) + a);

        // handle special cases
        ASSERT_TRUE(zero + (-a) == -a);
        ASSERT_TRUE(zero - a == -a);
        ASSERT_TRUE(a - zero == a);
        ASSERT_TRUE(a + zero == a);
        ASSERT_TRUE(zero + a == a);

        ASSERT_TRUE((a + b).dbl() == (a + b) + (b + a));
        ASSERT_TRUE(bigint<1>("2") * (a + b) == (a + b) + (b + a));

        ASSERT_TRUE((rand1 * a) + (rand2 * a) == (randsum * a));

        ASSERT_TRUE(GroupT::order() * a == zero);
        ASSERT_TRUE(GroupT::order() * one == zero);
        ASSERT_TRUE((GroupT::order() * a) - a != zero);
        ASSERT_TRUE((GroupT::order() * one) - one != zero);

        test_mixed_add<GroupT>();
    }

    template<typename GroupT>
    void test_mul_by_q()
    {
        GroupT a = GroupT::random_element();
        ASSERT_TRUE((GroupT::base_field_char()*a) == a.mul_by_q());
    }

    template<typename GroupT>
    void test_output()
    {
        GroupT g = GroupT::zero();

        for (size_t i = 0; i < 1000; ++i)
        {
            std::stringstream ss;
            ss << g;
            GroupT gg;
            ss >> gg;
            ASSERT_TRUE(g == gg);
            /* use a random point in next iteration */
            g = GroupT::random_element();
        }
    }

    TEST(libsnark, groups)
    {
        test_group<G1<alt_bn128_pp> >();
        test_output<G1<alt_bn128_pp> >();
        test_group<G2<alt_bn128_pp> >();
        test_output<G2<alt_bn128_pp> >();
        test_mul_by_q<G2<alt_bn128_pp> >();
    }
}

namespace {
    template<typename ppT>
    void pairing_test()
    {
        GT<ppT> GT_one = GT<ppT>::one();

        G1<ppT> P = (Fr<ppT>::random_element()) * G1<ppT>::one();
        G2<ppT> Q = (Fr<ppT>::random_element()) * G2<ppT>::one();

        Fr<ppT> s = Fr<ppT>::random_element();
        G1<ppT> sP = s * P;
        G2<ppT> sQ = s * Q;

        GT<ppT> ans1 = ppT::reduced_pairing(sP, Q);
        GT<ppT> ans2 = ppT::reduced_pairing(P, sQ);
        GT<ppT> ans3 = ppT::reduced_pairing(P, Q)^s;
        ASSERT_TRUE(ans1 == ans2);
        ASSERT_TRUE(ans2 == ans3);

        ASSERT_TRUE(ans1 != GT_one);
        ASSERT_TRUE((ans1^Fr<ppT>::field_char()) == GT_one);
    }

    template<typename ppT>
    void double_miller_loop_test()
    {
        const G1<ppT> P1 = (Fr<ppT>::random_element()) * G1<ppT>::one();
        const G1<ppT> P2 = (Fr<ppT>::random_element()) * G1<ppT>::one();
        const G2<ppT> Q1 = (Fr<ppT>::random_element()) * G2<ppT>::one();
        const G2<ppT> Q2 = (Fr<ppT>::random_element()) * G2<ppT>::one();

        const G1_precomp<ppT> prec_P1 = ppT::precompute_G1(P1);
        const G1_precomp<ppT> prec_P2 = ppT::precompute_G1(P2);
        const G2_precomp<ppT> prec_Q1 = ppT::precompute_G2(Q1);
        const G2_precomp<ppT> prec_Q2 = ppT::precompute_G2(Q2);

        const Fqk<ppT> ans_1 = ppT::miller_loop(prec_P1, prec_Q1);
        const Fqk<ppT> ans_2 = ppT::miller_loop(prec_P2, prec_Q2);
        const Fqk<ppT> ans_12 = ppT::double_miller_loop(prec_P1, prec_Q1, prec_P2, prec_Q2);
        ASSERT_TRUE(ans_1 * ans_2 == ans_12);
    }

    template<typename ppT>
    void affine_pairing_test()
    {
        GT<ppT> GT_one = GT<ppT>::one();

        G1<ppT> P = (Fr<ppT>::random_element()) * G1<ppT>::one();
        G2<ppT> Q = (Fr<ppT>::random_element()) * G2<ppT>::one();

        Fr<ppT> s = Fr<ppT>::random_element();
        G1<ppT> sP = s * P;
        G2<ppT> sQ = s * Q;

        GT<ppT> ans1 = ppT::affine_reduced_pairing(sP, Q);
        GT<ppT> ans2 = ppT::affine_reduced_pairing(P, sQ);
        GT<ppT> ans3 = ppT::affine_reduced_pairing(P, Q)^s;
        ASSERT_TRUE(ans1 == ans2);
        ASSERT_TRUE(ans2 == ans3);

        ASSERT_TRUE(ans1 != GT_one);
        ASSERT_TRUE((ans1^Fr<ppT>::field_char()) == GT_one);
    }

    TEST(libsnark, bilinearity)
    {
        pairing_test<alt_bn128_pp>();
        double_miller_loop_test<alt_bn128_pp>();
    }
}

namespace {
    template<typename FieldT>
    void test_qap(const size_t qap_degree, const size_t num_inputs, const bool binary_input)
    {
        /*
          We construct an instance where the QAP degree is qap_degree.
          So we generate an instance of R1CS where the number of constraints qap_degree - num_inputs - 1.
          See the transformation from R1CS to QAP for why this is the case.
          So we need that qap_degree >= num_inputs + 1.
        */
        ASSERT_TRUE(num_inputs + 1 <= qap_degree);

        const size_t num_constraints = qap_degree - num_inputs - 1;

        r1cs_example<FieldT> example;
        if (binary_input)
        {
            example = generate_r1cs_example_with_binary_input<FieldT>(num_constraints, num_inputs);
        }
        else
        {
            example = generate_r1cs_example_with_field_input<FieldT>(num_constraints, num_inputs);
        }
        ASSERT_TRUE(example.constraint_system.is_satisfied(example.primary_input, example.auxiliary_input));

        const FieldT t = FieldT::random_element(),
        d1 = FieldT::random_element(),
        d2 = FieldT::random_element(),
        d3 = FieldT::random_element();

        qap_instance<FieldT> qap_inst_1 = r1cs_to_qap_instance_map(example.constraint_system);

        qap_instance_evaluation<FieldT> qap_inst_2 = r1cs_to_qap_instance_map_with_evaluation(example.constraint_system, t);

        qap_witness<FieldT> qap_wit = r1cs_to_qap_witness_map(example.constraint_system, example.primary_input, example.auxiliary_input, d1, d2, d3);

        ASSERT_TRUE(qap_inst_1.is_satisfied(qap_wit));
        ASSERT_TRUE(qap_inst_2.is_satisfied(qap_wit));
    }

    TEST(libsnark, qap)
    {
        const size_t num_inputs = 10;

        test_qap<Fr<alt_bn128_pp> >(1ul << 21, num_inputs, true);
        test_qap<Fr<alt_bn128_pp> >(1ul << 21, num_inputs, false);
    }
}

namespace {
    template<typename ppT>
    void test_r1cs_ppzksnark(size_t num_constraints,
                             size_t input_size)
    {
        r1cs_example<Fr<ppT> > example = generate_r1cs_example_with_binary_input<Fr<ppT> >(num_constraints, input_size);
        example.constraint_system.swap_AB_if_beneficial();
        ASSERT_TRUE(run_r1cs_ppzksnark<ppT>(example, true));
    }

    TEST(libsnark, r1cs_ppzksnark)
    {
        test_r1cs_ppzksnark<alt_bn128_pp>(1000, 20);
    }
}
