#ifndef ZC_INCREMENTALMERKLETREE_H_
#define ZC_INCREMENTALMERKLETREE_H_

#include <array>
#include <deque>
#include <optional>

#include "uint256.h"
#include "serialize.h"
#include "streams_rust.h"

#include "Zcash.h"
#include "zcash/util.h"

#include <primitives/orchard.h>
#include <rust/bridge.h>

namespace libzcash {

typedef uint64_t SubtreeIndex;
typedef std::array<uint8_t, 32> SubtreeRoot;
static const uint8_t TRACKED_SUBTREE_HEIGHT = 16;

class LatestSubtree {
    public:

    //! Version of this structure for extensibility purposes
    uint8_t leadbyte = 0x00;
    //! The index of the latest complete subtree
    SubtreeIndex index;
    //! The latest complete subtree root at level TRACKED_SUBTREE_HEIGHT
    SubtreeRoot root;
    //! The height of the block that contains the note commitment that is
    //! the rightmost leaf of the most recently completed subtree.
    int nHeight;

    LatestSubtree() : nHeight(0) { }

    LatestSubtree(SubtreeIndex index, SubtreeRoot root, int nHeight)
        : index(index), root(root), nHeight(nHeight) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(leadbyte);
        READWRITE(index);
        READWRITE(root);
        READWRITE(nHeight);
    }
};

class SubtreeData {
    public:

    //! Version of this structure for extensibility purposes
    uint8_t leadbyte = 0x00;
    //! The root of the subtree at level TRACKED_SUBTREE_HEIGHT
    SubtreeRoot root;
    //! The height of the block that contains the note commitment
    //! that completed this subtree.
    int nHeight;

    SubtreeData() : nHeight(0) { }

    SubtreeData(SubtreeRoot root, int nHeight)
        : root(root), nHeight(nHeight) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(leadbyte);
        READWRITE(root);
        READWRITE(nHeight);
    }
};

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

    //! Returns the number of (filled) leaves present in this tree, or
    //! in other words, the 0-indexed position that the next leaf
    //! added to the tree will occupy.
    size_t size() const;

    //! Returns the current 2^TRACKED_SUBTREE_HEIGHT subtree index
    //! that this tree is currently on. Specifically, a leaf appended
    //! at this point will be located in the 2^TRACKED_SUBTREE_HEIGHT
    //! subtree with the index returned by this function.
    SubtreeIndex current_subtree_index() const;

    //! If the last leaf appended to this tree completed a
    //! 2^TRACKED_SUBTREE_HEIGHT subtree, this function will return
    //! the 2^TRACKED_SUBTREE_HEIGHT root of that subtree. Otherwise,
    //! this will return nullopt.
    std::optional<Hash> complete_subtree_root() const;

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

class OrchardWallet;
class OrchardMerkleFrontierLegacySer;

class OrchardMerkleFrontier
{
private:
    /// An incremental Sinsemilla tree. Memory is allocated by Rust.
    rust::Box<merkle_frontier::Orchard> inner;

    friend class OrchardWallet;
    friend class OrchardMerkleFrontierLegacySer;
public:
    OrchardMerkleFrontier() : inner(merkle_frontier::new_orchard()) {}

    OrchardMerkleFrontier(OrchardMerkleFrontier&& frontier) : inner(std::move(frontier.inner)) {}

    OrchardMerkleFrontier(const OrchardMerkleFrontier& frontier) :
        inner(frontier.inner->box_clone()) {}

    OrchardMerkleFrontier& operator=(OrchardMerkleFrontier&& frontier)
    {
        if (this != &frontier) {
            inner = std::move(frontier.inner);
        }
        return *this;
    }
    OrchardMerkleFrontier& operator=(const OrchardMerkleFrontier& frontier)
    {
        if (this != &frontier) {
            inner = frontier.inner->box_clone();
        }
        return *this;
    }

    template<typename Stream>
    void Serialize(Stream& s) const {
        try {
            inner->serialize(*ToRustStream(s));
        } catch (const std::exception& e) {
            throw std::ios_base::failure(e.what());
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        try {
            inner = merkle_frontier::parse_orchard(*ToRustStream(s));
        } catch (const std::exception& e) {
            throw std::ios_base::failure(e.what());
        }
    }

    size_t DynamicMemoryUsage() const {
        return inner->dynamic_memory_usage();
    }

    merkle_frontier::OrchardAppendResult AppendBundle(const OrchardBundle& bundle) {
        return inner->append_bundle(*bundle.GetDetails());
    }

    const uint256 root() const {
        return uint256::FromRawBytes(inner->root());
    }

    static uint256 empty_root() {
        return uint256::FromRawBytes(merkle_frontier::orchard_empty_root());
    }

    size_t size() const {
        return inner->size();
    }

    libzcash::SubtreeIndex current_subtree_index() const {
        return (inner->size() >> libzcash::TRACKED_SUBTREE_HEIGHT);
    }
};

class OrchardMerkleFrontierLegacySer {
private:
    const OrchardMerkleFrontier& frontier;
public:
    OrchardMerkleFrontierLegacySer(const OrchardMerkleFrontier& frontier): frontier(frontier) {}

    template<typename Stream>
    void Serialize(Stream& s) const {
        try {
            frontier.inner->serialize_legacy(*ToRustStream(s));
        } catch (const std::exception& e) {
            throw std::ios_base::failure(e.what());
        }
    }
};

#endif /* ZC_INCREMENTALMERKLETREE_H_ */
