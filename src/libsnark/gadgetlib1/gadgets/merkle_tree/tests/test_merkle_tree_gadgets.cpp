/**
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifdef CURVE_BN128
#include "algebra/curves/bn128/bn128_pp.hpp"
#endif
#include "algebra/curves/edwards/edwards_pp.hpp"
#include "algebra/curves/mnt/mnt4/mnt4_pp.hpp"
#include "algebra/curves/mnt/mnt6/mnt6_pp.hpp"
#include "gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.hpp"
#include "gadgetlib1/gadgets/merkle_tree/merkle_tree_check_update_gadget.hpp"
#include "gadgetlib1/gadgets/hashes/sha256/sha256_gadget.hpp"

using namespace libsnark;

template<typename ppT>
void test_all_merkle_tree_gadgets()
{
    typedef Fr<ppT> FieldT;
    test_merkle_tree_check_read_gadget<FieldT, CRH_with_bit_out_gadget<FieldT> >();
    test_merkle_tree_check_read_gadget<FieldT, sha256_two_to_one_hash_gadget<FieldT> >();

    test_merkle_tree_check_update_gadget<FieldT, CRH_with_bit_out_gadget<FieldT> >();
    test_merkle_tree_check_update_gadget<FieldT, sha256_two_to_one_hash_gadget<FieldT> >();
}

int main(void)
{
    start_profiling();

#ifdef CURVE_BN128       // BN128 has fancy dependencies so it may be disabled
    bn128_pp::init_public_params();
    test_all_merkle_tree_gadgets<bn128_pp>();
#endif

    edwards_pp::init_public_params();
    test_all_merkle_tree_gadgets<edwards_pp>();

    mnt4_pp::init_public_params();
    test_all_merkle_tree_gadgets<mnt4_pp>();

    mnt6_pp::init_public_params();
    test_all_merkle_tree_gadgets<mnt6_pp>();
}
