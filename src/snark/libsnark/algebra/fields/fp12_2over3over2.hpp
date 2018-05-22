/** @file
 *****************************************************************************
 Declaration of arithmetic in the finite field F[((p^2)^3)^2].
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef FP12_2OVER3OVER2_HPP_
#define FP12_2OVER3OVER2_HPP_
#include "algebra/fields/fp.hpp"
#include "algebra/fields/fp2.hpp"
#include "algebra/fields/fp6_3over2.hpp"
#include <vector>

namespace libsnark {

template<mp_size_t n, const bigint<n>& modulus>
class Fp12_2over3over2_model;

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream &, const Fp12_2over3over2_model<n, modulus> &);

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream &, Fp12_2over3over2_model<n, modulus> &);

/**
 * Arithmetic in the finite field F[((p^2)^3)^2].
 *
 * Let p := modulus. This interface provides arithmetic for the extension field
 * Fp12 = Fp6[W]/(W^2-V) where Fp6 = Fp2[V]/(V^3-non_residue) and non_residue is in Fp2
 *
 * ASSUMPTION: p = 1 (mod 6)
 */
template<mp_size_t n, const bigint<n>& modulus>
class Fp12_2over3over2_model {
public:
    typedef Fp_model<n, modulus> my_Fp;
    typedef Fp2_model<n, modulus> my_Fp2;
    typedef Fp6_3over2_model<n, modulus> my_Fp6;

    static Fp2_model<n, modulus> non_residue;
    static Fp2_model<n, modulus> Frobenius_coeffs_c1[12]; // non_residue^((modulus^i-1)/6) for i=0,...,11

    my_Fp6 c0, c1;
    Fp12_2over3over2_model() {};
    Fp12_2over3over2_model(const my_Fp6& c0, const my_Fp6& c1) : c0(c0), c1(c1) {};

    void clear() { c0.clear(); c1.clear(); }
    void print() const { printf("c0/c1:\n"); c0.print(); c1.print(); }

    static Fp12_2over3over2_model<n, modulus> zero();
    static Fp12_2over3over2_model<n, modulus> one();
    static Fp12_2over3over2_model<n, modulus> random_element();

    bool is_zero() const { return c0.is_zero() && c1.is_zero(); }
    bool operator==(const Fp12_2over3over2_model &other) const;
    bool operator!=(const Fp12_2over3over2_model &other) const;

    Fp12_2over3over2_model operator+(const Fp12_2over3over2_model &other) const;
    Fp12_2over3over2_model operator-(const Fp12_2over3over2_model &other) const;
    Fp12_2over3over2_model operator*(const Fp12_2over3over2_model &other) const;
    Fp12_2over3over2_model operator-() const;
    Fp12_2over3over2_model squared() const; // default is squared_complex
    Fp12_2over3over2_model squared_karatsuba() const;
    Fp12_2over3over2_model squared_complex() const;
    Fp12_2over3over2_model inverse() const;
    Fp12_2over3over2_model Frobenius_map(uint64_t power) const;
    Fp12_2over3over2_model unitary_inverse() const;
    Fp12_2over3over2_model cyclotomic_squared() const;

    Fp12_2over3over2_model mul_by_024(const my_Fp2 &ell_0, const my_Fp2 &ell_VW, const my_Fp2 &ell_VV) const;

    static my_Fp6 mul_by_non_residue(const my_Fp6 &elt);

    template<mp_size_t m>
    Fp12_2over3over2_model cyclotomic_exp(const bigint<m> &exponent) const;

    static bigint<n> base_field_char() { return modulus; }
    static uint64_t extension_degree() { return 12; }

    friend std::ostream& operator<< <n, modulus>(std::ostream &out, const Fp12_2over3over2_model<n, modulus> &el);
    friend std::istream& operator>> <n, modulus>(std::istream &in, Fp12_2over3over2_model<n, modulus> &el);
};

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream& out, const std::vector<Fp12_2over3over2_model<n, modulus> > &v);

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream& in, std::vector<Fp12_2over3over2_model<n, modulus> > &v);

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n, modulus> operator*(const Fp_model<n, modulus> &lhs, const Fp12_2over3over2_model<n, modulus> &rhs);

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n, modulus> operator*(const Fp2_model<n, modulus> &lhs, const Fp12_2over3over2_model<n, modulus> &rhs);

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n, modulus> operator*(const Fp6_3over2_model<n, modulus> &lhs, const Fp12_2over3over2_model<n, modulus> &rhs);

template<mp_size_t n, const bigint<n>& modulus, mp_size_t m>
Fp12_2over3over2_model<n, modulus> operator^(const Fp12_2over3over2_model<n, modulus> &self, const bigint<m> &exponent);

template<mp_size_t n, const bigint<n>& modulus, mp_size_t m, const bigint<m>& exp_modulus>
Fp12_2over3over2_model<n, modulus> operator^(const Fp12_2over3over2_model<n, modulus> &self, const Fp_model<m, exp_modulus> &exponent);

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n, modulus> Fp12_2over3over2_model<n, modulus>::non_residue;

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n, modulus> Fp12_2over3over2_model<n, modulus>::Frobenius_coeffs_c1[12];

} // libsnark
#include "algebra/fields/fp12_2over3over2.tcc"
#endif // FP12_2OVER3OVER2_HPP_
