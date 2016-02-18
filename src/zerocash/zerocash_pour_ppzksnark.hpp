/** @file
 *****************************************************************************

 Declaration of interfaces for a ppzkSNARK for the NP statement "Pour".

 This includes:
 - class for proving key
 - class for verification key
 - class for key pair (proving key & verification key)
 - class for proof
 - generator algorithm
 - prover algorithm
 - verifier algorithm

 The ppzkSNARK is obtained by using an R1CS ppzkSNARK relative to an R1CS
 realization of the NP statement "Pour". The implementation follows, extends,
 and optimizes the approach described in \[BCGGMTV14].


 Acronyms:

 - R1CS = "Rank-1 Constraint Systems"
 - ppzkSNARK = "PreProcessing Zero-Knowledge Succinct Non-interactive ARgument of Knowledge"

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

#ifndef ZEROCASH_POUR_PPZKSNARK_HPP_
#define ZEROCASH_POUR_PPZKSNARK_HPP_

#include "libsnark/common/data_structures/merkle_tree.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include <stdexcept>

namespace libzerocash {

/******************************** Proving key ********************************/

/**
 * A proving key for the Pour ppzkSNARK.
 */
template<typename ppzksnark_ppT>
class zerocash_pour_proving_key;

template<typename ppzksnark_ppT>
std::ostream& operator<<(std::ostream &out, const zerocash_pour_proving_key<ppzksnark_ppT> &pk);

template<typename ppzksnark_ppT>
std::istream& operator>>(std::istream &in, zerocash_pour_proving_key<ppzksnark_ppT> &pk);

template<typename ppzksnark_ppT>
class zerocash_pour_proving_key {
public:
    size_t num_old_coins;
    size_t num_new_coins;
    size_t tree_depth;
    r1cs_ppzksnark_proving_key<ppzksnark_ppT> r1cs_pk;

    zerocash_pour_proving_key() = default;
    zerocash_pour_proving_key(const zerocash_pour_proving_key<ppzksnark_ppT> &other) = default;
    zerocash_pour_proving_key(zerocash_pour_proving_key<ppzksnark_ppT> &&other) = default;
    zerocash_pour_proving_key(const size_t num_old_coins,
                              const size_t num_new_coins,
                              const size_t tree_depth,
                              const r1cs_ppzksnark_proving_key<ppzksnark_ppT> &r1cs_pk) :
        num_old_coins(num_old_coins), num_new_coins(num_new_coins),
        tree_depth(tree_depth), r1cs_pk(r1cs_pk) {}
    zerocash_pour_proving_key(const size_t num_old_coins,
                              const size_t num_new_coins,
                              const size_t tree_depth,
                              r1cs_ppzksnark_proving_key<ppzksnark_ppT> &&r1cs_pk) :
        num_old_coins(num_old_coins), num_new_coins(num_new_coins),
        tree_depth(tree_depth), r1cs_pk(std::move(r1cs_pk)) {}
    zerocash_pour_proving_key<ppzksnark_ppT>& operator=(const zerocash_pour_proving_key<ppzksnark_ppT> &other) = default;

    bool operator==(const zerocash_pour_proving_key<ppzksnark_ppT> &other) const;
    friend std::ostream& operator<< <ppzksnark_ppT>(std::ostream &out, const zerocash_pour_proving_key<ppzksnark_ppT> &pk);
    friend std::istream& operator>> <ppzksnark_ppT>(std::istream &in, zerocash_pour_proving_key<ppzksnark_ppT> &pk);
};

/******************************* Verification key ****************************/

/**
 * A verification key for the Pour ppzkSNARK.
 */
template<typename ppzksnark_ppT>
class zerocash_pour_verification_key;

template<typename ppzksnark_ppT>
std::ostream& operator<<(std::ostream &out, const zerocash_pour_verification_key<ppzksnark_ppT> &pk);

template<typename ppzksnark_ppT>
std::istream& operator>>(std::istream &in, zerocash_pour_verification_key<ppzksnark_ppT> &pk);

template<typename ppzksnark_ppT>
class zerocash_pour_verification_key {
public:
    size_t num_old_coins;
    size_t num_new_coins;
    r1cs_ppzksnark_verification_key<ppzksnark_ppT> r1cs_vk;

    zerocash_pour_verification_key() = default;
    zerocash_pour_verification_key(const zerocash_pour_verification_key<ppzksnark_ppT> &other) = default;
    zerocash_pour_verification_key(zerocash_pour_verification_key<ppzksnark_ppT> &&other) = default;
    zerocash_pour_verification_key(const size_t num_old_coins,
                                   const size_t num_new_coins,
                                   const r1cs_ppzksnark_verification_key<ppzksnark_ppT> &r1cs_vk) :
        num_old_coins(num_old_coins), num_new_coins(num_new_coins),
        r1cs_vk(r1cs_vk) {}
    zerocash_pour_verification_key(const size_t num_old_coins,
                                   const size_t num_new_coins,
                                   r1cs_ppzksnark_verification_key<ppzksnark_ppT> &&r1cs_vk) :
        num_old_coins(num_old_coins), num_new_coins(num_new_coins),
        r1cs_vk(std::move(r1cs_vk)) {}
    zerocash_pour_verification_key<ppzksnark_ppT>& operator=(const zerocash_pour_verification_key<ppzksnark_ppT> &other) = default;

    bool operator==(const zerocash_pour_verification_key<ppzksnark_ppT> &other) const;
    friend std::ostream& operator<< <ppzksnark_ppT>(std::ostream &out, const zerocash_pour_verification_key<ppzksnark_ppT> &pk);
    friend std::istream& operator>> <ppzksnark_ppT>(std::istream &in, zerocash_pour_verification_key<ppzksnark_ppT> &pk);
};

/********************************** Key pair *********************************/

/**
 * A key pair for the Pour ppzkSNARK, which consists of a proving key and a verification key.
 */
template<typename ppzksnark_ppT>
class zerocash_pour_keypair;

template<typename ppzksnark_ppT>
std::ostream& operator<<(std::ostream &out, const zerocash_pour_keypair<ppzksnark_ppT> &pk);

template<typename ppzksnark_ppT>
std::istream& operator>>(std::istream &in, zerocash_pour_keypair<ppzksnark_ppT> &pk);

template<typename ppzksnark_ppT>
class zerocash_pour_keypair {
public:
    zerocash_pour_proving_key<ppzksnark_ppT> pk;
    zerocash_pour_verification_key<ppzksnark_ppT> vk;

    zerocash_pour_keypair() = default;
    zerocash_pour_keypair(const zerocash_pour_keypair<ppzksnark_ppT> &other) = default;
    zerocash_pour_keypair(zerocash_pour_keypair<ppzksnark_ppT> &&other) = default;
    zerocash_pour_keypair(const zerocash_pour_proving_key<ppzksnark_ppT> &pk,
                          const zerocash_pour_verification_key<ppzksnark_ppT> &vk) :
        pk(pk), vk(vk) {}
    zerocash_pour_keypair(zerocash_pour_proving_key<ppzksnark_ppT> &&pk,
                          zerocash_pour_verification_key<ppzksnark_ppT> &&vk) :
        pk(std::move(pk)),
        vk(std::move(vk)) {}
    zerocash_pour_keypair<ppzksnark_ppT>& operator=(const zerocash_pour_keypair<ppzksnark_ppT> &other) = default;

    bool operator==(const zerocash_pour_keypair<ppzksnark_ppT> &other) const;
    friend std::ostream& operator<< <ppzksnark_ppT>(std::ostream &out, const zerocash_pour_keypair<ppzksnark_ppT> &pk);
    friend std::istream& operator>> <ppzksnark_ppT>(std::istream &in, zerocash_pour_keypair<ppzksnark_ppT> &pk);
};

/*********************************** Proof ***********************************/

/**
 * A proof for the Pour ppzkSNARK.
 */
template<typename ppzksnark_ppT>
using zerocash_pour_proof = r1cs_ppzksnark_proof<ppzksnark_ppT>;


/***************************** Main algorithms *******************************/

/**
 * A generator algorithm for the Pour ppzkSNARK.
 *
 * Given a tree depth d, this algorithm produces proving and verification keys
 * for Pour relative to a Merkle tree of depth d.
 */
template<typename ppzksnark_ppT>
zerocash_pour_keypair<ppzksnark_ppT> zerocash_pour_ppzksnark_generator(const size_t num_old_coins,
                                                                       const size_t num_new_coins,
                                                                       const size_t tree_depth);

/**
 * A prover algorithm for the Pour ppzkSNARK.
 *
 * TODO: add description
 */
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
                                                                  const bit_vector &signature_public_key_hash);

/**
 * A verifier algorithm for the Pour ppzkSNARK.
 */
template<typename ppzksnark_ppT>
bool zerocash_pour_ppzksnark_verifier(const zerocash_pour_verification_key<ppzksnark_ppT> &vk,
                                      const bit_vector &merkle_tree_root,
                                      const std::vector<bit_vector> &old_coin_serial_numbers,
                                      const std::vector<bit_vector> &new_coin_commitments,
                                      const bit_vector &public_old_value,
                                      const bit_vector &public_new_value,
                                      const bit_vector &signature_public_key_hash,
                                      const std::vector<bit_vector> &signature_public_key_hash_macs,
                                      const zerocash_pour_proof<ppzksnark_ppT> &proof);

} // libzerocash

#include "zerocash_pour_ppzksnark.tcc"

#endif // ZEROCASH_POUR_PPZKSNARK_HPP_
