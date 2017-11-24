/** @file
 *****************************************************************************

 Declaration of interfaces for a QAP ("Quadratic Arithmetic Program").

 QAPs are defined in \[GGPR13].

 References:

 \[GGPR13]:
 "Quadratic span programs and succinct NIZKs without PCPs",
 Rosario Gennaro, Craig Gentry, Bryan Parno, Mariana Raykova,
 EUROCRYPT 2013,
 <http://eprint.iacr.org/2012/215>

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef QAP_HPP_
#define QAP_HPP_

#include "algebra/evaluation_domain/evaluation_domain.hpp"

namespace libsnark {

/* forward declaration */
template<typename FieldT>
class qap_witness;

/**
 * A QAP instance.
 *
 * Specifically, the datastructure stores:
 * - a choice of domain (corresponding to a certain subset of the field);
 * - the number of variables, the degree, and the number of inputs; and
 * - coefficients of the A,B,C polynomials in the Lagrange basis.
 *
 * There is no need to store the Z polynomial because it is uniquely
 * determined by the domain (as Z is its vanishing polynomial).
 */
template<typename FieldT>
class qap_instance {
private:
    size_t num_variables_;
    size_t degree_;
    size_t num_inputs_;

public:
    std::shared_ptr<evaluation_domain<FieldT> > domain;

    std::vector<std::map<size_t, FieldT> > A_in_Lagrange_basis;
    std::vector<std::map<size_t, FieldT> > B_in_Lagrange_basis;
    std::vector<std::map<size_t, FieldT> > C_in_Lagrange_basis;

    qap_instance(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                 const size_t num_variables,
                 const size_t degree,
                 const size_t num_inputs,
                 const std::vector<std::map<size_t, FieldT> > &A_in_Lagrange_basis,
                 const std::vector<std::map<size_t, FieldT> > &B_in_Lagrange_basis,
                 const std::vector<std::map<size_t, FieldT> > &C_in_Lagrange_basis);

    qap_instance(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                 const size_t num_variables,
                 const size_t degree,
                 const size_t num_inputs,
                 std::vector<std::map<size_t, FieldT> > &&A_in_Lagrange_basis,
                 std::vector<std::map<size_t, FieldT> > &&B_in_Lagrange_basis,
                 std::vector<std::map<size_t, FieldT> > &&C_in_Lagrange_basis);

    qap_instance(const qap_instance<FieldT> &other) = default;
    qap_instance(qap_instance<FieldT> &&other) = default;
    qap_instance& operator=(const qap_instance<FieldT> &other) = default;
    qap_instance& operator=(qap_instance<FieldT> &&other) = default;

    size_t num_variables() const;
    size_t degree() const;
    size_t num_inputs() const;

    bool is_satisfied(const qap_witness<FieldT> &witness) const;
};

/**
 * A QAP instance evaluation is a QAP instance that is evaluated at a field element t.
 *
 * Specifically, the datastructure stores:
 * - a choice of domain (corresponding to a certain subset of the field);
 * - the number of variables, the degree, and the number of inputs;
 * - a field element t;
 * - evaluations of the A,B,C (and Z) polynomials at t;
 * - evaluations of all monomials of t;
 * - counts about how many of the above evaluations are in fact non-zero.
 */
template<typename FieldT>
class qap_instance_evaluation {
private:
    size_t num_variables_;
    size_t degree_;
    size_t num_inputs_;
public:
    std::shared_ptr<evaluation_domain<FieldT> > domain;

    FieldT t;

    std::vector<FieldT> At, Bt, Ct, Ht;

    FieldT Zt;

    qap_instance_evaluation(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                            const size_t num_variables,
                            const size_t degree,
                            const size_t num_inputs,
                            const FieldT &t,
                            const std::vector<FieldT> &At,
                            const std::vector<FieldT> &Bt,
                            const std::vector<FieldT> &Ct,
                            const std::vector<FieldT> &Ht,
                            const FieldT &Zt);
    qap_instance_evaluation(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                            const size_t num_variables,
                            const size_t degree,
                            const size_t num_inputs,
                            const FieldT &t,
                            std::vector<FieldT> &&At,
                            std::vector<FieldT> &&Bt,
                            std::vector<FieldT> &&Ct,
                            std::vector<FieldT> &&Ht,
                            const FieldT &Zt);

    qap_instance_evaluation(const qap_instance_evaluation<FieldT> &other) = default;
    qap_instance_evaluation(qap_instance_evaluation<FieldT> &&other) = default;
    qap_instance_evaluation& operator=(const qap_instance_evaluation<FieldT> &other) = default;
    qap_instance_evaluation& operator=(qap_instance_evaluation<FieldT> &&other) = default;

    size_t num_variables() const;
    size_t degree() const;
    size_t num_inputs() const;

    bool is_satisfied(const qap_witness<FieldT> &witness) const;
};

/**
 * A QAP witness.
 */
template<typename FieldT>
class qap_witness {
private:
    size_t num_variables_;
    size_t degree_;
    size_t num_inputs_;

public:
    FieldT d1, d2, d3;

    std::vector<FieldT> coefficients_for_ABCs;
    std::vector<FieldT> coefficients_for_H;

    qap_witness(const size_t num_variables,
                const size_t degree,
                const size_t num_inputs,
                const FieldT &d1,
                const FieldT &d2,
                const FieldT &d3,
                const std::vector<FieldT> &coefficients_for_ABCs,
                const std::vector<FieldT> &coefficients_for_H);

    qap_witness(const size_t num_variables,
                const size_t degree,
                const size_t num_inputs,
                const FieldT &d1,
                const FieldT &d2,
                const FieldT &d3,
                const std::vector<FieldT> &coefficients_for_ABCs,
                std::vector<FieldT> &&coefficients_for_H);

    qap_witness(const qap_witness<FieldT> &other) = default;
    qap_witness(qap_witness<FieldT> &&other) = default;
    qap_witness& operator=(const qap_witness<FieldT> &other) = default;
    qap_witness& operator=(qap_witness<FieldT> &&other) = default;

    size_t num_variables() const;
    size_t degree() const;
    size_t num_inputs() const;
};

} // libsnark

#include "relations/arithmetic_programs/qap/qap.tcc"

#endif // QAP_HPP_
