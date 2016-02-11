/** @file
 *****************************************************************************

 Implementation of interfaces for a ppzkSNARK for the NP statement "Pour".

 See zerocash_pour_ppzksnark.hpp .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ZEROCASH_POUR_PPZKSNARK_TCC_
#define ZEROCASH_POUR_PPZKSNARK_TCC_

#include "zerocash_pour_ppzksnark/zerocash_pour_gadget.hpp"
#include "common/profiling.hpp"

namespace libzerocash {

template<typename ppzksnark_ppT>
bool zerocash_pour_proving_key<ppzksnark_ppT>::operator==(const zerocash_pour_proving_key<ppzksnark_ppT> &other) const
{
    return (this->num_old_coins == other.num_old_coins &&
            this->num_new_coins == other.num_new_coins &&
            this->tree_depth == other.tree_depth &&
            this->r1cs_pk == other.r1cs_pk);
}

template<typename ppzksnark_ppT>
std::ostream& operator<<(std::ostream &out, const zerocash_pour_proving_key<ppzksnark_ppT> &pk)
{
    out << pk.num_old_coins << "\n";
    out << pk.num_new_coins << "\n";
    out << pk.tree_depth << "\n";
    out << pk.r1cs_pk;

    return out;
}

template<typename ppzksnark_ppT>
std::istream& operator>>(std::istream &in, zerocash_pour_proving_key<ppzksnark_ppT> &pk)
{
    in >> pk.num_old_coins;
    consume_newline(in);
    in >> pk.num_new_coins;
    consume_newline(in);
    in >> pk.tree_depth;
    consume_newline(in);
    in >> pk.r1cs_pk;

    return in;
}

template<typename ppzksnark_ppT>
bool zerocash_pour_verification_key<ppzksnark_ppT>::operator==(const zerocash_pour_verification_key<ppzksnark_ppT> &other) const
{
    return (this->num_old_coins == other.num_old_coins &&
            this->num_new_coins == other.num_new_coins &&
            this->r1cs_vk == other.r1cs_vk);
}

template<typename ppzksnark_ppT>
std::ostream& operator<<(std::ostream &out, const zerocash_pour_verification_key<ppzksnark_ppT> &vk)
{
    out << vk.num_old_coins << "\n";
    out << vk.num_new_coins << "\n";
    out << vk.r1cs_vk;

    return out;
}

template<typename ppzksnark_ppT>
std::istream& operator>>(std::istream &in, zerocash_pour_verification_key<ppzksnark_ppT> &vk)
{
    in >> vk.num_old_coins;
    consume_newline(in);
    in >> vk.num_new_coins;
    consume_newline(in);
    in >> vk.r1cs_vk;

    return in;
}

template<typename ppzksnark_ppT>
bool zerocash_pour_keypair<ppzksnark_ppT>::operator==(const zerocash_pour_keypair<ppzksnark_ppT> &other) const
{
    return (this->pk == other.pk &&
            this->vk == other.vk);
}

template<typename ppzksnark_ppT>
std::ostream& operator<<(std::ostream &out, const zerocash_pour_keypair<ppzksnark_ppT> &kp)
{
    out << kp.pk;
    out << kp.vk;

    return out;
}

template<typename ppzksnark_ppT>
std::istream& operator>>(std::istream &in, zerocash_pour_keypair<ppzksnark_ppT> &kp)
{
    in >> kp.pk;
    in >> kp.vk;

    return in;
}

template<typename ppzksnark_ppT>
zerocash_pour_keypair<ppzksnark_ppT> zerocash_pour_ppzksnark_generator(const size_t num_old_coins,
                                                                       const size_t num_new_coins,
                                                                       const size_t tree_depth)
{
    typedef Fr<ppzksnark_ppT> FieldT;
    enter_block("Call to zerocash_pour_ppzksnark_generator");

    protoboard<FieldT> pb;
    zerocash_pour_gadget<FieldT> g(pb, num_old_coins, num_new_coins, tree_depth, "zerocash_pour");
    g.generate_r1cs_constraints();
    const r1cs_constraint_system<FieldT> constraint_system = pb.get_constraint_system();
    r1cs_ppzksnark_keypair<ppzksnark_ppT> ppzksnark_keypair = r1cs_ppzksnark_generator<ppzksnark_ppT>(constraint_system);
    leave_block("Call to zerocash_pour_ppzksnark_generator");

    zerocash_pour_proving_key<ppzksnark_ppT> zerocash_pour_pk(num_old_coins, num_new_coins, tree_depth, std::move(ppzksnark_keypair.pk));
    zerocash_pour_verification_key<ppzksnark_ppT> zerocash_pour_vk(num_old_coins, num_new_coins, std::move(ppzksnark_keypair.vk));
    return zerocash_pour_keypair<ppzksnark_ppT>(std::move(zerocash_pour_pk), std::move(zerocash_pour_vk));
}

template<typename ppzksnark_ppT>
zerocash_pour_proof<ppzksnark_ppT> zerocash_pour_ppzksnark_prover(const zerocash_pour_proving_key<ppzksnark_ppT> &pk,
                                                                  const std::vector<merkle_authentication_path> &old_coin_authentication_paths,
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
    typedef Fr<ppzksnark_ppT> FieldT;

    enter_block("Call to zerocash_pour_ppzksnark_prover");

    protoboard<FieldT> pb;
    zerocash_pour_gadget<FieldT > g(pb, pk.num_old_coins, pk.num_new_coins, pk.tree_depth, "zerocash_pour");
    g.generate_r1cs_constraints();
    g.generate_r1cs_witness(old_coin_authentication_paths,
                            old_coin_merkle_tree_positions,
                            merkle_tree_root,
                            new_address_public_keys,
                            old_address_secret_keys,
                            new_address_commitment_nonces,
                            old_address_commitment_nonces,
                            new_coin_serial_number_nonces,
                            old_coin_serial_number_nonces,
                            new_coin_values,
                            public_old_value,
                            public_new_value,
                            old_coin_values,
                            signature_public_key_hash);
    if (!pb.is_satisfied()) {
      leave_block("Call to zerocash_pour_ppzksnark_prover");
      throw std::invalid_argument("Constraints not satisfied by inputs");
    }

    zerocash_pour_proof<ppzksnark_ppT> proof = r1cs_ppzksnark_prover<ppzksnark_ppT>(pk.r1cs_pk, pb.primary_input(), pb.auxiliary_input());

    leave_block("Call to zerocash_pour_ppzksnark_prover");

    return proof;
}

template<typename ppzksnark_ppT>
bool zerocash_pour_ppzksnark_verifier(const zerocash_pour_verification_key<ppzksnark_ppT> &vk,
                                      const bit_vector &merkle_tree_root,
                                      const std::vector<bit_vector> &old_coin_serial_numbers,
                                      const std::vector<bit_vector> &new_coin_commitments,
                                      const bit_vector &public_old_value,
                                      const bit_vector &public_new_value,
                                      const bit_vector &signature_public_key_hash,
                                      const std::vector<bit_vector> &signature_public_key_hash_macs,
                                      const zerocash_pour_proof<ppzksnark_ppT> &proof)
{
    typedef Fr<ppzksnark_ppT> FieldT;

    enter_block("Call to zerocash_pour_ppzksnark_verifier");
    const r1cs_primary_input<FieldT> input = zerocash_pour_input_map<FieldT>(vk.num_old_coins,
                                                                             vk.num_new_coins,
                                                                             merkle_tree_root,
                                                                             old_coin_serial_numbers,
                                                                             new_coin_commitments,
                                                                             public_old_value,
                                                                             public_new_value,
                                                                             signature_public_key_hash,
                                                                             signature_public_key_hash_macs);
    const bool ans = r1cs_ppzksnark_verifier_strong_IC<ppzksnark_ppT>(vk.r1cs_vk, input, proof);
    leave_block("Call to zerocash_pour_ppzksnark_verifier");

    return ans;
}

} // libzerocash

#endif // ZEROCASH_POUR_PPZKSNARK_TCC_
