/** @file
 *****************************************************************************

 Implementation of interfaces for a sparse vector.

 See sparse_vector.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SPARSE_VECTOR_TCC_
#define SPARSE_VECTOR_TCC_

#include "algebra/scalar_multiplication/multiexp.hpp"

#include <numeric>

namespace libsnark {

template<typename T>
sparse_vector<T>::sparse_vector(std::vector<T> &&v) :
    values(std::move(v)), domain_size_(values.size())
{
    indices.resize(domain_size_);
    std::iota(indices.begin(), indices.end(), 0);
}

template<typename T>
T sparse_vector<T>::operator[](const size_t idx) const
{
    auto it = std::lower_bound(indices.begin(), indices.end(), idx);
    return (it != indices.end() && *it == idx) ? values[it - indices.begin()] : T();
}

template<typename T>
bool sparse_vector<T>::operator==(const sparse_vector<T> &other) const
{
    if (this->domain_size_ != other.domain_size_)
    {
        return false;
    }

    size_t this_pos = 0, other_pos = 0;
    while (this_pos < this->indices.size() && other_pos < other.indices.size())
    {
        if (this->indices[this_pos] == other.indices[other_pos])
        {
            if (this->values[this_pos] != other.values[other_pos])
            {
                return false;
            }
            ++this_pos;
            ++other_pos;
        }
        else if (this->indices[this_pos] < other.indices[other_pos])
        {
            if (!this->values[this_pos].is_zero())
            {
                return false;
            }
            ++this_pos;
        }
        else
        {
            if (!other.values[other_pos].is_zero())
            {
                return false;
            }
            ++other_pos;
        }
    }

    /* at least one of the vectors has been exhausted, so other must be empty */
    while (this_pos < this->indices.size())
    {
        if (!this->values[this_pos].is_zero())
        {
            return false;
        }
        ++this_pos;
    }

    while (other_pos < other.indices.size())
    {
        if (!other.values[other_pos].is_zero())
        {
            return false;
        }
        ++other_pos;
    }

    return true;
}

template<typename T>
bool sparse_vector<T>::operator==(const std::vector<T> &other) const
{
    if (this->domain_size_ < other.size())
    {
        return false;
    }

    size_t j = 0;
    for (size_t i = 0; i < other.size(); ++i)
    {
        if (this->indices[j] == i)
        {
            if (this->values[j] != other[j])
            {
                return false;
            }
            ++j;
        }
        else
        {
            if (!other[j].is_zero())
            {
                return false;
            }
        }
    }

    return true;
}

template<typename T>
bool sparse_vector<T>::is_valid() const
{
    if (values.size() == indices.size() && values.size() <= domain_size_)
    {
        return false;
    }

    for (size_t i = 0; i + 1 < indices.size(); ++i)
    {
        if (indices[i] >= indices[i+1])
        {
            return false;
        }
    }

    if (!indices.empty() && indices[indices.size()-1] >= domain_size_)
    {
        return false;
    }

    return true;
}

template<typename T>
bool sparse_vector<T>::empty() const
{
    return indices.empty();
}

template<typename T>
size_t sparse_vector<T>::domain_size() const
{
    return domain_size_;
}

template<typename T>
size_t sparse_vector<T>::size() const
{
    return indices.size();
}

template<typename T>
size_t sparse_vector<T>::size_in_bits() const
{
    return indices.size() * (sizeof(size_t) * 8 + T::size_in_bits());
}

template<typename T>
template<typename FieldT>
std::pair<T, sparse_vector<T> > sparse_vector<T>::accumulate(const typename std::vector<FieldT>::const_iterator &it_begin,
                                                             const typename std::vector<FieldT>::const_iterator &it_end,
                                                             const size_t offset) const
{
    // TODO: does not really belong here.
    const size_t chunks = 1;
    const bool use_multiexp = true;

    T accumulated_value = T::zero();
    sparse_vector<T> resulting_vector;
    resulting_vector.domain_size_ = domain_size_;

    const size_t range_len = it_end - it_begin;
    bool in_block = false;
    size_t first_pos = -1, last_pos = -1; // g++ -flto emits unitialized warning, even though in_block guards for such cases.

    for (size_t i = 0; i < indices.size(); ++i)
    {
        const bool matching_pos = (offset <= indices[i] && indices[i] < offset + range_len);
        // printf("i = %zu, pos[i] = %zu, offset = %zu, w_size = %zu\n", i, indices[i], offset, w_size);
        bool copy_over;

        if (in_block)
        {
            if (matching_pos && last_pos == i-1)
            {
                // block can be extended, do it
                last_pos = i;
                copy_over = false;
            }
            else
            {
                // block has ended here
                in_block = false;
                copy_over = true;

#ifdef DEBUG
                print_indent(); printf("doing multiexp for w_%zu ... w_%zu\n", indices[first_pos], indices[last_pos]);
#endif
                accumulated_value = accumulated_value + multi_exp<T, FieldT>(values.begin() + first_pos,
                                                                             values.begin() + last_pos + 1,
                                                                             it_begin + (indices[first_pos] - offset),
                                                                             it_begin + (indices[last_pos] - offset) + 1,
                                                                             chunks, use_multiexp);
            }
        }
        else
        {
            if (matching_pos)
            {
                // block can be started
                first_pos = i;
                last_pos = i;
                in_block = true;
                copy_over = false;
            }
            else
            {
                copy_over = true;
            }
        }

        if (copy_over)
        {
            resulting_vector.indices.emplace_back(indices[i]);
            resulting_vector.values.emplace_back(values[i]);
        }
    }

    if (in_block)
    {
#ifdef DEBUG
        print_indent(); printf("doing multiexp for w_%zu ... w_%zu\n", indices[first_pos], indices[last_pos]);
#endif
        accumulated_value = accumulated_value + multi_exp<T, FieldT>(values.begin() + first_pos,
                                                                     values.begin() + last_pos + 1,
                                                                     it_begin + (indices[first_pos] - offset),
                                                                     it_begin + (indices[last_pos] - offset) + 1,
                                                                     chunks, use_multiexp);
    }

    return std::make_pair(accumulated_value, resulting_vector);
}

template<typename T>
std::ostream& operator<<(std::ostream& out, const sparse_vector<T> &v)
{
    out << v.domain_size_ << "\n";
    out << v.indices.size() << "\n";
    for (const size_t& i : v.indices)
    {
        out << i << "\n";
    }

    out << v.values.size() << "\n";
    for (const T& t : v.values)
    {
        out << t << OUTPUT_NEWLINE;
    }

    return out;
}

template<typename T>
std::istream& operator>>(std::istream& in, sparse_vector<T> &v)
{
    in >> v.domain_size_;
    consume_newline(in);

    size_t s;
    in >> s;
    consume_newline(in);
    v.indices.resize(s);
    for (size_t i = 0; i < s; ++i)
    {
        in >> v.indices[i];
        consume_newline(in);
    }

    v.values.clear();
    in >> s;
    consume_newline(in);
    v.values.reserve(s);

    for (size_t i = 0; i < s; ++i)
    {
        T t;
        in >> t;
        consume_OUTPUT_NEWLINE(in);
        v.values.emplace_back(t);
    }

    return in;
}

} // libsnark

#endif // SPARSE_VECTOR_TCC_
