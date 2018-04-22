/** @file
 *****************************************************************************
 Implementation of arithmetic in the finite field F[p^2].
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef FP2_TCC_
#define FP2_TCC_

#include "algebra/fields/field_utils.hpp"

namespace libsnark {

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::zero()
{
    return Fp2_model<n, modulus>(my_Fp::zero(), my_Fp::zero());
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::one()
{
    return Fp2_model<n, modulus>(my_Fp::one(), my_Fp::zero());
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::random_element()
{
    Fp2_model<n, modulus> r;
    r.c0 = my_Fp::random_element();
    r.c1 = my_Fp::random_element();

    return r;
}

template<mp_size_t n, const bigint<n>& modulus>
bool Fp2_model<n,modulus>::operator==(const Fp2_model<n,modulus> &other) const
{
    return (this->c0 == other.c0 && this->c1 == other.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
bool Fp2_model<n,modulus>::operator!=(const Fp2_model<n,modulus> &other) const
{
    return !(operator==(other));
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::operator+(const Fp2_model<n,modulus> &other) const
{
    return Fp2_model<n,modulus>(this->c0 + other.c0,
                                this->c1 + other.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::operator-(const Fp2_model<n,modulus> &other) const
{
    return Fp2_model<n,modulus>(this->c0 - other.c0,
                                this->c1 - other.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n, modulus> operator*(const Fp_model<n, modulus> &lhs, const Fp2_model<n, modulus> &rhs)
{
    return Fp2_model<n,modulus>(lhs*rhs.c0,
                                lhs*rhs.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::operator*(const Fp2_model<n,modulus> &other) const
{
    /* Devegili OhEig Scott Dahab --- Multiplication and Squaring on Pairing-Friendly Fields.pdf; Section 3 (Karatsuba) */
    const my_Fp
        &A = other.c0, &B = other.c1,
        &a = this->c0, &b = this->c1;
    const my_Fp aA = a * A;
    const my_Fp bB = b * B;

    return Fp2_model<n,modulus>(aA + non_residue * bB,
                                (a + b)*(A+B) - aA - bB);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::operator-() const
{
    return Fp2_model<n,modulus>(-this->c0,
                                -this->c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::squared() const
{
    return squared_complex();
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::squared_karatsuba() const
{
    /* Devegili OhEig Scott Dahab --- Multiplication and Squaring on Pairing-Friendly Fields.pdf; Section 3 (Karatsuba squaring) */
    const my_Fp &a = this->c0, &b = this->c1;
    const my_Fp asq = a.squared();
    const my_Fp bsq = b.squared();

    return Fp2_model<n,modulus>(asq + non_residue * bsq,
                                (a + b).squared() - asq - bsq);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::squared_complex() const
{
    /* Devegili OhEig Scott Dahab --- Multiplication and Squaring on Pairing-Friendly Fields.pdf; Section 3 (Complex squaring) */
    const my_Fp &a = this->c0, &b = this->c1;
    const my_Fp ab = a * b;

    return Fp2_model<n,modulus>((a + b) * (a + non_residue * b) - ab - non_residue * ab,
                                ab + ab);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::inverse() const
{
    const my_Fp &a = this->c0, &b = this->c1;

    /* From "High-Speed Software Implementation of the Optimal Ate Pairing over Barreto-Naehrig Curves"; Algorithm 8 */
    const my_Fp t0 = a.squared();
    const my_Fp t1 = b.squared();
    const my_Fp t2 = t0 - non_residue * t1;
    const my_Fp t3 = t2.inverse();
    const my_Fp c0 = a * t3;
    const my_Fp c1 = - (b * t3);

    return Fp2_model<n,modulus>(c0, c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::Frobenius_map(uint64_t power) const
{
    return Fp2_model<n,modulus>(c0,
                                Frobenius_coeffs_c1[power % 2] * c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp2_model<n,modulus> Fp2_model<n,modulus>::sqrt() const
{
    if (is_zero()) {
        return *this;
    }

    Fp2_model<n,modulus> one = Fp2_model<n,modulus>::one();

    unsigned long long v = Fp2_model<n,modulus>::s;
    Fp2_model<n,modulus> z = Fp2_model<n,modulus>::nqr_to_t;
    Fp2_model<n,modulus> w = (*this)^Fp2_model<n,modulus>::t_minus_1_over_2;
    Fp2_model<n,modulus> x = (*this) * w;
    Fp2_model<n,modulus> b = x * w; // b = (*this)^t


    // check if square with euler's criterion
    Fp2_model<n,modulus> check = b;
    for (size_t i = 0; i < v-1; ++i)
    {
        check = check.squared();
    }
    if (check != one)
    {
        assert_except(0);
    }


    // compute square root with Tonelli--Shanks
    // (does not terminate if not a square!)

    while (b != one)
    {
        unsigned long long m = 0;
        Fp2_model<n,modulus> b2m = b;
        while (b2m != one)
        {
            /* invariant: b2m = b^(2^m) after entering this loop */
            b2m = b2m.squared();
            m += 1;
        }

        int j = v-m-1;
        w = z;
        while (j > 0)
        {
            w = w.squared();
            --j;
        } // w = z^2^(v-m-1)

        z = w.squared();
        b = b * z;
        x = x * w;
        v = m;
    }

    return x;
}

template<mp_size_t n, const bigint<n>& modulus>
template<mp_size_t m>
Fp2_model<n,modulus> Fp2_model<n,modulus>::operator^(const bigint<m> &pow) const
{
    return power<Fp2_model<n, modulus>, m>(*this, pow);
}

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream &out, const Fp2_model<n, modulus> &el)
{
    out << el.c0 << OUTPUT_SEPARATOR << el.c1;
    return out;
}

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream &in, Fp2_model<n, modulus> &el)
{
    in >> el.c0 >> el.c1;
    return in;
}

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream& out, const std::vector<Fp2_model<n, modulus> > &v)
{
    out << v.size() << "\n";
    for (const Fp2_model<n, modulus>& t : v)
    {
        out << t << OUTPUT_NEWLINE;
    }

    return out;
}

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream& in, std::vector<Fp2_model<n, modulus> > &v)
{
    v.clear();

    unsigned long long s;
    in >> s;

    char b;
    in.read(&b, 1);

    v.reserve(s);

    for (size_t i = 0; i < s; ++i)
    {
        Fp2_model<n, modulus> el;
        in >> el;
        v.emplace_back(el);
    }

    return in;
}

} // libsnark
#endif // FP2_TCC_
