/** @file
 *****************************************************************************

 Declaration of interfaces for an accumulation vector.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ACCUMULATION_VECTOR_HPP_
#define ACCUMULATION_VECTOR_HPP_

#include "common/data_structures/sparse_vector.hpp"

namespace libsnark {

template<typename T>
class accumulation_vector;

template<typename T>
std::ostream& operator<<(std::ostream &out, const accumulation_vector<T> &v);

template<typename T>
std::istream& operator>>(std::istream &in, accumulation_vector<T> &v);

/**
 * An accumulation vector comprises an accumulation value and a sparse vector.
 * The method "accumulate_chunk" allows one to accumlate portions of the sparse
 * vector into the accumualation value.
 */
template<typename T>
class accumulation_vector {
public:
    T first;
    sparse_vector<T> rest;

    accumulation_vector() = default;
    accumulation_vector(const accumulation_vector<T> &other) = default;
    accumulation_vector(accumulation_vector<T> &&other) = default;
    accumulation_vector(T &&first, sparse_vector<T> &&rest) : first(std::move(first)), rest(std::move(rest)) {};
    accumulation_vector(T &&first, std::vector<T> &&v) : first(std::move(first)), rest(std::move(v)) {}
    accumulation_vector(std::vector<T> &&v) : first(T::zero()), rest(std::move(v)) {};

    accumulation_vector<T>& operator=(const accumulation_vector<T> &other) = default;
    accumulation_vector<T>& operator=(accumulation_vector<T> &&other) = default;

    bool operator==(const accumulation_vector<T> &other) const;

    bool is_fully_accumulated() const;

    size_t domain_size() const;
    size_t size() const;
    size_t size_in_bits() const;

    template<typename FieldT>
    accumulation_vector<T> accumulate_chunk(const typename std::vector<FieldT>::const_iterator &it_begin,
                                            const typename std::vector<FieldT>::const_iterator &it_end,
                                            const size_t offset) const;

};

template<typename T>
std::ostream& operator<<(std::ostream &out, const accumulation_vector<T> &v);

template<typename T>
std::istream& operator>>(std::istream &in, accumulation_vector<T> &v);

} // libsnark

#include "common/data_structures/accumulation_vector.tcc"

#endif // ACCUMULATION_VECTOR_HPP_
