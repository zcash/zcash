/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ZEROCASH_PPZKSNARK_TCC_
#define ZEROCASH_PPZKSNARK_TCC_

#include "zerocash_ppzksnark.hpp"
#include "pour_gadget.hpp"
#include "common/profiling.hpp"

namespace libzerocash {

template<typename ppT>
pour_keypair<ppT> zerocash_pour_ppzksnark_generator(const size_t tree_depth)
{
    enter_block("Call to zerocash_pour_ppzksnark_generator");

    protoboard<Fr<ppT> > pb;
    pour_gadget<Fr<ppT> > g(pb, tree_depth, "pour");
    g.generate_r1cs_constraints();
    r1cs_ppzksnark_keypair<ppT> ppzksnark_keypair = r1cs_ppzksnark_generator<ppT>(pb.constraint_system);

    leave_block("Call to zerocash_pour_ppzksnark_generator");
    return pour_keypair<ppT>(pour_proving_key<ppT>(tree_depth, std::move(ppzksnark_keypair.pk)),
                             std::move(ppzksnark_keypair.vk));
}

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
                                               const bit_vector &h_info_bv)
{
    enter_block("Call to zerocash_pour_ppzksnark_prover");

    protoboard<Fr<ppT> > pb;
    pour_gadget<Fr<ppT> > g(pb, pk.tree_depth, "pour");
    g.generate_r1cs_constraints();
    g.generate_r1cs_witness(path1, path2, root_bv, addr_pk_new_1_bv, addr_pk_new_2_bv, addr_sk_old_1_bv, addr_sk_old_2_bv,
                            rand_new_1_bv, rand_new_2_bv, rand_old_1_bv, rand_old_2_bv,
                            nonce_new_1_bv, nonce_new_2_bv, nonce_old_1_bv, nonce_old_2_bv,
                            val_new_1_bv, val_new_2_bv, val_pub_bv, val_old_1_bv, val_old_2_bv, h_info_bv);
    assert(pb.is_satisfied());

    pour_proof<ppT> proof = r1cs_ppzksnark_prover<ppT>(pk.r1cs_pk, pb.values);
    leave_block("Call to zerocash_pour_ppzksnark_prover");
    return proof;
}

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
                                      const pour_proof<ppT> &pi)
{
    enter_block("Call to zerocash_pour_ppzksnark_verifier");
    r1cs_variable_assignment<Fr<ppT> > input = pour_input_map<Fr<ppT> >(root_bv, sn_old_1_bv, sn_old_2_bv,
                                                                        cm_new_1_bv, cm_new_2_bv, val_pub_bv,
                                                                        h_info_bv, h_1_bv, h_2_bv);
    bool ans = r1cs_ppzksnark_verifier_strong_IC<ppT>(vk, input, pi);
    leave_block("Call to zerocash_pour_ppzksnark_verifier");
    return ans;
}

} // libzerocash

#endif // ZEROCASH_PPZKSNARK_TCC_
