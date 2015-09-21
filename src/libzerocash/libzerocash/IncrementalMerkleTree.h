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

#include "serialize.h"

#include "libsnark/common/data_structures/merkle_tree.hpp"

namespace libzerocash {

/******************* Incremental Merkle tree compact *************************/

class IncrementalMerkleTreeCompact {
public:
	uint32_t										treeHeight;
    std::vector< std::vector<unsigned char> >   	hashVec;
    std::vector< bool >								hashList;
    std::vector< unsigned char >					hashListBytes;

	IncrementalMerkleTreeCompact() : treeHeight(0) {}
    void		clear() { hashVec.clear(); hashList.clear(); hashListBytes.clear(); treeHeight = 0; }
    uint32_t	getHeight() { return this->treeHeight; }

    IMPLEMENT_SERIALIZE
	(
	 READWRITE(treeHeight);
     READWRITE(hashVec);
     READWRITE(hashListBytes);
    )
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
    bool getCompactRepresentation(IncrementalMerkleTreeCompact &rep);
    bool fromCompactRepresentation(IncrementalMerkleTreeCompact &rep, uint32_t pos);

    // Utility methods
    bool isLeaf()   { return (nodeDepth == treeHeight); }
    bool isPruned() { return subtreePruned; }
    bool hasFreeLeaves() { return (!subtreeFull); }
    bool hasRightChildren() { if (!right) return false; return true; }
    void getValue(std::vector<bool> &r) { r = value; }
    std::vector<bool>& getValue() { return value; }

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

    bool insertElement(const std::vector<bool> &hashV, std::vector<bool> &index);
	bool insertElement(const std::vector<unsigned char> &hashV, std::vector<unsigned char> &index);
    bool insertVector(std::vector< std::vector<bool> > &valueVector);
    bool getWitness(const std::vector<bool> &index, merkle_authentication_path &witness);
    bool getRootValue(std::vector<bool>& r);
	bool getRootValue(std::vector<unsigned char>& r);
	std::vector<unsigned char>getRoot();
    bool prune();
    bool getCompactRepresentation(IncrementalMerkleTreeCompact &rep);
    IncrementalMerkleTreeCompact getCompactRepresentation();

    bool fromCompactRepresentation(IncrementalMerkleTreeCompact &rep);

};

} /* namespace libzerocash */

#endif /* INCREMENTALMERKLETREE_H_ */

