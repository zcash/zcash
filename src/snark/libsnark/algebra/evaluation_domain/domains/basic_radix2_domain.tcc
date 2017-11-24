/** @file
 *****************************************************************************

 Implementation of interfaces for the "basic radix-2" evaluation domain.

 See basic_radix2_domain.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef BASIC_RADIX2_DOMAIN_TCC_
#define BASIC_RADIX2_DOMAIN_TCC_

#include "algebra/evaluation_domain/domains/basic_radix2_domain_aux.hpp"

namespace libsnark {

template<typename FieldT>
basic_radix2_domain<FieldT>::basic_radix2_domain(const size_t m) : evaluation_domain<FieldT>(m)
{
    assert(m > 1);
    const size_t logm = log2(m);
    assert(logm <= (FieldT::s));

    omega = get_root_of_unity<FieldT>(m);
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::FFT(std::vector<FieldT> &a)
{
    enter_block("Execute FFT");
    assert(a.size() == this->m);
    _basic_radix2_FFT(a, omega);
    leave_block("Execute FFT");
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::iFFT(std::vector<FieldT> &a)
{
    enter_block("Execute inverse FFT");
    assert(a.size() == this->m);
    _basic_radix2_FFT(a, omega.inverse());

    const FieldT sconst = FieldT(a.size()).inverse();
    for (size_t i = 0; i < a.size(); ++i)
    {
        a[i] *= sconst;
    }
    leave_block("Execute inverse FFT");
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::cosetFFT(std::vector<FieldT> &a, const FieldT &g)
{
    enter_block("Execute coset FFT");
    _multiply_by_coset(a, g);
    FFT(a);
    leave_block("Execute coset FFT");
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::icosetFFT(std::vector<FieldT> &a, const FieldT &g)
{
    enter_block("Execute inverse coset IFFT");
    iFFT(a);
    _multiply_by_coset(a, g.inverse());
    leave_block("Execute inverse coset IFFT");
}

template<typename FieldT>
std::vector<FieldT> basic_radix2_domain<FieldT>::lagrange_coeffs(const FieldT &t)
{
    return _basic_radix2_lagrange_coeffs(this->m, t);
}

template<typename FieldT>
FieldT basic_radix2_domain<FieldT>::get_element(const size_t idx)
{
    return omega^idx;
}

template<typename FieldT>
FieldT basic_radix2_domain<FieldT>::compute_Z(const FieldT &t)
{
    return (t^this->m) - FieldT::one();
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::add_poly_Z(const FieldT &coeff, std::vector<FieldT> &H)
{
    assert(H.size() == this->m+1);
    H[this->m] += coeff;
    H[0] -= coeff;
}

template<typename FieldT>
void basic_radix2_domain<FieldT>::divide_by_Z_on_coset(std::vector<FieldT> &P)
{
    const FieldT coset = FieldT::multiplicative_generator;
    const FieldT Z_inverse_at_coset = this->compute_Z(coset).inverse();
    for (size_t i = 0; i < this->m; ++i)
    {
        P[i] *= Z_inverse_at_coset;
    }
}

} // libsnark

#endif // BASIC_RADIX2_DOMAIN_TCC_
