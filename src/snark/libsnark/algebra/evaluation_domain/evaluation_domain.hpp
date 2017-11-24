/** @file
 *****************************************************************************

 Declaration of interfaces for evaluation domains.

 Roughly, given a desired size m for the domain, the constructor selects
 a choice of domain S with size ~m that has been selected so to optimize
 - computations of Lagrange polynomials, and
 - FFT/iFFT computations.
 An evaluation domain also provides other other functions, e.g., accessing
 individual elements in S or evaluating its vanishing polynomial.

 The descriptions below make use of the definition of a *Lagrange polynomial*,
 which we recall. Given a field F, a subset S=(a_i)_i of F, and an index idx
 in {0,...,|S-1|}, the idx-th Lagrange polynomial (wrt to subset S) is defined to be
 \f[   L_{idx,S}(z) := prod_{k \neq idx} (z - a_k) / prod_{k \neq idx} (a_{idx} - a_k)   \f]
 Note that, by construction:
 \f[   \forall j \neq idx: L_{idx,S}(a_{idx}) = 1  \text{ and }  L_{idx,S}(a_j) = 0   \f]

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef EVALUATION_DOMAIN_HPP_
#define EVALUATION_DOMAIN_HPP_

#include <memory>

namespace libsnark {

/**
 * An evaluation domain.
 */
template<typename FieldT>
class evaluation_domain {
public:

    const size_t m;

    /**
     * Construct an evaluation domain S of size m, if possible.
     *
     * (See the function get_evaluation_domain below.)
     */
    evaluation_domain(const size_t m) : m(m) {};

    /**
     * Get the idx-th element in S.
     */
    virtual FieldT get_element(const size_t idx) = 0;

    /**
     * Compute the FFT, over the domain S, of the vector a.
     */
    virtual void FFT(std::vector<FieldT> &a) = 0;

    /**
     * Compute the inverse FFT, over the domain S, of the vector a.
     */
    virtual void iFFT(std::vector<FieldT> &a) = 0;

    /**
     * Compute the FFT, over the domain g*S, of the vector a.
     */
    virtual void cosetFFT(std::vector<FieldT> &a, const FieldT &g) = 0;

    /**
     * Compute the inverse FFT, over the domain g*S, of the vector a.
     */
    virtual void icosetFFT(std::vector<FieldT> &a, const FieldT &g) = 0;

    /**
     * Evaluate all Lagrange polynomials.
     *
     * The inputs are:
     * - an integer m
     * - an element t
     * The output is a vector (b_{0},...,b_{m-1})
     * where b_{i} is the evaluation of L_{i,S}(z) at z = t.
     */
    virtual std::vector<FieldT> lagrange_coeffs(const FieldT &t) = 0;

    /**
     * Evaluate the vanishing polynomial of S at the field element t.
     */
    virtual FieldT compute_Z(const FieldT &t) = 0;

    /**
     * Add the coefficients of the vanishing polynomial of S to the coefficients of the polynomial H.
     */
    virtual void add_poly_Z(const FieldT &coeff, std::vector<FieldT> &H) = 0;

    /**
     * Multiply by the evaluation, on a coset of S, of the inverse of the vanishing polynomial of S.
     */
    virtual void divide_by_Z_on_coset(std::vector<FieldT> &P) = 0;
};

/**
 * Return an evaluation domain object in which the domain S has size |S| >= min_size.
 * The function chooses from different supported domains, depending on min_size.
 */
template<typename FieldT>
std::shared_ptr<evaluation_domain<FieldT> > get_evaluation_domain(const size_t min_size);

/**
 * Naive evaluation of a *single* Lagrange polynomial, used for testing purposes.
 *
 * The inputs are:
 * - an integer m
 * - a domain S = (a_{0},...,a_{m-1}) of size m
 * - a field element element t
 * - an index idx in {0,...,m-1}
 * The output is the polynomial L_{idx,S}(z) evaluated at z = t.
 */
template<typename FieldT>
FieldT lagrange_eval(const size_t m, const std::vector<FieldT> &domain, const FieldT &t, const size_t idx);

} // libsnark

#include "algebra/evaluation_domain/evaluation_domain.tcc"

#endif // EVALUATION_DOMAIN_HPP_
