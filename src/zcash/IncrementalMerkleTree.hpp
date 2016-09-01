#ifndef ZCINCREMENTALMERKLETREE_H_
#define ZCINCREMENTALMERKLETREE_H_

#include <deque>
#include <boost/optional.hpp>
#include <boost/static_assert.hpp>

#include "uint256.h"
#include "serialize.h"

#include "Zcash.h"

namespace libzcash {

class MerklePath {
public:
    std::vector<std::vector<bool>> authentication_path;
    std::vector<bool> index;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(authentication_path);
        READWRITE(index);
    }

    MerklePath() { }

    MerklePath(std::vector<std::vector<bool>> authentication_path, std::vector<bool> index)
    : authentication_path(authentication_path), index(index) { }
};

template<size_t Depth, typename Hash>
class EmptyMerkleRoots {
public:
    EmptyMerkleRoots() {
        empty_roots.at(0) = Hash();
        for (size_t d = 1; d <= Depth; d++) {
            empty_roots.at(d) = Hash::combine(empty_roots.at(d-1), empty_roots.at(d-1));
        }
    }
    Hash empty_root(size_t depth) {
        return empty_roots.at(depth);
    }
    template <size_t D, typename H>
    friend bool operator==(const EmptyMerkleRoots<D, H>& a,
                           const EmptyMerkleRoots<D, H>& b);
private:
    boost::array<Hash, Depth+1> empty_roots;
};

template<size_t Depth, typename Hash>
bool operator==(const EmptyMerkleRoots<Depth, Hash>& a,
                const EmptyMerkleRoots<Depth, Hash>& b) {
    return a.empty_roots == b.empty_roots;
}

template<size_t Depth, typename Hash>
class IncrementalWitness;

template<size_t Depth, typename Hash>
class IncrementalMerkleTree {

friend class IncrementalWitness<Depth, Hash>;

public:
    BOOST_STATIC_ASSERT(Depth >= 1);

    IncrementalMerkleTree() { }

    size_t DynamicMemoryUsage() const {
        return 32 + // left
               32 + // right
               parents.size() * 32; // parents
    }

    void append(Hash obj);
    Hash root() const {
        return root(Depth, std::deque<Hash>());
    }

    IncrementalWitness<Depth, Hash> witness() const {
        return IncrementalWitness<Depth, Hash>(*this);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(left);
        READWRITE(right);
        READWRITE(parents);

        wfcheck();
    }

    static Hash empty_root() {
        return emptyroots.empty_root(Depth);
    }

    template <size_t D, typename H>
    friend bool operator==(const IncrementalMerkleTree<D, H>& a,
                           const IncrementalMerkleTree<D, H>& b);

private:
    static EmptyMerkleRoots<Depth, Hash> emptyroots;
    boost::optional<Hash> left;
    boost::optional<Hash> right;

    // Collapsed "left" subtrees ordered toward the root of the tree.
    std::vector<boost::optional<Hash>> parents;
    MerklePath path(std::deque<Hash> filler_hashes = std::deque<Hash>()) const;
    Hash root(size_t depth, std::deque<Hash> filler_hashes = std::deque<Hash>()) const;
    bool is_complete(size_t depth = Depth) const;
    size_t next_depth(size_t skip) const;
    void wfcheck() const;
};

template<size_t Depth, typename Hash>
bool operator==(const IncrementalMerkleTree<Depth, Hash>& a,
                const IncrementalMerkleTree<Depth, Hash>& b) {
    return (a.emptyroots == b.emptyroots &&
            a.left == b.left &&
            a.right == b.right &&
            a.parents == b.parents);
}

template <size_t Depth, typename Hash>
class IncrementalWitness {
friend class IncrementalMerkleTree<Depth, Hash>;

public:
    // Required for Unserialize()
    IncrementalWitness() {}

    MerklePath path() const {
        return tree.path(partial_path());
    }

    Hash root() const {
        return tree.root(Depth, partial_path());
    }

    void append(Hash obj);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(tree);
        READWRITE(filled);
        READWRITE(cursor);

        cursor_depth = tree.next_depth(filled.size());
    }

    template <size_t D, typename H>
    friend bool operator==(const IncrementalWitness<D, H>& a,
                           const IncrementalWitness<D, H>& b);

private:
    IncrementalMerkleTree<Depth, Hash> tree;
    std::vector<Hash> filled;
    boost::optional<IncrementalMerkleTree<Depth, Hash>> cursor;
    size_t cursor_depth;
    std::deque<Hash> partial_path() const;
    IncrementalWitness(IncrementalMerkleTree<Depth, Hash> tree) : tree(tree) {}
};

template<size_t Depth, typename Hash>
bool operator==(const IncrementalWitness<Depth, Hash>& a,
                const IncrementalWitness<Depth, Hash>& b) {
    return (a.tree == b.tree &&
            a.filled == b.filled &&
            a.cursor == b.cursor &&
            a.cursor_depth == b.cursor_depth);
}

class SHA256Compress : public uint256 {
public:
    SHA256Compress() : uint256() {}
    SHA256Compress(uint256 contents) : uint256(contents) { }

    static SHA256Compress combine(const SHA256Compress& a, const SHA256Compress& b);
};

} // end namespace `libzcash`

typedef libzcash::IncrementalMerkleTree<INCREMENTAL_MERKLE_TREE_DEPTH, libzcash::SHA256Compress> ZCIncrementalMerkleTree;
typedef libzcash::IncrementalMerkleTree<INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, libzcash::SHA256Compress> ZCTestingIncrementalMerkleTree;

typedef libzcash::IncrementalWitness<INCREMENTAL_MERKLE_TREE_DEPTH, libzcash::SHA256Compress> ZCIncrementalWitness;
typedef libzcash::IncrementalWitness<INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, libzcash::SHA256Compress> ZCTestingIncrementalWitness;

#endif /* ZCINCREMENTALMERKLETREE_H_ */

