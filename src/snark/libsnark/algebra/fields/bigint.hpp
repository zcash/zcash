/** @file
 *****************************************************************************
 Declaration of bigint wrapper class around GMP's MPZ long integers.
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef BIGINT_HPP_
#define BIGINT_HPP_
#include <cstddef>
#include <iostream>
#include <gmp.h>
#include "common/serialization.hpp"

namespace libsnark {

template<mp_size_t n> class bigint;
template<mp_size_t n> std::ostream& operator<<(std::ostream &, const bigint<n>&);
template<mp_size_t n> std::istream& operator>>(std::istream &, bigint<n>&);

/**
 * Wrapper class around GMP's MPZ long integers. It supports arithmetic operations,
 * serialization and randomization. Serialization is fragile, see common/serialization.hpp.
 */

template<mp_size_t n>
class bigint {
public:
    static const mp_size_t N = n;

    mp_limb_t data[n] = {0};

    bigint() = default;
    bigint(const uint64_t x); /// Initalize from a small integer
    bigint(const char* s); /// Initialize from a string containing an integer in decimal notation
    bigint(const mpz_t r); /// Initialize from MPZ element

    void print() const;
    void print_hex() const;
    bool operator==(const bigint<n>& other) const;
    bool operator!=(const bigint<n>& other) const;
    void clear();
    bool is_zero() const;
    size_t max_bits() const { return n * GMP_NUMB_BITS; }
    size_t num_bits() const;

    uint64_t as_ulong() const; /* return the last limb of the integer */
    void to_mpz(mpz_t r) const;
    bool test_bit(const std::size_t bitno) const;

    template<mp_size_t m> inline void operator+=(const bigint<m>& other);
    template<mp_size_t m> inline bigint<n+m> operator*(const bigint<m>& other) const;
    template<mp_size_t d> static inline void div_qr(bigint<n-d+1>& quotient, bigint<d>& remainder,
                                                    const bigint<n>& dividend, const bigint<d>& divisor);
    template<mp_size_t m> inline bigint<m> shorten(const bigint<m>& q, const char *msg) const;

    inline void limit(const bigint<n>& q, const char *msg) const;
    bool operator>(const bigint<n>& other) const;

    bigint& randomize();

    friend std::ostream& operator<< <n>(std::ostream &out, const bigint<n> &b);
    friend std::istream& operator>> <n>(std::istream &in, bigint<n> &b);
};

} // libsnark
#include "algebra/fields/bigint.tcc"
#endif
