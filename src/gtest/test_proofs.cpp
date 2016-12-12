#include <gtest/gtest.h>
#include "zcash/Proof.hpp"

#include <iostream>

#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "relations/constraint_satisfaction_problems/r1cs/examples/r1cs_examples.hpp"

using namespace libzcash;

typedef libsnark::default_r1cs_ppzksnark_pp curve_pp;
typedef libsnark::default_r1cs_ppzksnark_pp::G1_type curve_G1;
typedef libsnark::default_r1cs_ppzksnark_pp::G2_type curve_G2;
typedef libsnark::default_r1cs_ppzksnark_pp::GT_type curve_GT;
typedef libsnark::default_r1cs_ppzksnark_pp::Fp_type curve_Fr;
typedef libsnark::default_r1cs_ppzksnark_pp::Fq_type curve_Fq;
typedef libsnark::default_r1cs_ppzksnark_pp::Fqe_type curve_Fq2;

#include "streams.h"
#include "version.h"
#include "utilstrencodings.h"

TEST(proofs, sqrt_zero)
{
    ASSERT_TRUE(curve_Fq::zero() == curve_Fq::zero().sqrt());
    ASSERT_TRUE(curve_Fq2::zero() == curve_Fq2::zero().sqrt());
}

TEST(proofs, sqrt_fq)
{
    // Poor man's PRNG
    curve_Fq acc = curve_Fq("348957923485290374852379485") ^ 1000;

    size_t quadratic_residues = 0;
    size_t quadratic_nonresidues = 0;

    for (size_t i = 1; i < 1000; i++) {
        try {
            acc += curve_Fq("45634563456") ^ i;

            curve_Fq x = acc.sqrt();
            ASSERT_TRUE((x*x) == acc);
            quadratic_residues += 1;
        } catch (std::runtime_error &e) {
            quadratic_nonresidues += 1;
        }
    }

    // Half of all nonzero elements in Fp are quadratic residues
    ASSERT_TRUE(quadratic_residues == 511);
    ASSERT_TRUE(quadratic_nonresidues == 488);

    for (size_t i = 0; i < 1000; i++) {
        curve_Fq x = curve_Fq::random_element();
        curve_Fq x2 = x * x;

        ASSERT_TRUE((x2.sqrt() == x) || (x2.sqrt() == -x));
    }

    // Test vectors
    ASSERT_TRUE(
        curve_Fq("5204065062716160319596273903996315000119019512886596366359652578430118331601")
        ==
        curve_Fq("348579348568").sqrt()
    );
    ASSERT_THROW(curve_Fq("348579348569").sqrt(), std::runtime_error);
}

TEST(proofs, sqrt_fq2)
{
    curve_Fq2 acc = curve_Fq2(
        curve_Fq("3456293840592348059238409578239048769348760238476029347885092384059238459834") ^ 1000,
        curve_Fq("2394578084760439457823945729347502374590283479582739485723945729384759823745") ^ 1000
    );

    size_t quadratic_residues = 0;
    size_t quadratic_nonresidues = 0;

    for (size_t i = 1; i < 1000; i++) {
        try {
            acc = acc + curve_Fq2(
                curve_Fq("5204065062716160319596273903996315000119019512886596366359652578430118331601") ^ i,
                curve_Fq("348957923485290374852379485348957923485290374852379485348957923485290374852") ^ i
            );

            curve_Fq2 x = acc.sqrt();
            ASSERT_TRUE((x*x) == acc);
            quadratic_residues += 1;
        } catch (std::runtime_error &e) {
            quadratic_nonresidues += 1;
        }
    }

    // Half of all nonzero elements in Fp^k are quadratic residues as long
    // as p != 2
    ASSERT_TRUE(quadratic_residues == 505);
    ASSERT_TRUE(quadratic_nonresidues == 494);

    for (size_t i = 0; i < 1000; i++) {
        curve_Fq2 x = curve_Fq2::random_element();
        curve_Fq2 x2 = x * x;

        ASSERT_TRUE((x2.sqrt() == x) || (x2.sqrt() == -x));
    }

    // Test vectors
    ASSERT_THROW(curve_Fq2(
        curve_Fq("2"),
        curve_Fq("1")
    ).sqrt(), std::runtime_error);

    ASSERT_THROW(curve_Fq2(
        curve_Fq("3345897230485723946872934576923485762803457692345760237495682347502347589473"),
        curve_Fq("1234912378405347958234756902345768290345762348957605678245967234857634857676")
    ).sqrt(), std::runtime_error);

    curve_Fq2 x = curve_Fq2(
        curve_Fq("12844195307879678418043983815760255909500142247603239203345049921980497041944"),
        curve_Fq("7476417578426924565731404322659619974551724117137577781074613937423560117731")
    );

    curve_Fq2 nx = -x;

    curve_Fq2 x2 = curve_Fq2(
        curve_Fq("3345897230485723946872934576923485762803457692345760237495682347502347589474"),
        curve_Fq("1234912378405347958234756902345768290345762348957605678245967234857634857676")
    );

    ASSERT_TRUE(x == x2.sqrt());
    ASSERT_TRUE(nx == -x2.sqrt());
    ASSERT_TRUE(x*x == x2);
    ASSERT_TRUE(nx*nx == x2);
}

TEST(proofs, size_is_expected)
{
    ZCProof p;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << p;

    ASSERT_EQ(ss.size(), 296);
}

TEST(proofs, fq_serializes_properly)
{
    for (size_t i = 0; i < 1000; i++) {
        curve_Fq e = curve_Fq::random_element();

        Fq e2(e);

        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << e2;

        Fq e3;
        ss >> e3;

        curve_Fq e4 = e3.to_libsnark_fq<curve_Fq>();

        ASSERT_TRUE(e == e4);
    }
}

TEST(proofs, fq2_serializes_properly)
{
    for (size_t i = 0; i < 1000; i++) {
        curve_Fq2 e = curve_Fq2::random_element();

        Fq2 e2(e);

        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << e2;

        Fq2 e3;
        ss >> e3;

        curve_Fq2 e4 = e3.to_libsnark_fq2<curve_Fq2>();

        ASSERT_TRUE(e == e4);
    }
}

template<typename T>
T deserialize_tv(std::string s)
{
    T e;
    CDataStream ss(ParseHex(s), SER_NETWORK, PROTOCOL_VERSION);
    ss >> e;

    return e;
}

curve_Fq deserialize_fq(std::string s)
{
    return deserialize_tv<Fq>(s).to_libsnark_fq<curve_Fq>();
}

curve_Fq2 deserialize_fq2(std::string s)
{
    return deserialize_tv<Fq2>(s).to_libsnark_fq2<curve_Fq2>();
}

TEST(proofs, fq_valid)
{
    curve_Fq e = deserialize_fq("30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd46");

    ASSERT_TRUE(e == curve_Fq("21888242871839275222246405745257275088696311157297823662689037894645226208582"));
    ASSERT_TRUE(e != curve_Fq("21888242871839275222246405745257275088696311157297823662689037894645226208581"));

    curve_Fq e2 = deserialize_fq("30644e72e131a029b75045b68181585d97816a916871ca8d3c208c16d87cfd46");

    ASSERT_TRUE(e2 == curve_Fq("21888242871839275222221885816603420866962577604863418715751138068690288573766"));
}

TEST(proofs, fq_invalid)
{
    // Should not be able to deserialize the modulus
    ASSERT_THROW(
        deserialize_fq("30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47"),
        std::logic_error
    );

    // Should not be able to deserialize the modulus plus one
    ASSERT_THROW(
        deserialize_fq("30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd48"),
        std::logic_error
    );

    // Should not be able to deserialize a ridiculously out of bound int
    ASSERT_THROW(
        deserialize_fq("ff644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd46"),
        std::logic_error
    );
}

TEST(proofs, fq2_valid)
{
    // (q - 1) * q + q
    curve_Fq2 e = deserialize_fq2("0925c4b8763cbf9c599a6f7c0348d21cb00b85511637560626edfa5c34c6b38d04689e957a1242c84a50189c6d96cadca602072d09eac1013b5458a2275d69b0");
    ASSERT_TRUE(e.c0 == curve_Fq("21888242871839275222246405745257275088696311157297823662689037894645226208582"));
    ASSERT_TRUE(e.c1 == curve_Fq("21888242871839275222246405745257275088696311157297823662689037894645226208582"));

    curve_Fq2 e2 = deserialize_fq2("000000000000000000000000000000000000000000000000010245be1c91e3186bbbe1c430a93fcfc5aada4ab10c3492f70eea97a91c7b29554db55acffa34d2");
    ASSERT_TRUE(e2.c0 == curve_Fq("238769481237490823"));
    ASSERT_TRUE(e2.c1 == curve_Fq("384579238459723485"));

    curve_Fq2 e3 = deserialize_fq2("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
    ASSERT_TRUE(e3.c0 == curve_Fq("0"));
    ASSERT_TRUE(e3.c1 == curve_Fq("0"));

    curve_Fq2 e4 = deserialize_fq2("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001");
    ASSERT_TRUE(e4.c0 == curve_Fq("1"));
    ASSERT_TRUE(e4.c1 == curve_Fq("0"));
}

TEST(proofs, fq2_invalid)
{
    // (q - 1) * q + q is invalid
    ASSERT_THROW(
        deserialize_fq2("0925c4b8763cbf9c599a6f7c0348d21cb00b85511637560626edfa5c34c6b38d04689e957a1242c84a50189c6d96cadca602072d09eac1013b5458a2275d69b1"),
        std::logic_error
    );

    // q * q + (q - 1) is invalid
    ASSERT_THROW(
        deserialize_fq2("0925c4b8763cbf9c599a6f7c0348d21cb00b85511637560626edfa5c34c6b38d34cced085b43e2f202a05e52ef18233a3d8371be725c8b8e7774e4b8ffda66f7"),
        std::logic_error
    );

    // Ridiculously out of bounds
    ASSERT_THROW(
        deserialize_fq2("0fffc4b8763cbf9c599a6f7c0348d21cb00b85511637560626edfa5c34c6b38d04689e957a1242c84a50189c6d96cadca602072d09eac1013b5458a2275d69b0"),
        std::logic_error
    );
    ASSERT_THROW(
        deserialize_fq2("ffffffff763cbf9c599a6f7c0348d21cb00b85511637560626edfa5c34c6b38d04689e957a1242c84a50189c6d96cadca602072d09eac1013b5458a2275d69b0"),
        std::logic_error
    );
}

TEST(proofs, g1_serializes_properly)
{
    // Cannot serialize zero
    {
        ASSERT_THROW({CompressedG1 g = CompressedG1(curve_G1::zero());}, std::domain_error);
    }

    for (size_t i = 0; i < 1000; i++) {
        curve_G1 e = curve_G1::random_element();

        CompressedG1 e2(e);

        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << e2;

        CompressedG1 e3;
        ss >> e3;

        ASSERT_TRUE(e2 == e3);

        curve_G1 e4 = e3.to_libsnark_g1<curve_G1>();

        ASSERT_TRUE(e == e4);
    }
}

TEST(proofs, g2_serializes_properly)
{
    // Cannot serialize zero
    {
        ASSERT_THROW({CompressedG2 g = CompressedG2(curve_G2::zero());}, std::domain_error);
    }

    for (size_t i = 0; i < 1000; i++) {
        curve_G2 e = curve_G2::random_element();

        CompressedG2 e2(e);

        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << e2;

        CompressedG2 e3;
        ss >> e3;

        ASSERT_TRUE(e2 == e3);

        curve_G2 e4 = e3.to_libsnark_g2<curve_G2>();

        ASSERT_TRUE(e == e4);
    }
}

TEST(proofs, zksnark_serializes_properly)
{
    auto example = libsnark::generate_r1cs_example_with_field_input<curve_Fr>(250, 4);
    example.constraint_system.swap_AB_if_beneficial();
    auto kp = libsnark::r1cs_ppzksnark_generator<curve_pp>(example.constraint_system);
    auto vkprecomp = libsnark::r1cs_ppzksnark_verifier_process_vk(kp.vk);

    for (size_t i = 0; i < 20; i++) {
        auto badproof = ZCProof::random_invalid();
        auto proof = badproof.to_libsnark_proof<libsnark::r1cs_ppzksnark_proof<curve_pp>>();
        
        auto verifierEnabled = ProofVerifier::Strict();
        auto verifierDisabled = ProofVerifier::Disabled();
        // This verifier should catch the bad proof
        ASSERT_FALSE(verifierEnabled.check(
            kp.vk,
            vkprecomp,
            example.primary_input,
            proof
        ));
        // This verifier won't!
        ASSERT_TRUE(verifierDisabled.check(
            kp.vk,
            vkprecomp,
            example.primary_input,
            proof
        ));
    }

    for (size_t i = 0; i < 20; i++) {
        auto proof = libsnark::r1cs_ppzksnark_prover<curve_pp>(
            kp.pk,
            example.primary_input,
            example.auxiliary_input,
            example.constraint_system
        );

        {
            auto verifierEnabled = ProofVerifier::Strict();
            auto verifierDisabled = ProofVerifier::Disabled();
            ASSERT_TRUE(verifierEnabled.check(
                kp.vk,
                vkprecomp,
                example.primary_input,
                proof
            ));
            ASSERT_TRUE(verifierDisabled.check(
                kp.vk,
                vkprecomp,
                example.primary_input,
                proof
            ));
        }

        ASSERT_TRUE(libsnark::r1cs_ppzksnark_verifier_strong_IC<curve_pp>(
            kp.vk,
            example.primary_input,
            proof
        ));

        ZCProof compressed_proof_0(proof);

        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << compressed_proof_0;

        ZCProof compressed_proof_1;
        ss >> compressed_proof_1;

        ASSERT_TRUE(compressed_proof_0 == compressed_proof_1);

        auto newproof = compressed_proof_1.to_libsnark_proof<libsnark::r1cs_ppzksnark_proof<curve_pp>>();

        ASSERT_TRUE(proof == newproof);
        ASSERT_TRUE(libsnark::r1cs_ppzksnark_verifier_strong_IC<curve_pp>(
            kp.vk,
            example.primary_input,
            newproof
        ));
    }
}

TEST(proofs, g1_deserialization)
{
    CompressedG1 g;
    curve_G1 expected;

    // Valid G1 element.
    {
        CDataStream ss(ParseHex("0230644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd46"), SER_NETWORK, PROTOCOL_VERSION);
        ss >> g;

        expected.X = curve_Fq("21888242871839275222246405745257275088696311157297823662689037894645226208582");
        expected.Y = curve_Fq("3969792565221544645472939191694882283483352126195956956354061729942568608776");
        expected.Z = curve_Fq::one();

        ASSERT_TRUE(g.to_libsnark_g1<curve_G1>() == expected);
    }

    // Its negation.
    {
        CDataStream ss(ParseHex("0330644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd46"), SER_NETWORK, PROTOCOL_VERSION);
        ss >> g;

        expected.X = curve_Fq("21888242871839275222246405745257275088696311157297823662689037894645226208582");
        expected.Y = curve_Fq("3969792565221544645472939191694882283483352126195956956354061729942568608776");
        expected.Z = curve_Fq::one();

        ASSERT_TRUE(g.to_libsnark_g1<curve_G1>() == -expected);
    }

    // Invalid leading bytes
    {
        CDataStream ss(ParseHex("ff30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd46"), SER_NETWORK, PROTOCOL_VERSION);

        ASSERT_THROW(ss >> g, std::ios_base::failure);
    }

    // Invalid point
    {
        CDataStream ss(ParseHex("0208c6d2adffacbc8438f09f321874ea66e2fcc29f8dcfec2caefa21ec8c96a77c"), SER_NETWORK, PROTOCOL_VERSION);
        ss >> g;

        ASSERT_THROW(g.to_libsnark_g1<curve_G1>(), std::runtime_error);
    }

    // Point with out of bounds Fq
    {
        CDataStream ss(ParseHex("02ffc6d2adffacbc8438f09f321874ea66e2fcc29f8dcfec2caefa21ec8c96a77c"), SER_NETWORK, PROTOCOL_VERSION);
        ss >> g;

        ASSERT_THROW(g.to_libsnark_g1<curve_G1>(), std::logic_error);
    }

    // Randomly produce valid G1 representations and fail/succeed to
    // turn them into G1 points based on whether they are valid.
    for (size_t i = 0; i < 5000; i++) {
        curve_Fq e = curve_Fq::random_element();
        CDataStream ss(ParseHex("02"), SER_NETWORK, PROTOCOL_VERSION);
        ss << Fq(e);
        CompressedG1 g;
        ss >> g;

        try {
            curve_G1 g_real = g.to_libsnark_g1<curve_G1>();
        } catch(...) {
            
        }
    }
}

TEST(proofs, g2_deserialization)
{
    CompressedG2 g;
    curve_G2 expected = curve_G2::random_element();

    // Valid G2 point
    {
        CDataStream ss(ParseHex("0a023aed31b5a9e486366ea9988b05dba469c6206e58361d9c065bbea7d928204a761efc6e4fa08ed227650134b52c7f7dd0463963e8a4bf21f4899fe5da7f984a"), SER_NETWORK, PROTOCOL_VERSION);
        ss >> g;

        expected.X = curve_Fq2(
            curve_Fq("5923585509243758863255447226263146374209884951848029582715967108651637186684"),
            curve_Fq("5336385337059958111259504403491065820971993066694750945459110579338490853570")
        );
        expected.Y = curve_Fq2(
            curve_Fq("10374495865873200088116930399159835104695426846400310764827677226300185211748"),
            curve_Fq("5256529835065685814318509161957442385362539991735248614869838648137856366932")
        );
        expected.Z = curve_Fq2::one();

        ASSERT_TRUE(g.to_libsnark_g2<curve_G2>() == expected);
    }

    // Its negation
    {
        CDataStream ss(ParseHex("0b023aed31b5a9e486366ea9988b05dba469c6206e58361d9c065bbea7d928204a761efc6e4fa08ed227650134b52c7f7dd0463963e8a4bf21f4899fe5da7f984a"), SER_NETWORK, PROTOCOL_VERSION);
        ss >> g;

        expected.X = curve_Fq2(
            curve_Fq("5923585509243758863255447226263146374209884951848029582715967108651637186684"),
            curve_Fq("5336385337059958111259504403491065820971993066694750945459110579338490853570")
        );
        expected.Y = curve_Fq2(
            curve_Fq("10374495865873200088116930399159835104695426846400310764827677226300185211748"),
            curve_Fq("5256529835065685814318509161957442385362539991735248614869838648137856366932")
        );
        expected.Z = curve_Fq2::one();

        ASSERT_TRUE(g.to_libsnark_g2<curve_G2>() == -expected);
    }

    // Invalid leading bytes
    {
        CDataStream ss(ParseHex("ff023aed31b5a9e486366ea9988b05dba469c6206e58361d9c065bbea7d928204a761efc6e4fa08ed227650134b52c7f7dd0463963e8a4bf21f4899fe5da7f984a"), SER_NETWORK, PROTOCOL_VERSION);

        ASSERT_THROW(ss >> g, std::ios_base::failure);
    }


    // Invalid point
    {
        CDataStream ss(ParseHex("0b023aed31b5a9e486366ea9988b05dba469c6206e58361d9c065bbea7d928204a761efc6e4fa08ed227650134b52c7f7dd0463963e8a4bf21f4899fe5da7f984b"), SER_NETWORK, PROTOCOL_VERSION);
        ss >> g;

        ASSERT_THROW(g.to_libsnark_g2<curve_G2>(), std::runtime_error);
    }

    // Point with out of bounds Fq2
    {
        CDataStream ss(ParseHex("0a0f3aed31b5a9e486366ea9988b05dba469c6206e58361d9c065bbea7d928204a761efc6e4fa08ed227650134b52c7f7dd0463963e8a4bf21f4899fe5da7f984a"), SER_NETWORK, PROTOCOL_VERSION);
        ss >> g;

        ASSERT_THROW(g.to_libsnark_g2<curve_G2>(), std::logic_error);
    }

    // Randomly produce valid G2 representations and fail/succeed to
    // turn them into G2 points based on whether they are valid.
    for (size_t i = 0; i < 5000; i++) {
        curve_Fq2 e = curve_Fq2::random_element();
        CDataStream ss(ParseHex("0a"), SER_NETWORK, PROTOCOL_VERSION);
        ss << Fq2(e);
        CompressedG2 g;
        ss >> g;

        try {
            curve_G2 g_real = g.to_libsnark_g2<curve_G2>();
        } catch(...) {
            
        }
    }
}

#include "json_test_vectors.h"
#include "test/data/g1_compressed.json.h"

TEST(proofs, g1_test_vectors)
{
    Array v = read_json(std::string(json_tests::g1_compressed, json_tests::g1_compressed + sizeof(json_tests::g1_compressed)));
    Array::iterator v_iterator = v.begin();

    curve_G1 e = curve_Fr("34958239045823") * curve_G1::one();
    for (size_t i = 0; i < 10000; i++) {
        e = (curve_Fr("34958239045823") ^ i) * e;
        auto expected = CompressedG1(e);

        expect_test_vector(v_iterator, expected);
        ASSERT_TRUE(expected.to_libsnark_g1<curve_G1>() == e);
    }
}

#include "test/data/g2_compressed.json.h"

TEST(proofs, g2_test_vectors)
{
    Array v = read_json(std::string(json_tests::g2_compressed, json_tests::g2_compressed + sizeof(json_tests::g2_compressed)));
    Array::iterator v_iterator = v.begin();

    curve_G2 e = curve_Fr("34958239045823") * curve_G2::one();
    for (size_t i = 0; i < 10000; i++) {
        e = (curve_Fr("34958239045823") ^ i) * e;
        auto expected = CompressedG2(e);

        expect_test_vector(v_iterator, expected);
        ASSERT_TRUE(expected.to_libsnark_g2<curve_G2>() == e);
    }
}
