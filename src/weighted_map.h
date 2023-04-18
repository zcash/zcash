// Copyright (c) 2019-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WEIGHTED_MAP_H
#define ZCASH_WEIGHTED_MAP_H

#include <map>
#include <optional>
#include <set>
#include <vector>

// A WeightedMap represents a map from keys (of type K) to values (of type V),
// each entry having a weight (of type W). Elements can be randomly selected and
// removed from the map with probability in proportion to their weight. This is
// used to implement mempool limiting specified in ZIP 401, and the block template
// construction algorithm specified in ZIP 317.
//
// In order to efficiently implement random selection by weight, we keep track
// of the total weight of all keys in the map. For performance reasons, the
// map is represented as a binary tree where each node knows the sum of the
// weights of the children. This allows for addition, removal, and random
// selection/dropping in logarithmic time.
//
// random(w) (which will only be called with positive w) must be defined to
// return a uniform random value between zero inclusive and w exclusive.
// The type W must be a signed numeric type that supports addition, binary
// and unary -, and < and <= comparisons, and W() must construct the zero value
// (these constraints are met for primitive signed integer types).
template <typename K, typename V, typename W, W random(W)>
class WeightedMap
{
    // W must be a signed numeric type.
    static_assert(std::numeric_limits<W>::min() < W());

    struct Node {
        K key;
        V value;
        W weight;
        W sumOfDescendantWeights;
    };

    // The following vector is the tree representation of this collection.
    // For each node, we keep track of the key, its associated value,
    // its weight, and the sum of the weights of all its descendants.
    std::vector<Node> nodes;

    // The following map is to simplify removal.
    std::map<K, size_t> indexMap;

    static inline size_t leftChild(size_t i) { return i*2 + 1; }
    static inline size_t rightChild(size_t i) { return i*2 + 2; }
    static inline size_t parent(size_t i) { return (i-1)/2; }

public:
    // Check internal invariants (for tests).
    void checkInvariants() const
    {
        assert(indexMap.size() == nodes.size());
        for (size_t i = 0; i < nodes.size(); i++) {
            assert(indexMap.at(nodes.at(i).key) == i);
            assert(nodes.at(i).sumOfDescendantWeights == getWeightAt(leftChild(i)) + getWeightAt(rightChild(i)));
        }
    }

private:
    // Return the sum of weights of the node at a given index and all of its descendants.
    W getWeightAt(size_t index) const
    {
        if (index >= nodes.size()) {
            return W();
        }
        auto& node = nodes.at(index);
        return node.weight + node.sumOfDescendantWeights;
    }

    // When adding and removing a node we need to update its parent and all of its
    // ancestors to reflect its weight.
    void backPropagate(size_t fromIndex, W weightDelta)
    {
        while (fromIndex > 0) {
            fromIndex = parent(fromIndex);
            nodes[fromIndex].sumOfDescendantWeights += weightDelta;
        }
    }

    // For a given random weight, this method recursively finds the index of the
    // correct entry. This is used by WeightedMap::takeRandom().
    size_t findByWeight(size_t fromIndex, W weightToFind) const
    {
        W leftWeight = getWeightAt(leftChild(fromIndex));
        // On Left
        if (weightToFind < leftWeight) {
            return findByWeight(leftChild(fromIndex), weightToFind);
        }
        W rightWeight = getWeightAt(fromIndex) - getWeightAt(rightChild(fromIndex));
        // Found
        if (weightToFind < rightWeight) {
            return fromIndex;
        }
        // On Right
        return findByWeight(rightChild(fromIndex), weightToFind - rightWeight);
    }

public:
    WeightedMap() {}

    // Return the total weight of all entries in the map.
    W getTotalWeight() const
    {
        return getWeightAt(0);
    }

    // Return true when the map has no entries.
    bool empty() const
    {
        return nodes.empty();
    }

    // Return the number of entries.
    size_t size() const
    {
        return nodes.size();
    }

    // Return false if the key already exists in the map.
    // Otherwise, add an entry mapping `key` to `value` with the given weight,
    // and return true. The weight must be positive.
    bool add(K key, V value, W weight)
    {
        assert(W() < weight);
        if (indexMap.count(key) > 0) {
            return false;
        }
        size_t index = nodes.size();
        nodes.push_back(Node {
            .key = key,
            .value = value,
            .weight = weight,
            .sumOfDescendantWeights = W(),
        });
        indexMap[key] = index;
        backPropagate(index, weight);
        return true;
    }

    // If the given key is not present in the map, return std::nullopt.
    // Otherwise, remove that key's entry and return its associated value.
    std::optional<V> remove(K key)
    {
        auto it = indexMap.find(key);
        if (it == indexMap.end()) {
            return std::nullopt;
        }

        size_t removeIndex = it->second;
        V removeValue = nodes.at(removeIndex).value;

        size_t lastIndex = nodes.size()-1;
        Node lastNode = nodes.at(lastIndex);
        W weightDelta = lastNode.weight - nodes.at(removeIndex).weight;
        backPropagate(lastIndex, -lastNode.weight);

        if (removeIndex < lastIndex) {
            nodes[removeIndex].key = lastNode.key;
            nodes[removeIndex].value = lastNode.value;
            nodes[removeIndex].weight = lastNode.weight;
            // nodes[removeIndex].sumOfDescendantWeights should not change here.
            indexMap[lastNode.key] = removeIndex;
            backPropagate(removeIndex, weightDelta);
        }

        indexMap.erase(it);
        nodes.pop_back();
        return removeValue;
    }

    // If the map is empty, return std::nullopt. Otherwise, pick a random entry
    // with probability proportional to its weight; remove it and return a tuple of
    // the key, its associated value, and its weight.
    std::optional<std::tuple<K, V, W>> takeRandom()
    {
        if (empty()) {
            return std::nullopt;
        }
        W totalWeight = getTotalWeight();
        assert(W() < totalWeight);
        W randomWeight = random(totalWeight);
        assert(W() <= randomWeight && randomWeight < totalWeight);
        size_t index = findByWeight(0, randomWeight);
        assert(index < nodes.size());
        const Node& drop = nodes.at(index);
        auto res = std::make_tuple(drop.key, drop.value, drop.weight); // copy values
        remove(drop.key);
        return res;
    }
};

#endif // ZCASH_WEIGHTED_MAP_H
