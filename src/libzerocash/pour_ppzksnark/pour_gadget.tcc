/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "common/field_utils.hpp"

namespace libzerocash {

template<typename FieldT>
pour_gadget<FieldT>::pour_gadget(protoboard<FieldT> &pb, const size_t tree_depth, const std::string &annotation_prefix) :
    gadget<FieldT>(pb, FMT(annotation_prefix, " pour_gadget"))
{
    /* allocate packed inputs */
    const size_t input_size_bits = sha256_digest_len + 2*sha256_digest_len + 2*sha256_digest_len + value_len + 3 * sha256_digest_len;
    const size_t max_chunk_size = FieldT::num_bits-1;
    const size_t input_num_chunks = div_ceil(input_size_bits, max_chunk_size);
    packed_inputs.allocate(pb, input_num_chunks, FMT(annotation_prefix, " packed_inputs"));
    this->pb.constraint_system.num_inputs = input_num_chunks;

    /* allocate inputs */
    root.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " root")));
    sn_old_1.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " sn_old_1")));
    sn_old_2.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " sn_old_2")));
    cm_new_1.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_new_1")));
    cm_new_2.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_new_2")));
    val_pub.allocate(pb, value_len, FMT(annotation_prefix, " val_pub"));
    h_info.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " h_info")));
    h_1.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " h_1")));
    h_2.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " h_2")));

    /* do the multipacking */
    input_bits.insert(input_bits.end(), root->bits.begin(), root->bits.end());
    input_bits.insert(input_bits.end(), sn_old_1->bits.begin(), sn_old_1->bits.end());
    input_bits.insert(input_bits.end(), sn_old_2->bits.begin(), sn_old_2->bits.end());
    input_bits.insert(input_bits.end(), cm_new_1->bits.begin(), cm_new_1->bits.end());
    input_bits.insert(input_bits.end(), cm_new_2->bits.begin(), cm_new_2->bits.end());
    input_bits.insert(input_bits.end(), val_pub.begin(), val_pub.end());
    input_bits.insert(input_bits.end(), h_info->bits.begin(), h_info->bits.end());
    input_bits.insert(input_bits.end(), h_1->bits.begin(), h_1->bits.end());
    input_bits.insert(input_bits.end(), h_2->bits.begin(), h_2->bits.end());
    unpack_inputs.reset(new multipacking_gadget<FieldT>(this->pb, input_bits, packed_inputs, max_chunk_size, FMT(this->annotation_prefix, " unpack_inputs")));

    /* allocate auxiliary things to deal with inefficiencies in constraints library */
    IV = allocate_SHA256_IV(pb);
    zero.allocate(this->pb, FMT(this->annotation_prefix, " zero"));

    /* allocate witness */
    addr_pk_new_1.allocate(pb, addr_pk_len, FMT(annotation_prefix, " addr_pk_new_1"));
    addr_pk_new_2.allocate(pb, addr_pk_len, FMT(annotation_prefix, " addr_pk_new_2"));
    addr_sk_old_1.allocate(pb, addr_sk_len, FMT(annotation_prefix, " addr_sk_old_1"));
    addr_sk_old_2.allocate(pb, addr_sk_len, FMT(annotation_prefix, " addr_sk_old_2"));
    rand_new_1.allocate(pb, rand_len, FMT(annotation_prefix, " rand_new_1"));
    rand_new_2.allocate(pb, rand_len, FMT(annotation_prefix, " rand_new_2"));
    rand_old_1.allocate(pb, rand_len, FMT(annotation_prefix, " rand_old_1"));
    rand_old_2.allocate(pb, rand_len, FMT(annotation_prefix, " rand_old_2"));
    nonce_old_1.allocate(pb, nonce_len, FMT(annotation_prefix, " nonce_old_1"));
    nonce_old_2.allocate(pb, nonce_len, FMT(annotation_prefix, " nonce_old_2"));
    nonce_new_1.allocate(pb, nonce_len, FMT(annotation_prefix, " nonce_new_1"));
    nonce_new_2.allocate(pb, nonce_len, FMT(annotation_prefix, " nonce_new_2"));
    val_new_1.allocate(pb, value_len, FMT(annotation_prefix, " val_new_1"));
    val_new_2.allocate(pb, value_len, FMT(annotation_prefix, " val_new_2"));
    val_old_1.allocate(pb, value_len, FMT(annotation_prefix, " val_old_1"));
    val_old_2.allocate(pb, value_len, FMT(annotation_prefix, " val_old_2"));

    /* do the actual hashing */
    pb_variable_array<FieldT> zero_one;
    zero_one.emplace_back(zero);
    zero_one.emplace_back(ONE);

    hash_sn_old_1_input.reset(new block_variable<FieldT>(pb, { addr_sk_old_1, zero_one, pb_variable_array<FieldT>(nonce_old_1.begin(), nonce_old_1.begin() + sn_trunc) }, FMT(annotation_prefix, " hash_sn_old_1_input")));
    hash_sn_old_1.reset(new compression_function_gadget<FieldT>(pb, IV, hash_sn_old_1_input->bits, *sn_old_1, FMT(annotation_prefix, " hash_sn_old_1")));
    hash_sn_old_2_input.reset(new block_variable<FieldT>(pb, { addr_sk_old_2, zero_one, pb_variable_array<FieldT>(nonce_old_2.begin(), nonce_old_2.begin() + sn_trunc) }, FMT(annotation_prefix, " hash_sn_old_2_input")));
    hash_sn_old_2.reset(new compression_function_gadget<FieldT>(pb, IV, hash_sn_old_2_input->bits, *sn_old_2, FMT(annotation_prefix, " hash_sn_old_2")));

    pb_variable_array<FieldT> addr_pk_pad(addr_pk_pad_len, zero);
    addr_pk_old_1.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " addr_pk_old_1")));
    addr_pk_old_2.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " addr_pk_old_2")));
    hash_addr_pk_old_1_input.reset(new block_variable<FieldT>(pb, { addr_sk_old_1, addr_pk_pad }, FMT(annotation_prefix, " hash_addr_pk_old_1_input")));
    hash_addr_pk_old_1.reset(new compression_function_gadget<FieldT>(pb, IV, hash_addr_pk_old_1_input->bits, *addr_pk_old_1, FMT(annotation_prefix, " hash_addr_pk_old_1")));
    hash_addr_pk_old_2_input.reset(new block_variable<FieldT>(pb, { addr_sk_old_2, addr_pk_pad }, FMT(annotation_prefix, " hash_addr_pk_old_2_input")));
    hash_addr_pk_old_2.reset(new compression_function_gadget<FieldT>(pb, IV, hash_addr_pk_old_2_input->bits, *addr_pk_old_2, FMT(annotation_prefix, " hash_addr_pk_old_2")));

    cm_old_1_inner_inner.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_old_1_inner_inner")));
    cm_old_2_inner_inner.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_old_2_inner_inner")));
    hash_cm_old_1_inner_inner_input.reset(new block_variable<FieldT>(pb, { addr_pk_old_1->bits, nonce_old_1 }, FMT(annotation_prefix, " hash_cm_old_1_inner_inner_input")));
    hash_cm_old_1_inner_inner.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_old_1_inner_inner_input->bits, *cm_old_1_inner_inner, FMT(annotation_prefix, " hash_cm_old_1_inner_inner")));
    hash_cm_old_2_inner_inner_input.reset(new block_variable<FieldT>(pb, { addr_pk_old_2->bits, nonce_old_2 }, FMT(annotation_prefix, " hash_cm_old_2_inner_inner_input")));
    hash_cm_old_2_inner_inner.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_old_2_inner_inner_input->bits, *cm_old_2_inner_inner, FMT(annotation_prefix, " hash_cm_old_2_inner_inner")));

    cm_old_1_inner.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_old_1_inner")));
    cm_old_2_inner.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_old_2_inner")));
    hash_cm_old_1_inner_input.reset(new block_variable<FieldT>(pb, { rand_old_1, pb_variable_array<FieldT>(cm_old_1_inner_inner->bits.begin(), cm_old_1_inner_inner->bits.begin()+ coincomm_trunc) }, FMT(annotation_prefix, " hash_cm_old_1_inner_input")));
    hash_cm_old_1_inner.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_old_1_inner_input->bits, *cm_old_1_inner, FMT(annotation_prefix, " hash_cm_old_1_inner")));
    hash_cm_old_2_inner_input.reset(new block_variable<FieldT>(pb, { rand_old_2, pb_variable_array<FieldT>(cm_old_2_inner_inner->bits.begin(), cm_old_2_inner_inner->bits.begin()+ coincomm_trunc) }, FMT(annotation_prefix, " hash_cm_old_2_inner_input")));
    hash_cm_old_2_inner.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_old_2_inner_input->bits, *cm_old_2_inner, FMT(annotation_prefix, " hash_cm_old_2_inner")));

    pb_variable_array<FieldT> coincomm_pad(coincomm_pad_len, zero);
    cm_old_1.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_old_1")));
    cm_old_2.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_old_2")));
    hash_cm_old_1_input.reset(new block_variable<FieldT>(pb, { cm_old_1_inner->bits, coincomm_pad, val_old_1 }, FMT(annotation_prefix, " hash_cm_old_1_input")));
    hash_cm_old_1.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_old_1_input->bits, *cm_old_1, FMT(annotation_prefix, " hash_cm_old_1")));
    hash_cm_old_2_input.reset(new block_variable<FieldT>(pb, { cm_old_2_inner->bits, coincomm_pad, val_old_2 }, FMT(annotation_prefix, " hash_cm_old_2_input")));
    hash_cm_old_2.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_old_2_input->bits, *cm_old_2, FMT(annotation_prefix, " hash_cm_old_2")));

    cm_new_1_inner_inner.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_new_1_inner_inner")));
    cm_new_2_inner_inner.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_new_2_inner_inner")));
    hash_cm_new_1_inner_inner_input.reset(new block_variable<FieldT>(pb, { addr_pk_new_1, nonce_new_1 }, FMT(annotation_prefix, " hash_cm_new_1_inner_inner_input")));
    hash_cm_new_1_inner_inner.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_new_1_inner_inner_input->bits, *cm_new_1_inner_inner, FMT(annotation_prefix, " hash_cm_new_1_inner_inner")));
    hash_cm_new_2_inner_inner_input.reset(new block_variable<FieldT>(pb, { addr_pk_new_2, nonce_new_2 }, FMT(annotation_prefix, " hash_cm_new_2_inner_inner_input")));
    hash_cm_new_2_inner_inner.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_new_2_inner_inner_input->bits, *cm_new_2_inner_inner, FMT(annotation_prefix, " hash_cm_new_2_inner_inner")));

    cm_new_1_inner.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_new_1_inner")));
    cm_new_2_inner.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " cm_new_2_inner")));
    hash_cm_new_1_inner_input.reset(new block_variable<FieldT>(pb, { rand_new_1, pb_variable_array<FieldT>(cm_new_1_inner_inner->bits.begin(), cm_new_1_inner_inner->bits.begin()+ coincomm_trunc) }, FMT(annotation_prefix, " hash_cm_new_1_inner_input")));
    hash_cm_new_1_inner.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_new_1_inner_input->bits, *cm_new_1_inner, FMT(annotation_prefix, " hash_cm_new_1_inner")));
    hash_cm_new_2_inner_input.reset(new block_variable<FieldT>(pb, { rand_new_2, pb_variable_array<FieldT>(cm_new_2_inner_inner->bits.begin(), cm_new_2_inner_inner->bits.begin()+ coincomm_trunc) }, FMT(annotation_prefix, " hash_cm_new_2_inner_input")));
    hash_cm_new_2_inner.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_new_2_inner_input->bits, *cm_new_2_inner, FMT(annotation_prefix, " hash_cm_new_2_inner")));

    hash_cm_new_1_input.reset(new block_variable<FieldT>(pb, { cm_new_1_inner->bits, coincomm_pad, val_new_1 }, FMT(annotation_prefix, " hash_cm_new_1_input")));
    hash_cm_new_1.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_new_1_input->bits, *cm_new_1, FMT(annotation_prefix, " hash_cm_new_1")));
    hash_cm_new_2_input.reset(new block_variable<FieldT>(pb, { cm_new_2_inner->bits, coincomm_pad, val_new_2 }, FMT(annotation_prefix, " hash_cm_new_2_input")));
    hash_cm_new_2.reset(new compression_function_gadget<FieldT>(pb, IV, hash_cm_new_2_input->bits, *cm_new_2, FMT(annotation_prefix, " hash_cm_new_2")));

    /* compute h_1 and h_2 */
    pb_variable_array<FieldT> one_zero_zero;
    one_zero_zero.emplace_back(ONE);
    one_zero_zero.emplace_back(zero);
    one_zero_zero.emplace_back(zero);

    pb_variable_array<FieldT> one_zero_one;
    one_zero_one.emplace_back(ONE);
    one_zero_one.emplace_back(zero);
    one_zero_one.emplace_back(ONE);

    hash_h_1_input.reset(new block_variable<FieldT>(pb, { addr_sk_old_1, one_zero_zero, pb_variable_array<FieldT>(h_info->bits.begin(), h_info->bits.begin()+h_trunc) }, FMT(annotation_prefix, " hash_h_1_input")));
    hash_h_1.reset(new compression_function_gadget<FieldT>(pb, IV, hash_h_1_input->bits, *h_1, FMT(annotation_prefix, " hash_h_1")));
    hash_h_2_input.reset(new block_variable<FieldT>(pb, { addr_sk_old_2, one_zero_one, pb_variable_array<FieldT>(h_info->bits.begin(), h_info->bits.begin()+h_trunc) }, FMT(annotation_prefix, " hash_h_2_input")));
    hash_h_2.reset(new compression_function_gadget<FieldT>(pb, IV, hash_h_2_input->bits, *h_2, FMT(annotation_prefix, " hash_h_2")));

    /* prove membership in the Merkle tree*/
    cm_old_1_in_tree.reset(new merkle_gadget<FieldT>(pb, IV, tree_depth, *cm_old_1, *root, FMT(annotation_prefix, " cm_old_1_in_tree")));
    cm_old_2_in_tree.reset(new merkle_gadget<FieldT>(pb, IV, tree_depth, *cm_old_2, *root, FMT(annotation_prefix, " cm_old_2_in_tree")));

    /* check values */
    val_new_1_packed.allocate(pb, FMT(annotation_prefix, " val_new_1_packed"));
    val_new_2_packed.allocate(pb, FMT(annotation_prefix, " val_new_2_packed"));
    val_old_1_packed.allocate(pb, FMT(annotation_prefix, " val_old_1_packed"));
    val_old_2_packed.allocate(pb, FMT(annotation_prefix, " val_old_2_packed"));
    val_pub_packed.allocate(pb, FMT(annotation_prefix, " val_pub_packed"));

    pack_val_new_1.reset(new packing_gadget<FieldT>(pb, pb_variable_array<FieldT>(val_new_1.rbegin(), val_new_1.rend()), val_new_1_packed, FMT(annotation_prefix, " pack_val_new_1")));
    pack_val_new_2.reset(new packing_gadget<FieldT>(pb, pb_variable_array<FieldT>(val_new_2.rbegin(), val_new_2.rend()), val_new_2_packed, FMT(annotation_prefix, " pack_val_new_2")));
    pack_val_old_1.reset(new packing_gadget<FieldT>(pb, pb_variable_array<FieldT>(val_old_1.rbegin(), val_old_1.rend()), val_old_1_packed, FMT(annotation_prefix, " pack_val_old_1")));
    pack_val_old_2.reset(new packing_gadget<FieldT>(pb, pb_variable_array<FieldT>(val_old_2.rbegin(), val_old_2.rend()), val_old_2_packed, FMT(annotation_prefix, " pack_val_old_2")));
    pack_val_pub.reset(new packing_gadget<FieldT>(pb, pb_variable_array<FieldT>(val_pub.rbegin(), val_pub.rend()), val_pub_packed, FMT(annotation_prefix, " pack_val_pub")));

    val_total.allocate(pb, FMT(annotation_prefix, " val_total"));
    val_total_bits.allocate(pb, value_len, FMT(annotation_prefix, " val_total_bits"));
    unpack_val_total.reset(new packing_gadget<FieldT>(pb, val_total_bits, val_total, FMT(annotation_prefix, " unpack_val_total")));
}

template<typename FieldT>
void pour_gadget<FieldT>::generate_r1cs_constraints()
{
    generate_r1cs_equals_const_constraint<FieldT>(this->pb, zero, FieldT::zero(), FMT(this->annotation_prefix, " zero"));

    hash_sn_old_1->generate_r1cs_constraints();
    hash_sn_old_2->generate_r1cs_constraints();

    hash_addr_pk_old_1->generate_r1cs_constraints();
    hash_addr_pk_old_2->generate_r1cs_constraints();

    hash_cm_old_1_inner_inner->generate_r1cs_constraints();
    hash_cm_old_2_inner_inner->generate_r1cs_constraints();

    hash_cm_old_1_inner->generate_r1cs_constraints();
    hash_cm_old_2_inner->generate_r1cs_constraints();

    hash_cm_old_1->generate_r1cs_constraints();
    hash_cm_old_2->generate_r1cs_constraints();

    hash_cm_new_1_inner_inner->generate_r1cs_constraints();
    hash_cm_new_2_inner_inner->generate_r1cs_constraints();

    hash_cm_new_1_inner->generate_r1cs_constraints();
    hash_cm_new_2_inner->generate_r1cs_constraints();

    hash_cm_new_1->generate_r1cs_constraints();
    hash_cm_new_2->generate_r1cs_constraints();

    hash_h_1->generate_r1cs_constraints();
    hash_h_2->generate_r1cs_constraints();

    cm_old_1_in_tree->generate_r1cs_constraints();
    cm_old_2_in_tree->generate_r1cs_constraints();

    pack_val_new_1->generate_r1cs_constraints(false);
    pack_val_new_2->generate_r1cs_constraints(false);
    pack_val_old_1->generate_r1cs_constraints(false);
    pack_val_old_2->generate_r1cs_constraints(false);
    pack_val_pub->generate_r1cs_constraints(false);

    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>({ ONE },
        { val_new_1_packed, val_new_2_packed, val_pub_packed },
        { val_old_1_packed, val_old_2_packed }),
        FMT(this->annotation_prefix, " balance_check"));

    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>({ ONE },
        { val_old_1_packed, val_old_2_packed },
        { val_total }),
        FMT(this->annotation_prefix, " val_total_computation"));

    unpack_val_total->generate_r1cs_constraints(true);
    unpack_inputs->generate_r1cs_constraints(true);
}

template<typename FieldT>
void pour_gadget<FieldT>::generate_r1cs_witness(const auth_path &path1,
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
                                                const bit_vector &h_info_bv)
{
    /* fill in the auxiliary variables */
    this->pb.val(zero) = FieldT::zero();

    /* fill in the witness */
    addr_pk_new_1.fill_with_bits(this->pb, addr_pk_new_1_bv);
    addr_pk_new_2.fill_with_bits(this->pb, addr_pk_new_2_bv);
    addr_sk_old_1.fill_with_bits(this->pb, addr_sk_old_1_bv);
    addr_sk_old_2.fill_with_bits(this->pb, addr_sk_old_2_bv);
    rand_new_1.fill_with_bits(this->pb, rand_new_1_bv);
    rand_new_2.fill_with_bits(this->pb, rand_new_2_bv);
    rand_old_1.fill_with_bits(this->pb, rand_old_1_bv);
    rand_old_2.fill_with_bits(this->pb, rand_old_2_bv);
    nonce_new_1.fill_with_bits(this->pb, nonce_new_1_bv);
    nonce_new_2.fill_with_bits(this->pb, nonce_new_2_bv);
    nonce_old_1.fill_with_bits(this->pb, nonce_old_1_bv);
    nonce_old_2.fill_with_bits(this->pb, nonce_old_2_bv);
    val_new_1.fill_with_bits(this->pb, val_new_1_bv);
    val_new_2.fill_with_bits(this->pb, val_new_2_bv);
    val_pub.fill_with_bits(this->pb, val_pub_bv);
    val_old_1.fill_with_bits(this->pb, val_old_1_bv);
    val_old_2.fill_with_bits(this->pb, val_old_2_bv);
    h_info->fill_with_bits(h_info_bv);

    /* do the hashing */
    hash_sn_old_1->generate_r1cs_witness();
    hash_sn_old_2->generate_r1cs_witness();

    hash_addr_pk_old_1->generate_r1cs_witness();
    hash_addr_pk_old_2->generate_r1cs_witness();

    hash_cm_old_1_inner_inner->generate_r1cs_witness();
    hash_cm_old_2_inner_inner->generate_r1cs_witness();

    hash_cm_old_1_inner->generate_r1cs_witness();
    hash_cm_old_2_inner->generate_r1cs_witness();

    hash_cm_old_1->generate_r1cs_witness();
    hash_cm_old_2->generate_r1cs_witness();

    hash_cm_new_1_inner_inner->generate_r1cs_witness();
    hash_cm_new_2_inner_inner->generate_r1cs_witness();

    hash_cm_new_1_inner->generate_r1cs_witness();
    hash_cm_new_2_inner->generate_r1cs_witness();

    hash_cm_new_1->generate_r1cs_witness();
    hash_cm_new_2->generate_r1cs_witness();

    hash_h_1->generate_r1cs_witness();
    hash_h_2->generate_r1cs_witness();

    /* prove the membership in the Merkle tree */
    const bit_vector cm_old_1_bits = cm_old_1->bits.get_bits(this->pb);
    const bit_vector cm_old_2_bits = cm_old_2->bits.get_bits(this->pb);

    cm_old_1_in_tree->generate_r1cs_witness(cm_old_1_bits, root_bv, path1);
    cm_old_2_in_tree->generate_r1cs_witness(cm_old_2_bits, root_bv, path2);

    /* generate witness for value computations */
    pack_val_new_1->generate_r1cs_witness_from_bits();
    pack_val_new_2->generate_r1cs_witness_from_bits();
    pack_val_old_1->generate_r1cs_witness_from_bits();
    pack_val_old_2->generate_r1cs_witness_from_bits();
    pack_val_pub->generate_r1cs_witness_from_bits();

    this->pb.val(val_total) = this->pb.val(val_old_1_packed) + this->pb.val(val_old_2_packed);
    unpack_val_total->generate_r1cs_witness_from_packed();

    /* pack the input */
    unpack_inputs->generate_r1cs_witness_from_bits();
    /*
    printf("cm_old_1_inner:\n");
    for (size_t j = 0; j < 8; ++j)
    {
        printf("%lx ", this->pb.val(hash_cm_old_1_inner->reduced_output[j]).as_ulong());
    }
    printf("\n");

    printf("cm_old_2_inner:\n");
    for (size_t j = 0; j < 8; ++j)
    {
        printf("%lx ", this->pb.val(hash_cm_old_2_inner->reduced_output[j]).as_ulong());
    }
    printf("\n");

    printf("cm_old_1_inner:\n");
    for (size_t j = 0; j < 8; ++j)
    {
        printf("%lx ", this->pb.val(hash_cm_old_1->reduced_output[j]).as_ulong());
    }
    printf("\n");

    printf("cm_old_2_inner:\n");
    for (size_t j = 0; j < 8; ++j)
    {
        printf("%lx ", this->pb.val(hash_cm_old_2->reduced_output[j]).as_ulong());
    }
    printf("\n");
    */
/*
    printf("real:\n");
    for (size_t i = 0; i < packed_inputs.size(); ++i)
    {
        this->pb.val(packed_inputs[i]).print();
    }
*/
}

template<typename FieldT>
std::vector<FieldT> pour_input_map(const bit_vector &root_bv,
                                   const bit_vector &sn_old_1_bv,
                                   const bit_vector &sn_old_2_bv,
                                   const bit_vector &cm_new_1_bv,
                                   const bit_vector &cm_new_2_bv,
                                   const bit_vector &val_pub_bv,
                                   const bit_vector &h_info_bv,
                                   const bit_vector &h_1_bv,
                                   const bit_vector &h_2_bv)
{
    enter_block("Call to pour_input_map");

    assert(root_bv.size() == sha256_digest_len);
    assert(sn_old_1_bv.size() == sn_len);
    assert(sn_old_2_bv.size() == sn_len);
    assert(cm_new_1_bv.size() == coincomm_len);
    assert(cm_new_2_bv.size() == coincomm_len);
    assert(val_pub_bv.size() == value_len);
    assert(h_info_bv.size() == sha256_digest_len);
    assert(h_1_bv.size() == sha256_digest_len);
    assert(h_2_bv.size() == sha256_digest_len);

    bit_vector input_bits;
    input_bits.insert(input_bits.end(), root_bv.begin(), root_bv.end());
    input_bits.insert(input_bits.end(), sn_old_1_bv.begin(), sn_old_1_bv.end());
    input_bits.insert(input_bits.end(), sn_old_2_bv.begin(), sn_old_2_bv.end());
    input_bits.insert(input_bits.end(), cm_new_1_bv.begin(), cm_new_1_bv.end());
    input_bits.insert(input_bits.end(), cm_new_2_bv.begin(), cm_new_2_bv.end());
    input_bits.insert(input_bits.end(), val_pub_bv.begin(), val_pub_bv.end());
    input_bits.insert(input_bits.end(), h_info_bv.begin(), h_info_bv.end());
    input_bits.insert(input_bits.end(), h_1_bv.begin(), h_1_bv.end());
    input_bits.insert(input_bits.end(), h_2_bv.begin(), h_2_bv.end());

    std::vector<FieldT> result = pack_bit_vector_into_field_element_vector<FieldT>(input_bits);
/*
    printf("mapped:\n");
    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i].print();
    }
*/
    leave_block("Call to pour_input_map");
    return result;
}
} // libzerocash

