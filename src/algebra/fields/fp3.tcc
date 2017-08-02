/** @file
 *****************************************************************************
 Implementation of arithmetic in the finite field F[p^3].
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef FP3_TCC_
#define FP3_TCC_

#include "algebra/fields/field_utils.hpp"

namespace libsnark {

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::zero()
{
    return Fp3_model<n, modulus>(my_Fp::zero(), my_Fp::zero(), my_Fp::zero());
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::one()
{
    return Fp3_model<n, modulus>(my_Fp::one(), my_Fp::zero(), my_Fp::zero());
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::random_element()
{
    Fp3_model<n, modulus> r;
    r.c0 = my_Fp::random_element();
    r.c1 = my_Fp::random_element();
    r.c2 = my_Fp::random_element();

    return r;
}

template<mp_size_t n, const bigint<n>& modulus>
bool Fp3_model<n,modulus>::operator==(const Fp3_model<n,modulus> &other) const
{
    return (this->c0 == other.c0 && this->c1 == other.c1 && this->c2 == other.c2);
}

template<mp_size_t n, const bigint<n>& modulus>
bool Fp3_model<n,modulus>::operator!=(const Fp3_model<n,modulus> &other) const
{
    return !(operator==(other));
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::operator+(const Fp3_model<n,modulus> &other) const
{
    return Fp3_model<n,modulus>(this->c0 + other.c0,
                                this->c1 + other.c1,
                                this->c2 + other.c2);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::operator-(const Fp3_model<n,modulus> &other) const
{
    return Fp3_model<n,modulus>(this->c0 - other.c0,
                                this->c1 - other.c1,
                                this->c2 - other.c2);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n, modulus> operator*(const Fp_model<n, modulus> &lhs, const Fp3_model<n, modulus> &rhs)
{
    return Fp3_model<n,modulus>(lhs*rhs.c0,
                                lhs*rhs.c1,
                                lhs*rhs.c2);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::operator*(const Fp3_model<n,modulus> &other) const
{
    /* Devegili OhEig Scott Dahab --- Multiplication and Squaring on Pairing-Friendly Fields.pdf; Section 4 (Karatsuba) */
    const my_Fp
        &A = other.c0, &B = other.c1, &C = other.c2,
        &a = this->c0, &b = this->c1, &c = this->c2;
    const my_Fp aA = a*A;
    const my_Fp bB = b*B;
    const my_Fp cC = c*C;

    return Fp3_model<n,modulus>(aA + non_residue*((b+c)*(B+C)-bB-cC),
                                (a+b)*(A+B)-aA-bB+non_residue*cC,
                                (a+c)*(A+C)-aA+bB-cC);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::operator-() const
{
    return Fp3_model<n,modulus>(-this->c0,
                                -this->c1,
                                -this->c2);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::squared() const
{
    /* Devegili OhEig Scott Dahab --- Multiplication and Squaring on Pairing-Friendly Fields.pdf; Section 4 (CH-SQR2) */
    const my_Fp
        &a = this->c0, &b = this->c1, &c = this->c2;
    const my_Fp s0 = a.squared();
    const my_Fp ab = a*b;
    const my_Fp s1 = ab + ab;
    const my_Fp s2 = (a - b + c).squared();
    const my_Fp bc = b*c;
    const my_Fp s3 = bc + bc;
    const my_Fp s4 = c.squared();

    return Fp3_model<n,modulus>(s0 + non_residue * s3,
                                s1 + non_residue * s4,
                                s1 + s2 + s3 - s0 - s4);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::inverse() const
{
    const my_Fp
        &a = this->c0, &b = this->c1, &c = this->c2;

    /* From "High-Speed Software Implementation of the Optimal Ate Pairing over Barreto-Naehrig Curves"; Algorithm 17 */
    const my_Fp t0 = a.squared();
    const my_Fp t1 = b.squared();
    const my_Fp t2 = c.squared();
    const my_Fp t3 = a*b;
    const my_Fp t4 = a*c;
    const my_Fp t5 = b*c;
    const my_Fp c0 = t0 - non_residue * t5;
    const my_Fp c1 = non_residue * t2 - t3;
    const my_Fp c2 = t1 - t4; // typo in paper referenced above. should be "-" as per Scott, but is "*"
    const my_Fp t6 = (a * c0 + non_residue * (c * c1 + b * c2)).inverse();
    return Fp3_model<n,modulus>(t6 * c0, t6 * c1, t6 * c2);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::Frobenius_map(unsigned long power) const
{
    return Fp3_model<n,modulus>(c0,
                                Frobenius_coeffs_c1[power % 3] * c1,
                                Frobenius_coeffs_c2[power % 3] * c2);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp3_model<n,modulus> Fp3_model<n,modulus>::sqrt() const
{
    Fp3_model<n,modulus> one = Fp3_model<n,modulus>::one();

    size_t v = Fp3_model<n,modulus>::s;
    Fp3_model<n,modulus> z = Fp3_model<n,modulus>::nqr_to_t;
    Fp3_model<n,modulus> w = (*this)^Fp3_model<n,modulus>::t_minus_1_over_2;
    Fp3_model<n,modulus> x = (*this) * w;
    Fp3_model<n,modulus> b = x * w; // b = (*this)^t

#if DEBUG
    // check if square with euler's criterion
    Fp3_model<n,modulus> check = b;
    for (size_t i = 0; i < v-1; ++i)
    {
        check = check.squared();
    }
    if (check != one)
    {
        assert(0);
    }
#endif

    // compute square root with Tonelli--Shanks
    // (does not terminate if not a square!)

    while (b != one)
    {
        size_t m = 0;
        Fp3_model<n,modulus> b2m = b;
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
Fp3_model<n,modulus> Fp3_model<n,modulus>::operator^(const bigint<m> &pow) const
{
    return power<Fp3_model<n, modulus> >(*this, pow);
}

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream &out, const Fp3_model<n, modulus> &el)
{
    out << el.c0 << OUTPUT_SEPARATOR << el.c1 << OUTPUT_SEPARATOR << el.c2;
    return out;
}

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream &in, Fp3_model<n, modulus> &el)
{
    in >> el.c0 >> el.c1 >> el.c2;
    return in;
}

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream& out, const std::vector<Fp3_model<n, modulus> > &v)
{
    out << v.size() << "\n";
    for (const Fp3_model<n, modulus>& t : v)
    {
        out << t << OUTPUT_NEWLINE;
    }

    return out;
}

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream& in, std::vector<Fp3_model<n, modulus> > &v)
{
    v.clear();

    size_t s;
    in >> s;

    char b;
    in.read(&b, 1);

    v.reserve(s);

    for (size_t i = 0; i < s; ++i)
    {
        Fp3_model<n, modulus> el;
        in >> el;
        v.emplace_back(el);
    }

    return in;
}

} // libsnark
#endif // FP3_TCC_
