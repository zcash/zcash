/** @file
 *****************************************************************************

 Declaration of interfaces for a Merkle tree.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MERKLE_TREE_HPP_
#define MERKLE_TREE_HPP_

#include <map>
#include <vector>
#include "common/utils.hpp"

namespace libsnark {

/**
 * A Merkle tree is maintained as two maps:
 * - a map from addresses to values, and
 * - a map from addresses to hashes.
 *
 * The second map maintains the intermediate hashes of a Merkle tree
 * built atop the values currently stored in the tree (the
 * implementation admits a very efficient support for sparse
 * trees). Besides offering methods to load and store values, the
 * class offers methods to retrieve the root of the Merkle tree and to
 * obtain the authentication paths for (the value at) a given address.
 */

typedef bit_vector merkle_authentication_node;
typedef std::vector<merkle_authentication_node> merkle_authentication_path;

template<typename HashT>
class merkle_tree {
private:

    typedef typename HashT::hash_value_type hash_value_type;
    typedef typename HashT::merkle_authentication_path_type merkle_authentication_path_type;

public:

    std::vector<hash_value_type> hash_defaults;
    std::map<size_t, bit_vector> values;
    std::map<size_t, hash_value_type> hashes;

    size_t depth;
    size_t value_size;
    size_t digest_size;

    merkle_tree(const size_t depth, const size_t value_size);
    merkle_tree(const size_t depth, const size_t value_size, const std::vector<bit_vector> &contents_as_vector);
    merkle_tree(const size_t depth, const size_t value_size, const std::map<size_t, bit_vector> &contents);

    bit_vector get_value(const size_t address) const;
    void set_value(const size_t address, const bit_vector &value);

    hash_value_type get_root() const;
    merkle_authentication_path_type get_path(const size_t address) const;

    void dump() const;
};

} // libsnark

#include "common/data_structures/merkle_tree.tcc"

#endif // MERKLE_TREE_HPP_
