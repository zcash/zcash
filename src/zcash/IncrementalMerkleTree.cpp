#include <stdexcept>

#include <boost/foreach.hpp>

#include "zcash/IncrementalMerkleTree.hpp"
#include "crypto/sha256.h"
#include "zcash/util.h"

namespace libzcash {

SHA256Compress SHA256Compress::combine(const SHA256Compress& a, const SHA256Compress& b)
{
    SHA256Compress res = SHA256Compress();

    CSHA256 hasher;
    hasher.Write(a.begin(), 32);
    hasher.Write(b.begin(), 32);
    hasher.FinalizeNoPadding(res.begin());

    return res;
}

template <size_t Depth, typename Hash>
class PathFiller {
private:
    std::deque<Hash> queue;
    static EmptyMerkleRoots<Depth, Hash> emptyroots;
public:
    PathFiller() : queue() { }
    PathFiller(std::deque<Hash> queue) : queue(queue) { }

    Hash next(size_t depth) {
        if (queue.size() > 0) {
            Hash h = queue.front();
            queue.pop_front();

            return h;
        } else {
            return emptyroots.empty_root(depth);
        }
    }

};

template<size_t Depth, typename Hash>
EmptyMerkleRoots<Depth, Hash> PathFiller<Depth, Hash>::emptyroots;

template<size_t Depth, typename Hash>
EmptyMerkleRoots<Depth, Hash> IncrementalMerkleTree<Depth, Hash>::emptyroots;

template<size_t Depth, typename Hash>
void IncrementalMerkleTree<Depth, Hash>::wfcheck() const {
    if (parents.size() >= Depth) {
        throw std::ios_base::failure("tree has too many parents");
    }

    // The last parent cannot be null.
    if (!(parents.empty()) && !(parents.back())) {
        throw std::ios_base::failure("tree has non-canonical representation of parent");
    }

    // Left cannot be empty when right exists.
    if (!left && right) {
        throw std::ios_base::failure("tree has non-canonical representation; right should not exist");
    }

    // Left cannot be empty when parents is nonempty.
    if (!left && parents.size() > 0) {
        throw std::ios_base::failure("tree has non-canonical representation; parents should not be unempty");
    }
}

template<size_t Depth, typename Hash>
Hash IncrementalMerkleTree<Depth, Hash>::last() const {
    if (right) {
        return *right;
    } else if (left) {
        return *left;
    } else {
        throw std::runtime_error("tree has no cursor");
    }
}

template<size_t Depth, typename Hash>
size_t IncrementalMerkleTree<Depth, Hash>::size() const {
    size_t ret = 0;
    if (left) {
        ret++;
    }
    if (right) {
        ret++;
    }
    // Treat occupation of parents array as a binary number
    // (right-shifted by 1)
    for (size_t i = 0; i < parents.size(); i++) {
        if (parents[i]) {
            ret += (1 << (i+1));
        }
    }
    return ret;
}

template<size_t Depth, typename Hash>
void IncrementalMerkleTree<Depth, Hash>::append(Hash obj) {
    if (is_complete(Depth)) {
        throw std::runtime_error("tree is full");
    }

    if (!left) {
        // Set the left leaf
        left = obj;
    } else if (!right) {
        // Set the right leaf
        right = obj;
    } else {
        // Combine the leaves and propagate it up the tree
        boost::optional<Hash> combined = Hash::combine(*left, *right);

        // Set the "left" leaf to the object and make the "right" leaf none
        left = obj;
        right = boost::none;

        for (size_t i = 0; i < Depth; i++) {
            if (i < parents.size()) {
                if (parents[i]) {
                    combined = Hash::combine(*parents[i], *combined);
                    parents[i] = boost::none;
                } else {
                    parents[i] = *combined;
                    break;
                }
            } else {
                parents.push_back(combined);
                break;
            }
        }
    }
}

// This is for allowing the witness to determine if a subtree has filled
// to a particular depth, or for append() to ensure we're not appending
// to a full tree.
template<size_t Depth, typename Hash>
bool IncrementalMerkleTree<Depth, Hash>::is_complete(size_t depth) const {
    if (!left || !right) {
        return false;
    }

    if (parents.size() != (depth - 1)) {
        return false;
    }

    BOOST_FOREACH(const boost::optional<Hash>& parent, parents) {
        if (!parent) {
            return false;
        }
    }

    return true;
}

// This finds the next "depth" of an unfilled subtree, given that we've filled
// `skip` uncles/subtrees.
template<size_t Depth, typename Hash>
size_t IncrementalMerkleTree<Depth, Hash>::next_depth(size_t skip) const {
    if (!left) {
        if (skip) {
            skip--;
        } else {
            return 0;
        }
    }

    if (!right) {
        if (skip) {
            skip--;
        } else {
            return 0;
        }
    }

    size_t d = 1;

    BOOST_FOREACH(const boost::optional<Hash>& parent, parents) {
        if (!parent) {
            if (skip) {
                skip--;
            } else {
                return d;
            }
        }

        d++;
    }

    return d + skip;
}

// This calculates the root of the tree.
template<size_t Depth, typename Hash>
Hash IncrementalMerkleTree<Depth, Hash>::root(size_t depth,
                                              std::deque<Hash> filler_hashes) const {
    PathFiller<Depth, Hash> filler(filler_hashes);

    Hash combine_left =  left  ? *left  : filler.next(0);
    Hash combine_right = right ? *right : filler.next(0);

    Hash root = Hash::combine(combine_left, combine_right);

    size_t d = 1;

    BOOST_FOREACH(const boost::optional<Hash>& parent, parents) {
        if (parent) {
            root = Hash::combine(*parent, root);
        } else {
            root = Hash::combine(root, filler.next(d));
        }

        d++;
    }

    // We may not have parents for ancestor trees, so we fill
    // the rest in here.
    while (d < depth) {
        root = Hash::combine(root, filler.next(d));
        d++;
    }

    return root;
}

// This constructs an authentication path into the tree in the format that the circuit
// wants. The caller provides `filler_hashes` to fill in the uncle subtrees.
template<size_t Depth, typename Hash>
MerklePath IncrementalMerkleTree<Depth, Hash>::path(std::deque<Hash> filler_hashes) const {
    if (!left) {
        throw std::runtime_error("can't create an authentication path for the beginning of the tree");
    }

    PathFiller<Depth, Hash> filler(filler_hashes);

    std::vector<Hash> path;
    std::vector<bool> index;

    if (right) {
        index.push_back(true);
        path.push_back(*left);
    } else {
        index.push_back(false);
        path.push_back(filler.next(0));
    }

    size_t d = 1;

    BOOST_FOREACH(const boost::optional<Hash>& parent, parents) {
        if (parent) {
            index.push_back(true);
            path.push_back(*parent);
        } else {
            index.push_back(false);
            path.push_back(filler.next(d));
        }

        d++;
    }

    while (d < Depth) {
        index.push_back(false);
        path.push_back(filler.next(d));
        d++;
    }

    std::vector<std::vector<bool>> merkle_path;
    BOOST_FOREACH(Hash b, path)
    {
        std::vector<unsigned char> hashv(b.begin(), b.end());

        merkle_path.push_back(convertBytesVectorToVector(hashv));
    }

    std::reverse(merkle_path.begin(), merkle_path.end());
    std::reverse(index.begin(), index.end());

    return MerklePath(merkle_path, index);
}

template<size_t Depth, typename Hash>
std::deque<Hash> IncrementalWitness<Depth, Hash>::partial_path() const {
    std::deque<Hash> uncles(filled.begin(), filled.end());

    if (cursor) {
        uncles.push_back(cursor->root(cursor_depth));
    }

    return uncles;
}

template<size_t Depth, typename Hash>
void IncrementalWitness<Depth, Hash>::append(Hash obj) {
    if (cursor) {
        cursor->append(obj);

        if (cursor->is_complete(cursor_depth)) {
            filled.push_back(cursor->root(cursor_depth));
            cursor = boost::none;
        }
    } else {
        cursor_depth = tree.next_depth(filled.size());

        if (cursor_depth >= Depth) {
            throw std::runtime_error("tree is full");
        }

        if (cursor_depth == 0) {
            filled.push_back(obj);
        } else {
            cursor = IncrementalMerkleTree<Depth, Hash>();
            cursor->append(obj);
        }
    }
}

template class IncrementalMerkleTree<INCREMENTAL_MERKLE_TREE_DEPTH, SHA256Compress>;
template class IncrementalMerkleTree<INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, SHA256Compress>;

template class IncrementalWitness<INCREMENTAL_MERKLE_TREE_DEPTH, SHA256Compress>;
template class IncrementalWitness<INCREMENTAL_MERKLE_TREE_DEPTH_TESTING, SHA256Compress>;

} // end namespace `libzcash`
