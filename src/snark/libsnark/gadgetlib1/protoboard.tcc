/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef PROTOBOARD_TCC_
#define PROTOBOARD_TCC_

#include <cstdio>
#include <cstdarg>
#include "common/profiling.hpp"

namespace libsnark {

template<typename FieldT>
protoboard<FieldT>::protoboard()
{
    constant_term = FieldT::one();

#ifdef DEBUG
    constraint_system.variable_annotations[0] = "ONE";
#endif

    next_free_var = 1; /* to account for constant 1 term */
    next_free_lc = 0;
}

template<typename FieldT>
void protoboard<FieldT>::clear_values()
{
    std::fill(values.begin(), values.end(), FieldT::zero());
}

template<typename FieldT>
var_index_t protoboard<FieldT>::allocate_var_index(const std::string &annotation)
{
#ifdef DEBUG
    assert(annotation != "");
    constraint_system.variable_annotations[next_free_var] = annotation;
#else
    UNUSED(annotation);
#endif
    ++constraint_system.auxiliary_input_size;
    values.emplace_back(FieldT::zero());
    return next_free_var++;
}

template<typename FieldT>
lc_index_t protoboard<FieldT>::allocate_lc_index()
{
    lc_values.emplace_back(FieldT::zero());
    return next_free_lc++;
}

template<typename FieldT>
FieldT& protoboard<FieldT>::val(const pb_variable<FieldT> &var)
{
    assert(var.index <= values.size());
    return (var.index == 0 ? constant_term : values[var.index-1]);
}

template<typename FieldT>
FieldT protoboard<FieldT>::val(const pb_variable<FieldT> &var) const
{
    assert(var.index <= values.size());
    return (var.index == 0 ? constant_term : values[var.index-1]);
}

template<typename FieldT>
FieldT& protoboard<FieldT>::lc_val(const pb_linear_combination<FieldT> &lc)
{
    if (lc.is_variable)
    {
        return this->val(pb_variable<FieldT>(lc.index));
    }
    else
    {
        assert(lc.index < lc_values.size());
        return lc_values[lc.index];
    }
}

template<typename FieldT>
FieldT protoboard<FieldT>::lc_val(const pb_linear_combination<FieldT> &lc) const
{
    if (lc.is_variable)
    {
        return this->val(pb_variable<FieldT>(lc.index));
    }
    else
    {
        assert(lc.index < lc_values.size());
        return lc_values[lc.index];
    }
}

template<typename FieldT>
void protoboard<FieldT>::add_r1cs_constraint(const r1cs_constraint<FieldT> &constr, const std::string &annotation)
{
#ifdef DEBUG
    assert(annotation != "");
    constraint_system.constraint_annotations[constraint_system.constraints.size()] = annotation;
#else
    UNUSED(annotation);
#endif
    constraint_system.constraints.emplace_back(constr);
}

template<typename FieldT>
void protoboard<FieldT>::augment_variable_annotation(const pb_variable<FieldT> &v, const std::string &postfix)
{
#ifdef DEBUG
    auto it = constraint_system.variable_annotations.find(v.index);
    constraint_system.variable_annotations[v.index] = (it == constraint_system.variable_annotations.end() ? "" : it->second + " ") + postfix;
#endif
}

template<typename FieldT>
bool protoboard<FieldT>::is_satisfied() const
{
    return constraint_system.is_satisfied(primary_input(), auxiliary_input());
}

template<typename FieldT>
void protoboard<FieldT>::dump_variables() const
{
#ifdef DEBUG
    for (size_t i = 0; i < constraint_system.num_variables; ++i)
    {
        printf("%-40s --> ", constraint_system.variable_annotations[i].c_str());
        values[i].as_bigint().print_hex();
    }
#endif
}

template<typename FieldT>
size_t protoboard<FieldT>::num_constraints() const
{
    return constraint_system.num_constraints();
}

template<typename FieldT>
size_t protoboard<FieldT>::num_inputs() const
{
    return constraint_system.num_inputs();
}

template<typename FieldT>
size_t protoboard<FieldT>::num_variables() const
{
    return next_free_var - 1;
}

template<typename FieldT>
void protoboard<FieldT>::set_input_sizes(const size_t primary_input_size)
{
    assert(primary_input_size <= num_variables());
    constraint_system.primary_input_size = primary_input_size;
    constraint_system.auxiliary_input_size = num_variables() - primary_input_size;
}

template<typename FieldT>
r1cs_variable_assignment<FieldT> protoboard<FieldT>::full_variable_assignment() const
{
    return values;
}

template<typename FieldT>
r1cs_primary_input<FieldT> protoboard<FieldT>::primary_input() const
{
    return r1cs_primary_input<FieldT>(values.begin(), values.begin() + num_inputs());
}

template<typename FieldT>
r1cs_auxiliary_input<FieldT> protoboard<FieldT>::auxiliary_input() const
{
    return r1cs_primary_input<FieldT>(values.begin() + num_inputs(), values.end());
}

template<typename FieldT>
r1cs_constraint_system<FieldT> protoboard<FieldT>::get_constraint_system() const
{
    return constraint_system;
}

} // libsnark
#endif // PROTOBOARD_TCC_
