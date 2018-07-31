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

#include "algebra/curves/mnt/mnt6/mnt6_pp.hpp"
#include "algebra/fields/field_utils.hpp"
#include "common/profiling.hpp"
#include "common/utils.hpp"
#include "reductions/r1cs_to_qap/r1cs_to_qap.hpp"
#include "relations/constraint_satisfaction_problems/r1cs/examples/r1cs_examples.hpp"

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
    assert(num_inputs + 1 <= qap_degree);
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
    assert(example.constraint_system.is_satisfied(example.primary_input, example.auxiliary_input));
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
    assert(qap_inst_1.is_satisfied(qap_wit));
    leave_block("Check satisfiability of QAP instance 1");

    enter_block("Check satisfiability of QAP instance 2");
    assert(qap_inst_2.is_satisfied(qap_wit));
    leave_block("Check satisfiability of QAP instance 2");

    leave_block("Call to test_qap");
}

int main()
{
    start_profiling();

    mnt6_pp::init_public_params();

    const size_t num_inputs = 10;

    const size_t basic_domain_size = UINT64_C(1)<<mnt6_Fr::s;
    const size_t step_domain_size = (UINT64_C(1)<<10) + (UINT64_C(1)<<8);
    const size_t extended_domain_size = UINT64_C(1)<<(mnt6_Fr::s+1);
    const size_t extended_domain_size_special = extended_domain_size-1;

    enter_block("Test QAP with binary input");

    test_qap<Fr<mnt6_pp> >(basic_domain_size, num_inputs, true);
    test_qap<Fr<mnt6_pp> >(step_domain_size, num_inputs, true);
    test_qap<Fr<mnt6_pp> >(extended_domain_size, num_inputs, true);
    test_qap<Fr<mnt6_pp> >(extended_domain_size_special, num_inputs, true);

    leave_block("Test QAP with binary input");

    enter_block("Test QAP with field input");

    test_qap<Fr<mnt6_pp> >(basic_domain_size, num_inputs, false);
    test_qap<Fr<mnt6_pp> >(step_domain_size, num_inputs, false);
    test_qap<Fr<mnt6_pp> >(extended_domain_size, num_inputs, false);
    test_qap<Fr<mnt6_pp> >(extended_domain_size_special, num_inputs, false);

    leave_block("Test QAP with field input");
}
