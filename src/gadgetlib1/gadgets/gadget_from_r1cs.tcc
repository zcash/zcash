/** @file
 *****************************************************************************

 Implementation of interfaces for a gadget that can be created from an R1CS constraint system.

 See gadget_from_r1cs.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef GADGET_FROM_R1CS_TCC_
#define GADGET_FROM_R1CS_TCC_

namespace libsnark {

template<typename FieldT>
gadget_from_r1cs<FieldT>::gadget_from_r1cs(protoboard<FieldT> &pb,
                                           const std::vector<pb_variable_array<FieldT> > &vars,
                                           const r1cs_constraint_system<FieldT> &cs,
                                           const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    vars(vars),
    cs(cs)
{
    cs_to_vars[0] = 0; /* constant term maps to constant term */

    size_t cs_var_idx = 1;
    for (auto va : vars)
    {
#ifdef DEBUG
        printf("gadget_from_r1cs: translating a block of variables with length %zu\n", va.size());
#endif
        for (auto v : va)
        {
            cs_to_vars[cs_var_idx] = v.index;

#ifdef DEBUG
            if (v.index != 0)
            {
                // handle annotations, except for re-annotating constant term
                const std::map<size_t, std::string>::const_iterator it = cs.variable_annotations.find(cs_var_idx);

                std::string annotation = FMT(annotation_prefix, " variable_%zu", cs_var_idx);
                if (it != cs.variable_annotations.end())
                {
                    annotation = annotation_prefix + " " + it->second;
                }

                pb.augment_variable_annotation(v, annotation);
            }
#endif
            ++cs_var_idx;
        }
    }

#ifdef DEBUG
    printf("gadget_from_r1cs: sum of all block lengths: %zu\n", cs_var_idx-1);
    printf("gadget_from_r1cs: cs.num_variables(): %zu\n", cs.num_variables());
#endif

    assert(cs_var_idx - 1 == cs.num_variables());
}

template<typename FieldT>
void gadget_from_r1cs<FieldT>::generate_r1cs_constraints()
{
    for (size_t i = 0; i < cs.num_constraints(); ++i)
    {
        const r1cs_constraint<FieldT> &constr = cs.constraints[i];
        r1cs_constraint<FieldT> translated_constr;

        for (const linear_term<FieldT> &t: constr.a.terms)
        {
            translated_constr.a.terms.emplace_back(linear_term<FieldT>(pb_variable<FieldT>(cs_to_vars[t.index]), t.coeff));
        }

        for (const linear_term<FieldT> &t: constr.b.terms)
        {
            translated_constr.b.terms.emplace_back(linear_term<FieldT>(pb_variable<FieldT>(cs_to_vars[t.index]), t.coeff));
        }

        for (const linear_term<FieldT> &t: constr.c.terms)
        {
            translated_constr.c.terms.emplace_back(linear_term<FieldT>(pb_variable<FieldT>(cs_to_vars[t.index]), t.coeff));
        }

        std::string annotation = FMT(this->annotation_prefix, " constraint_%zu", i);

#ifdef DEBUG
        auto it = cs.constraint_annotations.find(i);
        if (it != cs.constraint_annotations.end())
        {
            annotation = this->annotation_prefix + " " + it->second;
        }
#endif
        this->pb.add_r1cs_constraint(translated_constr, annotation);
    }
}

template<typename FieldT>
void gadget_from_r1cs<FieldT>::generate_r1cs_witness(const r1cs_primary_input<FieldT> &primary_input,
                                                     const r1cs_auxiliary_input<FieldT> &auxiliary_input)
{
    assert(cs.num_inputs() == primary_input.size());
    assert(cs.num_variables() == primary_input.size() + auxiliary_input.size());

    for (size_t i = 0; i < primary_input.size(); ++i)
    {
        this->pb.val(pb_variable<FieldT>(cs_to_vars[i+1])) = primary_input[i];
    }

    for (size_t i = 0; i < auxiliary_input.size(); ++i)
    {
        this->pb.val(pb_variable<FieldT>(cs_to_vars[primary_input.size()+i+1])) = auxiliary_input[i];
    }
}

} // libsnark

#endif // GADGET_FROM_R1CS_TCC_
