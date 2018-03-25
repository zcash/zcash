/** @file
 *****************************************************************************

 Declaration of interfaces for a R1CS-to-QAP reduction, that is, constructing
 a QAP ("Quadratic Arithmetic Program") from a R1CS ("Rank-1 Constraint System").

 QAPs are defined in \[GGPR13], and constructed for R1CS also in \[GGPR13].

 The implementation of the reduction follows, extends, and optimizes
 the efficient approach described in Appendix E of \[BCGTV13].

 References:

 \[BCGTV13]
 "SNARKs for C: Verifying Program Executions Succinctly and in Zero Knowledge",
 Eli Ben-Sasson, Alessandro Chiesa, Daniel Genkin, Eran Tromer, Madars Virza,
 CRYPTO 2013,
 <http://eprint.iacr.org/2013/507>

 \[GGPR13]:
 "Quadratic span programs and succinct NIZKs without PCPs",
 Rosario Gennaro, Craig Gentry, Bryan Parno, Mariana Raykova,
 EUROCRYPT 2013,
 <http://eprint.iacr.org/2012/215>

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef R1CS_TO_QAP_HPP_
#define R1CS_TO_QAP_HPP_

#include "relations/arithmetic_programs/qap/qap.hpp"
#include "relations/constraint_satisfaction_problems/r1cs/r1cs.hpp"

namespace libsnark {

/**
 * Instance map for the R1CS-to-QAP reduction.
 */
template<typename FieldT>
qap_instance<FieldT> r1cs_to_qap_instance_map(const r1cs_constraint_system<FieldT> &cs);

/**
 * Instance map for the R1CS-to-QAP reduction followed by evaluation of the resulting QAP instance.
 */
template<typename FieldT>
qap_instance_evaluation<FieldT> r1cs_to_qap_instance_map_with_evaluation(const r1cs_constraint_system<FieldT> &cs,
                                                                         const FieldT &t);

/**
 * Witness map for the R1CS-to-QAP reduction.
 *
 * The witness map takes zero knowledge into account when d1,d2,d3 are random.
 */
template<typename FieldT>
qap_witness<FieldT> r1cs_to_qap_witness_map(const r1cs_constraint_system<FieldT> &cs,
                                            const r1cs_primary_input<FieldT> &primary_input,
                                            const r1cs_auxiliary_input<FieldT> &auxiliary_input,
                                            const FieldT &d1,
                                            const FieldT &d2,
                                            const FieldT &d3);

} // libsnark

#include "reductions/r1cs_to_qap/r1cs_to_qap.tcc"

#endif // R1CS_TO_QAP_HPP_
