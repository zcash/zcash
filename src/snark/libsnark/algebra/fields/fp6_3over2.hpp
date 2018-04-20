/** @file
 *****************************************************************************
 Declaration of arithmetic in the finite field F[(p^2)^3]
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef FP6_3OVER2_HPP_
#define FP6_3OVER2_HPP_
#include "algebra/fields/fp.hpp"
#include "algebra/fields/fp2.hpp"
#include <vector>

namespace libsnark {

template<mp_size_t n, const bigint<n>& modulus>
class Fp6_3over2_model;

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream &, const Fp6_3over2_model<n, modulus> &);

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream &, Fp6_3over2_model<n, modulus> &);

/**
 * Arithmetic in the finite field F[(p^2)^3].
 *
 * Let p := modulus. This interface provides arithmetic for the extension field
 *  Fp6 = Fp2[V]/(V^3-non_residue) where non_residue is in Fp.
 *
 * ASSUMPTION: p = 1 (mod 6)
 */
template<mp_size_t n, const bigint<n>& modulus>
class Fp6_3over2_model {
public:
    typedef Fp_model<n, modulus> my_Fp;
    typedef Fp2_model<n, modulus> my_Fp2;

    static my_Fp2 non_residue;
    static my_Fp2 Frobenius_coeffs_c1[6]; // non_residue^((modulus^i-1)/3)   for i=0,1,2,3,4,5
    static my_Fp2 Frobenius_coeffs_c2[6]; // non_residue^((2*modulus^i-2)/3) for i=0,1,2,3,4,5

    my_Fp2 c0, c1, c2;
    Fp6_3over2_model() {};
    Fp6_3over2_model(const my_Fp2& c0, const my_Fp2& c1, const my_Fp2& c2) : c0(c0), c1(c1), c2(c2) {};

    void clear() { c0.clear(); c1.clear(); c2.clear(); }
    void print() const { printf("c0/c1/c2:\n"); c0.print(); c1.print(); c2.print(); }

    static Fp6_3over2_model<n, modulus> zero();
    static Fp6_3over2_model<n, modulus> one();
    static Fp6_3over2_model<n, modulus> random_element();

    bool is_zero() const { return c0.is_zero() && c1.is_zero() && c2.is_zero(); }
    bool operator==(const Fp6_3over2_model &other) const;
    bool operator!=(const Fp6_3over2_model &other) const;

    Fp6_3over2_model operator+(const Fp6_3over2_model &other) const;
    Fp6_3over2_model operator-(const Fp6_3over2_model &other) const;
    Fp6_3over2_model operator*(const Fp6_3over2_model &other) const;
    Fp6_3over2_model operator-() const;
    Fp6_3over2_model squared() const;
    Fp6_3over2_model inverse() const;
    Fp6_3over2_model Frobenius_map(uint64_t power) const;

    static my_Fp2 mul_by_non_residue(const my_Fp2 &elt);

    template<mp_size_t m>
    Fp6_3over2_model operator^(const bigint<m> &other) const;

    static bigint<n> base_field_char() { return modulus; }
    static uint64_t extension_degree() { return 6; }

    friend std::ostream& operator<< <n, modulus>(std::ostream &out, const Fp6_3over2_model<n, modulus> &el);
    friend std::istream& operator>> <n, modulus>(std::istream &in, Fp6_3over2_model<n, modulus> &el);
};

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream& out, const std::vector<Fp6_3over2_model<n, modulus> > &v);

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream& in, std::vector<Fp6_3over2_model<n, modulus> > &v);

template<mp_size_t n, const bigint<n>& modulus>
Fp6_3over2_model<n, modulus> operator*(const Fp_model<n, modulus> &lhs, const Fp6_3over2_model<n, modulus> &rhs);

template<mp_size_t n, const bigint<n>& modulus>
Fp6_3over2_model<n, modulus> operator*(const Fp2_model<n, modulus> &lhs, const Fp6_3over2_model<n, modulus> &rhs);

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n, modulus> Fp6_3over2_model<n, modulus>::non_residue;

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n, modulus> Fp6_3over2_model<n, modulus>::Frobenius_coeffs_c1[6];

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n, modulus> Fp6_3over2_model<n, modulus>::Frobenius_coeffs_c2[6];

} // libsnark
#include "algebra/fields/fp6_3over2.tcc"

#endif // FP6_3OVER2_HPP_
