/** @file
 *****************************************************************************

 Declaration of interfaces for a sparse vector.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SPARSE_VECTOR_HPP_
#define SPARSE_VECTOR_HPP_

#include <vector>

namespace libsnark {

template<typename T>
struct sparse_vector;

template<typename T>
std::ostream& operator<<(std::ostream &out, const sparse_vector<T> &v);

template<typename T>
std::istream& operator>>(std::istream &in, sparse_vector<T> &v);

/**
 * A sparse vector is a list of indices along with corresponding values.
 * The indices are selected from the set {0,1,...,domain_size-1}.
 */
template<typename T>
struct sparse_vector {

    std::vector<uint64_t> indices;
    std::vector<T> values;
    uint64_t domain_size_ = 0;

    sparse_vector() = default;
    sparse_vector(const sparse_vector<T> &other) = default;
    sparse_vector(sparse_vector<T> &&other) = default;
    sparse_vector(std::vector<T> &&v); /* constructor from std::vector */

    sparse_vector<T>& operator=(const sparse_vector<T> &other) = default;
    sparse_vector<T>& operator=(sparse_vector<T> &&other) = default;

    T operator[](const uint64_t idx) const;

    bool operator==(const sparse_vector<T> &other) const;
    bool operator==(const std::vector<T> &other) const;

    bool is_valid() const;
    bool empty() const;

    uint64_t domain_size() const; // return domain_size_
    uint64_t size() const; // return the number of indices (representing the number of non-zero entries)
    uint64_t size_in_bits() const; // return the number bits needed to store the sparse vector

    /* return a pair consisting of the accumulated value and the sparse vector of non-accumuated values */
    template<typename FieldT>
    std::pair<T, sparse_vector<T> > accumulate(const typename std::vector<FieldT>::const_iterator &it_begin,
                                               const typename std::vector<FieldT>::const_iterator &it_end,
                                               const uint64_t offset) const;

    friend std::ostream& operator<< <T>(std::ostream &out, const sparse_vector<T> &v);
    friend std::istream& operator>> <T>(std::istream &in, sparse_vector<T> &v);
};

template<typename T>
std::ostream& operator<<(std::ostream& out, const sparse_vector<T> &v);

template<typename T>
std::istream& operator>>(std::istream& in, sparse_vector<T> &v);

} // libsnark

#include "common/data_structures/sparse_vector.tcc"

#endif // SPARSE_VECTOR_HPP_
