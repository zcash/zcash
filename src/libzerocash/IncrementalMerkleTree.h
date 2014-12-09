#ifndef INCREMENTALMERKLETREE_H_
#define INCREMENTALMERKLETREE_H_

#include "utils/sha256.h"

#include "Zerocash.h"
#include <vector>
#include <iostream>
#include <map>
#include <cstring>

#include "../serialize.h"

namespace libzerocash {

class IncrementalMerkleTreeCompact {
public:
	uint32_t										treeHeight;
    std::vector< std::vector<unsigned char> >   	hashVec;
    std::vector< bool >								hashList;
    std::vector< unsigned char >					hashListBytes;
    
	IncrementalMerkleTreeCompact() : treeHeight(0) {}
    void		clear() { hashVec.clear(); hashList.clear(); hashListBytes.clear(); treeHeight = 0; }
    uint32_t	getHeight() { return this->treeHeight; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(treeHeight);
        READWRITE(hashVec);
        READWRITE(hashListBytes);
    }
};

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
    bool getWitness(const std::vector<bool> &index, auth_path &witness);
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
    
class IncrementalMerkleTree {
protected:
    
	IncrementalMerkleNode	 root;
    uint32_t   				 treeHeight;

public:
    IncrementalMerkleTree(uint32_t height = ZEROCASH_DEFAULT_TREE_SIZE);
    IncrementalMerkleTree(std::vector< std::vector<bool> > &valueVector, uint32_t height);
	IncrementalMerkleTree(IncrementalMerkleTreeCompact &compact);
    
    bool insertElement(const std::vector<bool> &hashV, std::vector<bool> &index); // TODO: move to protected/private
	bool insertElement(const std::vector<unsigned char> &hashV, std::vector<unsigned char> &index);
    bool insertVector(std::vector< std::vector<bool> > &valueVector);
    bool getWitness(const std::vector<bool> &index, auth_path &witness);
    bool getRootValue(std::vector<bool>& r);
	bool getRootValue(std::vector<unsigned char>& r);
	std::vector<unsigned char>getRoot();
    bool prune();
    bool getCompactRepresentation(IncrementalMerkleTreeCompact &rep);
    IncrementalMerkleTreeCompact getCompactRepresentation();

    bool fromCompactRepresentation(IncrementalMerkleTreeCompact &rep);

};

} /* namespace olibzerocash */
#endif /* INCREMENTALMERKLETREE_H_ */

