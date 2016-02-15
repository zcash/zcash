/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include <algorithm>
#include <random>
#include <set>
#include <vector>

#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/common/data_structures/merkle_tree.hpp"
#include "libsnark/common/utils.hpp"
#include "libsnark/common/profiling.hpp"
#include "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_gadget.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_gadget.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_ppzksnark.hpp"

using namespace libzerocash;

bit_vector int_to_bit_vector(const size_t value, const size_t length)
{
    /* the returned result will have 0-th position equal to MSB of
       `value`, when written as `length`-bit integer */

    assert(log2(value) < length);
    bit_vector result(length, false);

    for (size_t i = 0; i < length; ++i)
    {
        result[length-1-i] = ((value >> i) & 1) ? true : false;
    }

    return result;
}

bit_vector get_random_bit_vector(const size_t length)
{
    bit_vector result(length);
    std::generate(result.begin(), result.end(), [] { return std::rand() % 2; });
    return result;
}

std::vector<size_t> sample_random_positions(const size_t num_positions, const size_t log2_universe_size)
{
    /* not asymptotically optimal, but likely not to be a problem in
       practice, where num_positions is much smaller than
       universe_size */
    assert(log2(num_positions) <= log2_universe_size);
    assert(log2_universe_size <= 8 * sizeof(size_t));
    std::set<size_t> positions;
    std::vector<size_t> result;
    while (positions.size() != num_positions)
    {
        size_t new_pos = std::rand();
        if (log2_universe_size < 8 * sizeof(size_t))
        {
            new_pos &= ((1ul << log2_universe_size) - 1); /* clear higher bits appropriately */
        }
        if (positions.find(new_pos) == positions.end())
        {
            positions.insert(new_pos);
            result.emplace_back(new_pos);
        }
    }

    return result;
}

template<typename FieldT>
bit_vector compute_coin_commitment(const bit_vector &address_public_key,
                                   const bit_vector &coin_value,
                                   const bit_vector &serial_number_nonce,
                                   const bit_vector &address_commitment_nonce)
{
    /* commitment_to_address_public_key = H(address_public_key || serial_number_nonce) */
    bit_vector commitment_to_address_public_key_hash_input;
    commitment_to_address_public_key_hash_input.insert(commitment_to_address_public_key_hash_input.end(),
                                                       address_public_key.begin(),
                                                       address_public_key.end());
    commitment_to_address_public_key_hash_input.insert(commitment_to_address_public_key_hash_input.end(),
                                                       serial_number_nonce.begin(),
                                                       serial_number_nonce.end());
    const bit_vector commitment_to_address_public_key = sha256_two_to_one_hash_gadget<FieldT>::get_hash(commitment_to_address_public_key_hash_input);

    /* coin_value_commitment_nonce = H(address_commitment_nonce || commitment_to_address_public_key[0..128]) */
    bit_vector coin_value_commitment_nonce_hash_input;
    coin_value_commitment_nonce_hash_input.insert(coin_value_commitment_nonce_hash_input.end(),
                                                  address_commitment_nonce.begin(),
                                                  address_commitment_nonce.end());
    coin_value_commitment_nonce_hash_input.insert(coin_value_commitment_nonce_hash_input.end(),
                                                  commitment_to_address_public_key.begin(),
                                                  commitment_to_address_public_key.begin() + truncated_coin_commitment_length);

    const bit_vector coin_value_commitment_nonce = sha256_two_to_one_hash_gadget<FieldT>::get_hash(coin_value_commitment_nonce_hash_input);

    /* coin_commitment = H(coin_value_commitment_nonce || 0^{192} || coin_value) */
    bit_vector coin_commitment_hash_input;
    coin_commitment_hash_input.insert(coin_commitment_hash_input.end(),
                                      coin_value_commitment_nonce.begin(),
                                      coin_value_commitment_nonce.end());
    coin_commitment_hash_input.resize(coin_commitment_hash_input.size() + coin_commitment_padding_length, false);
    coin_commitment_hash_input.insert(coin_commitment_hash_input.end(),
                                      coin_value.begin(),
                                      coin_value.end());
    const bit_vector coin_commitment = sha256_two_to_one_hash_gadget<FieldT>::get_hash(coin_commitment_hash_input);

    return coin_commitment;
}

std::vector<size_t> randomly_split_up_value(const size_t value, const size_t num_parts)
{
    std::vector<size_t> points(num_parts-1);
    std::generate(points.begin(), points.end(), [value] { return std::rand() % value; });
    points.emplace_back(0);
    points.emplace_back(value);
    std::sort(points.begin(), points.end()); /* now points is a num_parts+1-length vector with endpoints of 0 and value */

    std::vector<size_t> result(num_parts);
    for (size_t i = 0; i < num_parts; ++i)
    {
        result[i] = points[i+1] - points[i];
    }
    /* the resulting sum telescopes to value-0 = value */

    return result;
}

template<typename ppT>
void test_zerocash_pour_ppzksnark(const size_t num_old_coins, const size_t num_new_coins, const size_t tree_depth)
{
    typedef Fr<ppT> FieldT;

    assert(log2(num_old_coins) <= tree_depth);

    /* information used by the prover in the witness map */
    std::vector<merkle_authentication_path> old_coin_authentication_paths(num_old_coins); //
    std::vector<size_t> old_coin_merkle_tree_positions; //
    bit_vector merkle_tree_root; //
    std::vector<bit_vector> new_address_public_keys(num_new_coins); //
    std::vector<bit_vector> old_address_secret_keys(num_old_coins); //
    std::vector<bit_vector> new_address_commitment_nonces(num_new_coins); //
    std::vector<bit_vector> old_address_commitment_nonces(num_old_coins); //
    std::vector<bit_vector> new_coin_serial_number_nonces(num_new_coins); //
    std::vector<bit_vector> old_coin_serial_number_nonces(num_old_coins); //
    std::vector<bit_vector> new_coin_values(num_new_coins); //
    bit_vector public_in_value; //
    bit_vector public_out_value; //
    std::vector<bit_vector> old_coin_values(num_old_coins); //
    bit_vector signature_public_key_hash; //

    /* generate split for the money */
    std::vector<size_t> old_coin_values_as_integers(num_old_coins);
    std::generate(old_coin_values_as_integers.begin(), old_coin_values_as_integers.end(), [] { return std::rand() % 10000; });
    const size_t total_value_as_integer = std::accumulate(old_coin_values_as_integers.begin(), old_coin_values_as_integers.end(), 0);
    std::vector<size_t> all_new_values_as_integers = randomly_split_up_value(total_value_as_integer, num_new_coins + 1);

    std::transform(old_coin_values_as_integers.begin(), old_coin_values_as_integers.end(),
                   old_coin_values.begin(),
                   [] (const size_t value) { return int_to_bit_vector(value, coin_value_length); });
    public_in_value = int_to_bit_vector(0, coin_value_length);
    public_out_value = int_to_bit_vector(all_new_values_as_integers[0], coin_value_length);
    std::transform(all_new_values_as_integers.begin() + 1, all_new_values_as_integers.end(),
                   new_coin_values.begin(),
                   [] (const size_t value) { return int_to_bit_vector(value, coin_value_length); });

    /* generate random private values for the prover */
    old_coin_merkle_tree_positions = sample_random_positions(num_old_coins, tree_depth);
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        old_address_secret_keys[i] = get_random_bit_vector(address_secret_key_length);
        old_address_commitment_nonces[i] = get_random_bit_vector(address_commitment_nonce_length);
        old_coin_serial_number_nonces[i] = get_random_bit_vector(serial_number_nonce_length);
    }

    for (size_t i = 0; i < num_new_coins; ++i)
    {
        new_address_public_keys[i] = get_random_bit_vector(address_public_key_length);
        new_address_commitment_nonces[i] = get_random_bit_vector(address_commitment_nonce_length);
        new_coin_serial_number_nonces[i] = get_random_bit_vector(serial_number_nonce_length);
    }

    merkle_tree<sha256_two_to_one_hash_gadget<FieldT> > tree(tree_depth, coin_commitment_length);
    for (size_t i = 0; i < num_old_coins; ++i)
    {
        /* calculate old coin and place it in the Merkle tree */

        /* old_address_public_key = H(old_address_secret_key || 00...) */
        bit_vector old_address_public_key_hash_input = old_address_secret_keys[i];
        old_address_public_key_hash_input.resize(sha256_block_len);
        const bit_vector old_address_public_key = sha256_two_to_one_hash_gadget<FieldT>::get_hash(old_address_public_key_hash_input);

        const bit_vector old_coin = compute_coin_commitment<FieldT>(old_address_public_key,
                                                                    old_coin_values[i],
                                                                    old_coin_serial_number_nonces[i],
                                                                    old_address_commitment_nonces[i]);
        tree.set_value(old_coin_merkle_tree_positions[i], old_coin);
    }

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        /* get the corresponding authentication paths */
        old_coin_authentication_paths[i] = tree.get_path(old_coin_merkle_tree_positions[i]);
    }

    merkle_tree_root = tree.get_root();
    signature_public_key_hash = get_random_bit_vector(sha256_digest_len);

    /* calculate the values used by the verifier */
    std::vector<bit_vector> old_coin_serial_numbers(num_old_coins); //
    std::vector<bit_vector> new_coin_commitments(num_new_coins); //
    std::vector<bit_vector> signature_public_key_hash_macs(num_old_coins); //

    for (size_t i = 0; i < num_old_coins; ++i)
    {
        /* serial_number = H(address_secret_key || 01 || serial_number_nonce[0..254]) */
        bit_vector old_coin_serial_number_hash_input;
        old_coin_serial_number_hash_input.insert(old_coin_serial_number_hash_input.end(),
                                                 old_address_secret_keys[i].begin(),
                                                 old_address_secret_keys[i].end());
        old_coin_serial_number_hash_input.push_back(false);
        old_coin_serial_number_hash_input.push_back(true);
        old_coin_serial_number_hash_input.insert(old_coin_serial_number_hash_input.end(),
                                                 old_coin_serial_number_nonces[i].begin(),
                                                 old_coin_serial_number_nonces[i].begin() + truncated_serial_number_length);

        old_coin_serial_numbers[i] = sha256_two_to_one_hash_gadget<FieldT>::get_hash(old_coin_serial_number_hash_input);

        /* signature_public_key_hash_macs[i] = H(old_address_secret_keys[i] || 10 || i || signature_public_key_hash) */
        const size_t truncated_signature_public_key_hash_length = indexed_signature_public_key_hash_length - log2(num_old_coins);

        bit_vector signature_public_key_hash_macs_hash_input;
        signature_public_key_hash_macs_hash_input.insert(signature_public_key_hash_macs_hash_input.end(),
                                                         old_address_secret_keys[i].begin(),
                                                         old_address_secret_keys[i].end());
        signature_public_key_hash_macs_hash_input.push_back(true);
        signature_public_key_hash_macs_hash_input.push_back(false);
        const bit_vector i_as_bits = int_to_bit_vector(i, log2(num_old_coins));
        signature_public_key_hash_macs_hash_input.insert(signature_public_key_hash_macs_hash_input.end(),
                                                         i_as_bits.begin(),
                                                         i_as_bits.end());
        signature_public_key_hash_macs_hash_input.insert(signature_public_key_hash_macs_hash_input.end(),
                                                         signature_public_key_hash.begin(),
                                                         signature_public_key_hash.begin() + truncated_signature_public_key_hash_length);
        signature_public_key_hash_macs[i] = sha256_two_to_one_hash_gadget<FieldT>::get_hash(signature_public_key_hash_macs_hash_input);
    }

    for (size_t i = 0; i < num_new_coins; ++i)
    {
        new_coin_commitments[i] = compute_coin_commitment<FieldT>(new_address_public_keys[i],
                                                                  new_coin_values[i],
                                                                  new_coin_serial_number_nonces[i],
                                                                  new_address_commitment_nonces[i]);
    }

    /* perform basic sanity checks */
    {
        protoboard<FieldT> pb;
        zerocash_pour_gadget<FieldT> pour(pb, num_old_coins, num_new_coins, tree_depth, "pour");
        pour.generate_r1cs_constraints();
        pour.generate_r1cs_witness(old_coin_authentication_paths,
                                   old_coin_merkle_tree_positions,
                                   merkle_tree_root,
                                   new_address_public_keys,
                                   old_address_secret_keys,
                                   new_address_commitment_nonces,
                                   old_address_commitment_nonces,
                                   new_coin_serial_number_nonces,
                                   old_coin_serial_number_nonces,
                                   new_coin_values,
                                   public_in_value,
                                   public_out_value,
                                   old_coin_values,
                                   signature_public_key_hash);
        assert(pb.is_satisfied());
        printf("gadget test OK for num_old_coins = %zu, num_new_coins = %zu, tree_depth = %zu\n",
               num_old_coins, num_new_coins, tree_depth);
    }

    /* do the end-to-end test */
    zerocash_pour_keypair<ppT> keypair = zerocash_pour_ppzksnark_generator<ppT>(num_old_coins, num_new_coins, tree_depth);
    keypair = reserialize<zerocash_pour_keypair<ppT> >(keypair);

    zerocash_pour_proof<ppT> proof = zerocash_pour_ppzksnark_prover<ppT>(keypair.pk,
                                                                         old_coin_authentication_paths,
                                                                         old_coin_merkle_tree_positions,
                                                                         merkle_tree_root,
                                                                         new_address_public_keys,
                                                                         old_address_secret_keys,
                                                                         new_address_commitment_nonces,
                                                                         old_address_commitment_nonces,
                                                                         new_coin_serial_number_nonces,
                                                                         old_coin_serial_number_nonces,
                                                                         new_coin_values,
                                                                         public_in_value,
                                                                         public_out_value,
                                                                         old_coin_values,
                                                                         signature_public_key_hash);
    proof = reserialize<zerocash_pour_proof<ppT> >(proof);

    const bool verification_result = zerocash_pour_ppzksnark_verifier<ppT>(keypair.vk,
                                                                           merkle_tree_root,
                                                                           old_coin_serial_numbers,
                                                                           new_coin_commitments,
                                                                           public_in_value,
                                                                           public_out_value,
                                                                           signature_public_key_hash,
                                                                           signature_public_key_hash_macs,
                                                                           proof);
    printf("Verification result: %s\n", verification_result ? "pass" : "FAIL");
    assert(verification_result);
}

int main(int argc, const char * argv[])
{
    start_profiling();
    default_r1cs_ppzksnark_pp::init_public_params();
    test_zerocash_pour_ppzksnark<default_r1cs_ppzksnark_pp>(2, 2, 4);
    test_zerocash_pour_ppzksnark<default_r1cs_ppzksnark_pp>(2, 3, 4);
    test_zerocash_pour_ppzksnark<default_r1cs_ppzksnark_pp>(3, 2, 4);
    test_zerocash_pour_ppzksnark<default_r1cs_ppzksnark_pp>(3, 3, 4);
    test_zerocash_pour_ppzksnark<default_r1cs_ppzksnark_pp>(2, 2, 32);
}
