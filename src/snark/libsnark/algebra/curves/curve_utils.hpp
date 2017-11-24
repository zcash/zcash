/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef CURVE_UTILS_HPP_
#define CURVE_UTILS_HPP_
#include <cstdint>

#include "algebra/fields/bigint.hpp"

namespace libsnark {

template<typename GroupT, mp_size_t m>
GroupT scalar_mul(const GroupT &base, const bigint<m> &scalar);

} // libsnark
#include "algebra/curves/curve_utils.tcc"

#endif // CURVE_UTILS_HPP_
