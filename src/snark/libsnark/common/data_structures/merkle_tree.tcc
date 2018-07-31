/** @file
 *****************************************************************************

 Implementation of interfaces for Merkle tree.

 See merkle_tree.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MERKLE_TREE_TCC
#define MERKLE_TREE_TCC

#include <algorithm>

#include "common/profiling.hpp"
#include "common/utils.hpp"

namespace libsnark {

template<typename HashT>
typename HashT::hash_value_type two_to_one_CRH(const typename HashT::hash_value_type &l,
                                               const typename HashT::hash_value_type &r)
{
    typename HashT::hash_value_type new_input;
    new_input.insert(new_input.end(), l.begin(), l.end());
    new_input.insert(new_input.end(), r.begin(), r.end());

    const size_t digest_size = HashT::get_digest_len();
    assert(l.size() == digest_size);
    assert(r.size() == digest_size);

    return HashT::get_hash(new_input);
}

template<typename HashT>
merkle_tree<HashT>::merkle_tree(const size_t depth, const size_t value_size) :
    depth(depth), value_size(value_size)
{
    assert(depth < sizeof(size_t) * 8);

    digest_size = HashT::get_digest_len();
    assert(value_size <= digest_size);

    hash_value_type last(digest_size);
    hash_defaults.reserve(depth+1);
    hash_defaults.emplace_back(last);
    for (size_t i = 0; i < depth; ++i)
    {
        last = two_to_one_CRH<HashT>(last, last);
        hash_defaults.emplace_back(last);
    }

    std::reverse(hash_defaults.begin(), hash_defaults.end());
}

template<typename HashT>
merkle_tree<HashT>::merkle_tree(const size_t depth,
                                const size_t value_size,
                                const std::vector<bit_vector> &contents_as_vector) :
    merkle_tree<HashT>(depth, value_size)
{
    assert(log2(contents_as_vector.size()) <= depth);
    for (size_t address = 0; address < contents_as_vector.size(); ++address)
    {
        const size_t idx = address + (UINT64_C(1)<<depth) - 1;
        values[idx] = contents_as_vector[address];
        hashes[idx] = contents_as_vector[address];
        hashes[idx].resize(digest_size);
    }

    size_t idx_begin = (UINT64_C(1)<<depth) - 1;
    size_t idx_end = contents_as_vector.size() + ((UINT64_C(1)<<depth) - 1);

    for (int layer = depth; layer > 0; --layer)
    {
        for (size_t idx = idx_begin; idx < idx_end; idx += 2)
        {
            hash_value_type l = hashes[idx]; // this is sound, because idx_begin is always a left child
            hash_value_type r = (idx + 1 < idx_end ? hashes[idx+1] : hash_defaults[layer]);

            hash_value_type h = two_to_one_CRH<HashT>(l, r);
            hashes[(idx-1)/2] = h;
        }

        idx_begin = (idx_begin-1)/2;
        idx_end = (idx_end-1)/2;
    }
}

template<typename HashT>
merkle_tree<HashT>::merkle_tree(const size_t depth,
                                const size_t value_size,
                                const std::map<size_t, bit_vector> &contents) :
    merkle_tree<HashT>(depth, value_size)
{

    if (!contents.empty())
    {
        assert(contents.rbegin()->first < UINT64_C(1)<<depth);

        for (auto it = contents.begin(); it != contents.end(); ++it)
        {
            const size_t address = it->first;
            const bit_vector value = it->second;
            const size_t idx = address + (UINT64_C(1)<<depth) - 1;

            values[address] = value;
            hashes[idx] = value;
            hashes[idx].resize(digest_size);
        }

        auto last_it = hashes.end();

        for (int layer = depth; layer > 0; --layer)
        {
            auto next_last_it = hashes.begin();

            for (auto it = hashes.begin(); it != last_it; ++it)
            {
                const size_t idx = it->first;
                const hash_value_type hash = it->second;

                if (idx % 2 == 0)
                {
                    // this is the right child of its parent and by invariant we are missing the left child
                    hashes[(idx-1)/2] = two_to_one_CRH<HashT>(hash_defaults[layer], hash);
                }
                else
                {
                    if (std::next(it) == last_it || std::next(it)->first != idx + 1)
                    {
                        // this is the left child of its parent and is missing its right child
                        hashes[(idx-1)/2] = two_to_one_CRH<HashT>(hash, hash_defaults[layer]);
                    }
                    else
                    {
                        // typical case: this is the left child of the parent and adjecent to it there is a right child
                        hashes[(idx-1)/2] = two_to_one_CRH<HashT>(hash, std::next(it)->second);
                        ++it;
                    }
                }
            }

            last_it = next_last_it;
        }
    }
}

template<typename HashT>
bit_vector merkle_tree<HashT>::get_value(const size_t address) const
{
    assert(log2(address) <= depth);

    auto it = values.find(address);
    bit_vector padded_result = (it == values.end() ? bit_vector(digest_size) : it->second);
    padded_result.resize(value_size);

    return padded_result;
}

template<typename HashT>
void merkle_tree<HashT>::set_value(const size_t address,
                                   const bit_vector &value)
{
    assert(log2(address) <= depth);
    size_t idx = address + (UINT64_C(1)<<depth) - 1;

    assert(value.size() == value_size);
    values[address] = value;
    hashes[idx] = value;
    hashes[idx].resize(digest_size);

    for (int layer = depth-1; layer >=0; --layer)
    {
        idx = (idx-1)/2;

        auto it = hashes.find(2*idx+1);
        hash_value_type l = (it == hashes.end() ? hash_defaults[layer+1] : it->second);

        it = hashes.find(2*idx+2);
        hash_value_type r = (it == hashes.end() ? hash_defaults[layer+1] : it->second);

        hash_value_type h = two_to_one_CRH<HashT>(l, r);
        hashes[idx] = h;
    }
}

template<typename HashT>
typename HashT::hash_value_type merkle_tree<HashT>::get_root() const
{
    auto it = hashes.find(0);
    return (it == hashes.end() ? hash_defaults[0] : it->second);
}

template<typename HashT>
typename HashT::merkle_authentication_path_type merkle_tree<HashT>::get_path(const size_t address) const
{
    typename HashT::merkle_authentication_path_type result(depth);
    assert(log2(address) <= depth);
    size_t idx = address + (UINT64_C(1)<<depth) - 1;

    for (size_t layer = depth; layer > 0; --layer)
    {
        size_t sibling_idx = ((idx + 1) ^ 1) - 1;
        auto it = hashes.find(sibling_idx);
        if (layer == depth)
        {
            auto it2 = values.find(sibling_idx - ((UINT64_C(1)<<depth) - 1));
            result[layer-1] = (it2 == values.end() ? bit_vector(value_size, false) : it2->second);
            result[layer-1].resize(digest_size);
        }
        else
        {
            result[layer-1] = (it == hashes.end() ? hash_defaults[layer] : it->second);
        }

        idx = (idx-1)/2;
    }

    return result;
}

template<typename HashT>
void merkle_tree<HashT>::dump() const
{
    for (size_t i = 0; i < UINT64_C(1)<<depth; ++i)
    {
        auto it = values.find(i);
        printf("[%zu] -> ", i);
        const bit_vector value = (it == values.end() ? bit_vector(value_size) : it->second);
        for (bool b : value)
        {
            printf("%d", b ? 1 : 0);
        }
        printf("\n");
    }
    printf("\n");
}

} // libsnark

#endif // MERKLE_TREE_TCC
