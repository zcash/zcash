/** @file
 *****************************************************************************

 Declaration of interfaces for a gadget that can be created from an R1CS constraint system.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef GADGET_FROM_R1CS_HPP_
#define GADGET_FROM_R1CS_HPP_

#include <map>

#include "gadgetlib1/gadget.hpp"

namespace libsnark {

template<typename FieldT>
class gadget_from_r1cs : public gadget<FieldT> {

private:
    const std::vector<pb_variable_array<FieldT> > vars;
    const r1cs_constraint_system<FieldT> cs;
    std::map<size_t, size_t> cs_to_vars;

public:

    gadget_from_r1cs(protoboard<FieldT> &pb,
                     const std::vector<pb_variable_array<FieldT> > &vars,
                     const r1cs_constraint_system<FieldT> &cs,
                     const std::string &annotation_prefix);

    void generate_r1cs_constraints();
    void generate_r1cs_witness(const r1cs_primary_input<FieldT> &primary_input,
                               const r1cs_auxiliary_input<FieldT> &auxiliary_input);
};

} // libsnark

#include "gadgetlib1/gadgets/gadget_from_r1cs.tcc"

#endif // GADGET_FROM_R1CS_HPP_
