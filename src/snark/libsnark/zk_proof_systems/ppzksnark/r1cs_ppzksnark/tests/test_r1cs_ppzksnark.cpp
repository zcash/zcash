/** @file
 *****************************************************************************
 Test program that exercises the ppzkSNARK (first generator, then
 prover, then verifier) on a synthetic R1CS instance.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/
#include <cassert>
#include <cstdio>

#include "common/default_types/r1cs_ppzksnark_pp.hpp"
#include "common/profiling.hpp"
#include "common/utils.hpp"
#include "relations/constraint_satisfaction_problems/r1cs/examples/r1cs_examples.hpp"
#include "zk_proof_systems/ppzksnark/r1cs_ppzksnark/examples/run_r1cs_ppzksnark.hpp"

using namespace libsnark;

template<typename ppT>
void test_r1cs_ppzksnark(size_t num_constraints,
                         size_t input_size)
{
    print_header("(enter) Test R1CS ppzkSNARK");

    const bool test_serialization = true;
    r1cs_example<Fr<ppT> > example = generate_r1cs_example_with_binary_input<Fr<ppT> >(num_constraints, input_size);
    const bool bit = run_r1cs_ppzksnark<ppT>(example, test_serialization);
    assert(bit);

    print_header("(leave) Test R1CS ppzkSNARK");
}

int main()
{
    default_r1cs_ppzksnark_pp::init_public_params();
    start_profiling();

    test_r1cs_ppzksnark<default_r1cs_ppzksnark_pp>(1000, 100);
}
