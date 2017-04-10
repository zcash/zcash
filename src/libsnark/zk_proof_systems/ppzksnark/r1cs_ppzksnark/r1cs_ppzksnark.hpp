/** @file
 *****************************************************************************

 Declaration of interfaces for a ppzkSNARK for R1CS.

 This includes:
 - class for proving key
 - class for verification key
 - class for processed verification key
 - class for key pair (proving key & verification key)
 - class for proof
 - generator algorithm
 - prover algorithm
 - verifier algorithm (with strong or weak input consistency)
 - online verifier algorithm (with strong or weak input consistency)

 The implementation instantiates (a modification of) the protocol of \[PGHR13],
 by following extending, and optimizing the approach described in \[BCTV14].


 Acronyms:

 - R1CS = "Rank-1 Constraint Systems"
 - ppzkSNARK = "PreProcessing Zero-Knowledge Succinct Non-interactive ARgument of Knowledge"

 References:

 \[BCTV14]:
 "Succinct Non-Interactive Zero Knowledge for a von Neumann Architecture",
 Eli Ben-Sasson, Alessandro Chiesa, Eran Tromer, Madars Virza,
 USENIX Security 2014,
 <http://eprint.iacr.org/2013/879>

 \[PGHR13]:
 "Pinocchio: Nearly practical verifiable computation",
 Bryan Parno, Craig Gentry, Jon Howell, Mariana Raykova,
 IEEE S&P 2013,
 <https://eprint.iacr.org/2013/279>

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef R1CS_PPZKSNARK_HPP_
#define R1CS_PPZKSNARK_HPP_

#include <memory>

#include "algebra/curves/public_params.hpp"
#include "common/data_structures/accumulation_vector.hpp"
#include "algebra/knowledge_commitment/knowledge_commitment.hpp"
#include "relations/constraint_satisfaction_problems/r1cs/r1cs.hpp"
#include "zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark_params.hpp"

namespace libsnark {

/******************************** Proving key ********************************/

template<typename ppT>
class r1cs_ppzksnark_proving_key;

template<typename ppT>
std::ostream& operator<<(std::ostream &out, const r1cs_ppzksnark_proving_key<ppT> &pk);

template<typename ppT>
std::istream& operator>>(std::istream &in, r1cs_ppzksnark_proving_key<ppT> &pk);

/**
 * A proving key for the R1CS ppzkSNARK.
 */
template<typename ppT>
class r1cs_ppzksnark_proving_key {
public:
    knowledge_commitment_vector<G1<ppT>, G1<ppT> > A_query;
    knowledge_commitment_vector<G2<ppT>, G1<ppT> > B_query;
    knowledge_commitment_vector<G1<ppT>, G1<ppT> > C_query;
    G1_vector<ppT> H_query;
    G1_vector<ppT> K_query;

    r1cs_ppzksnark_proving_key() {};
    r1cs_ppzksnark_proving_key<ppT>& operator=(const r1cs_ppzksnark_proving_key<ppT> &other) = default;
    r1cs_ppzksnark_proving_key(const r1cs_ppzksnark_proving_key<ppT> &other) = default;
    r1cs_ppzksnark_proving_key(r1cs_ppzksnark_proving_key<ppT> &&other) = default;
    r1cs_ppzksnark_proving_key(knowledge_commitment_vector<G1<ppT>, G1<ppT> > &&A_query,
                               knowledge_commitment_vector<G2<ppT>, G1<ppT> > &&B_query,
                               knowledge_commitment_vector<G1<ppT>, G1<ppT> > &&C_query,
                               G1_vector<ppT> &&H_query,
                               G1_vector<ppT> &&K_query) :
        A_query(std::move(A_query)),
        B_query(std::move(B_query)),
        C_query(std::move(C_query)),
        H_query(std::move(H_query)),
        K_query(std::move(K_query))
    {};

    size_t G1_size() const
    {
        return 2*(A_query.domain_size() + C_query.domain_size()) + B_query.domain_size() + H_query.size() + K_query.size();
    }

    size_t G2_size() const
    {
        return B_query.domain_size();
    }

    size_t G1_sparse_size() const
    {
        return 2*(A_query.size() + C_query.size()) + B_query.size() + H_query.size() + K_query.size();
    }

    size_t G2_sparse_size() const
    {
        return B_query.size();
    }

    size_t size_in_bits() const
    {
        return A_query.size_in_bits() + B_query.size_in_bits() + C_query.size_in_bits() + libsnark::size_in_bits(H_query) + libsnark::size_in_bits(K_query);
    }

    void print_size() const
    {
        print_indent(); printf("* G1 elements in PK: %zu\n", this->G1_size());
        print_indent(); printf("* Non-zero G1 elements in PK: %zu\n", this->G1_sparse_size());
        print_indent(); printf("* G2 elements in PK: %zu\n", this->G2_size());
        print_indent(); printf("* Non-zero G2 elements in PK: %zu\n", this->G2_sparse_size());
        print_indent(); printf("* PK size in bits: %zu\n", this->size_in_bits());
    }

    bool operator==(const r1cs_ppzksnark_proving_key<ppT> &other) const;
    friend std::ostream& operator<< <ppT>(std::ostream &out, const r1cs_ppzksnark_proving_key<ppT> &pk);
    friend std::istream& operator>> <ppT>(std::istream &in, r1cs_ppzksnark_proving_key<ppT> &pk);
};


/******************************* Verification key ****************************/

template<typename ppT>
class r1cs_ppzksnark_verification_key;

template<typename ppT>
std::ostream& operator<<(std::ostream &out, const r1cs_ppzksnark_verification_key<ppT> &vk);

template<typename ppT>
std::istream& operator>>(std::istream &in, r1cs_ppzksnark_verification_key<ppT> &vk);

/**
 * A verification key for the R1CS ppzkSNARK.
 */
template<typename ppT>
class r1cs_ppzksnark_verification_key {
public:
    G2<ppT> alphaA_g2;
    G1<ppT> alphaB_g1;
    G2<ppT> alphaC_g2;
    G2<ppT> gamma_g2;
    G1<ppT> gamma_beta_g1;
    G2<ppT> gamma_beta_g2;
    G2<ppT> rC_Z_g2;

    accumulation_vector<G1<ppT> > encoded_IC_query;

    r1cs_ppzksnark_verification_key() = default;
    r1cs_ppzksnark_verification_key(const G2<ppT> &alphaA_g2,
                                    const G1<ppT> &alphaB_g1,
                                    const G2<ppT> &alphaC_g2,
                                    const G2<ppT> &gamma_g2,
                                    const G1<ppT> &gamma_beta_g1,
                                    const G2<ppT> &gamma_beta_g2,
                                    const G2<ppT> &rC_Z_g2,
                                    const accumulation_vector<G1<ppT> > &eIC) :
        alphaA_g2(alphaA_g2),
        alphaB_g1(alphaB_g1),
        alphaC_g2(alphaC_g2),
        gamma_g2(gamma_g2),
        gamma_beta_g1(gamma_beta_g1),
        gamma_beta_g2(gamma_beta_g2),
        rC_Z_g2(rC_Z_g2),
        encoded_IC_query(eIC)
    {};

    size_t G1_size() const
    {
        return 2 + encoded_IC_query.size();
    }

    size_t G2_size() const
    {
        return 5;
    }

    size_t size_in_bits() const
    {
        return (2 * G1<ppT>::size_in_bits() + encoded_IC_query.size_in_bits() + 5 * G2<ppT>::size_in_bits());
    }

    void print_size() const
    {
        print_indent(); printf("* G1 elements in VK: %zu\n", this->G1_size());
        print_indent(); printf("* G2 elements in VK: %zu\n", this->G2_size());
        print_indent(); printf("* VK size in bits: %zu\n", this->size_in_bits());
    }

    bool operator==(const r1cs_ppzksnark_verification_key<ppT> &other) const;
    friend std::ostream& operator<< <ppT>(std::ostream &out, const r1cs_ppzksnark_verification_key<ppT> &vk);
    friend std::istream& operator>> <ppT>(std::istream &in, r1cs_ppzksnark_verification_key<ppT> &vk);

    static r1cs_ppzksnark_verification_key<ppT> dummy_verification_key(const size_t input_size);
};


/************************ Processed verification key *************************/

template<typename ppT>
class r1cs_ppzksnark_processed_verification_key;

template<typename ppT>
std::ostream& operator<<(std::ostream &out, const r1cs_ppzksnark_processed_verification_key<ppT> &pvk);

template<typename ppT>
std::istream& operator>>(std::istream &in, r1cs_ppzksnark_processed_verification_key<ppT> &pvk);

/**
 * A processed verification key for the R1CS ppzkSNARK.
 *
 * Compared to a (non-processed) verification key, a processed verification key
 * contains a small constant amount of additional pre-computed information that
 * enables a faster verification time.
 */
template<typename ppT>
class r1cs_ppzksnark_processed_verification_key {
public:
    G2_precomp<ppT> pp_G2_one_precomp;
    G2_precomp<ppT> vk_alphaA_g2_precomp;
    G1_precomp<ppT> vk_alphaB_g1_precomp;
    G2_precomp<ppT> vk_alphaC_g2_precomp;
    G2_precomp<ppT> vk_rC_Z_g2_precomp;
    G2_precomp<ppT> vk_gamma_g2_precomp;
    G1_precomp<ppT> vk_gamma_beta_g1_precomp;
    G2_precomp<ppT> vk_gamma_beta_g2_precomp;

    accumulation_vector<G1<ppT> > encoded_IC_query;

    bool operator==(const r1cs_ppzksnark_processed_verification_key &other) const;
    friend std::ostream& operator<< <ppT>(std::ostream &out, const r1cs_ppzksnark_processed_verification_key<ppT> &pvk);
    friend std::istream& operator>> <ppT>(std::istream &in, r1cs_ppzksnark_processed_verification_key<ppT> &pvk);
};


/********************************** Key pair *********************************/

/**
 * A key pair for the R1CS ppzkSNARK, which consists of a proving key and a verification key.
 */
template<typename ppT>
class r1cs_ppzksnark_keypair {
public:
    r1cs_ppzksnark_proving_key<ppT> pk;
    r1cs_ppzksnark_verification_key<ppT> vk;

    r1cs_ppzksnark_keypair() = default;
    r1cs_ppzksnark_keypair(const r1cs_ppzksnark_keypair<ppT> &other) = default;
    r1cs_ppzksnark_keypair(r1cs_ppzksnark_proving_key<ppT> &&pk,
                           r1cs_ppzksnark_verification_key<ppT> &&vk) :
        pk(std::move(pk)),
        vk(std::move(vk))
    {}

    r1cs_ppzksnark_keypair(r1cs_ppzksnark_keypair<ppT> &&other) = default;
};


/*********************************** Proof ***********************************/

template<typename ppT>
class r1cs_ppzksnark_proof;

template<typename ppT>
std::ostream& operator<<(std::ostream &out, const r1cs_ppzksnark_proof<ppT> &proof);

template<typename ppT>
std::istream& operator>>(std::istream &in, r1cs_ppzksnark_proof<ppT> &proof);

/**
 * A proof for the R1CS ppzkSNARK.
 *
 * While the proof has a structure, externally one merely opaquely produces,
 * seralizes/deserializes, and verifies proofs. We only expose some information
 * about the structure for statistics purposes.
 */
template<typename ppT>
class r1cs_ppzksnark_proof {
public:
    knowledge_commitment<G1<ppT>, G1<ppT> > g_A;
    knowledge_commitment<G2<ppT>, G1<ppT> > g_B;
    knowledge_commitment<G1<ppT>, G1<ppT> > g_C;
    G1<ppT> g_H;
    G1<ppT> g_K;

    r1cs_ppzksnark_proof()
    {
        // invalid proof with valid curve points
        this->g_A.g = G1<ppT> ::one();
        this->g_A.h = G1<ppT>::one();
        this->g_B.g = G2<ppT> ::one();
        this->g_B.h = G1<ppT>::one();
        this->g_C.g = G1<ppT> ::one();
        this->g_C.h = G1<ppT>::one();
        this->g_H = G1<ppT>::one();
        this->g_K = G1<ppT>::one();
    }
    r1cs_ppzksnark_proof(knowledge_commitment<G1<ppT>, G1<ppT> > &&g_A,
                         knowledge_commitment<G2<ppT>, G1<ppT> > &&g_B,
                         knowledge_commitment<G1<ppT>, G1<ppT> > &&g_C,
                         G1<ppT> &&g_H,
                         G1<ppT> &&g_K) :
        g_A(std::move(g_A)),
        g_B(std::move(g_B)),
        g_C(std::move(g_C)),
        g_H(std::move(g_H)),
        g_K(std::move(g_K))
    {};

    size_t G1_size() const
    {
        return 7;
    }

    size_t G2_size() const
    {
        return 1;
    }

    size_t size_in_bits() const
    {
        return G1_size() * G1<ppT>::size_in_bits() + G2_size() * G2<ppT>::size_in_bits();
    }

    void print_size() const
    {
        print_indent(); printf("* G1 elements in proof: %zu\n", this->G1_size());
        print_indent(); printf("* G2 elements in proof: %zu\n", this->G2_size());
        print_indent(); printf("* Proof size in bits: %zu\n", this->size_in_bits());
    }

    bool is_well_formed() const
    {
        return (g_A.g.is_well_formed() && g_A.h.is_well_formed() &&
                g_B.g.is_well_formed() && g_B.h.is_well_formed() &&
                g_C.g.is_well_formed() && g_C.h.is_well_formed() &&
                g_H.is_well_formed() &&
                g_K.is_well_formed());
    }

    bool operator==(const r1cs_ppzksnark_proof<ppT> &other) const;
    friend std::ostream& operator<< <ppT>(std::ostream &out, const r1cs_ppzksnark_proof<ppT> &proof);
    friend std::istream& operator>> <ppT>(std::istream &in, r1cs_ppzksnark_proof<ppT> &proof);
};


/***************************** Main algorithms *******************************/

/**
 * A generator algorithm for the R1CS ppzkSNARK.
 *
 * Given a R1CS constraint system CS, this algorithm produces proving and verification keys for CS.
 */
template<typename ppT>
r1cs_ppzksnark_keypair<ppT> r1cs_ppzksnark_generator(const r1cs_ppzksnark_constraint_system<ppT> &cs);

template<typename ppT>
r1cs_ppzksnark_keypair<ppT> r1cs_ppzksnark_generator(
    const r1cs_ppzksnark_constraint_system<ppT> &cs,
    const Fr<ppT>& t,
    const Fr<ppT>& alphaA,
    const Fr<ppT>& alphaB,
    const Fr<ppT>& alphaC,
    const Fr<ppT>& rA,
    const Fr<ppT>& rB,
    const Fr<ppT>& beta,
    const Fr<ppT>& gamma
);

/**
 * A prover algorithm for the R1CS ppzkSNARK.
 *
 * Given a R1CS primary input X and a R1CS auxiliary input Y, this algorithm
 * produces a proof (of knowledge) that attests to the following statement:
 *               ``there exists Y such that CS(X,Y)=0''.
 * Above, CS is the R1CS constraint system that was given as input to the generator algorithm.
 */
template<typename ppT>
r1cs_ppzksnark_proof<ppT> r1cs_ppzksnark_prover(const r1cs_ppzksnark_proving_key<ppT> &pk,
                                                const r1cs_ppzksnark_primary_input<ppT> &primary_input,
                                                const r1cs_ppzksnark_auxiliary_input<ppT> &auxiliary_input);

/*
 Below are four variants of verifier algorithm for the R1CS ppzkSNARK.

 These are the four cases that arise from the following two choices:

 (1) The verifier accepts a (non-processed) verification key or, instead, a processed verification key.
     In the latter case, we call the algorithm an "online verifier".

 (2) The verifier checks for "weak" input consistency or, instead, "strong" input consistency.
     Strong input consistency requires that |primary_input| = CS.num_inputs, whereas
     weak input consistency requires that |primary_input| <= CS.num_inputs (and
     the primary input is implicitly padded with zeros up to length CS.num_inputs).
 */

/**
 * A verifier algorithm for the R1CS ppzkSNARK that:
 * (1) accepts a non-processed verification key, and
 * (2) has weak input consistency.
 */
template<typename ppT>
bool r1cs_ppzksnark_verifier_weak_IC(const r1cs_ppzksnark_verification_key<ppT> &vk,
                                     const r1cs_ppzksnark_primary_input<ppT> &primary_input,
                                     const r1cs_ppzksnark_proof<ppT> &proof);

/**
 * A verifier algorithm for the R1CS ppzkSNARK that:
 * (1) accepts a non-processed verification key, and
 * (2) has strong input consistency.
 */
template<typename ppT>
bool r1cs_ppzksnark_verifier_strong_IC(const r1cs_ppzksnark_verification_key<ppT> &vk,
                                       const r1cs_ppzksnark_primary_input<ppT> &primary_input,
                                       const r1cs_ppzksnark_proof<ppT> &proof);

/**
 * Convert a (non-processed) verification key into a processed verification key.
 */
template<typename ppT>
r1cs_ppzksnark_processed_verification_key<ppT> r1cs_ppzksnark_verifier_process_vk(const r1cs_ppzksnark_verification_key<ppT> &vk);

/**
 * A verifier algorithm for the R1CS ppzkSNARK that:
 * (1) accepts a processed verification key, and
 * (2) has weak input consistency.
 */
template<typename ppT>
bool r1cs_ppzksnark_online_verifier_weak_IC(const r1cs_ppzksnark_processed_verification_key<ppT> &pvk,
                                            const r1cs_ppzksnark_primary_input<ppT> &input,
                                            const r1cs_ppzksnark_proof<ppT> &proof);

/**
 * A verifier algorithm for the R1CS ppzkSNARK that:
 * (1) accepts a processed verification key, and
 * (2) has strong input consistency.
 */
template<typename ppT>
bool r1cs_ppzksnark_online_verifier_strong_IC(const r1cs_ppzksnark_processed_verification_key<ppT> &pvk,
                                              const r1cs_ppzksnark_primary_input<ppT> &primary_input,
                                              const r1cs_ppzksnark_proof<ppT> &proof);

/****************************** Miscellaneous ********************************/

/**
 * For debugging purposes (of r1cs_ppzksnark_r1cs_ppzksnark_verifier_gadget):
 *
 * A verifier algorithm for the R1CS ppzkSNARK that:
 * (1) accepts a non-processed verification key,
 * (2) has weak input consistency, and
 * (3) uses affine coordinates for elliptic-curve computations.
 */
template<typename ppT>
bool r1cs_ppzksnark_affine_verifier_weak_IC(const r1cs_ppzksnark_verification_key<ppT> &vk,
                                            const r1cs_ppzksnark_primary_input<ppT> &primary_input,
                                            const r1cs_ppzksnark_proof<ppT> &proof);


} // libsnark

#include "zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.tcc"

#endif // R1CS_PPZKSNARK_HPP_
