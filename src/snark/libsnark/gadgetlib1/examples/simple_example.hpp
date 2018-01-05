/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SIMPLE_EXAMPLE_HPP_
#define SIMPLE_EXAMPLE_HPP_

#include "examples/r1cs_examples.hpp"

namespace libsnark {

template<typename FieldT>
r1cs_example<FieldT> gen_r1cs_example_from_protoboard(const size_t num_constraints,
                                                      const size_t num_inputs);

} // libsnark

#include "gadgetlib1/examples/simple_example.tcc"

#endif // SIMPLE_EXAMPLE_HPP_
