/** @file
 *****************************************************************************

 Declaration of interfaces for:
 - a variable (i.e., x_i),
 - a linear term (i.e., a_i * x_i), and
 - a linear combination (i.e., sum_i a_i * x_i).

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef VARIABLE_HPP_
#define VARIABLE_HPP_

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace libsnark {

/**
 * Mnemonic typedefs.
 */
typedef size_t var_index_t;
typedef int64_t integer_coeff_t;

/**
 * Forward declaration.
 */
template<typename FieldT>
class linear_term;

/**
 * Forward declaration.
 */
template<typename FieldT>
class linear_combination;

/********************************* Variable **********************************/

/**
 * A variable represents a formal expression of the form "x_{index}".
 */
template<typename FieldT>
class variable {
public:

    var_index_t index;

    variable(const var_index_t index = 0) : index(index) {};

    linear_term<FieldT> operator*(const integer_coeff_t int_coeff) const;
    linear_term<FieldT> operator*(const FieldT &field_coeff) const;

    linear_combination<FieldT> operator+(const linear_combination<FieldT> &other) const;
    linear_combination<FieldT> operator-(const linear_combination<FieldT> &other) const;

    linear_term<FieldT> operator-() const;

    bool operator==(const variable<FieldT> &other) const;
};

template<typename FieldT>
linear_term<FieldT> operator*(const integer_coeff_t int_coeff, const variable<FieldT> &var);

template<typename FieldT>
linear_term<FieldT> operator*(const FieldT &field_coeff, const variable<FieldT> &var);

template<typename FieldT>
linear_combination<FieldT> operator+(const integer_coeff_t int_coeff, const variable<FieldT> &var);

template<typename FieldT>
linear_combination<FieldT> operator+(const FieldT &field_coeff, const variable<FieldT> &var);

template<typename FieldT>
linear_combination<FieldT> operator-(const integer_coeff_t int_coeff, const variable<FieldT> &var);

template<typename FieldT>
linear_combination<FieldT> operator-(const FieldT &field_coeff, const variable<FieldT> &var);


/****************************** Linear term **********************************/

/**
 * A linear term represents a formal expression of the form "coeff * x_{index}".
 */
template<typename FieldT>
class linear_term {
public:

    var_index_t index = 0;
    FieldT coeff;

    linear_term() {};
    linear_term(const variable<FieldT> &var);
    linear_term(const variable<FieldT> &var, const integer_coeff_t int_coeff);
    linear_term(const variable<FieldT> &var, const FieldT &field_coeff);

    linear_term<FieldT> operator*(const integer_coeff_t int_coeff) const;
    linear_term<FieldT> operator*(const FieldT &field_coeff) const;

    linear_combination<FieldT> operator+(const linear_combination<FieldT> &other) const;
    linear_combination<FieldT> operator-(const linear_combination<FieldT> &other) const;

    linear_term<FieldT> operator-() const;

    bool operator==(const linear_term<FieldT> &other) const;
};

template<typename FieldT>
linear_term<FieldT> operator*(const integer_coeff_t int_coeff, const linear_term<FieldT> &lt);

template<typename FieldT>
linear_term<FieldT> operator*(const FieldT &field_coeff, const linear_term<FieldT> &lt);

template<typename FieldT>
linear_combination<FieldT> operator+(const integer_coeff_t int_coeff, const linear_term<FieldT> &lt);

template<typename FieldT>
linear_combination<FieldT> operator+(const FieldT &field_coeff, const linear_term<FieldT> &lt);

template<typename FieldT>
linear_combination<FieldT> operator-(const integer_coeff_t int_coeff, const linear_term<FieldT> &lt);

template<typename FieldT>
linear_combination<FieldT> operator-(const FieldT &field_coeff, const linear_term<FieldT> &lt);


/***************************** Linear combination ****************************/

template<typename FieldT>
class linear_combination;

template<typename FieldT>
std::ostream& operator<<(std::ostream &out, const linear_combination<FieldT> &lc);

template<typename FieldT>
std::istream& operator>>(std::istream &in, linear_combination<FieldT> &lc);

/**
 * A linear combination represents a formal expression of the form "sum_i coeff_i * x_{index_i}".
 */
template<typename FieldT>
class linear_combination {
public:

    std::vector<linear_term<FieldT> > terms;

    linear_combination() {};
    linear_combination(const integer_coeff_t int_coeff);
    linear_combination(const FieldT &field_coeff);
    linear_combination(const variable<FieldT> &var);
    linear_combination(const linear_term<FieldT> &lt);
    linear_combination(const std::vector<linear_term<FieldT> > &all_terms);

    /* for supporting range-based for loops over linear_combination */
    typename std::vector<linear_term<FieldT> >::const_iterator begin() const;
    typename std::vector<linear_term<FieldT> >::const_iterator end() const;

    void add_term(const variable<FieldT> &var);
    void add_term(const variable<FieldT> &var, const integer_coeff_t int_coeff);
    void add_term(const variable<FieldT> &var, const FieldT &field_coeff);

    void add_term(const linear_term<FieldT> &lt);

    FieldT evaluate(const std::vector<FieldT> &assignment) const;

    linear_combination<FieldT> operator*(const integer_coeff_t int_coeff) const;
    linear_combination<FieldT> operator*(const FieldT &field_coeff) const;

    linear_combination<FieldT> operator+(const linear_combination<FieldT> &other) const;

    linear_combination<FieldT> operator-(const linear_combination<FieldT> &other) const;
    linear_combination<FieldT> operator-() const;

    bool operator==(const linear_combination<FieldT> &other) const;

    bool is_valid(const size_t num_variables) const;

    void print(const std::map<size_t, std::string> &variable_annotations = std::map<size_t, std::string>()) const;
    void print_with_assignment(const std::vector<FieldT> &full_assignment, const std::map<size_t, std::string> &variable_annotations = std::map<size_t, std::string>()) const;

    friend std::ostream& operator<< <FieldT>(std::ostream &out, const linear_combination<FieldT> &lc);
    friend std::istream& operator>> <FieldT>(std::istream &in, linear_combination<FieldT> &lc);
};

template<typename FieldT>
linear_combination<FieldT> operator*(const integer_coeff_t int_coeff, const linear_combination<FieldT> &lc);

template<typename FieldT>
linear_combination<FieldT> operator*(const FieldT &field_coeff, const linear_combination<FieldT> &lc);

template<typename FieldT>
linear_combination<FieldT> operator+(const integer_coeff_t int_coeff, const linear_combination<FieldT> &lc);

template<typename FieldT>
linear_combination<FieldT> operator+(const FieldT &field_coeff, const linear_combination<FieldT> &lc);

template<typename FieldT>
linear_combination<FieldT> operator-(const integer_coeff_t int_coeff, const linear_combination<FieldT> &lc);

template<typename FieldT>
linear_combination<FieldT> operator-(const FieldT &field_coeff, const linear_combination<FieldT> &lc);

} // libsnark

#include "relations/variable.tcc"

#endif // VARIABLE_HPP_
