/** @file
 *****************************************************************************

 Implementation of interfaces for an accumulation vector.

 See accumulation_vector.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ACCUMULATION_VECTOR_TCC_
#define ACCUMULATION_VECTOR_TCC_

namespace libsnark {

template<typename T>
bool accumulation_vector<T>::operator==(const accumulation_vector<T> &other) const
{
    return (this->first == other.first && this->rest == other.rest);
}

template<typename T>
bool accumulation_vector<T>::is_fully_accumulated() const
{
    return rest.empty();
}

template<typename T>
size_t accumulation_vector<T>::domain_size() const
{
    return rest.domain_size();
}

template<typename T>
size_t accumulation_vector<T>::size() const
{
    return rest.domain_size();
}

template<typename T>
size_t accumulation_vector<T>::size_in_bits() const
{
    const size_t first_size_in_bits = T::size_in_bits();
    const size_t rest_size_in_bits = rest.size_in_bits();
    return first_size_in_bits + rest_size_in_bits;
}

template<typename T>
template<typename FieldT>
accumulation_vector<T> accumulation_vector<T>::accumulate_chunk(const typename std::vector<FieldT>::const_iterator &it_begin,
                                                                const typename std::vector<FieldT>::const_iterator &it_end,
                                                                const size_t offset) const
{
    std::pair<T, sparse_vector<T> > acc_result = rest.template accumulate<FieldT>(it_begin, it_end, offset);
    T new_first = first + acc_result.first;
    return accumulation_vector<T>(std::move(new_first), std::move(acc_result.second));
}

template<typename T>
std::ostream& operator<<(std::ostream& out, const accumulation_vector<T> &v)
{
    out << v.first << OUTPUT_NEWLINE;
    out << v.rest << OUTPUT_NEWLINE;

    return out;
}

template<typename T>
std::istream& operator>>(std::istream& in, accumulation_vector<T> &v)
{
    in >> v.first;
    consume_OUTPUT_NEWLINE(in);
    in >> v.rest;
    consume_OUTPUT_NEWLINE(in);

    return in;
}

} // libsnark

#endif // ACCUMULATION_VECTOR_TCC_
