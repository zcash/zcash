/** @file
 *****************************************************************************

 Declaration of interfaces for wNAF ("width-w Non-Adjacent Form") exponentiation routines.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef WNAF_HPP_
#define WNAF_HPP_

namespace libsnark {

/**
 * Find the wNAF representation of the given scalar relative to the given window size.
 */
template<mp_size_t n>
std::vector<int64_t> find_wnaf(const size_t window_size, const bigint<n> &scalar);

/**
 * In additive notation, use wNAF exponentiation (with the given window size) to compute scalar * base.
 */
template<typename T, mp_size_t n>
T fixed_window_wnaf_exp(const size_t window_size, const T &base, const bigint<n> &scalar);

/**
 * In additive notation, use wNAF exponentiation (with the window size determined by T) to compute scalar * base.
 */
template<typename T, mp_size_t n>
T opt_window_wnaf_exp(const T &base, const bigint<n> &scalar, const size_t scalar_bits);

} // libsnark

#include "algebra/scalar_multiplication/wnaf.tcc"

#endif // WNAF_HPP_
