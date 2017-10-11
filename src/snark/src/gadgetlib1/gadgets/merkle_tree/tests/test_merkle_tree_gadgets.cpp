/**
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "algebra/curves/alt_bn128/alt_bn128_pp.hpp"
#ifdef CURVE_BN128
#include "algebra/curves/bn128/bn128_pp.hpp"
#endif
#include "gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.hpp"
#include "gadgetlib1/gadgets/merkle_tree/merkle_tree_check_update_gadget.hpp"
#include "gadgetlib1/gadgets/hashes/sha256/sha256_gadget.hpp"

#include <gtest/gtest.h>

using namespace libsnark;

template<typename ppT>
void test_all_merkle_tree_gadgets()
{
    typedef Fr<ppT> FieldT;
    test_merkle_tree_check_read_gadget<FieldT, sha256_two_to_one_hash_gadget<FieldT> >();

    test_merkle_tree_check_update_gadget<FieldT, sha256_two_to_one_hash_gadget<FieldT> >();
}

TEST(gadgetlib1, merkle_tree)
{
    start_profiling();

    alt_bn128_pp::init_public_params();
    test_all_merkle_tree_gadgets<alt_bn128_pp>();

#ifdef CURVE_BN128       // BN128 has fancy dependencies so it may be disabled
    bn128_pp::init_public_params();
    test_all_merkle_tree_gadgets<bn128_pp>();
#endif
}
