/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef POUR_GADGET_HPP_
#define POUR_GADGET_HPP_
#include "zerocash_params.hpp"
#include "hashes/sha256_gadget.hpp"
#include "hashes/sha256_merkle_tree.hpp"

namespace libzerocash {

using namespace libsnark;

template<typename FieldT>
class pour_gadget : public gadget<FieldT> {
public:
    size_t tree_depth;

    pb_variable_array<FieldT> IV;
    pb_variable<FieldT> zero;

    std::shared_ptr<digest_variable<FieldT> > root;
    std::shared_ptr<digest_variable<FieldT> > sn_old_1;
    std::shared_ptr<digest_variable<FieldT> > sn_old_2;
    std::shared_ptr<digest_variable<FieldT> > cm_new_1;
    std::shared_ptr<digest_variable<FieldT> > cm_new_2;
    pb_variable_array<FieldT> val_pub;
    std::shared_ptr<digest_variable<FieldT> > h_info;
    std::shared_ptr<digest_variable<FieldT> > h_1;
    std::shared_ptr<digest_variable<FieldT> > h_2;
    /////////// end inputs ///////////////
    pb_variable_array<FieldT> addr_pk_new_1;
    pb_variable_array<FieldT> addr_pk_new_2;
    pb_variable_array<FieldT> addr_sk_old_1;
    pb_variable_array<FieldT> addr_sk_old_2;
    pb_variable_array<FieldT> rand_new_1;
    pb_variable_array<FieldT> rand_new_2;
    pb_variable_array<FieldT> rand_old_1;
    pb_variable_array<FieldT> rand_old_2;
    pb_variable_array<FieldT> nonce_new_1;
    pb_variable_array<FieldT> nonce_new_2;
    pb_variable_array<FieldT> nonce_old_1;
    pb_variable_array<FieldT> nonce_old_2;
    pb_variable_array<FieldT> val_new_1;
    pb_variable_array<FieldT> val_new_2;
    pb_variable_array<FieldT> val_old_1;
    pb_variable_array<FieldT> val_old_2;

    std::shared_ptr<block_variable<FieldT> > hash_sn_old_1_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_sn_old_1;
    std::shared_ptr<block_variable<FieldT> > hash_sn_old_2_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_sn_old_2;

    std::shared_ptr<digest_variable<FieldT> > addr_pk_old_1;
    std::shared_ptr<digest_variable<FieldT> > addr_pk_old_2;
    std::shared_ptr<block_variable<FieldT> > hash_addr_pk_old_1_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_addr_pk_old_1;
    std::shared_ptr<block_variable<FieldT> > hash_addr_pk_old_2_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_addr_pk_old_2;

    std::shared_ptr<digest_variable<FieldT> > cm_old_1_inner_inner;
    std::shared_ptr<digest_variable<FieldT> > cm_old_2_inner_inner;
    std::shared_ptr<block_variable<FieldT> > hash_cm_old_1_inner_inner_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_old_1_inner_inner;
    std::shared_ptr<block_variable<FieldT> > hash_cm_old_2_inner_inner_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_old_2_inner_inner;

    std::shared_ptr<digest_variable<FieldT> > cm_old_1_inner;
    std::shared_ptr<digest_variable<FieldT> > cm_old_2_inner;
    std::shared_ptr<block_variable<FieldT> > hash_cm_old_1_inner_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_old_1_inner;
    std::shared_ptr<block_variable<FieldT> > hash_cm_old_2_inner_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_old_2_inner;

    std::shared_ptr<digest_variable<FieldT> > cm_old_1;
    std::shared_ptr<digest_variable<FieldT> > cm_old_2;
    std::shared_ptr<block_variable<FieldT> > hash_cm_old_1_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_old_1;
    std::shared_ptr<block_variable<FieldT> > hash_cm_old_2_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_old_2;

    std::shared_ptr<digest_variable<FieldT> > cm_new_1_inner_inner;
    std::shared_ptr<digest_variable<FieldT> > cm_new_2_inner_inner;
    std::shared_ptr<block_variable<FieldT> > hash_cm_new_1_inner_inner_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_new_1_inner_inner;
    std::shared_ptr<block_variable<FieldT> > hash_cm_new_2_inner_inner_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_new_2_inner_inner;

    std::shared_ptr<digest_variable<FieldT> > cm_new_1_inner;
    std::shared_ptr<digest_variable<FieldT> > cm_new_2_inner;
    std::shared_ptr<block_variable<FieldT> > hash_cm_new_1_inner_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_new_1_inner;
    std::shared_ptr<block_variable<FieldT> > hash_cm_new_2_inner_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_new_2_inner;

    std::shared_ptr<block_variable<FieldT> > hash_cm_new_1_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_new_1;
    std::shared_ptr<block_variable<FieldT> > hash_cm_new_2_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_cm_new_2;

    std::shared_ptr<block_variable<FieldT> > hash_h_1_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_h_1;
    std::shared_ptr<block_variable<FieldT> > hash_h_2_input;
    std::shared_ptr<compression_function_gadget<FieldT> > hash_h_2;

    pb_variable_array<FieldT> packed_inputs;
    pb_variable_array<FieldT> input_bits;
    std::shared_ptr<multipacking_gadget<FieldT> > unpack_inputs;

    std::shared_ptr<merkle_gadget<FieldT> > cm_old_1_in_tree;
    std::shared_ptr<merkle_gadget<FieldT> > cm_old_2_in_tree;

    pb_variable<FieldT> val_new_1_packed;
    std::shared_ptr<packing_gadget<FieldT> > pack_val_new_1;
    pb_variable<FieldT> val_new_2_packed;
    std::shared_ptr<packing_gadget<FieldT> > pack_val_new_2;
    pb_variable<FieldT> val_old_1_packed;
    std::shared_ptr<packing_gadget<FieldT> > pack_val_old_1;
    pb_variable<FieldT> val_old_2_packed;
    std::shared_ptr<packing_gadget<FieldT> > pack_val_old_2;
    pb_variable<FieldT> val_pub_packed;
    std::shared_ptr<packing_gadget<FieldT> > pack_val_pub;

    pb_variable<FieldT> val_total;
    pb_variable_array<FieldT> val_total_bits;
    std::shared_ptr<packing_gadget<FieldT> > unpack_val_total;

    pour_gadget(protoboard<FieldT> &pb, const size_t tree_depth, const std::string &annotation_prefix);
    void generate_r1cs_constraints();
    void generate_r1cs_witness(const auth_path &path1,
                               const auth_path &path2,
                               const bit_vector &root_bv,
                               const bit_vector &addr_pk_new_1_bv,
                               const bit_vector &addr_pk_new_2_bv,
                               const bit_vector &addr_sk_old_1_bv,
                               const bit_vector &addr_sk_old_2_bv,
                               const bit_vector &rand_new_1_bv,
                               const bit_vector &rand_new_2_bv,
                               const bit_vector &rand_old_1_bv,
                               const bit_vector &rand_old_2_bv,
                               const bit_vector &nonce_new_1_bv,
                               const bit_vector &nonce_new_2_bv,
                               const bit_vector &nonce_old_1_bv,
                               const bit_vector &nonce_old_2_bv,
                               const bit_vector &val_new_1_bv,
                               const bit_vector &val_new_2_bv,
                               const bit_vector &val_pub_bv,
                               const bit_vector &val_old_1_bv,
                               const bit_vector &val_old_2_bv,
                               const bit_vector &h_info_bv);
};

template<typename FieldT>
std::vector<FieldT> pour_input_map(const bit_vector &root_bv,
                                   const bit_vector &sn_old_1_bv,
                                   const bit_vector &sn_old_2_bv,
                                   const bit_vector &cm_new_1_bv,
                                   const bit_vector &cm_new_2_bv,
                                   const bit_vector &val_pub_bv,
                                   const bit_vector &h_info_bv,
                                   const bit_vector &h_1_bv,
                                   const bit_vector &h_2_bv);

} // libzerocash

#include "pour_gadget.tcc"
#endif // POUR_GADGET_HPP_
