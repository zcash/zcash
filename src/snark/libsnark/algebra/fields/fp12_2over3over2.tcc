/** @file
 *****************************************************************************
 Implementation of arithmetic in the finite field F[((p^2)^3)^2].
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef FP12_2OVER3OVER2_TCC_
#define FP12_2OVER3OVER2_TCC_

namespace libsnark {

template<mp_size_t n, const bigint<n>& modulus>
Fp6_3over2_model<n, modulus> Fp12_2over3over2_model<n,modulus>::mul_by_non_residue(const Fp6_3over2_model<n, modulus> &elt)
{
    return Fp6_3over2_model<n, modulus>(non_residue * elt.c2, elt.c0, elt.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::zero()
{
    return Fp12_2over3over2_model<n, modulus>(my_Fp6::zero(), my_Fp6::zero());
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::one()
{
    return Fp12_2over3over2_model<n, modulus>(my_Fp6::one(), my_Fp6::zero());
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::random_element()
{
    Fp12_2over3over2_model<n, modulus> r;
    r.c0 = my_Fp6::random_element();
    r.c1 = my_Fp6::random_element();

    return r;
}

template<mp_size_t n, const bigint<n>& modulus>
bool Fp12_2over3over2_model<n,modulus>::operator==(const Fp12_2over3over2_model<n,modulus> &other) const
{
    return (this->c0 == other.c0 && this->c1 == other.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
bool Fp12_2over3over2_model<n,modulus>::operator!=(const Fp12_2over3over2_model<n,modulus> &other) const
{
    return !(operator==(other));
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::operator+(const Fp12_2over3over2_model<n,modulus> &other) const
{
    return Fp12_2over3over2_model<n,modulus>(this->c0 + other.c0,
                                             this->c1 + other.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::operator-(const Fp12_2over3over2_model<n,modulus> &other) const
{
    return Fp12_2over3over2_model<n,modulus>(this->c0 - other.c0,
                                             this->c1 - other.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n, modulus> operator*(const Fp_model<n, modulus> &lhs, const Fp12_2over3over2_model<n, modulus> &rhs)
{
    return Fp12_2over3over2_model<n,modulus>(lhs*rhs.c0,
                                             lhs*rhs.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n, modulus> operator*(const Fp2_model<n, modulus> &lhs, const Fp12_2over3over2_model<n, modulus> &rhs)
{
    return Fp12_2over3over2_model<n,modulus>(lhs*rhs.c0,
                                             lhs*rhs.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n, modulus> operator*(const Fp6_3over2_model<n, modulus> &lhs, const Fp12_2over3over2_model<n, modulus> &rhs)
{
    return Fp12_2over3over2_model<n,modulus>(lhs*rhs.c0,
                                             lhs*rhs.c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::operator*(const Fp12_2over3over2_model<n,modulus> &other) const
{
    /* Devegili OhEig Scott Dahab --- Multiplication and Squaring on Pairing-Friendly Fields.pdf; Section 3 (Karatsuba) */

    const my_Fp6 &A = other.c0, &B = other.c1,
        &a = this->c0, &b = this->c1;
    const my_Fp6 aA = a * A;
    const my_Fp6 bB = b * B;

    return Fp12_2over3over2_model<n,modulus>(aA + Fp12_2over3over2_model<n, modulus>::mul_by_non_residue(bB),
                                             (a + b)*(A+B) - aA - bB);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::operator-() const
{
    return Fp12_2over3over2_model<n,modulus>(-this->c0,
                                             -this->c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::squared() const
{
    return squared_complex();
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::squared_karatsuba() const
{
    /* Devegili OhEig Scott Dahab --- Multiplication and Squaring on Pairing-Friendly Fields.pdf; Section 3 (Karatsuba squaring) */

    const my_Fp6 &a = this->c0, &b = this->c1;
    const my_Fp6 asq = a.squared();
    const my_Fp6 bsq = b.squared();

    return Fp12_2over3over2_model<n,modulus>(asq + Fp12_2over3over2_model<n, modulus>::mul_by_non_residue(bsq),
                                             (a + b).squared() - asq - bsq);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::squared_complex() const
{
    /* Devegili OhEig Scott Dahab --- Multiplication and Squaring on Pairing-Friendly Fields.pdf; Section 3 (Complex squaring) */

    const my_Fp6 &a = this->c0, &b = this->c1;
    const my_Fp6 ab = a * b;

    return Fp12_2over3over2_model<n,modulus>((a + b) * (a + Fp12_2over3over2_model<n, modulus>::mul_by_non_residue(b)) - ab - Fp12_2over3over2_model<n, modulus>::mul_by_non_residue(ab),
                                             ab + ab);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::inverse() const
{
    /* From "High-Speed Software Implementation of the Optimal Ate Pairing over Barreto-Naehrig Curves"; Algorithm 8 */

    const my_Fp6 &a = this->c0, &b = this->c1;
    const my_Fp6 t0 = a.squared();
    const my_Fp6 t1 = b.squared();
    const my_Fp6 t2 = t0 - Fp12_2over3over2_model<n, modulus>::mul_by_non_residue(t1);
    const my_Fp6 t3 = t2.inverse();
    const my_Fp6 c0 = a * t3;
    const my_Fp6 c1 = - (b * t3);

    return Fp12_2over3over2_model<n,modulus>(c0, c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::Frobenius_map(unsigned long power) const
{
    return Fp12_2over3over2_model<n,modulus>(c0.Frobenius_map(power),
                                             Frobenius_coeffs_c1[power % 12] * c1.Frobenius_map(power));
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::unitary_inverse() const
{
    return Fp12_2over3over2_model<n,modulus>(this->c0,
                                             -this->c1);
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::cyclotomic_squared() const
{
    /* OLD: naive implementation
       return (*this).squared();
    */
    my_Fp2 z0 = this->c0.c0;
    my_Fp2 z4 = this->c0.c1;
    my_Fp2 z3 = this->c0.c2;
    my_Fp2 z2 = this->c1.c0;
    my_Fp2 z1 = this->c1.c1;
    my_Fp2 z5 = this->c1.c2;

    my_Fp2 t0, t1, t2, t3, t4, t5, tmp;

    // t0 + t1*y = (z0 + z1*y)^2 = a^2
    tmp = z0 * z1;
    t0 = (z0 + z1) * (z0 + my_Fp6::non_residue * z1) - tmp - my_Fp6::non_residue * tmp;
    t1 = tmp + tmp;
    // t2 + t3*y = (z2 + z3*y)^2 = b^2
    tmp = z2 * z3;
    t2 = (z2 + z3) * (z2 + my_Fp6::non_residue * z3) - tmp - my_Fp6::non_residue * tmp;
    t3 = tmp + tmp;
    // t4 + t5*y = (z4 + z5*y)^2 = c^2
    tmp = z4 * z5;
    t4 = (z4 + z5) * (z4 + my_Fp6::non_residue * z5) - tmp - my_Fp6::non_residue * tmp;
    t5 = tmp + tmp;

    // for A

    // z0 = 3 * t0 - 2 * z0
    z0 = t0 - z0;
    z0 = z0 + z0;
    z0 = z0 + t0;
    // z1 = 3 * t1 + 2 * z1
    z1 = t1 + z1;
    z1 = z1 + z1;
    z1 = z1 + t1;

    // for B

    // z2 = 3 * (xi * t5) + 2 * z2
    tmp = my_Fp6::non_residue * t5;
    z2 = tmp + z2;
    z2 = z2 + z2;
    z2 = z2 + tmp;

    // z3 = 3 * t4 - 2 * z3
    z3 = t4 - z3;
    z3 = z3 + z3;
    z3 = z3 + t4;

    // for C

    // z4 = 3 * t2 - 2 * z4
    z4 = t2 - z4;
    z4 = z4 + z4;
    z4 = z4 + t2;

    // z5 = 3 * t3 + 2 * z5
    z5 = t3 + z5;
    z5 = z5 + z5;
    z5 = z5 + t3;

    return Fp12_2over3over2_model<n,modulus>(my_Fp6(z0,z4,z3),my_Fp6(z2,z1,z5));
}

template<mp_size_t n, const bigint<n>& modulus>
Fp12_2over3over2_model<n,modulus> Fp12_2over3over2_model<n,modulus>::mul_by_024(const Fp2_model<n, modulus> &ell_0,
                                                                                const Fp2_model<n, modulus> &ell_VW,
                                                                                const Fp2_model<n, modulus> &ell_VV) const
{
    /* OLD: naive implementation
       Fp12_2over3over2_model<n,modulus> a(my_Fp6(ell_0, my_Fp2::zero(), ell_VV),
       my_Fp6(my_Fp2::zero(), ell_VW, my_Fp2::zero()));

       return (*this) * a;
    */
    my_Fp2 z0 = this->c0.c0;
    my_Fp2 z1 = this->c0.c1;
    my_Fp2 z2 = this->c0.c2;
    my_Fp2 z3 = this->c1.c0;
    my_Fp2 z4 = this->c1.c1;
    my_Fp2 z5 = this->c1.c2;

    my_Fp2 x0 = ell_0;
    my_Fp2 x2 = ell_VV;
    my_Fp2 x4 = ell_VW;

    my_Fp2 t0, t1, t2, s0, T3, T4, D0, D2, D4, S1;

    D0 = z0 * x0;
    D2 = z2 * x2;
    D4 = z4 * x4;
    t2 = z0 + z4;
    t1 = z0 + z2;
    s0 = z1 + z3 + z5;

    // For z.a_.a_ = z0.
    S1 = z1 * x2;
    T3 = S1 + D4;
    T4 = my_Fp6::non_residue * T3 + D0;
    z0 = T4;

    // For z.a_.b_ = z1
    T3 = z5 * x4;
    S1 = S1 + T3;
    T3 = T3 + D2;
    T4 = my_Fp6::non_residue * T3;
    T3 = z1 * x0;
    S1 = S1 + T3;
    T4 = T4 + T3;
    z1 = T4;

    // For z.a_.c_ = z2
    t0 = x0 + x2;
    T3 = t1 * t0 - D0 - D2;
    T4 = z3 * x4;
    S1 = S1 + T4;
    T3 = T3 + T4;

    // For z.b_.a_ = z3 (z3 needs z2)
    t0 = z2 + z4;
    z2 = T3;
    t1 = x2 + x4;
    T3 = t0 * t1 - D2 - D4;
    T4 = my_Fp6::non_residue * T3;
    T3 = z3 * x0;
    S1 = S1 + T3;
    T4 = T4 + T3;
    z3 = T4;

    // For z.b_.b_ = z4
    T3 = z5 * x2;
    S1 = S1 + T3;
    T4 = my_Fp6::non_residue * T3;
    t0 = x0 + x4;
    T3 = t2 * t0 - D0 - D4;
    T4 = T4 + T3;
    z4 = T4;

    // For z.b_.c_ = z5.
    t0 = x0 + x2 + x4;
    T3 = s0 * t0 - S1;
    z5 = T3;

    return Fp12_2over3over2_model<n,modulus>(my_Fp6(z0,z1,z2),my_Fp6(z3,z4,z5));

}

template<mp_size_t n, const bigint<n>& modulus, mp_size_t m>
Fp12_2over3over2_model<n, modulus> operator^(const Fp12_2over3over2_model<n, modulus> &self, const bigint<m> &exponent)
{
    return power<Fp12_2over3over2_model<n, modulus> >(self, exponent);
}

template<mp_size_t n, const bigint<n>& modulus, mp_size_t m, const bigint<m>& exp_modulus>
Fp12_2over3over2_model<n, modulus> operator^(const Fp12_2over3over2_model<n, modulus> &self, const Fp_model<m, exp_modulus> &exponent)
{
    return self^(exponent.as_bigint());
}


template<mp_size_t n, const bigint<n>& modulus>
template<mp_size_t m>
Fp12_2over3over2_model<n, modulus> Fp12_2over3over2_model<n,modulus>::cyclotomic_exp(const bigint<m> &exponent) const
{
    Fp12_2over3over2_model<n,modulus> res = Fp12_2over3over2_model<n,modulus>::one();

    bool found_one = false;
    for (long i = m-1; i >= 0; --i)
    {
        for (long j = GMP_NUMB_BITS - 1; j >= 0; --j)
        {
            if (found_one)
            {
                res = res.cyclotomic_squared();
            }

            if (exponent.data[i] & (1ul<<j))
            {
                found_one = true;
                res = res * (*this);
            }
        }
    }

    return res;
}

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream &out, const Fp12_2over3over2_model<n, modulus> &el)
{
    out << el.c0 << OUTPUT_SEPARATOR << el.c1;
    return out;
}

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream &in, Fp12_2over3over2_model<n, modulus> &el)
{
    in >> el.c0 >> el.c1;
    return in;
}

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream& out, const std::vector<Fp12_2over3over2_model<n, modulus> > &v)
{
    out << v.size() << "\n";
    for (const Fp12_2over3over2_model<n, modulus>& t : v)
    {
        out << t << OUTPUT_NEWLINE;
    }

    return out;
}

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream& in, std::vector<Fp12_2over3over2_model<n, modulus> > &v)
{
    v.clear();

    size_t s;
    in >> s;

    char b;
    in.read(&b, 1);

    v.reserve(s);

    for (size_t i = 0; i < s; ++i)
    {
        Fp12_2over3over2_model<n, modulus> el;
        in >> el;
        v.emplace_back(el);
    }

    return in;
}

} // libsnark
#endif // FP12_2OVER3OVER2_TCC_
