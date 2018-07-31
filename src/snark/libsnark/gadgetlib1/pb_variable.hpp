/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef PB_VARIABLE_HPP_
#define PB_VARIABLE_HPP_

#include <cstddef>
#include <string>
#include <vector>
#include "common/utils.hpp"
#include "relations/variable.hpp"

namespace libsnark {

typedef size_t lc_index_t;

template<typename FieldT>
class protoboard;

template<typename FieldT>
class pb_variable : public variable<FieldT> {
public:
    pb_variable(const var_index_t index = 0) : variable<FieldT>(index) {};

    void allocate(protoboard<FieldT> &pb, const std::string &annotation="");
};

template<typename FieldT>
class pb_variable_array : private std::vector<pb_variable<FieldT> >
{
    typedef std::vector<pb_variable<FieldT> > contents;
public:
    using typename contents::iterator;
    using typename contents::const_iterator;
    using typename contents::reverse_iterator;
    using typename contents::const_reverse_iterator;

    using contents::begin;
    using contents::end;
    using contents::rbegin;
    using contents::rend;
    using contents::emplace_back;
    using contents::insert;
    using contents::reserve;
    using contents::size;
    using contents::empty;
    using contents::operator[];
    using contents::resize;

    pb_variable_array() : contents() {};
    pb_variable_array(size_t count, const pb_variable<FieldT> &value) : contents(count, value) {};
    pb_variable_array(typename contents::const_iterator first, typename contents::const_iterator last) : contents(first, last) {};
    pb_variable_array(typename contents::const_reverse_iterator first, typename contents::const_reverse_iterator last) : contents(first, last) {};
    void allocate(protoboard<FieldT> &pb, const size_t n, const std::string &annotation_prefix="");

    void fill_with_field_elements(protoboard<FieldT> &pb, const std::vector<FieldT>& vals) const;
    void fill_with_bits(protoboard<FieldT> &pb, const bit_vector& bits) const;
    void fill_with_bits_of_ulong(protoboard<FieldT> &pb, const uint64_t i) const;
    void fill_with_bits_of_field_element(protoboard<FieldT> &pb, const FieldT &r) const;

    std::vector<FieldT> get_vals(const protoboard<FieldT> &pb) const;
    bit_vector get_bits(const protoboard<FieldT> &pb) const;

    FieldT get_field_element_from_bits(const protoboard<FieldT> &pb) const;
};

/* index 0 corresponds to the constant term (used in legacy code) */
#define ONE pb_variable<FieldT>(0)

template<typename FieldT>
class pb_linear_combination : public linear_combination<FieldT> {
public:
    bool is_variable;
    lc_index_t index;

    pb_linear_combination();
    pb_linear_combination(const pb_variable<FieldT> &var);

    void assign(protoboard<FieldT> &pb, const linear_combination<FieldT> &lc);
    void evaluate(protoboard<FieldT> &pb) const;

    bool is_constant() const;
    FieldT constant_term() const;
};

template<typename FieldT>
class pb_linear_combination_array : private std::vector<pb_linear_combination<FieldT> >
{
    typedef std::vector<pb_linear_combination<FieldT> > contents;
public:
    using typename contents::iterator;
    using typename contents::const_iterator;
    using typename contents::reverse_iterator;
    using typename contents::const_reverse_iterator;

    using contents::begin;
    using contents::end;
    using contents::rbegin;
    using contents::rend;
    using contents::emplace_back;
    using contents::insert;
    using contents::reserve;
    using contents::size;
    using contents::empty;
    using contents::operator[];
    using contents::resize;

    pb_linear_combination_array() : contents() {};
    pb_linear_combination_array(const pb_variable_array<FieldT> &arr) { for (auto &v : arr) this->emplace_back(pb_linear_combination<FieldT>(v)); };
    pb_linear_combination_array(size_t count) : contents(count) {};
    pb_linear_combination_array(size_t count, const pb_linear_combination<FieldT> &value) : contents(count, value) {};
    pb_linear_combination_array(typename contents::const_iterator first, typename contents::const_iterator last) : contents(first, last) {};
    pb_linear_combination_array(typename contents::const_reverse_iterator first, typename contents::const_reverse_iterator last) : contents(first, last) {};

    void evaluate(protoboard<FieldT> &pb) const;

    void fill_with_field_elements(protoboard<FieldT> &pb, const std::vector<FieldT>& vals) const;
    void fill_with_bits(protoboard<FieldT> &pb, const bit_vector& bits) const;
    void fill_with_bits_of_ulong(protoboard<FieldT> &pb, const uint64_t i) const;
    void fill_with_bits_of_field_element(protoboard<FieldT> &pb, const FieldT &r) const;

    std::vector<FieldT> get_vals(const protoboard<FieldT> &pb) const;
    bit_vector get_bits(const protoboard<FieldT> &pb) const;

    FieldT get_field_element_from_bits(const protoboard<FieldT> &pb) const;
};

template<typename FieldT>
linear_combination<FieldT> pb_sum(const pb_linear_combination_array<FieldT> &v);

template<typename FieldT>
linear_combination<FieldT> pb_packing_sum(const pb_linear_combination_array<FieldT> &v);

template<typename FieldT>
linear_combination<FieldT> pb_coeff_sum(const pb_linear_combination_array<FieldT> &v, const std::vector<FieldT> &coeffs);

} // libsnark
#include "gadgetlib1/pb_variable.tcc"

#endif // PB_VARIABLE_HPP_
