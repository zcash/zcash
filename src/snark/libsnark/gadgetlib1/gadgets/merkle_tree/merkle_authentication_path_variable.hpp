/**
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MERKLE_AUTHENTICATION_PATH_VARIABLE_HPP_
#define MERKLE_AUTHENTICATION_PATH_VARIABLE_HPP_

#include "common/data_structures/merkle_tree.hpp"
#include "gadgetlib1/gadget.hpp"
#include "gadgetlib1/gadgets/hashes/hash_io.hpp"

namespace libsnark {

template<typename FieldT, typename HashT>
class merkle_authentication_path_variable : public gadget<FieldT> {
public:

    const size_t tree_depth;
    std::vector<digest_variable<FieldT> > left_digests;
    std::vector<digest_variable<FieldT> > right_digests;

    merkle_authentication_path_variable(protoboard<FieldT> &pb,
                                        const size_t tree_depth,
                                        const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness(const size_t address, const merkle_authentication_path &path);
    merkle_authentication_path get_authentication_path(const size_t address) const;
};

} // libsnark

#include "gadgetlib1/gadgets/merkle_tree/merkle_authentication_path_variable.tcc"

#endif // MERKLE_AUTHENTICATION_PATH_VARIABLE_HPP
