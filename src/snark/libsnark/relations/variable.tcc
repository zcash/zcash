/** @file
 *****************************************************************************

 Implementation of interfaces for:
 - a variable (i.e., x_i),
 - a linear term (i.e., a_i * x_i), and
 - a linear combination (i.e., sum_i a_i * x_i).

 See variabe.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef VARIABLE_TCC_
#define VARIABLE_TCC_

#include <algorithm>
#include <cassert>

#include "algebra/fields/bigint.hpp"

namespace libsnark {

template<typename FieldT>
linear_term<FieldT> variable<FieldT>::operator*(const integer_coeff_t int_coeff) const
{
    return linear_term<FieldT>(*this, int_coeff);
}

template<typename FieldT>
linear_term<FieldT> variable<FieldT>::operator*(const FieldT &field_coeff) const
{
    return linear_term<FieldT>(*this, field_coeff);
}

template<typename FieldT>
linear_combination<FieldT> variable<FieldT>::operator+(const linear_combination<FieldT> &other) const
{
    linear_combination<FieldT> result;

    result.add_term(*this);
    result.terms.insert(result.terms.begin(), other.terms.begin(), other.terms.end());

    return result;
}

template<typename FieldT>
linear_combination<FieldT> variable<FieldT>::operator-(const linear_combination<FieldT> &other) const
{
    return (*this) + (-other);
}

template<typename FieldT>
linear_term<FieldT> variable<FieldT>::operator-() const
{
    return linear_term<FieldT>(*this, -FieldT::one());
}

template<typename FieldT>
bool variable<FieldT>::operator==(const variable<FieldT> &other) const
{
    return (this->index == other.index);
}

template<typename FieldT>
linear_term<FieldT> operator*(const integer_coeff_t int_coeff, const variable<FieldT> &var)
{
    return linear_term<FieldT>(var, int_coeff);
}

template<typename FieldT>
linear_term<FieldT> operator*(const FieldT &field_coeff, const variable<FieldT> &var)
{
    return linear_term<FieldT>(var, field_coeff);
}

template<typename FieldT>
linear_combination<FieldT> operator+(const integer_coeff_t int_coeff, const variable<FieldT> &var)
{
    return linear_combination<FieldT>(int_coeff) + var;
}

template<typename FieldT>
linear_combination<FieldT> operator+(const FieldT &field_coeff, const variable<FieldT> &var)
{
    return linear_combination<FieldT>(field_coeff) + var;
}

template<typename FieldT>
linear_combination<FieldT> operator-(const integer_coeff_t int_coeff, const variable<FieldT> &var)
{
    return linear_combination<FieldT>(int_coeff) - var;
}

template<typename FieldT>
linear_combination<FieldT> operator-(const FieldT &field_coeff, const variable<FieldT> &var)
{
    return linear_combination<FieldT>(field_coeff) - var;
}

template<typename FieldT>
linear_term<FieldT>::linear_term(const variable<FieldT> &var) :
    index(var.index), coeff(FieldT::one())
{
}

template<typename FieldT>
linear_term<FieldT>::linear_term(const variable<FieldT> &var, const integer_coeff_t int_coeff) :
    index(var.index), coeff(FieldT(int_coeff))
{
}

template<typename FieldT>
linear_term<FieldT>::linear_term(const variable<FieldT> &var, const FieldT &coeff) :
    index(var.index), coeff(coeff)
{
}

template<typename FieldT>
linear_term<FieldT> linear_term<FieldT>::operator*(const integer_coeff_t int_coeff) const
{
    return (this->operator*(FieldT(int_coeff)));
}

template<typename FieldT>
linear_term<FieldT> linear_term<FieldT>::operator*(const FieldT &field_coeff) const
{
    return linear_term<FieldT>(this->index, field_coeff * this->coeff);
}

template<typename FieldT>
linear_combination<FieldT> operator+(const integer_coeff_t int_coeff, const linear_term<FieldT> &lt)
{
    return linear_combination<FieldT>(int_coeff) + lt;
}

template<typename FieldT>
linear_combination<FieldT> operator+(const FieldT &field_coeff, const linear_term<FieldT> &lt)
{
    return linear_combination<FieldT>(field_coeff) + lt;
}

template<typename FieldT>
linear_combination<FieldT> operator-(const integer_coeff_t int_coeff, const linear_term<FieldT> &lt)
{
    return linear_combination<FieldT>(int_coeff) - lt;
}

template<typename FieldT>
linear_combination<FieldT> operator-(const FieldT &field_coeff, const linear_term<FieldT> &lt)
{
    return linear_combination<FieldT>(field_coeff) - lt;
}

template<typename FieldT>
linear_combination<FieldT> linear_term<FieldT>::operator+(const linear_combination<FieldT> &other) const
{
    return linear_combination<FieldT>(*this) + other;
}

template<typename FieldT>
linear_combination<FieldT> linear_term<FieldT>::operator-(const linear_combination<FieldT> &other) const
{
    return (*this) + (-other);
}

template<typename FieldT>
linear_term<FieldT> linear_term<FieldT>::operator-() const
{
    return linear_term<FieldT>(this->index, -this->coeff);
}

template<typename FieldT>
bool linear_term<FieldT>::operator==(const linear_term<FieldT> &other) const
{
    return (this->index == other.index &&
            this->coeff == other.coeff);
}

template<typename FieldT>
linear_term<FieldT> operator*(const integer_coeff_t int_coeff, const linear_term<FieldT> &lt)
{
    return FieldT(int_coeff) * lt;
}

template<typename FieldT>
linear_term<FieldT> operator*(const FieldT &field_coeff, const linear_term<FieldT> &lt)
{
    return linear_term<FieldT>(lt.index, field_coeff * lt.coeff);
}

template<typename FieldT>
linear_combination<FieldT>::linear_combination(const integer_coeff_t int_coeff)
{
    this->add_term(linear_term<FieldT>(0, int_coeff));
}

template<typename FieldT>
linear_combination<FieldT>::linear_combination(const FieldT &field_coeff)
{
    this->add_term(linear_term<FieldT>(0, field_coeff));
}

template<typename FieldT>
linear_combination<FieldT>::linear_combination(const variable<FieldT> &var)
{
    this->add_term(var);
}

template<typename FieldT>
linear_combination<FieldT>::linear_combination(const linear_term<FieldT> &lt)
{
    this->add_term(lt);
}

template<typename FieldT>
typename std::vector<linear_term<FieldT> >::const_iterator linear_combination<FieldT>::begin() const
{
    return terms.begin();
}

template<typename FieldT>
typename std::vector<linear_term<FieldT> >::const_iterator linear_combination<FieldT>::end() const
{
    return terms.end();
}

template<typename FieldT>
void linear_combination<FieldT>::add_term(const variable<FieldT> &var)
{
    this->terms.emplace_back(linear_term<FieldT>(var.index, FieldT::one()));
}

template<typename FieldT>
void linear_combination<FieldT>::add_term(const variable<FieldT> &var, const integer_coeff_t int_coeff)
{
    this->terms.emplace_back(linear_term<FieldT>(var.index, int_coeff));
}

template<typename FieldT>
void linear_combination<FieldT>::add_term(const variable<FieldT> &var, const FieldT &coeff)
{
    this->terms.emplace_back(linear_term<FieldT>(var.index, coeff));
}

template<typename FieldT>
void linear_combination<FieldT>::add_term(const linear_term<FieldT> &other)
{
    this->terms.emplace_back(other);
}

template<typename FieldT>
linear_combination<FieldT> linear_combination<FieldT>::operator*(const integer_coeff_t int_coeff) const
{
    return (*this) * FieldT(int_coeff);
}

template<typename FieldT>
FieldT linear_combination<FieldT>::evaluate(const std::vector<FieldT> &assignment) const
{
    FieldT acc = FieldT::zero();
    for (auto &lt : terms)
    {
        acc += (lt.index == 0 ? FieldT::one() : assignment[lt.index-1]) * lt.coeff;
    }
    return acc;
}

template<typename FieldT>
linear_combination<FieldT> linear_combination<FieldT>::operator*(const FieldT &field_coeff) const
{
    linear_combination<FieldT> result;
    result.terms.reserve(this->terms.size());
    for (const linear_term<FieldT> &lt : this->terms)
    {
        result.terms.emplace_back(lt * field_coeff);
    }
    return result;
}

template<typename FieldT>
linear_combination<FieldT> linear_combination<FieldT>::operator+(const linear_combination<FieldT> &other) const
{
    linear_combination<FieldT> result;

    auto it1 = this->terms.begin();
    auto it2 = other.terms.begin();

    /* invariant: it1 and it2 always point to unprocessed items in the corresponding linear combinations */
    while (it1 != this->terms.end() && it2 != other.terms.end())
    {
        if (it1->index < it2->index)
        {
            result.terms.emplace_back(*it1);
            ++it1;
        }
        else if (it1->index > it2->index)
        {
            result.terms.emplace_back(*it2);
            ++it2;
        }
        else
        {
            /* it1->index == it2->index */
            result.terms.emplace_back(linear_term<FieldT>(variable<FieldT>(it1->index), it1->coeff + it2->coeff));
            ++it1;
            ++it2;
        }
    }

    if (it1 != this->terms.end())
    {
        result.terms.insert(result.terms.end(), it1, this->terms.end());
    }
    else
    {
        result.terms.insert(result.terms.end(), it2, other.terms.end());
    }

    return result;
}

template<typename FieldT>
linear_combination<FieldT> linear_combination<FieldT>::operator-(const linear_combination<FieldT> &other) const
{
    return (*this) + (-other);
}

template<typename FieldT>
linear_combination<FieldT> linear_combination<FieldT>::operator-() const
{
    return (*this) * (-FieldT::one());
}

template<typename FieldT>
bool linear_combination<FieldT>::operator==(const linear_combination<FieldT> &other) const
{
    return (this->terms == other.terms);
}

template<typename FieldT>
bool linear_combination<FieldT>::is_valid(const size_t num_variables) const
{
    /* check that all terms in linear combination are sorted */
    for (size_t i = 1; i < terms.size(); ++i)
    {
        if (terms[i-1].index >= terms[i].index)
        {
            return false;
        }
    }

    /* check that the variables are in proper range. as the variables
       are sorted, it suffices to check the last term */
    if ((--terms.end())->index >= num_variables)
    {
        return false;
    }

    return true;
}

template<typename FieldT>
void linear_combination<FieldT>::print(const std::map<size_t, std::string> &variable_annotations) const
{
    for (auto &lt : terms)
    {
        if (lt.index == 0)
        {
            printf("    1 * ");
            lt.coeff.print();
        }
        else
        {
            auto it = variable_annotations.find(lt.index);
            printf("    x_%zu (%s) * ", lt.index, (it == variable_annotations.end() ? "no annotation" : it->second.c_str()));
            lt.coeff.print();
        }
    }
}

template<typename FieldT>
void linear_combination<FieldT>::print_with_assignment(const std::vector<FieldT> &full_assignment, const std::map<size_t, std::string> &variable_annotations) const
{
    for (auto &lt : terms)
    {
        if (lt.index == 0)
        {
            printf("    1 * ");
            lt.coeff.print();
        }
        else
        {
            printf("    x_%zu * ", lt.index);
            lt.coeff.print();

            auto it = variable_annotations.find(lt.index);
            printf("    where x_%zu (%s) was assigned value ", lt.index,
                   (it == variable_annotations.end() ? "no annotation" : it->second.c_str()));
            full_assignment[lt.index-1].print();
            printf("      i.e. negative of ");
            (-full_assignment[lt.index-1]).print();
        }
    }
}

template<typename FieldT>
std::ostream& operator<<(std::ostream &out, const linear_combination<FieldT> &lc)
{
    out << lc.terms.size() << "\n";
    for (const linear_term<FieldT>& lt : lc.terms)
    {
        out << lt.index << "\n";
        out << lt.coeff << OUTPUT_NEWLINE;
    }

    return out;
}

template<typename FieldT>
std::istream& operator>>(std::istream &in, linear_combination<FieldT> &lc)
{
    lc.terms.clear();

    size_t s;
    in >> s;

    consume_newline(in);

    lc.terms.reserve(s);

    for (size_t i = 0; i < s; ++i)
    {
        linear_term<FieldT> lt;
        in >> lt.index;
        consume_newline(in);
        in >> lt.coeff;
        consume_OUTPUT_NEWLINE(in);
        lc.terms.emplace_back(lt);
    }

    return in;
}

template<typename FieldT>
linear_combination<FieldT> operator*(const integer_coeff_t int_coeff, const linear_combination<FieldT> &lc)
{
    return lc * int_coeff;
}

template<typename FieldT>
linear_combination<FieldT> operator*(const FieldT &field_coeff, const linear_combination<FieldT> &lc)
{
    return lc * field_coeff;
}

template<typename FieldT>
linear_combination<FieldT> operator+(const integer_coeff_t int_coeff, const linear_combination<FieldT> &lc)
{
    return linear_combination<FieldT>(int_coeff) + lc;
}

template<typename FieldT>
linear_combination<FieldT> operator+(const FieldT &field_coeff, const linear_combination<FieldT> &lc)
{
    return linear_combination<FieldT>(field_coeff) + lc;
}

template<typename FieldT>
linear_combination<FieldT> operator-(const integer_coeff_t int_coeff, const linear_combination<FieldT> &lc)
{
    return linear_combination<FieldT>(int_coeff) - lc;
}

template<typename FieldT>
linear_combination<FieldT> operator-(const FieldT &field_coeff, const linear_combination<FieldT> &lc)
{
    return linear_combination<FieldT>(field_coeff) - lc;
}

template<typename FieldT>
linear_combination<FieldT>::linear_combination(const std::vector<linear_term<FieldT> > &all_terms)
{
    if (all_terms.empty())
    {
        return;
    }

    terms = all_terms;
    std::sort(terms.begin(), terms.end(), [](linear_term<FieldT> a, linear_term<FieldT> b) { return a.index < b.index; });

    auto result_it = terms.begin();
    for (auto it = ++terms.begin(); it != terms.end(); ++it)
    {
        if (it->index == result_it->index)
        {
            result_it->coeff += it->coeff;
        }
        else
        {
            *(++result_it) = *it;
        }
    }
    terms.resize((result_it - terms.begin()) + 1);
}

} // libsnark

#endif // VARIABLE_TCC
