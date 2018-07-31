/** @file
 *****************************************************************************

 Declaration of functions for supporting the use of templates.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef TEMPLATE_UTILS_HPP_
#define TEMPLATE_UTILS_HPP_

namespace libsnark {

/* A commonly used SFINAE helper type */
template<typename T>
struct void_type
{
    typedef void type;
};

} // libsnark

#endif // TEMPLATE_UTILS_HPP_
