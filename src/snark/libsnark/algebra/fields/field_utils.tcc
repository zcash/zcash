/** @file
 *****************************************************************************
 Implementation of misc. math and serialization utility functions
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef FIELD_UTILS_TCC_
#define FIELD_UTILS_TCC_

#include "common/utils.hpp"
#include "common/assert_except.hpp"

namespace libsnark {

template<typename FieldT>
FieldT coset_shift()
{
    return FieldT::multiplicative_generator.squared();
}

template<typename FieldT>
FieldT get_root_of_unity(const uint64_t n)
{
    const uint64_t logn = log2(n);
    assert_except(n == (1u << logn));
    assert_except(logn <= FieldT::s);

    FieldT omega = FieldT::root_of_unity;
    for (uint64_t i = FieldT::s; i > logn; --i)
    {
        omega *= omega;
    }

    return omega;
}

template<typename FieldT>
std::vector<FieldT> pack_int_vector_into_field_element_vector(const std::vector<uint64_t> &v, const uint64_t w)
{
    const uint64_t chunk_bits = FieldT::capacity();
    const uint64_t repacked_size = div_ceil(v.size() * w, chunk_bits);
    std::vector<FieldT> result(repacked_size);

    for (uint64_t i = 0; i < repacked_size; ++i)
    {
        bigint<FieldT::num_limbs> b;
        for (uint64_t j = 0; j < chunk_bits; ++j)
        {
            const uint64_t word_index = (i * chunk_bits + j) / w;
            const uint64_t pos_in_word = (i * chunk_bits + j) % w;
            const uint64_t word_or_0 = (word_index < v.size() ? v[word_index] : 0);
            const uint64_t bit = (word_or_0 >> pos_in_word) & 1;

            b.data[j / GMP_NUMB_BITS] |= bit << (j % GMP_NUMB_BITS);
        }
        result[i] = FieldT(b);
    }

    return result;
}

template<typename FieldT>
std::vector<FieldT> pack_bit_vector_into_field_element_vector(const bit_vector &v, const uint64_t chunk_bits)
{
    assert_except(chunk_bits <= FieldT::capacity());

    const uint64_t repacked_size = div_ceil(v.size(), chunk_bits);
    std::vector<FieldT> result(repacked_size);

    for (size_t i = 0; i < repacked_size; ++i)
    {
        bigint<FieldT::num_limbs> b;
        for (size_t j = 0; j < chunk_bits; ++j)
        {
            b.data[j / GMP_NUMB_BITS] |= ((i * chunk_bits + j) < v.size() && v[i * chunk_bits + j] ? 1ll : 0ll) << (j % GMP_NUMB_BITS);
        }
        result[i] = FieldT(b);
    }

    return result;
}

template<typename FieldT>
std::vector<FieldT> pack_bit_vector_into_field_element_vector(const bit_vector &v)
{
    return pack_bit_vector_into_field_element_vector<FieldT>(v, FieldT::capacity());
}

template<typename FieldT>
std::vector<FieldT> convert_bit_vector_to_field_element_vector(const bit_vector &v)
{
    std::vector<FieldT> result;
    result.reserve(v.size());

    for (const bool b : v)
    {
        result.emplace_back(b ? FieldT::one() : FieldT::zero());
    }

    return result;
}

template<typename FieldT>
bit_vector convert_field_element_vector_to_bit_vector(const std::vector<FieldT> &v)
{
    bit_vector result;

    for (const FieldT &el : v)
    {
        const bit_vector el_bits = convert_field_element_to_bit_vector<FieldT>(el);
        result.insert(result.end(), el_bits.begin(), el_bits.end());
    }

    return result;
}

template<typename FieldT>
bit_vector convert_field_element_to_bit_vector(const FieldT &el)
{
    bit_vector result;

    bigint<FieldT::num_limbs> b = el.as_bigint();
    for (size_t i = 0; i < FieldT::size_in_bits(); ++i)
    {
        result.push_back(b.test_bit(i));
    }

    return result;
}

template<typename FieldT>
bit_vector convert_field_element_to_bit_vector(const FieldT &el, const uint64_t bitcount)
{
    bit_vector result = convert_field_element_to_bit_vector(el);
    result.resize(bitcount);

    return result;
}

template<typename FieldT>
FieldT convert_bit_vector_to_field_element(const bit_vector &v)
{
    assert_except(v.size() <= FieldT::size_in_bits());

    FieldT res = FieldT::zero();
    FieldT c = FieldT::one();
    for (bool b : v)
    {
        res += b ? c : FieldT::zero();
        c += c;
    }
    return res;
}

template<typename FieldT>
void batch_invert(std::vector<FieldT> &vec)
{
    std::vector<FieldT> prod;
    prod.reserve(vec.size());

    FieldT acc = FieldT::one();

    for (auto el : vec)
    {
        assert_except(!el.is_zero());
        prod.emplace_back(acc);
        acc = acc * el;
    }

    FieldT acc_inverse = acc.inverse();

    for (int64_t i = vec.size()-1; i >= 0; --i)
    {
        const FieldT old_el = vec[i];
        vec[i] = acc_inverse * prod[i];
        acc_inverse = acc_inverse * old_el;
    }
}

} // libsnark
#endif // FIELD_UTILS_TCC_
