/** @file
 *****************************************************************************

 Declaration of interfaces for the classes IncrementalMerkleTreeCompact,
 IncrementalMerkleNode, and IncrementalMerkleTree.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef INCREMENTALMERKLETREE_H_
#define INCREMENTALMERKLETREE_H_

#include "utils/sha256.h"

#include "Zerocash.h"
#include <vector>
#include <iostream>
#include <map>
#include <cstring>

#include "libsnark/common/data_structures/merkle_tree.hpp"

namespace libzerocash {

/******************* Incremental Merkle tree compact *************************/

/* This is a comapct way to represent an incremental merkle tree, where all full
 * subtrees are replaced by their hashes. It contains just enough information
 * that you can continue addding elements to the tree.
 *
 * This class can only be constructed by IncrementalMerkleTree, and after that,
 * it is immutable. To act on a compact representation, it must first be
 * de-compactified by loading it into an IncrementalMerkleTree.
 */
class IncrementalMerkleTreeCompact {
    friend class IncrementalMerkleTree;
    friend class IncrementalMerkleNode;
public:
    uint32_t getHeight() { return this->treeHeight; }

    uint32_t getTreeHeight() { return treeHeight; }
    std::vector< std::vector<unsigned char> > const& getHashVec() { return hashVec; }
    std::vector< bool > const& getHashList() { return hashList; }

    std::vector<unsigned char> serialize() const;
    static IncrementalMerkleTreeCompact deserialize(const std::vector<unsigned char>& serialized);

private:
    IncrementalMerkleTreeCompact() : treeHeight(0) {}
    uint32_t treeHeight;
    std::vector< std::vector<unsigned char> > hashVec;
    std::vector< bool > hashList;
};

/********************* Incremental Merkle tree node **************************/

class IncrementalMerkleNode {
public:
    SHA256_CTX_mod ctx256;
	IncrementalMerkleNode* left;
    IncrementalMerkleNode* right;
    std::vector<bool> value;
    uint32_t nodeDepth;
    uint32_t treeHeight;
    bool subtreeFull;
    bool subtreePruned;

    IncrementalMerkleNode(uint32_t depth, uint32_t height);
    IncrementalMerkleNode(const IncrementalMerkleNode& toCopy);
    ~IncrementalMerkleNode();

    // Methods
    bool insertElement(const std::vector<bool> &hashV, std::vector<bool> &index);
    bool getWitness(const std::vector<bool> &index, merkle_authentication_path &witness);
    bool prune();
    void getCompactRepresentation(IncrementalMerkleTreeCompact &rep) const;
    bool fromCompactRepresentation(IncrementalMerkleTreeCompact &rep, uint32_t pos);

    // Utility methods
    bool isLeaf() const  { return (nodeDepth == treeHeight); }
    bool isPruned() const { return subtreePruned; }
    bool hasFreeLeaves() const { return (!subtreeFull); }
    bool hasRightChildren() const { if (!right) return false; return true; }
    void getValue(std::vector<bool> &r) const { r = value; }
    const std::vector<bool>& getValue() const { return value; }

    bool checkIfNodeFull();
    void updateHashValue();

	IncrementalMerkleNode	operator=(const IncrementalMerkleNode &rhs);
};

/************************ Incremental Merkle tree ****************************/

class IncrementalMerkleTree {
protected:

	IncrementalMerkleNode	 root;
    uint32_t   				 treeHeight;

public:
    IncrementalMerkleTree(uint32_t height = ZEROCASH_DEFAULT_TREE_SIZE);
    IncrementalMerkleTree(std::vector< std::vector<bool> > &valueVector, uint32_t height);
	IncrementalMerkleTree(IncrementalMerkleTreeCompact &compact);

    void setTo(const IncrementalMerkleTree &other) {
        auto compact = other.getCompactRepresentation();
        fromCompactRepresentation(compact);
    }

    bool insertElement(const std::vector<bool> &hashV, std::vector<bool> &index);
	bool insertElement(const std::vector<unsigned char> &hashV, std::vector<unsigned char> &index);
    bool insertVector(std::vector< std::vector<bool> > &valueVector);
    bool getWitness(const std::vector<bool> &index, merkle_authentication_path &witness);
    bool getRootValue(std::vector<bool>& r) const;
	bool getRootValue(std::vector<unsigned char>& r) const;
	std::vector<unsigned char>getRoot();
    bool prune();
    IncrementalMerkleTreeCompact getCompactRepresentation() const;
    std::vector<unsigned char> serialize() const {
        auto compact = getCompactRepresentation();
        return compact.serialize();
    }

    static IncrementalMerkleTree deserialize(const std::vector<unsigned char>& serialized) {
        auto deserialized = IncrementalMerkleTreeCompact::deserialize(serialized);
        return IncrementalMerkleTree(deserialized);
    }

    bool fromCompactRepresentation(IncrementalMerkleTreeCompact &rep);

};

} /* namespace libzerocash */

#endif /* INCREMENTALMERKLETREE_H_ */

