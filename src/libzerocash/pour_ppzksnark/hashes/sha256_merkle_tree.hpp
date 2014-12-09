/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SHA256_MERKLE_TREE_HPP_
#define SHA256_MERKLE_TREE_HPP_
#include "sha256_gadget.hpp"

namespace libsnark {

template<typename FieldT>
class merkle_gadget : public gadget<FieldT> {
private:
    std::vector<two_to_one_hash_function_gadget<FieldT> > hashers;
    std::vector<digest_propagator_gadget<FieldT> > propagators;
    std::vector<digest_variable<FieldT> > internal_left;
    std::vector<digest_variable<FieldT> > internal_right;
    std::vector<digest_variable<FieldT> > internal_output;

public:
    pb_variable_array<FieldT> IV;
    const size_t tree_depth;
    digest_variable<FieldT> leaf;
    digest_variable<FieldT> root;
    pb_variable_array<FieldT> is_right;

    merkle_gadget(protoboard<FieldT> &pb,
                  const pb_variable_array<FieldT> &IV,
                  const size_t tree_depth,
                  const digest_variable<FieldT> &leaf_digest,
                  const digest_variable<FieldT> &root_digest,
                  const std::string &annotation_prefix);
    void generate_r1cs_constraints();
    void generate_r1cs_witness(const bit_vector &leaf, const bit_vector &root, const auth_path &path);
};

} // libsnark
#include "sha256_merkle_tree.tcc"

#endif // SHA256_MERKLE_TREE_HPP_
