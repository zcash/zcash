/**
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "algebra/curves/alt_bn128/alt_bn128_pp.hpp"
#include "algebra/fields/field_utils.hpp"
#include "common/profiling.hpp"
#include "common/utils.hpp"
#include "reductions/r1cs_to_qap/r1cs_to_qap.hpp"
#include "relations/constraint_satisfaction_problems/r1cs/examples/r1cs_examples.hpp"

#include <gtest/gtest.h>

using namespace libsnark;

template<typename FieldT>
void test_qap(const size_t qap_degree, const size_t num_inputs, const bool binary_input)
{
    /*
      We construct an instance where the QAP degree is qap_degree.
      So we generate an instance of R1CS where the number of constraints qap_degree - num_inputs - 1.
      See the transformation from R1CS to QAP for why this is the case.
      So we need that qap_degree >= num_inputs + 1.
    */
    ASSERT_LE(num_inputs + 1, qap_degree);
    enter_block("Call to test_qap");

    const size_t num_constraints = qap_degree - num_inputs - 1;

    print_indent(); printf("* QAP degree: %zu\n", qap_degree);
    print_indent(); printf("* Number of inputs: %zu\n", num_inputs);
    print_indent(); printf("* Number of R1CS constraints: %zu\n", num_constraints);
    print_indent(); printf("* Input type: %s\n", binary_input ? "binary" : "field");

    enter_block("Generate constraint system and assignment");
    r1cs_example<FieldT> example;
    if (binary_input)
    {
        example = generate_r1cs_example_with_binary_input<FieldT>(num_constraints, num_inputs);
    }
    else
    {
        example = generate_r1cs_example_with_field_input<FieldT>(num_constraints, num_inputs);
    }
    leave_block("Generate constraint system and assignment");

    enter_block("Check satisfiability of constraint system");
    EXPECT_TRUE(example.constraint_system.is_satisfied(example.primary_input, example.auxiliary_input));
    leave_block("Check satisfiability of constraint system");

    const FieldT t = FieldT::random_element(),
    d1 = FieldT::random_element(),
    d2 = FieldT::random_element(),
    d3 = FieldT::random_element();

    enter_block("Compute QAP instance 1");
    qap_instance<FieldT> qap_inst_1 = r1cs_to_qap_instance_map(example.constraint_system);
    leave_block("Compute QAP instance 1");

    enter_block("Compute QAP instance 2");
    qap_instance_evaluation<FieldT> qap_inst_2 = r1cs_to_qap_instance_map_with_evaluation(example.constraint_system, t);
    leave_block("Compute QAP instance 2");

    enter_block("Compute QAP witness");
    qap_witness<FieldT> qap_wit = r1cs_to_qap_witness_map(example.constraint_system, example.primary_input, example.auxiliary_input, d1, d2, d3);
    leave_block("Compute QAP witness");

    enter_block("Check satisfiability of QAP instance 1");
    EXPECT_TRUE(qap_inst_1.is_satisfied(qap_wit));
    leave_block("Check satisfiability of QAP instance 1");

    enter_block("Check satisfiability of QAP instance 2");
    EXPECT_TRUE(qap_inst_2.is_satisfied(qap_wit));
    leave_block("Check satisfiability of QAP instance 2");

    leave_block("Call to test_qap");
}

TEST(relations, qap)
{
    start_profiling();

    const size_t num_inputs = 10;

    enter_block("Test QAP with binary input");

    test_qap<Fr<alt_bn128_pp> >(UINT64_C(1) << 21, num_inputs, true);

    leave_block("Test QAP with binary input");

    enter_block("Test QAP with field input");

    test_qap<Fr<alt_bn128_pp> >(UINT64_C(1) << 21, num_inputs, false);

    leave_block("Test QAP with field input");
}
