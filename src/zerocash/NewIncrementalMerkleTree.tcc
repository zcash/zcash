#include <stdexcept>

namespace NewTree {

template <typename Hash>
class PathFiller {
private:
    std::deque<Hash> queue;
public:
    PathFiller() : queue() { }
    PathFiller(std::deque<Hash> queue) : queue(queue) { }

    Hash next() {
        if (queue.size() > 0) {
            Hash h = queue.front();
            queue.pop_front();

            return h;
        } else {
            return Hash();
        }
    }
};

template<size_t Depth, typename Hash>
void IncrementalMerkleTree<Depth, Hash>::append(Hash obj) {
    if (!left) {
        left = obj;
    } else if (!right) {
        right = obj;
    } else {
        boost::optional<Hash> combined = Hash::combine(*left, *right);
        left = obj;
        right = boost::none;

        BOOST_FOREACH(boost::optional<Hash>& parent, parents) {
            if (parent) {
                combined = Hash::combine(*parent, *combined);
                parent = boost::none;
            } else {
                parent = *combined;
                combined = boost::none;
                break;
            }
        }

        if (combined) {
            parents.push_back(combined);
        }
    }
}

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

template<size_t Depth, typename Hash>
Hash IncrementalMerkleTree<Depth, Hash>::root(size_t depth,
                                              std::deque<Hash> filler_hashes) const {
    PathFiller<Hash> filler(filler_hashes);

    Hash combine_left =  left  ? *left  : filler.next();
    Hash combine_right = right ? *right : filler.next();

    Hash root = Hash::combine(combine_left, combine_right);

    size_t d = 1;

    BOOST_FOREACH(const boost::optional<Hash>& parent, parents) {
        if (parent) {
            root = Hash::combine(*parent, root);
        } else {
            root = Hash::combine(root, filler.next());
        }

        d++;
    }

    while (d < depth) {
        root = Hash::combine(root, filler.next());
        d++;
    }

    return root;
}

template<size_t Depth, typename Hash>
MerklePath IncrementalMerkleTree<Depth, Hash>::path(std::deque<Hash> filler_hashes) const {
    if (!left) {
        throw std::runtime_error("can't create an authentication path for the beginning of the tree");
    }

    PathFiller<Hash> filler(filler_hashes);

    std::vector<Hash> path;
    std::vector<bool> index;

    if (right) {
        index.push_back(true);
        path.push_back(*left);
    } else {
        index.push_back(false);
        path.push_back(filler.next());
    }

    size_t d = 1;

    BOOST_FOREACH(const boost::optional<Hash>& parent, parents) {
        if (parent) {
            index.push_back(true);
            path.push_back(*parent);
        } else {
            index.push_back(false);
            path.push_back(filler.next());
        }

        d++;
    }

    while (d < Depth) {
        index.push_back(false);
        path.push_back(filler.next());
        d++;
    }

    std::vector<std::vector<bool>> merkle_path;
    BOOST_FOREACH(Hash b, path)
    {
        std::vector<unsigned char> hashv(b.begin(), b.end());
        std::vector<bool> tmp_b;

        libzerocash::convertBytesVectorToVector(hashv, tmp_b);

        merkle_path.push_back(tmp_b);
    }

    std::reverse(merkle_path.begin(), merkle_path.end());
    std::reverse(index.begin(), index.end());

    return MerklePath(merkle_path, index);
}

template<size_t Depth, typename Hash>
std::deque<Hash> IncrementalWitness<Depth, Hash>::uncle_train() const {
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

        if (cursor_depth == 0) {
            filled.push_back(obj);
        } else {
            cursor = IncrementalMerkleTree<Depth, Hash>();
            cursor->append(obj);
        }
    }
}

} // end namespace