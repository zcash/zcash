#ifndef ZCINCREMENTALMERKLETREE_H_
#define ZCINCREMENTALMERKLETREE_H_

#include <deque>
#include <boost/variant.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>

#include "uint256.h"
#include "serialize.h"
#include "zerocash/utils/util.h"
#include <boost/static_assert.hpp>

#include <iostream>

using namespace libzerocash;

namespace NewTree {

class MerklePath {
public:
    std::vector<std::vector<bool>> authentication_path;
    std::vector<bool> index;

    MerklePath(std::vector<std::vector<bool>> authentication_path, std::vector<bool> index)
    : authentication_path(authentication_path), index(index) { }
};

template<size_t Depth, typename Hash>
class IncrementalWitness;

template<size_t Depth, typename Hash>
class IncrementalMerkleTree {

friend class IncrementalWitness<Depth, Hash>;

private:
    boost::optional<Hash> left;
    boost::optional<Hash> right;
    std::vector<boost::optional<Hash>> parents;

public:
    BOOST_STATIC_ASSERT(Depth >= 1);

    IncrementalMerkleTree() { }

    void append(Hash obj);
    Hash root(size_t depth = Depth, std::deque<Hash> filler_hashes = std::deque<Hash>()) const;
    MerklePath path(std::deque<Hash> filler_hashes = std::deque<Hash>()) const;

    bool is_complete(size_t depth = Depth) const;
    size_t next_depth(size_t skip) const;

    IncrementalWitness<Depth, Hash> witness() const {
        return IncrementalWitness<Depth, Hash>(*this);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(left);
        READWRITE(right);
        READWRITE(parents);
    }
};

template <size_t Depth, typename Hash>
class IncrementalWitness {
private:
    IncrementalMerkleTree<Depth, Hash> tree;
    std::vector<Hash> filled;
    boost::optional<IncrementalMerkleTree<Depth, Hash>> cursor;
    size_t cursor_depth;

    std::deque<Hash> uncle_train() const;

public:
    IncrementalWitness(IncrementalMerkleTree<Depth, Hash> tree) : tree(tree) {}

    MerklePath path() const {
        return tree.path(uncle_train());
    }

    Hash root() const {
        return tree.root(Depth, uncle_train());
    }

    void append(Hash obj);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(tree);
        READWRITE(filled);
        READWRITE(cursor);
        READWRITE(cursor_depth);
    }
};

class SHA256Compress : public uint256 {
public:
    SHA256Compress() : uint256() {}
    SHA256Compress(uint256 contents) : uint256(contents) { }

    static SHA256Compress combine(const SHA256Compress& a, const SHA256Compress& b);
};

}

#include "NewIncrementalMerkleTree.tcc"

#endif /* ZCINCREMENTALMERKLETREE_H_ */

