/** @file
 *****************************************************************************
 Implementation of templatized utility functions
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef UTILS_TCC_
#define UTILS_TCC_

namespace libsnark {

template<typename T>
size_t size_in_bits(const std::vector<T> &v)
{
    return v.size() * T::size_in_bits();
}

} // libsnark

#endif // UTILS_TCC_
