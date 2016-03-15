#include <stdexcept>

namespace NewTree {

template <typename Hash>
class PathFiller {
private:
    std::deque<Hash> queue;
public:
    PathFiller(std::deque<Hash> queue) : queue(queue) { }

    boost::optional<Hash> next() {
        if (queue.size() > 0) {
            Hash h = queue.front();
            queue.pop_front();

            return h;
        } else {
            return boost::none;
        }
    }
};

template <typename Hash>
class IndexCalculator {
public:
    typedef std::vector<bool> Result;

    std::vector<bool> index;

    IndexCalculator(const boost::optional<Hash>& left,
                    const boost::optional<Hash>& right) : index() {
        if (right) {
            index.push_back(true);
        } else {
            index.push_back(false);
        }
    }

    boost::optional<std::vector<bool>> feed(size_t depth, const boost::optional<Hash>& left) {
        if (depth == 0) {
            std::reverse(index.begin(), index.end());
            return index;
        }

        if (left) {
            index.push_back(true);
        } else {
            index.push_back(false);
        }

        return boost::none;
    }
};

// This will construct an authentication path in the style
// that libsnark wants.
template <typename Hash>
class PathCalculator : PathFiller<Hash> {
public:
    typedef std::vector<Hash> Result;

    std::vector<Hash> path;

    PathCalculator(Hash left,
                   const boost::optional<Hash>& right,
                   std::deque<Hash> fill) : PathFiller<Hash>(fill), path() {
        if (right) {
            // The cursor of the tree is on the right, so our path hashes
            // with the left.
            path.push_back(left);
        } else {
            // The cursor of the tree is on the left, so unless there is
            // a fill by the delta, we hash with a null digest.
            path.push_back(PathFiller<Hash>::next().value_or(Hash()));
        }
    }

    boost::optional<std::vector<Hash>> feed(size_t depth, const boost::optional<Hash>& left) {
        if (depth == 0) {
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (left) {
            // The cursor of the tree is on the right because we have a left,
            // so hash with the left.
            path.push_back(*left);
        } else {
            // The cursor of the tree is on the left, so unless there is
            // a fill by the delta, we hash with a null digest.
            path.push_back(PathFiller<Hash>::next().value_or(Hash()));
        }

        return boost::none;
    }
};

// This will calculate the root of the tree, given a path filler
// in case a delta is involved.
template <typename Hash>
class RootCalculator : PathFiller<Hash> {
public:
    typedef Hash Result;

    Hash child;

    RootCalculator(const boost::optional<Hash>& left,
                   const boost::optional<Hash>& right,
                   std::deque<Hash> fill) : PathFiller<Hash>(fill) {
        // Initialize the calculator with the state of the leaves.
        Hash leftFill = left ? *left : PathFiller<Hash>::next().value_or(Hash());
        Hash rightFill = right ? *right : PathFiller<Hash>::next().value_or(Hash());

        child = Hash::combine(leftFill, rightFill);
    }

    boost::optional<Hash> feed(size_t depth, const boost::optional<Hash>& left) {
        if (depth == 0) {
            // We've reached the top, there's nothing left to combine.
            return child;
        }

        // Hash the collapsed left side with our child, otherwise
        // our child with a blank right side. (If it is not filled by the delta.)
        if (left) {
            child = Hash::combine(*left, child);
        } else {
            child = Hash::combine(child, PathFiller<Hash>::next().value_or(Hash()));
        }

        return boost::none;
    }
};

// This will find the depth of the tree that must be constructed next to append
// to the witness, skipping some number of depths we've already collapsed into
// the delta.
template <typename Hash>
class NextIncomplete {
public:
    typedef size_t Result;

    size_t incomplete_depth;
    size_t skip;

    NextIncomplete(size_t skip) : incomplete_depth(1), skip(skip) {}

    boost::optional<size_t> feed(size_t depth, const boost::optional<Hash>& left) {
        if (depth == 0) {
            return incomplete_depth;
        }

        if (!left) {
            if (skip > 0) {
                skip--;
            } else {
                return incomplete_depth;
            }
        }

        incomplete_depth++;

        return boost::none;
    }
};

// This will determine if the tree is "complete" or entirely filled to
// its depth.
template <typename Hash>
class IsComplete {
public:
    typedef bool Result;

    IsComplete() {}

    boost::optional<bool> feed(size_t depth, const boost::optional<Hash>& left) {
        if (depth == 0) {
            return true;
        }

        if (!left) {
            return false;
        }

        return boost::none;
    }
};

template <size_t Depth, typename Hash>
MerklePath IncrementalWitness<Depth, Hash>::witness() const
{
    std::deque<Hash> fill(delta.uncles.begin(), delta.uncles.end());
    if (delta.active) {
        fill.push_back(delta.active->root(delta.depth));
    }

    std::vector<std::vector<bool>> merkle_path;

    std::vector<Hash> v = tree.path(fill);

    BOOST_FOREACH(Hash b, v)
    {
        std::vector<unsigned char> hashv(b.begin(), b.end());
        std::vector<bool> tmp_b;

        libzerocash::convertBytesVectorToVector(hashv, tmp_b);

        merkle_path.push_back(tmp_b);
    }

    return MerklePath(merkle_path, tree.index());
}

template <size_t Depth, typename Hash>
Hash IncrementalWitness<Depth, Hash>::root() const
{
    std::deque<Hash> fill(delta.uncles.begin(), delta.uncles.end());
    if (delta.active) {
        fill.push_back(delta.active->root(delta.depth));
    }

    return tree.root(fill);
}

template <size_t Depth, typename Hash>
void IncrementalWitness<Depth, Hash>::append(Hash obj)
{
    if (!delta.active) {
        size_t depth = tree.next_incomplete(delta.uncles.size());

        if (depth == 0) {
            delta.uncles.push_back(obj);
        } else {
            delta.active = IncrementalMerkleTree<Depth, Hash>();
            delta.depth = depth;
            delta.active->append(obj);
        }
    } else {
        if (delta.active->is_complete(delta.depth)) {
            delta.uncles.push_back(delta.active->root(delta.depth));

            delta.depth = tree.next_incomplete(delta.uncles.size());

            assert(delta.depth > 0);

            delta.active = IncrementalMerkleTree<Depth, Hash>();
            delta.active->append(obj);
        } else {
            delta.active->append(obj);
        }
    }
}

template <size_t Depth, typename Hash>
void IncrementalMerkleTree<Depth, Hash>::append(Hash obj)
{
    if (!left) {
        left = obj;
    } else if (!right) {
        right = obj;
    } else {
        // Collapse the two entries into the parent.

        parent.advance(Hash::combine(*left, *right));
        right = boost::none;
        left = obj;
    }
}

template <size_t Depth, typename Hash>
bool IncrementalMerkleTree<Depth, Hash>::is_complete(size_t depth)
{
    if (!left || !right) {
        return false;
    }

    IsComplete<Hash> calc;

    return parent.ascend(depth, calc);
}

template <size_t Depth, typename Hash>
size_t IncrementalMerkleTree<Depth, Hash>::next_incomplete(size_t skip)
{
    if (!left) {
        if (skip > 0) {
            skip--;
        } else {
            return 0;
        }
    }

    if (!right) {
        if (skip > 0) {
            skip--;
        } else {
            return 0;
        }
    }

    NextIncomplete<Hash> calc(skip);

    return parent.ascend(Depth, calc);
}

template <size_t Depth, typename Hash>
std::vector<bool> IncrementalMerkleTree<Depth, Hash>::index() const
{
    IndexCalculator<Hash> calc(left, right);

    return parent.ascend(Depth, calc);
}

template <size_t Depth, typename Hash>
Hash IncrementalMerkleTree<Depth, Hash>::root(size_t depth) const
{
    std::deque<Hash> fill;

    return root(fill, depth);
}

template <size_t Depth, typename Hash>
Hash IncrementalMerkleTree<Depth, Hash>::root(std::deque<Hash> fill, size_t depth) const
{
    RootCalculator<Hash> calc(left, right, fill);

    return parent.ascend(depth, calc);
}

template <size_t Depth, typename Hash>
std::vector<Hash> IncrementalMerkleTree<Depth, Hash>::path(std::deque<Hash> fill, size_t depth) const
{
    if (!left && !right) {
        throw std::logic_error("cannot create authentication path for empty tree");
    }

    PathCalculator<Hash> calc(*left, right, fill);

    return parent.ascend(depth, calc);
}

template<typename Hash>
template<typename Calculator>
typename Calculator::Result Parent<Hash>::ascend(size_t depth, Calculator& calc) const
{
    depth--;

    boost::optional<typename Calculator::Result> res = calc.feed(depth, left);
    if (res) {
        return *res;
    }

    if (parent) {
        return parent->ascend(depth, calc);
    } else {
        Parent<Hash> p;
        return p.ascend(depth, calc);
    }
}

template <typename Hash>
void Parent<Hash>::advance(Hash hash)
{
    if (!left) {
        left = hash;
    }
    else {
        Hash combined = Hash::combine(*left, hash);
        left = boost::none;

        if (!parent) {
            parent = new Parent();
        }

        parent->advance(combined);
    }
}

} // end namespace