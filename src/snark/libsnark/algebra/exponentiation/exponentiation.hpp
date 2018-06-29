/** @file
 *****************************************************************************

 Declaration of interfaces for (square-and-multiply) exponentiation.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef EXPONENTIATION_HPP_
#define EXPONENTIATION_HPP_

#include <cstdint>

#include "algebra/fields/bigint.hpp"

namespace libsnark {

template<typename FieldT, mp_size_t m>
FieldT power(const FieldT &base, const bigint<m> &exponent);

template<typename FieldT>
FieldT power(const FieldT &base, const uint64_t exponent);

} // libsnark

#include "algebra/exponentiation/exponentiation.tcc"

#endif // EXPONENTIATION_HPP_
