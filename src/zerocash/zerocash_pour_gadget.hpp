/** @file
 *****************************************************************************

 Declaration of interfaces for the Zerocash Pour gadget.

 The Pour gadget implements the NP statement "Zerocash Pour" described in \[BCGGMTV14].


 References:

 \[BCGGMTV14]:
 "Zerocash: Decentralized Anonymous Payments from Bitcoin",
 Eli Ben-Sasson, Alessandro Chiesa, Christina Garman, Matthew Green, Ian Miers, Eran Tromer, Madars Virza,
 S&P 2014,
 <https://eprint.iacr.org/2014/349>

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ZEROCASH_POUR_GADGET_HPP_
#define ZEROCASH_POUR_GADGET_HPP_

#include "zerocash_pour_params.hpp"
#include "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_gadget.hpp"
#include "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.hpp"

namespace libzerocash {

using namespace libsnark;

/**
 * Gadget for the NP statement Zerocash Pour.
 *
 * More precisely, this gadgets checks the following NP statement.
 * (See Section 4.2 and Section 5.1 of the Zerocash paper for more details.)
 *
 * (A) old_coin_commitment_variables[i] appears on path old_coin_authentication_paths[i] to merkle_tree_root_variable
 *
 * (B) old_address_public_keys[i]
 *     = PRF_{old_address_secret_key_variables[i]}^{addr}(z)
 *     = H(old_address_secret_key_variables[i] || 00 || z)
 *     where z = 0...0
 *
 * (C) old_coin_serial_number_variables[i]
 *     = PRF_{old_address_secret_key_variables[i]}^{sn}(old_coin_serial_number_nonce_variables[0..254])
 *     = H(old_address_secret_key_variables[i] || 01 || old_coin_serial_number_nonce_variables[0..254])
 *
 * Properties (D0) and (D1) jointly ensure that
 *
 *       old_coin_value_commitment_nonces[i]
 *       = COMM_{old_address_commitment_nonce_variables[i]}(old_address_public_key_variables[i] || old_coin_serial_number_nonce_variables[i])
 *
 * as follows:
 *
 *   (D0) commitments_to_old_address_public_keys[i]
 *        = H(old_address_public_key_variables[i] || old_coin_serial_number_nonce_variables[i])
 *
 *   (D1) old_coin_value_commitment_nonces[i]
 *        = H(old_address_commitment_nonce_variables[i] || commitments_to_old_address_public_keys[i] [0..128])
 *
 * Given this, (D2) computes old_coin_commitments:
 *
 *   (D2) old_coin_commitment_variables[i]
 *        = COMM_s(old_coin_value_variables[i] || old_coin_value_commitment_nonces[i])
 *        = H(old_coin_value_commitment_nonces[i] || 0^{192} || old_coin_value_variables[i])
 *
 *        Here we ignore commitment randomness s, because
 *                   k = old_coin_value_commitment_nonces[i]
 *        is an output of a statistically-hiding commitment scheme.
 *
 * While (D0) and (D1) check that old coin commitments are well formed,
 * (E0) and (E1) check the same properties for new coins.
 *
 * (F) mac_of_signature_public_key_hash_variables[i]
 *     = PRF_{old_address_secret_key_variables[i]}^{pk}(i || signature_public_key_hash_variable)
 *     = H(old_address_secret_key_variables[i] || 10 || i || signature_public_key_hash_variable)
 *
 *     Here signature_public_key_hash is truncated so that the entire argument fits inside SHA256 block.
 *     Furthermore, the representation of i is MSB to LSB and is exactly log2(num_old_coins) bits long.
 */
template<typename FieldT>
class zerocash_pour_gadget : public gadget<FieldT> {
public:
    size_t tree_depth;

    pb_variable_array<FieldT> input_as_field_elements; /* R1CS input */
    pb_variable_array<FieldT> input_as_bits; /* unpacked R1CS input */
    std::shared_ptr<multipacking_gadget<FieldT> > unpack_inputs;

    /* individual components of the unpacked R1CS input */
    std::shared_ptr<digest_variable<FieldT> > merkle_tree_root_variable;
    std::vector<std::shared_ptr<digest_variable<FieldT> > > old_coin_serial_number_variables;
    pb_variable_array<FieldT> old_coin_enforce_commitment;
    std::vector<std::shared_ptr<digest_variable<FieldT> > > new_coin_commitment_variables;
    pb_variable_array<FieldT> public_old_value_variable;
    pb_variable_array<FieldT> public_new_value_variable;
    std::shared_ptr<digest_variable<FieldT> > signature_public_key_hash_variable;
    std::vector<std::shared_ptr<digest_variable<FieldT> > > mac_of_signature_public_key_hash_variables;

    /* TODO */
    pb_variable<FieldT> zero;

    std::vector<pb_variable_array<FieldT> > new_address_public_key_variables;
    std::vector<pb_variable_array<FieldT> > old_address_secret_key_variables;
    std::vector<pb_variable_array<FieldT> > new_address_commitment_nonce_variables;
    std::vector<pb_variable_array<FieldT> > old_address_commitment_nonce_variables;
    std::vector<pb_variable_array<FieldT> > new_coin_serial_number_nonce_variables;
    std::vector<pb_variable_array<FieldT> > old_coin_serial_number_nonce_variables;
    std::vector<pb_variable_array<FieldT> > new_coin_value_variables;
    std::vector<pb_variable_array<FieldT> > old_coin_value_variables;

    std::vector<std::shared_ptr<block_variable<FieldT> > > prf_for_old_coin_serial_number_input_variables;
    std::vector<std::shared_ptr<sha256_compression_function_gadget<FieldT> > > prfs_for_old_coin_serial_numbers; // (C)

    std::vector<std::shared_ptr<digest_variable<FieldT> > > old_address_public_key_variables;
    std::vector<std::shared_ptr<block_variable<FieldT> > > prf_for_old_address_public_key_input_variables;
    std::vector<std::shared_ptr<sha256_compression_function_gadget<FieldT> > > prfs_for_old_address_public_keys; // (B)

    std::vector<std::shared_ptr<digest_variable<FieldT> > > commitments_to_old_address_public_keys;
    std::vector<std::shared_ptr<block_variable<FieldT> > > commit_to_old_address_public_key_input_variables;
    std::vector<std::shared_ptr<sha256_compression_function_gadget<FieldT> > > commit_to_old_address_public_keys; // (D0)

    std::vector<std::shared_ptr<digest_variable<FieldT> > > old_coin_value_commitment_nonces;
    std::vector<std::shared_ptr<block_variable<FieldT> > > commit_to_old_coin_value_commitment_nonce_input_variables;
    std::vector<std::shared_ptr<sha256_compression_function_gadget<FieldT> > > commit_to_old_coin_value_commitment_nonces; // (D1)

    std::vector<std::shared_ptr<digest_variable<FieldT> > > old_coin_commitment_variables;
    std::vector<std::shared_ptr<block_variable<FieldT> > > compute_old_coin_commitment_input_variables;
    std::vector<std::shared_ptr<sha256_compression_function_gadget<FieldT> > > compute_old_coin_commitments; // (D2)

    std::vector<std::shared_ptr<digest_variable<FieldT> > > commitments_to_new_address_public_keys;
    std::vector<std::shared_ptr<block_variable<FieldT> > > commit_to_new_address_public_key_input_variables;
    std::vector<std::shared_ptr<sha256_compression_function_gadget<FieldT> > > commit_to_new_address_public_keys; // (E0)

    std::vector<std::shared_ptr<digest_variable<FieldT> > > new_coin_value_commitment_nonces;
    std::vector<std::shared_ptr<block_variable<FieldT> > > commit_to_new_coin_value_commitment_nonce_input_variables;
    std::vector<std::shared_ptr<sha256_compression_function_gadget<FieldT> > > commit_to_new_coin_value_commitment_nonces; // (E1)

    std::vector<std::shared_ptr<block_variable<FieldT> > > compute_new_coin_commitment_input_variables;
    std::vector<std::shared_ptr<sha256_compression_function_gadget<FieldT> > > compute_new_coin_commitments; // (E2)

    std::vector<std::shared_ptr<block_variable<FieldT> > > prf_for_macs_of_signature_public_key_hash_input_variables;
    std::vector<std::shared_ptr<sha256_compression_function_gadget<FieldT> > > prfs_for_macs_of_signature_public_key_hash; // (F)

    std::vector<pb_variable_array<FieldT> > old_coin_merkle_tree_position_variables;
    std::vector<std::shared_ptr<merkle_authentication_path_variable<FieldT, sha256_two_to_one_hash_gadget<FieldT> > > > old_coin_authentication_path_variables;
    std::vector<std::shared_ptr<merkle_tree_check_read_gadget<FieldT, sha256_two_to_one_hash_gadget<FieldT> > > > old_coin_commitments_in_tree; // (A)

    size_t num_old_coins;
    size_t num_new_coins;

    zerocash_pour_gadget(protoboard<FieldT> &pb, const size_t num_old_coins, const size_t num_new_coins, const size_t tree_depth, const std::string &annotation_prefix);
    void generate_r1cs_constraints();
    void generate_r1cs_witness(const std::vector<merkle_authentication_path> &old_coin_authentication_paths,
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
                               const bit_vector &signature_public_key_hash);
};

template<typename FieldT>
r1cs_primary_input<FieldT> zerocash_pour_input_map(const size_t num_old_coins,
                                                   const size_t num_new_coins,
                                                   const bit_vector &merkle_tree_root,
                                                   const std::vector<bit_vector> &old_coin_serial_numbers,
                                                   const std::vector<bit_vector> &new_coin_commitments,
                                                   const bit_vector &public_old_value,
                                                   const bit_vector &public_new_value,
                                                   const bit_vector &signature_public_key_hash,
                                                   const std::vector<bit_vector> &signature_public_key_hash_macs);

} // libzerocash

#include "zerocash_pour_gadget.tcc"

#endif // ZEROCASH_POUR_GADGET_HPP_
