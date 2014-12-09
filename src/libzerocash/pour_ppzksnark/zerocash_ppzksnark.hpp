/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ZEROCASH_PPZKSNARK_HPP_
#define ZEROCASH_PPZKSNARK_HPP_

#include "r1cs_ppzksnark/r1cs_ppzksnark.hpp"

namespace libzerocash {

template<typename ppT>
class pour_proving_key {
public:
    size_t tree_depth;
    r1cs_ppzksnark_proving_key<ppT> r1cs_pk;

    pour_proving_key() = default;
    pour_proving_key(const pour_proving_key<ppT> &other) = default;
    pour_proving_key(pour_proving_key<ppT> &&other) = default;
    pour_proving_key(const size_t tree_depth,
                     r1cs_ppzksnark_proving_key<ppT> &&r1cs_pk) :
        tree_depth(tree_depth), r1cs_pk(std::move(r1cs_pk)) {}
};

template<typename ppT>
using pour_verification_key = r1cs_ppzksnark_verification_key<ppT>;

template<typename ppT>
class pour_keypair {
public:
    pour_proving_key<ppT> pk;
    pour_verification_key<ppT> vk;

    pour_keypair(const pour_keypair<ppT> &other) = default;
    pour_keypair(pour_keypair<ppT> &&other) = default;
    pour_keypair(pour_proving_key<ppT> &&pk,
                 pour_verification_key<ppT> &&vk) :
        pk(std::move(pk)),
        vk(std::move(vk)) {};
};

template<typename ppT>
using pour_proof = r1cs_ppzksnark_proof<ppT>;

template<typename ppT>
pour_keypair<ppT> zerocash_pour_ppzksnark_generator(const size_t tree_depth);

template<typename ppT>
pour_proof<ppT> zerocash_pour_ppzksnark_prover(const pour_proving_key<ppT> &pk,
                                               const auth_path &path1,
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

template<typename ppT>
bool zerocash_pour_ppzksnark_verifier(const pour_verification_key<ppT> &vk,
                                      const bit_vector &root_bv,
                                      const bit_vector &sn_old_1_bv,
                                      const bit_vector &sn_old_2_bv,
                                      const bit_vector &cm_new_1_bv,
                                      const bit_vector &cm_new_2_bv,
                                      const bit_vector &val_pub_bv,
                                      const bit_vector &h_info_bv,
                                      const bit_vector &h_1_bv,
                                      const bit_vector &h_2_bv,
                                      const pour_proof<ppT> &pi);

} // libzerocash

#include "zerocash_ppzksnark.tcc"
#endif // ZEROCASH_PPZKSNARK_HPP_
