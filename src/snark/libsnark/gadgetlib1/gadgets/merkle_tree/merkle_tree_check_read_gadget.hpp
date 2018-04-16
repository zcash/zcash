/** @file
 *****************************************************************************

 Declaration of interfaces for the Merkle tree check read gadget.

 The gadget checks the following: given a root R, address A, value V, and
 authentication path P, check that P is a valid authentication path for the
 value V as the A-th leaf in a Merkle tree with root R.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MERKLE_TREE_CHECK_READ_GADGET_HPP_
#define MERKLE_TREE_CHECK_READ_GADGET_HPP_

#include "common/data_structures/merkle_tree.hpp"
#include "gadgetlib1/gadget.hpp"
#include "gadgetlib1/gadgets/hashes/hash_io.hpp"
#include "gadgetlib1/gadgets/hashes/digest_selector_gadget.hpp"
#include "gadgetlib1/gadgets/merkle_tree/merkle_authentication_path_variable.hpp"

namespace libsnark {

template<typename FieldT, typename HashT>
class merkle_tree_check_read_gadget : public gadget<FieldT> {
private:

    std::vector<HashT> hashers;
    std::vector<block_variable<FieldT> > hasher_inputs;
    std::vector<digest_selector_gadget<FieldT> > propagators;
    std::vector<digest_variable<FieldT> > internal_output;

    std::shared_ptr<digest_variable<FieldT> > computed_root;
    std::shared_ptr<bit_vector_copy_gadget<FieldT> > check_root;

public:

    const size_t digest_size;
    const size_t tree_depth;
    pb_linear_combination_array<FieldT> address_bits;
    digest_variable<FieldT> leaf;
    digest_variable<FieldT> root;
    merkle_authentication_path_variable<FieldT, HashT> path;
    pb_linear_combination<FieldT> read_successful;

    merkle_tree_check_read_gadget(protoboard<FieldT> &pb,
                                  const size_t tree_depth,
                                  const pb_linear_combination_array<FieldT> &address_bits,
                                  const digest_variable<FieldT> &leaf_digest,
                                  const digest_variable<FieldT> &root_digest,
                                  const merkle_authentication_path_variable<FieldT, HashT> &path,
                                  const pb_linear_combination<FieldT> &read_successful,
                                  const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness();

    static size_t root_size_in_bits();
    /* for debugging purposes */
    static size_t expected_constraints(const size_t tree_depth);
};

template<typename FieldT, typename HashT>
void test_merkle_tree_check_read_gadget();

} // libsnark

#include "gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.tcc"

#endif // MERKLE_TREE_CHECK_READ_GADGET_HPP_
