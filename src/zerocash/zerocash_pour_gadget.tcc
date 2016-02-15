/** @file
 *****************************************************************************

 Implementation of interfaces for the Zerocash Pour gadget.

 See zerocash_pour_gadget.hpp .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "algebra/fields/field_utils.hpp"

namespace libzerocash {

template<typename FieldT>
zerocash_pour_gadget<FieldT>::zerocash_pour_gadget(protoboard<FieldT> &pb,
                                                   const size_t num_old_coins,
                                                   const size_t num_new_coins,
                                                   const size_t tree_depth,
                                                   const std::string &annotation_prefix) :
    gadget<FieldT>(pb, FMT(annotation_prefix, " zerocash_pour_gadget")),
    tree_depth(tree_depth),
    num_old_coins(num_old_coins),
    num_new_coins(num_new_coins)
{
    /* allocate packed inputs */
    const size_t input_size_in_bits = sha256_digest_len + num_old_coins*sha256_digest_len + num_new_coins*sha256_digest_len + (coin_value_length * 2) + (num_old_coins + 1) * sha256_digest_len;
    const size_t input_size_in_field_elements = div_ceil(input_size_in_bits, FieldT::capacity());
    input_as_field_elements.allocate(pb, input_size_in_field_elements, FMT(annotation_prefix, " input_as_field_elements"));
    this->pb.set_input_sizes(input_size_in_field_elements);

    /* allocate inputs */
    merkle_tree_root_variable.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " merkle_tree_root_variable")));

    old_coin_enforce_commitment.allocate(pb, num_old_coins, FMT(annotation_prefix, " old_coin_enforce_commitment"));
    old_coin_serial_number_variables.resize(num_old_coins);
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        old_coin_serial_number_variables[i].reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " old_coin_serial_number_variables_%zu", i)));
    }

    new_coin_commitment_variables.resize(num_new_coins);
    for (size_t i = 0; i < num_new_coins; ++i)
    {
        new_coin_commitment_variables[i].reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " new_coin_commitment_variables_%zu", i)));
    }

    public_old_value_variable.allocate(pb, coin_value_length, FMT(annotation_prefix, " public_old_value_variable"));
    public_new_value_variable.allocate(pb, coin_value_length, FMT(annotation_prefix, " public_new_value_variable"));
    signature_public_key_hash_variable.reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " signature_public_key_hash")));

    mac_of_signature_public_key_hash_variables.resize(num_old_coins);
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        mac_of_signature_public_key_hash_variables[i].reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " mac_of_signature_public_key_hash_variables_%zu", i)));
    }

    /* do the multipacking */
    input_as_bits.insert(input_as_bits.end(), merkle_tree_root_variable->bits.begin(), merkle_tree_root_variable->bits.end());
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        input_as_bits.insert(input_as_bits.end(), old_coin_serial_number_variables[i]->bits.begin(), old_coin_serial_number_variables[i]->bits.end());
    }
    for (size_t i = 0; i < num_new_coins; ++i)
    {
        input_as_bits.insert(input_as_bits.end(), new_coin_commitment_variables[i]->bits.begin(), new_coin_commitment_variables[i]->bits.end());
    }
    input_as_bits.insert(input_as_bits.end(), public_old_value_variable.begin(), public_old_value_variable.end());
    input_as_bits.insert(input_as_bits.end(), public_new_value_variable.begin(), public_new_value_variable.end());
    input_as_bits.insert(input_as_bits.end(), signature_public_key_hash_variable->bits.begin(), signature_public_key_hash_variable->bits.end());
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        input_as_bits.insert(input_as_bits.end(), mac_of_signature_public_key_hash_variables[i]->bits.begin(), mac_of_signature_public_key_hash_variables[i]->bits.end());
    }
    assert(input_as_bits.size() == input_size_in_bits);
    unpack_inputs.reset(new multipacking_gadget<FieldT>(this->pb, input_as_bits, input_as_field_elements, FieldT::capacity(), FMT(this->annotation_prefix, " unpack_inputs")));

    pb_linear_combination_array<FieldT> IV = SHA256_default_IV(pb);
    zero.allocate(this->pb, FMT(this->annotation_prefix, " zero")); /* TODO */

    /* allocate witness */
    new_address_public_key_variables.resize(num_new_coins);
    new_address_commitment_nonce_variables.resize(num_new_coins);
    new_coin_serial_number_nonce_variables.resize(num_new_coins);
    new_coin_value_variables.resize(num_new_coins);
    for (size_t i = 0; i < num_new_coins; ++i)
    {
        new_address_public_key_variables[i].allocate(pb, address_public_key_length, FMT(annotation_prefix, " new_address_public_key_variables_%zu", i));
        new_address_commitment_nonce_variables[i].allocate(pb, address_commitment_nonce_length, FMT(annotation_prefix, " new_address_commitment_nonce_variables_%zu", i));
        new_coin_serial_number_nonce_variables[i].allocate(pb, serial_number_nonce_length, FMT(annotation_prefix, " new_coin_serial_number_nonce_variables_%zu", i));
        new_coin_value_variables[i].allocate(pb, coin_value_length, FMT(annotation_prefix, " new_coin_value_variables_%zu", i));
    }

    old_address_secret_key_variables.resize(num_old_coins);
    old_address_commitment_nonce_variables.resize(num_old_coins);
    old_coin_serial_number_nonce_variables.resize(num_old_coins);
    old_coin_value_variables.resize(num_old_coins);
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        old_address_secret_key_variables[i].allocate(pb, address_secret_key_length, FMT(annotation_prefix, " old_address_secret_key_variables_%zu", i));
        old_address_commitment_nonce_variables[i].allocate(pb, address_commitment_nonce_length, FMT(annotation_prefix, " old_address_commitment_nonce_variables_%zu", i));
        old_coin_serial_number_nonce_variables[i].allocate(pb, serial_number_nonce_length, FMT(annotation_prefix, " old_coin_serial_number_nonce_variables_%zu", i));
        old_coin_value_variables[i].allocate(pb, coin_value_length, FMT(annotation_prefix, " old_coin_value_variables_%zu", i));
    }

    /* do the actual hashing */
    pb_variable_array<FieldT> zero_one;
    zero_one.emplace_back(zero);
    zero_one.emplace_back(ONE);

    prf_for_old_coin_serial_number_input_variables.resize(num_old_coins);
    prfs_for_old_coin_serial_numbers.resize(num_old_coins);
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        /* (C) old_coin_serial_number_variables[i] = PRF_{old_address_secret_key_variables[i]}^{sn}
           (old_coin_serial_number_nonce_variables[0..254]) =
           H(old_address_secret_key_variables[i] || 01 || old_coin_serial_number_nonce_variables[0..254]) */
        prf_for_old_coin_serial_number_input_variables[i].reset(new block_variable<FieldT>(pb, {
                    old_address_secret_key_variables[i],
                        zero_one,
                        pb_variable_array<FieldT>(old_coin_serial_number_nonce_variables[i].begin(),
                                                  old_coin_serial_number_nonce_variables[i].begin() + truncated_serial_number_length) },
                FMT(annotation_prefix, " prf_for_old_coin_serial_number_input_variables_%zu", i)));
        prfs_for_old_coin_serial_numbers[i].reset(new sha256_compression_function_gadget<FieldT>(pb, IV, prf_for_old_coin_serial_number_input_variables[i]->bits, *old_coin_serial_number_variables[i], FMT(annotation_prefix, " prfs_for_old_coin_serial_numbers_%zu", i)));
    }

    old_address_public_key_variables.resize(num_old_coins);
    prf_for_old_address_public_key_input_variables.resize(num_old_coins);
    prfs_for_old_address_public_keys.resize(num_old_coins);

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        old_address_public_key_variables[i].reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " old_address_public_key_variables_%zu", i)));

        /* (B) old_address_public_keys[i] = PRF_{old_address_secret_key_variables[i]}^{addr}(z) =
           H(old_address_secret_key_variables[i] || 00 || z), where z = 0...0 */
        pb_variable_array<FieldT> addr_pk_pad(address_public_key_padding_length, zero);
        prf_for_old_address_public_key_input_variables[i].reset(new block_variable<FieldT>(pb,
            { old_address_secret_key_variables[i], addr_pk_pad },
                FMT(annotation_prefix, " prf_for_old_address_public_key_input_variables_%zu", i)));
        prfs_for_old_address_public_keys[i].reset(new sha256_compression_function_gadget<FieldT>(pb,
                                                                                                 IV,
                                                                                                 prf_for_old_address_public_key_input_variables[i]->bits,
                                                                                                 *old_address_public_key_variables[i],
                                                                                                 FMT(annotation_prefix, " prfs_for_old_address_public_keys_%zu", i)));
    }

    commitments_to_old_address_public_keys.resize(num_old_coins);
    commit_to_old_address_public_key_input_variables.resize(num_old_coins);
    commit_to_old_address_public_keys.resize(num_old_coins);

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        /* (D0) commitments_to_old_address_public_keys[i] = H(old_address_public_key_variables[i] || old_coin_serial_number_nonce_variables[i]) */
        commitments_to_old_address_public_keys[i].reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " commitments_to_old_address_public_keys_%zu", i)));
        commit_to_old_address_public_key_input_variables[i].reset(new block_variable<FieldT>(pb, { old_address_public_key_variables[i]->bits, old_coin_serial_number_nonce_variables[i] }, FMT(annotation_prefix, " commit_to_old_address_public_key_input_variables_%zu", i)));
        commit_to_old_address_public_keys[i].reset(new sha256_compression_function_gadget<FieldT>(pb, IV, commit_to_old_address_public_key_input_variables[i]->bits, *commitments_to_old_address_public_keys[i], FMT(annotation_prefix, " commit_to_old_address_public_keys_%zu", i)));
    }

    old_coin_value_commitment_nonces.resize(num_old_coins);
    commit_to_old_coin_value_commitment_nonce_input_variables.resize(num_old_coins);
    commit_to_old_coin_value_commitment_nonces.resize(num_old_coins);

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        /* (D1) old_coin_value_commitment_nonces[i] =
           H(old_address_commitment_nonce_variables[i] || commitments_to_old_address_public_keys[i] [0..128]) */
        old_coin_value_commitment_nonces[i].reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " old_coin_value_commitment_nonces_%zu", i)));
        commit_to_old_coin_value_commitment_nonce_input_variables[i].reset(new block_variable<FieldT>(pb, { old_address_commitment_nonce_variables[i], pb_variable_array<FieldT>(commitments_to_old_address_public_keys[i]->bits.begin(), commitments_to_old_address_public_keys[i]->bits.begin()+ truncated_coin_commitment_length) }, FMT(annotation_prefix, " commit_to_old_coin_value_commitment_nonce_input_variables_%zu", i)));
        commit_to_old_coin_value_commitment_nonces[i].reset(new sha256_compression_function_gadget<FieldT>(pb, IV, commit_to_old_coin_value_commitment_nonce_input_variables[i]->bits, *old_coin_value_commitment_nonces[i], FMT(annotation_prefix, " commit_to_old_coin_value_commitment_nonces_%zu", i)));
    }

    pb_variable_array<FieldT> coincomm_pad(coin_commitment_padding_length, zero);
    old_coin_commitment_variables.resize(num_old_coins);
    compute_old_coin_commitment_input_variables.resize(num_old_coins);
    compute_old_coin_commitments.resize(num_old_coins);

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        /* (D2) old_coin_commitment_variables[i] = COMM_s(old_coin_value_variables[i] || old_coin_value_commitment_nonces[i])
           H(old_coin_value_commitment_nonces[i] || 0^{192} || old_coin_value_variables[i])

           Here we ignore commitment randomness s, as k = old_coin_value_commitment_nonces[i] is an output of a
           statistically hiding commitment scheme. */
        old_coin_commitment_variables[i].reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " old_coin_commitment_variables_%zu", i)));
        compute_old_coin_commitment_input_variables[i].reset(new block_variable<FieldT>(pb, { old_coin_value_commitment_nonces[i]->bits, coincomm_pad, old_coin_value_variables[i] }, FMT(annotation_prefix, " compute_old_coin_commitment_input_variables_%zu", i)));
        compute_old_coin_commitments[i].reset(new sha256_compression_function_gadget<FieldT>(pb, IV, compute_old_coin_commitment_input_variables[i]->bits, *old_coin_commitment_variables[i], FMT(annotation_prefix, " compute_old_coin_commitment_%zu", i)));
    }

    commitments_to_new_address_public_keys.resize(num_new_coins);
    commit_to_new_address_public_key_input_variables.resize(num_new_coins);
    commit_to_new_address_public_keys.resize(num_new_coins);

    for (size_t i = 0; i < num_new_coins; ++i)
    {
        /* (E0) commitments_to_new_address_public_keys[i] = H(new_address_public_key_variables[i] || new_coin_serial_number_nonce_variables[i]) */
        commitments_to_new_address_public_keys[i].reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " commitments_to_new_address_public_keys_%zu", i)));
        commit_to_new_address_public_key_input_variables[i].reset(new block_variable<FieldT>(pb, { new_address_public_key_variables[i], new_coin_serial_number_nonce_variables[i] }, FMT(annotation_prefix, " commit_to_new_address_public_key_input_variables_%zu", i)));
        commit_to_new_address_public_keys[i].reset(new sha256_compression_function_gadget<FieldT>(pb, IV, commit_to_new_address_public_key_input_variables[i]->bits, *commitments_to_new_address_public_keys[i], FMT(annotation_prefix, " commit_to_new_address_public_keys_%zu", i)));
    }

    new_coin_value_commitment_nonces.resize(num_new_coins);
    commit_to_new_coin_value_commitment_nonce_input_variables.resize(num_new_coins);
    commit_to_new_coin_value_commitment_nonces.resize(num_new_coins);
    for (size_t i = 0; i < num_new_coins; ++i)
    {
        /* (E1) new_coin_value_commitment_nonces[i] =
           H(new_address_commitment_nonce_variables[i] || commitments_to_new_address_public_keys[i] [0..128]) */
        new_coin_value_commitment_nonces[i].reset(new digest_variable<FieldT>(pb, sha256_digest_len, FMT(annotation_prefix, " new_coin_value_commitment_nonces_%zu", i)));
        commit_to_new_coin_value_commitment_nonce_input_variables[i].reset(new block_variable<FieldT>(pb, { new_address_commitment_nonce_variables[i], pb_variable_array<FieldT>(commitments_to_new_address_public_keys[i]->bits.begin(), commitments_to_new_address_public_keys[i]->bits.begin()+ truncated_coin_commitment_length) }, FMT(annotation_prefix, " commit_to_new_coin_value_commitment_nonce_input_variables_%zu", i)));
        commit_to_new_coin_value_commitment_nonces[i].reset(new sha256_compression_function_gadget<FieldT>(pb, IV, commit_to_new_coin_value_commitment_nonce_input_variables[i]->bits, *new_coin_value_commitment_nonces[i], FMT(annotation_prefix, " commit_to_new_coin_value_commitment_nonces_%zu", i)));
    }

    compute_new_coin_commitment_input_variables.resize(num_new_coins);
    compute_new_coin_commitments.resize(num_new_coins);

    for (size_t i = 0; i < num_new_coins; ++i)
    {
        /* (E2) new_coin_commitment_variables[i] = COMM_s(new_coin_value_variables[i] || new_coin_value_commitment_nonces[i])
           H(new_coin_value_commitment_nonces[i] || 0^{192} || new_coin_value_variables[i]) */
        compute_new_coin_commitment_input_variables[i].reset(new block_variable<FieldT>(pb, { new_coin_value_commitment_nonces[i]->bits, coincomm_pad, new_coin_value_variables[i] }, FMT(annotation_prefix, " compute_new_coin_commitment_input_variables_%zu", i)));
        compute_new_coin_commitments[i].reset(new sha256_compression_function_gadget<FieldT>(pb, IV, compute_new_coin_commitment_input_variables[i]->bits, *new_coin_commitment_variables[i], FMT(annotation_prefix, " compute_new_coin_commitment_%zu", i)));
    }

    /* compute signature public key macs */
    prf_for_macs_of_signature_public_key_hash_input_variables.resize(num_old_coins);
    prfs_for_macs_of_signature_public_key_hash.resize(num_old_coins);
    const size_t truncated_signature_public_key_hash_length = indexed_signature_public_key_hash_length - log2(num_old_coins);

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        /* (F) mac_of_signature_public_key_hash_variables[i] = PRF_{old_address_secret_key_variables[i]}^{pk}
           (i || signature_public_key_hash_variable) =
           H(old_address_secret_key_variables[i] || 10 || i || signature_public_key_hash_variable)

           Here signature_public_key_hash is truncated so that the entire argument fits inside SHA256 block.
           Furthermore, the representation of i is MSB to LSB and is exactly log2(num_old_coins) bits long. */
        pb_variable_array<FieldT> prf_padding;
        prf_padding.emplace_back(ONE);
        prf_padding.emplace_back(zero);

        for (size_t j = 0; j < log2(num_old_coins); ++j)
        {
            prf_padding.emplace_back((i >> (log2(num_old_coins) - j - 1)) & 1 ? ONE : zero);
        }

        prf_for_macs_of_signature_public_key_hash_input_variables[i].reset(new block_variable<FieldT>(pb, { old_address_secret_key_variables[i], prf_padding, pb_variable_array<FieldT>(signature_public_key_hash_variable->bits.begin(), signature_public_key_hash_variable->bits.begin()+truncated_signature_public_key_hash_length) }, FMT(annotation_prefix, " prf_for_macs_of_signature_public_key_hash_input_variables_%zu", i)));
        prfs_for_macs_of_signature_public_key_hash[i].reset(new sha256_compression_function_gadget<FieldT>(pb, IV, prf_for_macs_of_signature_public_key_hash_input_variables[i]->bits, *mac_of_signature_public_key_hash_variables[i], FMT(annotation_prefix, " prfs_for_macs_of_signature_public_key_hash_%zu", i)));
    }

    /* prove membership in the Merkle tree*/
    old_coin_merkle_tree_position_variables.resize(num_old_coins);
    old_coin_authentication_path_variables.resize(num_old_coins);
    old_coin_commitments_in_tree.resize(num_old_coins);
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        /* (A) old_coin_commitment_variables[i] appears on path old_coin_authentication_paths[i]
           to merkle_tree_root_variable */
        old_coin_merkle_tree_position_variables[i].allocate(pb, tree_depth, FMT(annotation_prefix, " old_coin_merkle_tree_position_variables_%zu", i));
        old_coin_authentication_path_variables[i].reset(new merkle_authentication_path_variable<FieldT, sha256_two_to_one_hash_gadget<FieldT> >(pb, tree_depth, FMT(annotation_prefix, " old_coin_authentication_path_variables_%zu", i)));
        old_coin_commitments_in_tree[i].reset(new merkle_tree_check_read_gadget<FieldT, sha256_two_to_one_hash_gadget<FieldT> >(
                                                  pb, tree_depth, old_coin_merkle_tree_position_variables[i], *old_coin_commitment_variables[i], *merkle_tree_root_variable,
                                                  *old_coin_authentication_path_variables[i], old_coin_enforce_commitment[i], FMT(annotation_prefix, " old_coin_commitments_in_tree_%zu", i)));
    }
}

template<typename FieldT>
void zerocash_pour_gadget<FieldT>::generate_r1cs_constraints()
{
    generate_r1cs_equals_const_constraint<FieldT>(this->pb, zero, FieldT::zero(), FMT(this->annotation_prefix, " zero"));

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        prfs_for_old_coin_serial_numbers[i]->generate_r1cs_constraints();
        prfs_for_old_address_public_keys[i]->generate_r1cs_constraints();
        commit_to_old_address_public_keys[i]->generate_r1cs_constraints();
        commit_to_old_coin_value_commitment_nonces[i]->generate_r1cs_constraints();
        compute_old_coin_commitments[i]->generate_r1cs_constraints();
        old_coin_commitments_in_tree[i]->generate_r1cs_constraints();
        prfs_for_macs_of_signature_public_key_hash[i]->generate_r1cs_constraints();

        for (size_t j = 0; j < tree_depth; ++j)
        {
            generate_boolean_r1cs_constraint<FieldT>(this->pb, old_coin_merkle_tree_position_variables[i][j], FMT(this->annotation_prefix, " old_coin_merkle_tree_position_variables_%zu_%zu", i, j));
        }
    }

    for (size_t i = 0; i < num_new_coins; ++i)
    {
        commit_to_new_address_public_keys[i]->generate_r1cs_constraints();
        commit_to_new_coin_value_commitment_nonces[i]->generate_r1cs_constraints();
        compute_new_coin_commitments[i]->generate_r1cs_constraints();
    }

    unpack_inputs->generate_r1cs_constraints(true);

    /* ensure bitness of all values */
    for (size_t j = 0; j < coin_value_length; ++j)
    {
        for (size_t i = 0; i < num_old_coins; ++i)
        {
            generate_boolean_r1cs_constraint<FieldT>(this->pb, old_coin_value_variables[i][j], FMT(this->annotation_prefix, " old_coin_value_variables_%zu_%zu", i, j));
        }
        for (size_t i = 0; i < num_new_coins; ++i)
        {
            generate_boolean_r1cs_constraint<FieldT>(this->pb, new_coin_value_variables[i][j], FMT(this->annotation_prefix, " new_coin_value_variables_%zu_%zu", i, j));
        }
    }

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        generate_boolean_r1cs_constraint<FieldT>(this->pb, old_coin_enforce_commitment[i], FMT(this->annotation_prefix, " old_coin_enforce_commitment_%zu", i));
        this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(
            pb_packing_sum<FieldT>(pb_variable_array<FieldT>(old_coin_value_variables[i].rbegin(), old_coin_value_variables[i].rend())),
            1 - old_coin_enforce_commitment[i],
            0), FMT(this->annotation_prefix, " enforce_%zu", i));
    }

    /* check the balance equation */
    linear_combination<FieldT> old_packed_value;
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        old_packed_value = old_packed_value + pb_packing_sum<FieldT>(pb_variable_array<FieldT>(old_coin_value_variables[i].rbegin(), old_coin_value_variables[i].rend()));
    }
    old_packed_value = old_packed_value + pb_packing_sum<FieldT>(pb_variable_array<FieldT>(public_old_value_variable.rbegin(), public_old_value_variable.rend()));

    linear_combination<FieldT> new_packed_value;
    for (size_t i = 0; i < num_new_coins; ++i)
    {
        new_packed_value = new_packed_value + pb_packing_sum<FieldT>(pb_variable_array<FieldT>(new_coin_value_variables[i].rbegin(), new_coin_value_variables[i].rend()));
    }
    new_packed_value = new_packed_value + pb_packing_sum<FieldT>(pb_variable_array<FieldT>(public_new_value_variable.rbegin(), public_new_value_variable.rend()));

    this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(1, old_packed_value, new_packed_value), FMT(this->annotation_prefix, " balance"));
}

template<typename FieldT>
void zerocash_pour_gadget<FieldT>::generate_r1cs_witness(const std::vector<merkle_authentication_path> &old_coin_authentication_paths,
                                                         const std::vector<size_t> &old_coin_merkle_tree_positions,
                                                         const bit_vector &merkle_tree_root,
                                                         const std::vector<bit_vector> &new_address_public_keys,
                                                         const std::vector<bit_vector> &old_address_secret_keys,
                                                         const std::vector<bit_vector> &new_address_commitment_nonces,
                                                         const std::vector<bit_vector> &old_address_commitment_nonces,
                                                         const std::vector<bit_vector> &new_coin_serial_number_nonces,
                                                         const std::vector<bit_vector> &old_coin_serial_number_nonces,
                                                         const std::vector<bit_vector> &new_coin_values,
                                                         const bit_vector &public_old_value,
                                                         const bit_vector &public_new_value,
                                                         const std::vector<bit_vector> &old_coin_values,
                                                         const bit_vector &signature_public_key_hash)
{
    /* fill in the auxiliary variables */
    this->pb.val(zero) = FieldT::zero();

    /* fill in the witness */
    for (size_t i = 0; i < num_new_coins; ++i)
    {
        new_address_public_key_variables[i].fill_with_bits(this->pb, new_address_public_keys[i]);
        new_address_commitment_nonce_variables[i].fill_with_bits(this->pb, new_address_commitment_nonces[i]);
    }

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        old_address_secret_key_variables[i].fill_with_bits(this->pb, old_address_secret_keys[i]);
        old_address_commitment_nonce_variables[i].fill_with_bits(this->pb, old_address_commitment_nonces[i]);
    }

    for (size_t i = 0; i < num_new_coins; ++i)
    {
        new_coin_serial_number_nonce_variables[i].fill_with_bits(this->pb, new_coin_serial_number_nonces[i]);
        new_coin_value_variables[i].fill_with_bits(this->pb, new_coin_values[i]);
    }

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        this->pb.val(old_coin_enforce_commitment[i]) = FieldT::zero();
        old_coin_serial_number_nonce_variables[i].fill_with_bits(this->pb, old_coin_serial_number_nonces[i]);
        old_coin_value_variables[i].fill_with_bits(this->pb, old_coin_values[i]);

        for (size_t j = 0; j < coin_value_length; ++j)
        {
            if (old_coin_values[i][j]) {
                // If any bit in the value is nonzero, the value is nonzero.
                // Thus, the old coin must be committed in the tree.
                this->pb.val(old_coin_enforce_commitment[i]) = FieldT::one();
                break;
            }
        }
    }

    public_old_value_variable.fill_with_bits(this->pb, public_old_value);
    public_new_value_variable.fill_with_bits(this->pb, public_new_value);
    signature_public_key_hash_variable->generate_r1cs_witness(signature_public_key_hash);

    /* do the hashing */
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        prfs_for_old_coin_serial_numbers[i]->generate_r1cs_witness();
        prfs_for_old_address_public_keys[i]->generate_r1cs_witness();
        commit_to_old_address_public_keys[i]->generate_r1cs_witness();
        commit_to_old_coin_value_commitment_nonces[i]->generate_r1cs_witness();
        compute_old_coin_commitments[i]->generate_r1cs_witness();
        prfs_for_macs_of_signature_public_key_hash[i]->generate_r1cs_witness();
    }

    for (size_t i = 0; i < num_new_coins; ++i)
    {
        commit_to_new_address_public_keys[i]->generate_r1cs_witness();
        commit_to_new_coin_value_commitment_nonces[i]->generate_r1cs_witness();
        compute_new_coin_commitments[i]->generate_r1cs_witness();
    }

    /* prove the membership in the Merkle tree */
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        /* (A) old_coin_commitment_variables[i] appears on path old_coin_authentication_paths[i]
           to merkle_tree_root_variable */
        old_coin_merkle_tree_position_variables[i].fill_with_bits_of_ulong(this->pb, old_coin_merkle_tree_positions[i]);
        old_coin_authentication_path_variables[i]->generate_r1cs_witness(old_coin_merkle_tree_positions[i], old_coin_authentication_paths[i]);
        old_coin_commitments_in_tree[i]->generate_r1cs_witness();
    }

    /* pack the input */
    unpack_inputs->generate_r1cs_witness_from_bits();

#ifdef DEBUG
    printf("input_as_field_elements according to witness map:\n");
    for (size_t i = 0; i < input_as_field_elements.size(); ++i)
    {
        this->pb.val(input_as_field_elements[i]).print();
    }
#endif
}

template<typename FieldT>
r1cs_primary_input<FieldT> zerocash_pour_input_map(const size_t num_old_coins,
                                                   const size_t num_new_coins,
                                                   const bit_vector &merkle_tree_root,
                                                   const std::vector<bit_vector> &old_coin_serial_numbers,
                                                   const std::vector<bit_vector> &new_coin_commitments,
                                                   const bit_vector &public_old_value,
                                                   const bit_vector &public_new_value,
                                                   const bit_vector &signature_public_key_hash,
                                                   const std::vector<bit_vector> &signature_public_key_hash_macs)
{
    enter_block("Call to zerocash_pour_input_map");
    assert(merkle_tree_root.size() == sha256_digest_len);
    assert(old_coin_serial_numbers.size() == num_old_coins);
    for (auto &old_coin_serial_number : old_coin_serial_numbers)
    {
        assert(old_coin_serial_number.size() == serial_number_length);
    }
    assert(new_coin_commitments.size() == num_new_coins);
    for (auto &new_coin_commitment : new_coin_commitments)
    {
        assert(new_coin_commitment.size() == coin_commitment_length);
    }
    assert(public_old_value.size() == coin_value_length);
    assert(public_new_value.size() == coin_value_length);
    assert(signature_public_key_hash.size() == sha256_digest_len);
    assert(signature_public_key_hash_macs.size() == num_old_coins);
    for (auto &signature_public_key_hash_mac : signature_public_key_hash_macs)
    {
        assert(signature_public_key_hash_mac.size() == sha256_digest_len);
    }

    bit_vector input_as_bits;

    input_as_bits.insert(input_as_bits.end(), merkle_tree_root.begin(), merkle_tree_root.end());
    for (auto &old_coin_serial_number : old_coin_serial_numbers)
    {
        input_as_bits.insert(input_as_bits.end(), old_coin_serial_number.begin(), old_coin_serial_number.end());
    }
    for (auto &new_coin_commitment : new_coin_commitments)
    {
        input_as_bits.insert(input_as_bits.end(), new_coin_commitment.begin(), new_coin_commitment.end());
    }
    input_as_bits.insert(input_as_bits.end(), public_old_value.begin(), public_old_value.end());
    input_as_bits.insert(input_as_bits.end(), public_new_value.begin(), public_new_value.end());
    input_as_bits.insert(input_as_bits.end(), signature_public_key_hash.begin(), signature_public_key_hash.end());
    for (auto &signature_public_key_hash_mac : signature_public_key_hash_macs)
    {
        input_as_bits.insert(input_as_bits.end(), signature_public_key_hash_mac.begin(), signature_public_key_hash_mac.end());
    }
    std::vector<FieldT> input_as_field_elements = pack_bit_vector_into_field_element_vector<FieldT>(input_as_bits);

#ifdef DEBUG
    printf("input_as_field_elements from zerocash_pour_input_map:\n");
    for (size_t i = 0; i < input_as_field_elements.size(); ++i)
    {
        input_as_field_elements[i].print();
    }
#endif
    leave_block("Call to zerocash_pour_input_map");

    return input_as_field_elements;
}

} // libzerocash
