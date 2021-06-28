#ifndef ZC_INCREMENTALMERKLETREE_H_
#define ZC_INCREMENTALMERKLETREE_H_

#include <array>
#include <deque>
#include <optional>

#include "uint256.h"
#include "serialize.h"

#include "Zcash.h"
#include "zcash/util.h"

#include <primitives/orchard.h>
#include <rust/orchard/incremental_sinsemilla_tree.h>

namespace libzcash {

class MerklePath {
public:
    std::vector<std::vector<bool>> authentication_path;
    std::vector<bool> index;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        std::vector<std::vector<unsigned char>> pathBytes;
        uint64_t indexInt;
        if (ser_action.ForRead()) {
            READWRITE(pathBytes);
            READWRITE(indexInt);
            MerklePath &us = *(const_cast<MerklePath*>(this));
            for (size_t i = 0; i < pathBytes.size(); i++) {
                us.authentication_path.push_back(convertBytesVectorToVector(pathBytes[i]));
                us.index.push_back((indexInt >> ((pathBytes.size() - 1) - i)) & 1);
            }
        } else {
            assert(authentication_path.size() == index.size());
            pathBytes.resize(authentication_path.size());
            for (size_t i = 0; i < authentication_path.size(); i++) {
                pathBytes[i].resize((authentication_path[i].size()+7)/8);
                for (unsigned int p = 0; p < authentication_path[i].size(); p++) {
                    pathBytes[i][p / 8] |= authentication_path[i][p] << (7-(p % 8));
                }
            }
            indexInt = convertVectorToInt(index);
            READWRITE(pathBytes);
            READWRITE(indexInt);
        }
    }

    MerklePath() { }

    MerklePath(std::vector<std::vector<bool>> authentication_path, std::vector<bool> index)
    : authentication_path(authentication_path), index(index) { }
};

template<size_t Depth, typename Hash>
class EmptyMerkleRoots {
public:
    EmptyMerkleRoots() { }
    Hash empty_root(size_t depth) const {
        return Hash::EmptyRoot(depth);
    }
    template <size_t D, typename H>
    friend bool operator==(const EmptyMerkleRoots<D, H>& a,
                           const EmptyMerkleRoots<D, H>& b);
private:
    std::array<Hash, Depth+1> empty_roots;
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
    static_assert(Depth >= 1);

    IncrementalMerkleTree() { }

    size_t DynamicMemoryUsage() const {
        return 32 + // left
               32 + // right
               parents.size() * 32; // parents
    }

    size_t size() const;

    void append(Hash obj);
    Hash root() const {
        return root(Depth, std::deque<Hash>());
    }
    Hash last() const;

    IncrementalWitness<Depth, Hash> witness() const {
        return IncrementalWitness<Depth, Hash>(*this);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
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
    std::optional<Hash> left;
    std::optional<Hash> right;

    // Collapsed "left" subtrees ordered toward the root of the tree.
    std::vector<std::optional<Hash>> parents;
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

    // Return the element being witnessed (should be a note
    // commitment!)
    Hash element() const {
        return tree.last();
    }

    uint64_t position() const {
        return tree.size() - 1;
    }

    Hash root() const {
        return tree.root(Depth, partial_path());
    }

    void append(Hash obj);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
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
    std::optional<IncrementalMerkleTree<Depth, Hash>> cursor;
    size_t cursor_depth = 0;
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

    static SHA256Compress combine(
        const SHA256Compress& a,
        const SHA256Compress& b,
        size_t depth
    );

    static SHA256Compress uncommitted() {
        return SHA256Compress();
    }
    static SHA256Compress EmptyRoot(size_t);
};

class PedersenHash : public uint256 {
public:
    PedersenHash() : uint256() {}
    PedersenHash(uint256 contents) : uint256(contents) { }

    static PedersenHash combine(
        const PedersenHash& a,
        const PedersenHash& b,
        size_t depth
    );

    static PedersenHash uncommitted();
    static PedersenHash EmptyRoot(size_t);
};

template<size_t Depth, typename Hash>
EmptyMerkleRoots<Depth, Hash> IncrementalMerkleTree<Depth, Hash>::emptyroots;

} // end namespace `libzcash`

typedef libzcash::IncrementalMerkleTree<INCREMENTAL_MERKLE_TREE_DEPTH, libzcash::SHA256Compress> SproutMerkleTree;
typedef libzcash::IncrementalMerkleTree<INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, libzcash::SHA256Compress> SproutTestingMerkleTree;

typedef libzcash::IncrementalWitness<INCREMENTAL_MERKLE_TREE_DEPTH, libzcash::SHA256Compress> SproutWitness;
typedef libzcash::IncrementalWitness<INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, libzcash::SHA256Compress> SproutTestingWitness;

typedef libzcash::IncrementalMerkleTree<SAPLING_INCREMENTAL_MERKLE_TREE_DEPTH, libzcash::PedersenHash> SaplingMerkleTree;
typedef libzcash::IncrementalMerkleTree<INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, libzcash::PedersenHash> SaplingTestingMerkleTree;

typedef libzcash::IncrementalWitness<SAPLING_INCREMENTAL_MERKLE_TREE_DEPTH, libzcash::PedersenHash> SaplingWitness;
typedef libzcash::IncrementalWitness<INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, libzcash::PedersenHash> SaplingTestingWitness;

class OrchardMerkleTree
{
private:
    /// An incremental Sinsemilla tree; this pointer may never be null.
    /// Memory is allocated by Rust.
    std::unique_ptr<OrchardMerkleFrontierPtr, decltype(&orchard_merkle_frontier_free)> inner;
public:
    OrchardMerkleTree() : inner(orchard_merkle_frontier_empty(), orchard_merkle_frontier_free) {}

    OrchardMerkleTree(OrchardMerkleTree&& frontier) : inner(std::move(frontier.inner)) {}

    OrchardMerkleTree(const OrchardMerkleTree& frontier) :
        inner(orchard_merkle_frontier_clone(frontier.inner.get()), orchard_merkle_frontier_free) {}

    OrchardMerkleTree& operator=(OrchardMerkleTree&& frontier)
    {
        if (this != &frontier) {
            inner = std::move(frontier.inner);
        }
        return *this;
    }
    OrchardMerkleTree& operator=(const OrchardMerkleTree& frontier)
    {
        if (this != &frontier) {
            inner.reset(orchard_merkle_frontier_clone(frontier.inner.get()));
        }
        return *this;
    }

    template<typename Stream>
    void Serialize(Stream& s) const {
        RustStream rs(s);
        if (!orchard_merkle_frontier_serialize(inner.get(), &rs, RustStream<Stream>::write_callback)) {
            throw std::ios_base::failure("Failed to serialize v5 Orchard tree");
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        RustStream rs(s);
        OrchardMerkleFrontierPtr* tree = orchard_merkle_frontier_parse(
                &rs, RustStream<Stream>::read_callback);
        if (tree == nullptr) {
            throw std::ios_base::failure("Failed to parse v5 Orchard tree");
        }
        inner.reset(tree);
    }

    size_t DynamicMemoryUsage() const {
        return orchard_merkle_frontier_dynamic_mem_usage(inner.get());
    }

    bool AppendBundle(const OrchardBundle& bundle) {
       return orchard_merkle_frontier_append_bundle(inner.get(), bundle.inner.get());
    }

    const uint256 root() const {
        uint256 value;
        orchard_merkle_frontier_root(inner.get(), value.begin());
        return value;
    }

    static uint256 empty_root() {
        uint256 value;
        incremental_sinsemilla_tree_empty_root(value.begin());
        return value;
    }
};

#endif /* ZC_INCREMENTALMERKLETREE_H_ */
