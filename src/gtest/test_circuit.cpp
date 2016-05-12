#include <gtest/gtest.h>
#include "uint256.h"

#include "zerocash/utils/util.h"
#include "zcash/util.h"

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>

#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_gadget.hpp"
#include "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.hpp"

using namespace libsnark;
using namespace libzerocash;

#include "zcash/circuit/utils.tcc"

template<typename FieldT>
void test_value_equals(uint64_t i) {
    protoboard<FieldT> pb;
    pb_variable_array<FieldT> num;
    num.allocate(pb, 64, "");
    num.fill_with_bits(pb, uint64_to_bool_vector(i));
    pb.add_r1cs_constraint(r1cs_constraint<FieldT>(
        packed_addition(num),
        FieldT::one(),
        FieldT::one() * i
    ), "");
    ASSERT_TRUE(pb.is_satisfied());
}

TEST(circuit, values)
{
    default_r1cs_ppzksnark_pp::init_public_params();
    typedef Fr<default_r1cs_ppzksnark_pp> FieldT;
    test_value_equals<FieldT>(0);
    test_value_equals<FieldT>(1);
    test_value_equals<FieldT>(3);
    test_value_equals<FieldT>(5391);
    test_value_equals<FieldT>(883128374);
    test_value_equals<FieldT>(173419028459);
    test_value_equals<FieldT>(2205843009213693953);
}

TEST(circuit, endianness)
{
    std::vector<unsigned char> before = {
         0,  1,  2,  3,  4,  5,  6,  7,
         8,  9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39,
        40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 62, 63
    };
    auto result = swap_endianness_u64(before);

    std::vector<unsigned char> after = {
        56, 57, 58, 59, 60, 61, 62, 63,
        48, 49, 50, 51, 52, 53, 54, 55,
        40, 41, 42, 43, 44, 45, 46, 47,
        32, 33, 34, 35, 36, 37, 38, 39,
        24, 25, 26, 27, 28, 29, 30, 31,
        16, 17, 18, 19, 20, 21, 22, 23,
         8,  9, 10, 11, 12, 13, 14, 15,
         0,  1,  2,  3,  4,  5,  6,  7
    };

    EXPECT_EQ(after, result);

    std::vector<unsigned char> bad = {0, 1, 2, 3};

    ASSERT_THROW(swap_endianness_u64(bad), std::length_error);
}
