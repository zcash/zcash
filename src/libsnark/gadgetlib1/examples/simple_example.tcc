/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SIMPLE_EXAMPLE_TCC_
#define SIMPLE_EXAMPLE_TCC_

#include <cassert>
#include "gadgetlib1/gadgets/basic_gadgets.hpp"

namespace libsnark {

/* NOTE: all examples here actually generate one constraint less to account for soundness constraint in QAP */

template<typename FieldT>
r1cs_example<FieldT> gen_r1cs_example_from_protoboard(const size_t num_constraints,
                                                      const size_t num_inputs)
{
    const size_t new_num_constraints = num_constraints - 1;

    /* construct dummy example: inner products of two vectors */
    protoboard<FieldT> pb;
    pb_variable_array<FieldT> A;
    pb_variable_array<FieldT> B;
    pb_variable<FieldT> res;

    // the variables on the protoboard are (ONE (constant 1 term), res, A[0], ..., A[num_constraints-1], B[0], ..., B[num_constraints-1])
    res.allocate(pb, "res");
    A.allocate(pb, new_num_constraints, "A");
    B.allocate(pb, new_num_constraints, "B");

    inner_product_gadget<FieldT> compute_inner_product(pb, A, B, res, "compute_inner_product");
    compute_inner_product.generate_r1cs_constraints();

    /* fill in random example */
    for (size_t i = 0; i < new_num_constraints; ++i)
    {
        pb.val(A[i]) = FieldT::random_element();
        pb.val(B[i]) = FieldT::random_element();
    }

    compute_inner_product.generate_r1cs_witness();

    pb.constraint_system.num_inputs = num_inputs;
    const r1cs_variable_assignment<FieldT> va = pb.values;
    const r1cs_variable_assignment<FieldT> input(va.begin(), va.begin() + num_inputs);
    return r1cs_example<FieldT>(pb.constraint_system, input, va, num_inputs);
}

} // libsnark
#endif // R1CS_EXAMPLES_TCC_
