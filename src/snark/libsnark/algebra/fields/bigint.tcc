/** @file
 *****************************************************************************
 Implementation of bigint wrapper class around GMP's MPZ long integers.
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef BIGINT_TCC_
#define BIGINT_TCC_
#include <cassert>
#include <climits>
#include <cstring>
#include "sodium.h"
#include "common/assert_except.hpp"

namespace libsnark {

template<mp_size_t n>
bigint<n>::bigint(const uint64_t x) /// Initialize from a small integer
{
    static_assert(UINT64_MAX <= GMP_NUMB_MAX, "uint64_t does not fit in a GMP limb");
    this->data[0] = x;
}

template<mp_size_t n>
bigint<n>::bigint(const char* s) /// Initialize from a string containing an integer in decimal notation
{
    size_t l = strlen(s);
    unsigned char* s_copy = new unsigned char[l];

    for (size_t i = 0; i < l; ++i)
    {
        assert_except(s[i] >= '0' && s[i] <= '9');
        s_copy[i] = s[i] - '0';
    }

    mp_size_t limbs_written = mpn_set_str(this->data, s_copy, l, 10);
    assert_except(limbs_written <= n);

    delete[] s_copy;
}

template<mp_size_t n>
bigint<n>::bigint(const mpz_t r) /// Initialize from MPZ element
{
    mpz_t k;
    mpz_init_set(k, r);

    for (size_t i = 0; i < n; ++i)
    {
        data[i] = mpz_get_ui(k);
        mpz_fdiv_q_2exp(k, k, GMP_NUMB_BITS);
    }

    assert_except(mpz_sgn(k) == 0);
    mpz_clear(k);
}

template<mp_size_t n>
void bigint<n>::print() const
{
    gmp_printf("%Nd\n", this->data, n);
}

template<mp_size_t n>
void bigint<n>::print_hex() const
{
    gmp_printf("%Nx\n", this->data, n);
}

template<mp_size_t n>
bool bigint<n>::operator==(const bigint<n>& other) const
{
    return (mpn_cmp(this->data, other.data, n) == 0);
}

template<mp_size_t n>
bool bigint<n>::operator!=(const bigint<n>& other) const
{
    return !(operator==(other));
}

template<mp_size_t n>
void bigint<n>::clear()
{
    mpn_zero(this->data, n);
}

template<mp_size_t n>
bool bigint<n>::is_zero() const
{
    for (mp_size_t i = 0; i < n; ++i)
    {
        if (this->data[i])
        {
            return false;
        }
    }

    return true;
}

template<mp_size_t n>
size_t bigint<n>::num_bits() const
{
/*
    for (int64_t i = max_bits(); i >= 0; --i)
    {
        if (this->test_bit(i))
        {
            return i+1;
        }
    }

    return 0;
*/
    for (int64_t i = n-1; i >= 0; --i)
    {
        mp_limb_t x = this->data[i];
        if (x == 0)
        {
            continue;
        }
        else
        {
            static_assert(GMP_NUMB_MAX <= ULLONG_MAX, "coercing limb to unsigned long long might truncate");
            return ((i+1) * GMP_NUMB_BITS) - __builtin_clzll(x);
        }
    }
    return 0;
}

template<mp_size_t n>
uint64_t bigint<n>::as_uint64() const
{
    return this->data[0];
}

template<mp_size_t n>
void bigint<n>::to_mpz(mpz_t r) const
{
    mpz_set_ui(r, 0);

    for (int i = n-1; i >= 0; --i)
    {
        mpz_mul_2exp(r, r, GMP_NUMB_BITS);
        mpz_add_ui(r, r, this->data[i]);
    }
}

template<mp_size_t n>
bool bigint<n>::test_bit(const std::size_t bitno) const
{
    if (bitno >= n * GMP_NUMB_BITS)
    {
        return false;
    }
    else
    {
        const std::size_t part = bitno/GMP_NUMB_BITS;
        const std::size_t bit = bitno - (GMP_NUMB_BITS*part);
        const mp_limb_t one = 1;
        return (this->data[part] & (one<<bit));
    }
}


template<mp_size_t n> template<mp_size_t m>
inline void bigint<n>::operator+=(const bigint<m>& other)
{
    static_assert(n >= m, "first arg must not be smaller than second arg for bigint in-place add");
    mpn_add(data, data, n, other.data, m);
}

template<mp_size_t n> template<mp_size_t m>
inline bigint<n+m> bigint<n>::operator*(const bigint<m>& other) const
{
    static_assert(n >= m, "first arg must not be smaller than second arg for bigint mul");
    bigint<n+m> res;
    mpn_mul(res.data, data, n, other.data, m);
    return res;
}

template<mp_size_t n> template<mp_size_t d>
inline void bigint<n>::div_qr(bigint<n-d+1>& quotient, bigint<d>& remainder,
                              const bigint<n>& dividend, const bigint<d>& divisor)
{
    static_assert(n >= d, "dividend must not be smaller than divisor for bigint::div_qr");
    assert_except(divisor.data[d-1] != 0);
    mpn_tdiv_qr(quotient.data, remainder.data, 0, dividend.data, n, divisor.data, d);
}

// Return a copy shortened to m limbs provided it is less than limit, throwing std::domain_error if not in range.
template<mp_size_t n> template<mp_size_t m>
inline bigint<m> bigint<n>::shorten(const bigint<m>& q, const char *msg) const
{
    static_assert(m <= n, "number of limbs must not increase for bigint::shorten");
    for (mp_size_t i = m; i < n; i++) { // high-order limbs
        if (data[i] != 0) {
            throw std::domain_error(msg);
        }
    }
    bigint<m> res;
    mpn_copyi(res.data, data, m);
    res.limit(q, msg);
    return res;
}

template<mp_size_t n>
inline void bigint<n>::limit(const bigint<n>& q, const char *msg) const
{
    if (!(q > *this)) {
        throw std::domain_error(msg);
    }
}

template<mp_size_t n>
inline bool bigint<n>::operator>(const bigint<n>& other) const
{
    return mpn_cmp(this->data, other.data, n) > 0;
}

template<mp_size_t n>
bigint<n>& bigint<n>::randomize()
{
    assert_except(GMP_NUMB_BITS == sizeof(mp_limb_t) * 8);

    randombytes_buf(this->data, sizeof(mp_limb_t) * n);

    return (*this);
}


template<mp_size_t n>
std::ostream& operator<<(std::ostream &out, const bigint<n> &b)
{
#ifdef BINARY_OUTPUT
    out.write((char*)b.data, sizeof(b.data[0]) * n);
#else
    mpz_t t;
    mpz_init(t);
    b.to_mpz(t);

    out << t;

    mpz_clear(t);
#endif
    return out;
}

template<mp_size_t n>
std::istream& operator>>(std::istream &in, bigint<n> &b)
{
#ifdef BINARY_OUTPUT
    in.read((char*)b.data, sizeof(b.data[0]) * n);
#else
    std::string s;
    in >> s;

    size_t l = s.size();
    unsigned char* s_copy = new unsigned char[l];

    for (size_t i = 0; i < l; ++i)
    {
        assert_except(s[i] >= '0' && s[i] <= '9');
        s_copy[i] = s[i] - '0';
    }

    mp_size_t limbs_written = mpn_set_str(b.data, s_copy, l, 10);
    assert_except(limbs_written <= n);

    delete[] s_copy;
#endif
    return in;
}

} // libsnark
#endif // BIGINT_TCC_
