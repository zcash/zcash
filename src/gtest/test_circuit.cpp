#include <gtest/gtest.h>
#include "uint256.h"

#include "zcash/util.h"

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>

#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_gadget.hpp"
#include "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.hpp"
#include "zcash/IncrementalMerkleTree.hpp"

using namespace libsnark;
using namespace libzcash;

#include "zcash/circuit/utils.tcc"
#include "zcash/circuit/merkle.tcc"

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

template<typename FieldT>
bool test_merkle_gadget(
    bool enforce_a,
    bool enforce_b,
    bool write_root_first
)
{
    protoboard<FieldT> pb;
    digest_variable<FieldT> root(pb, 256, "root");
    pb.set_input_sizes(256);

    digest_variable<FieldT> commitment1(pb, 256, "commitment1");
    digest_variable<FieldT> commitment2(pb, 256, "commitment2");

    pb_variable<FieldT> commitment1_read;
    commitment1_read.allocate(pb);
    pb_variable<FieldT> commitment2_read;
    commitment2_read.allocate(pb);

    merkle_tree_gadget<FieldT> mgadget1(pb, commitment1, root, commitment1_read);
    merkle_tree_gadget<FieldT> mgadget2(pb, commitment2, root, commitment2_read);

    commitment1.generate_r1cs_constraints();
    commitment2.generate_r1cs_constraints();
    root.generate_r1cs_constraints();
    mgadget1.generate_r1cs_constraints();
    mgadget2.generate_r1cs_constraints();

    ZCIncrementalMerkleTree tree;
    uint256 commitment1_data = uint256S("54d626e08c1c802b305dad30b7e54a82f102390cc92c7d4db112048935236e9c");
    uint256 commitment2_data = uint256S("59d2cde5e65c1414c32ba54f0fe4bdb3d67618125286e6a191317917c812c6d7");
    tree.append(commitment1_data);
    auto wit1 = tree.witness();
    tree.append(commitment2_data);
    wit1.append(commitment2_data);
    auto wit2 = tree.witness();
    auto expected_root = tree.root();
    tree.append(uint256S("3e243c8798678570bb8d42616c23a536af44be15c4eef073490c2a44ae5f32c3"));
    auto unexpected_root = tree.root();
    tree.append(uint256S("26d9b20c7f1c3d2528bbcd43cd63344b0afd3b6a0a8ebd37ec51cba34907bec7"));
    auto badwit1 = tree.witness();
    tree.append(uint256S("02c2467c9cd15e0d150f74cd636505ed675b0b71b66a719f6f52fdb49a5937bb"));
    auto badwit2 = tree.witness();

    // Perform the test

    pb.val(commitment1_read) = enforce_a ? FieldT::one() : FieldT::zero();
    pb.val(commitment2_read) = enforce_b ? FieldT::one() : FieldT::zero();

    commitment1.bits.fill_with_bits(pb, uint256_to_bool_vector(commitment1_data));
    commitment2.bits.fill_with_bits(pb, uint256_to_bool_vector(commitment2_data));

    if (write_root_first) {
        root.bits.fill_with_bits(pb, uint256_to_bool_vector(expected_root));
    }

    mgadget1.generate_r1cs_witness(wit1.path());
    mgadget2.generate_r1cs_witness(wit2.path());

    // Overwrite with our expected root
    root.bits.fill_with_bits(pb, uint256_to_bool_vector(expected_root));

    return pb.is_satisfied();
}

TEST(circuit, merkle_tree_gadget_weirdness)
{
    /*
    The merkle tree gadget takes a leaf in the merkle tree (the Note commitment),
    a merkle tree authentication path, and a root (anchor). It also takes a parameter
    called read_success, which is used to determine if the commitment actually needs to
    appear in the tree.

    If two input notes use the same root (which our protocol does) then if `read_success`
    is disabled on the first note but enabled on the second note (i.e., the first note
    has value of zero and second note has nonzero value) then there is an edge case in
    the witnessing behavior. The first witness will accidentally constrain the root to
    equal null (the default value of the anchor) and the second witness will actually
    copy the bits, violating the constraint system.

    Notice that this edge case is not in the constraint system but in the witnessing
    behavior.
    */

    typedef Fr<default_r1cs_ppzksnark_pp> FieldT;

    // Test the normal case
    ASSERT_TRUE(test_merkle_gadget<FieldT>(true, true, false));
    ASSERT_TRUE(test_merkle_gadget<FieldT>(true, true, true));

    // Test the case where the first commitment is enforced but the second isn't
    // Works because the first read is performed before the second one
    ASSERT_TRUE(test_merkle_gadget<FieldT>(true, false, false));
    ASSERT_TRUE(test_merkle_gadget<FieldT>(true, false, true));

    // Test the case where the first commitment isn't enforced but the second is
    // Doesn't work because the first multipacker witnesses the existing root (which
    // is null)
    ASSERT_TRUE(!test_merkle_gadget<FieldT>(false, true, false));

    // Test the last again, except this time write the root first.
    ASSERT_TRUE(test_merkle_gadget<FieldT>(false, true, true));
}
