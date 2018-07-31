/** @file
 *****************************************************************************
 Declaration of arithmetic in the finite field F[p], for prime p of fixed length.
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef FP_HPP_
#define FP_HPP_

#include "algebra/fields/bigint.hpp"
#include "algebra/exponentiation/exponentiation.hpp"

namespace libsnark {

template<mp_size_t n, const bigint<n>& modulus>
class Fp_model;

template<mp_size_t n, const bigint<n>& modulus>
std::ostream& operator<<(std::ostream &, const Fp_model<n, modulus>&);

template<mp_size_t n, const bigint<n>& modulus>
std::istream& operator>>(std::istream &, Fp_model<n, modulus> &);

/**
 * Arithmetic in the finite field F[p], for prime p of fixed length.
 *
 * This class implements Fp-arithmetic, for a large prime p, using a fixed number
 * of words. It is optimized for tight memory consumption, so the modulus p is
 * passed as a template parameter, to avoid per-element overheads.
 *
 * The implementation is mostly a wrapper around GMP's MPN (constant-size integers).
 * But for the integer sizes of interest for libsnark (3 to 5 limbs of 64 bits each),
 * we implement performance-critical routines, like addition and multiplication,
 * using hand-optimzied assembly code.
*/
template<mp_size_t n, const bigint<n>& modulus>
class Fp_model {
public:
    bigint<n> mont_repr;
public:
    static const mp_size_t num_limbs = n;
    static const constexpr bigint<n>& mod = modulus;
#ifdef PROFILE_OP_COUNTS
    static int64_t add_cnt;
    static int64_t sub_cnt;
    static int64_t mul_cnt;
    static int64_t sqr_cnt;
    static int64_t inv_cnt;
#endif
    static uint64_t num_bits;
    static bigint<n> euler; // (modulus-1)/2
    static uint64_t s; // modulus = 2^s * t + 1
    static bigint<n> t; // with t odd
    static bigint<n> t_minus_1_over_2; // (t-1)/2
    static Fp_model<n, modulus> nqr; // a quadratic nonresidue
    static Fp_model<n, modulus> nqr_to_t; // nqr^t
    static Fp_model<n, modulus> multiplicative_generator; // generator of Fp^*
    static Fp_model<n, modulus> root_of_unity; // generator^((modulus-1)/2^s)
    static mp_limb_t inv; // modulus^(-1) mod W, where W = 2^(word size)
    static bigint<n> Rsquared; // R^2, where R = W^k, where k = ??
    static bigint<n> Rcubed;   // R^3

    static bool modulus_is_valid() { return modulus.data[n-1] != 0; } // mpn inverse assumes that highest limb is non-zero

    Fp_model() {};
    Fp_model(const bigint<n> &b);
    Fp_model(const int64_t x, const bool is_unsigned=false);

    void set_ulong(const uint64_t x);

    void mul_reduce(const bigint<n> &other);

    void clear();

    /* Return the standard (not Montgomery) representation of the
       Field element's requivalence class. I.e. Fp(2).as_bigint()
        would return bigint(2) */
    bigint<n> as_bigint() const;
    /* Return the last limb of the standard representation of the
       field element. E.g. on 64-bit architectures Fp(123).as_ulong()
       and Fp(2^64+123).as_ulong() would both return 123. */
    uint64_t as_ulong() const;

    bool operator==(const Fp_model& other) const;
    bool operator!=(const Fp_model& other) const;
    bool is_zero() const;

    void print() const;

    Fp_model& operator+=(const Fp_model& other);
    Fp_model& operator-=(const Fp_model& other);
    Fp_model& operator*=(const Fp_model& other);
    Fp_model& operator^=(const uint64_t pow);

    template<mp_size_t m>
    Fp_model& operator^=(const bigint<m> &pow);

    Fp_model operator+(const Fp_model& other) const;
    Fp_model operator-(const Fp_model& other) const;
    Fp_model operator*(const Fp_model& other) const;
    Fp_model operator-() const;
    Fp_model squared() const;
    Fp_model& invert();
    Fp_model inverse() const;
    Fp_model sqrt() const; // HAS TO BE A SQUARE (else does not terminate)

    Fp_model operator^(const uint64_t pow) const;
    template<mp_size_t m>
    Fp_model operator^(const bigint<m> &pow) const;

    static uint64_t size_in_bits() { return num_bits; }
    static uint64_t capacity() { return num_bits - 1; }
    static bigint<n> field_char() { return modulus; }

    static Fp_model<n, modulus> zero();
    static Fp_model<n, modulus> one();
    static Fp_model<n, modulus> random_element();

    friend std::ostream& operator<< <n,modulus>(std::ostream &out, const Fp_model<n, modulus> &p);
    friend std::istream& operator>> <n,modulus>(std::istream &in, Fp_model<n, modulus> &p);
};

#ifdef PROFILE_OP_COUNTS
template<mp_size_t n, const bigint<n>& modulus>
int64_t Fp_model<n, modulus>::add_cnt = 0;

template<mp_size_t n, const bigint<n>& modulus>
int64_t Fp_model<n, modulus>::sub_cnt = 0;

template<mp_size_t n, const bigint<n>& modulus>
int64_t Fp_model<n, modulus>::mul_cnt = 0;

template<mp_size_t n, const bigint<n>& modulus>
int64_t Fp_model<n, modulus>::sqr_cnt = 0;

template<mp_size_t n, const bigint<n>& modulus>
int64_t Fp_model<n, modulus>::inv_cnt = 0;
#endif

template<mp_size_t n, const bigint<n>& modulus>
uint64_t Fp_model<n, modulus>::num_bits;

template<mp_size_t n, const bigint<n>& modulus>
bigint<n> Fp_model<n, modulus>::euler;

template<mp_size_t n, const bigint<n>& modulus>
uint64_t Fp_model<n, modulus>::s;

template<mp_size_t n, const bigint<n>& modulus>
bigint<n> Fp_model<n, modulus>::t;

template<mp_size_t n, const bigint<n>& modulus>
bigint<n> Fp_model<n, modulus>::t_minus_1_over_2;

template<mp_size_t n, const bigint<n>& modulus>
Fp_model<n, modulus> Fp_model<n, modulus>::nqr;

template<mp_size_t n, const bigint<n>& modulus>
Fp_model<n, modulus> Fp_model<n, modulus>::nqr_to_t;

template<mp_size_t n, const bigint<n>& modulus>
Fp_model<n, modulus> Fp_model<n, modulus>::multiplicative_generator;

template<mp_size_t n, const bigint<n>& modulus>
Fp_model<n, modulus> Fp_model<n, modulus>::root_of_unity;

template<mp_size_t n, const bigint<n>& modulus>
mp_limb_t Fp_model<n, modulus>::inv;

template<mp_size_t n, const bigint<n>& modulus>
bigint<n> Fp_model<n, modulus>::Rsquared;

template<mp_size_t n, const bigint<n>& modulus>
bigint<n> Fp_model<n, modulus>::Rcubed;

} // libsnark
#include "algebra/fields/fp.tcc"

#endif // FP_HPP_
