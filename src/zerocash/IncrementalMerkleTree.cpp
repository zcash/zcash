/** @file
 *****************************************************************************

 Implementation of interfaces for the classes IncrementalMerkleTreeCompact,
 IncrementalMerkleNode, and IncrementalMerkleTree.

 See IncrementalMerkleTree.h .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "IncrementalMerkleTree.h"
#include "Zerocash.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace libzerocash {

    /////////////////////////////////////////////
    // IncrementalMerkleTreeCompact class
    /////////////////////////////////////////////

    std::vector<unsigned char> IncrementalMerkleTreeCompact::serialize() const {
        /* Serialization format:
         *  treeHeight (4 bytes, big endian)
         *  hashList (ceil(treeHeight / 8) bytes)
         *  hashVec (32 bytes for every 1 bit in hashList)
         */
        std::vector<unsigned char> serialized;

        /* treeHeight (4 bytes, big endian) */
        std::vector<unsigned char> treeHeightBytes(4);
        convertIntToBytesVector((uint64_t)this->treeHeight, treeHeightBytes);
        serialized.insert(serialized.end(), treeHeightBytes.begin(), treeHeightBytes.end());

        /* hashList */
        assert(this->hashList.size() == this->treeHeight);

        /* Pad it out to a multiple of 8 bits. */
        std::vector<bool> hashList = this->hashList;
        if (hashList.size() % 8 != 0) {
            hashList.insert(hashList.begin(), 8 - (hashList.size() % 8), false);
        }
        assert(hashList.size() % 8 == 0);

        /* Convert it to a byte vector. */
        std::vector<unsigned char> hashListBytes(hashList.size() / 8);
        convertVectorToBytesVector(hashList, hashListBytes);
        serialized.insert(serialized.end(), hashListBytes.begin(), hashListBytes.end());

        /* hashVec */
        assert(this->hashVec.size() == countOnes(this->hashList));
        for (uint32_t i = 0; i < this->hashVec.size(); i++) {
            assert(this->hashVec.at(i).size() == 32);
            serialized.insert(serialized.end(), this->hashVec.at(i).begin(), this->hashVec.at(i).end());
        }

        return serialized;
    }

    IncrementalMerkleTreeCompact IncrementalMerkleTreeCompact::deserialize(const std::vector<unsigned char>& serialized) {
        IncrementalMerkleTreeCompact deserialized;

        size_t currentPos = 0;


        /* treeHeight */
        std::vector<unsigned char> treeHeightBytes = vectorSlice(serialized, 0, 4);
        currentPos += 4;
        deserialized.treeHeight = convertBytesVectorToInt(treeHeightBytes);

        /* hashList */
        uint32_t hashListBytesLength = ceil(deserialized.treeHeight / 8.0);
        std::vector<unsigned char> hashListBytes = vectorSlice(serialized, currentPos, hashListBytesLength);
        currentPos += hashListBytesLength;
        convertBytesVectorToVector(hashListBytes, deserialized.hashList);
        /* Remove the multiple-of-8-bits padding. */
        deserialized.hashList.erase(deserialized.hashList.begin(),
                                    deserialized.hashList.end() - deserialized.treeHeight
                                   );

        /* hashVec */
        size_t hashVecSize = countOnes(deserialized.hashList);
        for (size_t i = 0; i < hashVecSize; i++) {
            std::vector<unsigned char> hashVecElement = vectorSlice(serialized, currentPos, 32);
            currentPos += 32;
            deserialized.hashVec.push_back(hashVecElement);
        }

        if (currentPos != serialized.size()) {
            throw std::runtime_error("Serialized vector is longer than expected.");
        }

        return deserialized;
    }

    /////////////////////////////////////////////
    // IncrementalMerkleTree class
    /////////////////////////////////////////////

    // Custom tree constructor (initialize tree of specified height)
    IncrementalMerkleTree::IncrementalMerkleTree(uint32_t height) : root(0, height) {
        treeHeight = height;
    }

    // Vector constructor. Initializes and inserts a list of elements.
    IncrementalMerkleTree::IncrementalMerkleTree(std::vector< std::vector<bool> > &valueVector, uint32_t height) : root(0, height)
    {
        // Initialize the tree
        treeHeight = height;

        // Load the tree with all the given values
        if (this->insertVector(valueVector) == false) {
			throw std::runtime_error("Could not insert vector into Merkle Tree: too many elements");
		}
    }

    // Custom tree constructor (initialize tree from compact representation)
    //
    IncrementalMerkleTree::IncrementalMerkleTree(IncrementalMerkleTreeCompact &compact) : root(0, 0)
	{
        // Initialize the tree
        this->treeHeight = compact.getHeight();
        root.treeHeight = treeHeight;

        // Reconstitute tree from compact representation
        this->fromCompactRepresentation(compact);
    }

    bool
    IncrementalMerkleTree::insertElement(const std::vector<bool> &hashV, std::vector<bool> &index) {

        // Resize the index vector
        index.resize(this->treeHeight);

        // Insert the element
        return this->root.insertElement(hashV, index);
    }

	bool
    IncrementalMerkleTree::insertElement(const std::vector<unsigned char> &hashV, std::vector<unsigned char> &index) {

		// Create a temporary vector to hold hashV
		std::vector<bool> hashVBool(hashV.size() * 8);
		convertBytesVectorToVector(hashV, hashVBool);

        // Create a temporary vector to hold the index
		std::vector<bool> indexBool(this->treeHeight, 0);

        // Insert the element
        bool result = this->insertElement(hashVBool, indexBool);

		// Convert the returned vector
		index.resize(index.size() / 8); // this might need to include a ceil
		convertVectorToBytesVector(indexBool, index);

		return result;
    }

    bool
    IncrementalMerkleTree::getWitness(const std::vector<bool> &index, merkle_authentication_path &witness) {

		// Resize the witness if necessary
		if (witness.size() < this->treeHeight) {
			witness.resize(treeHeight);
		}

		std::vector<bool> indexPadded = index;

		// Discard leading bits of the index if necessary
		if (indexPadded.size() > this->treeHeight) {
			indexPadded.erase(indexPadded.begin(), indexPadded.begin() + (indexPadded.size() - this->treeHeight));
		}

		// If the index vector is less than 'treeHeight' bits, pad the leftmost bits with 0 (false)
		// This is to deal with the situation where somebody encodes e.g., a 32-bit integer as an index
		// into a 64 height tree and does not explicitly pad to length.
		if (indexPadded.size() < this->treeHeight) {
			indexPadded.insert(indexPadded.begin(), (this->treeHeight - 1) - indexPadded.size(), false);
		}

        return this->root.getWitness(indexPadded, witness);
    }

    bool
    IncrementalMerkleTree::insertVector(std::vector< std::vector<bool> > &valueVector)
    {
        std::vector<bool> index;

        for (std::vector< std::vector<bool> >::iterator iter = valueVector.begin();
             iter != valueVector.end(); ++iter) {

            if (this->insertElement(*iter, index) == false) {
				return false;
			}

        }

        return true;
    }

    bool
    IncrementalMerkleTree::getRootValue(std::vector<bool>& r) const {

        // Query the root for its hash
        this->root.getValue(r);
        return true;
    }

	bool
    IncrementalMerkleTree::getRootValue(std::vector<unsigned char>& r) const {

		// Create a temporary byte vector
		std::vector<bool> tempR(r.size() * 8, 0);

        // Query the root for its hash
        this->root.getValue(tempR);

		// Convert the result back into the given vector
		convertVectorToBytesVector(tempR, r);

        return true;
    }
	std::vector<unsigned char>
	IncrementalMerkleTree::getRoot(){
		std::vector<unsigned char> temp(8);
		this->getRootValue(temp);
		return temp;
	}

    bool
    IncrementalMerkleTree::prune()
    {
		return this->root.prune();
    }

    IncrementalMerkleTreeCompact
    IncrementalMerkleTree::getCompactRepresentation() const
    {
        IncrementalMerkleTreeCompact rep;
        rep.hashList.resize(this->treeHeight);
		rep.treeHeight = this->treeHeight;
        std::fill (rep.hashList.begin(), rep.hashList.end(), false);

		this->root.getCompactRepresentation(rep);
        return rep;
    }

    bool
    IncrementalMerkleTree::fromCompactRepresentation(IncrementalMerkleTreeCompact &rep)
    {
		return this->root.fromCompactRepresentation(rep, 0);
	}

    /////////////////////////////////////////////
    // IncrementalMerkleNode class
    /////////////////////////////////////////////

    // Standard constructor
    //
    IncrementalMerkleNode::IncrementalMerkleNode(uint32_t depth, uint32_t height) : left(NULL), right(NULL), value(SHA256_BLOCK_SIZE * 8, 0), nodeDepth(depth), treeHeight(height),
				subtreeFull(false), subtreePruned(false)
    {
        sha256_init(&ctx256);
    }

    // Copy constructor
    //
    IncrementalMerkleNode::IncrementalMerkleNode(const IncrementalMerkleNode& toCopy) : left(NULL), right(NULL), value(SHA256_BLOCK_SIZE * 8, 0)
    {
        sha256_init(&ctx256);
        this->nodeDepth = toCopy.nodeDepth;
        this->subtreePruned = toCopy.subtreePruned;
        this->subtreeFull = toCopy.subtreeFull;
        this->value = toCopy.value;
		this->treeHeight = toCopy.treeHeight;

        // Recursively copy the subtrees
        if (toCopy.left) {
			this->left = new IncrementalMerkleNode(toCopy.left->nodeDepth, toCopy.left->treeHeight);
            *(this->left) = *(toCopy.left);
        }

        if (toCopy.right) {
			this->right = new IncrementalMerkleNode(toCopy.right->nodeDepth, toCopy.right->treeHeight);
            *(this->right) = *(toCopy.right);
        }
    }

    IncrementalMerkleNode::~IncrementalMerkleNode()
    {
        if (this->left) {
            delete this->left;
            this->left = NULL;
        }

        if (this->right) {
            delete this->right;
            this->right = NULL;
        }
    }

    bool
    IncrementalMerkleNode::insertElement(const std::vector<bool> &hashV, std::vector<bool> &index)
    {
        bool result = false;

        // Check if we have any free leaves. If not, bail.
        if (this->subtreeFull == true) {
            return false;
        }

        // Are we a leaf? If so, store the hash value.
        if (this->isLeaf()) {
            // Store the given hash value here and return success.
            this->value = hashV;
            this->subtreeFull = true;
            return true;
        }

        // We're not a leaf. Try to insert into subtrees, creating them if necessary.
        // Try to recurse on left subtree
        if (!this->left) {
            this->left = new IncrementalMerkleNode(this->nodeDepth + 1, this->treeHeight);
        }
        result = this->left->insertElement(hashV, index);
        if (result == true) {
            // Update the index value to indicate where the new node went
            index.at(this->nodeDepth) = false;
        }

        // If that failed, try to recurse on right subtree.
        if (result == false) {
            if (!this->right) {
                this->right = new IncrementalMerkleNode(this->nodeDepth + 1, this->treeHeight);
            }
            result = this->right->insertElement(hashV, index);
            if (result == true) {
                index.at(this->nodeDepth) = true;
            }
        }

        // If one of the inserts succeeded, update our 'fullness' status.
        if (result == true) {
            this->updateHashValue();
            if (this->isLeaf()) { this->subtreeFull = true; }
            else {
                this->subtreeFull = this->checkIfNodeFull();
            }
        }

        // Return the result
        return result;
    }

    bool
    IncrementalMerkleNode::getWitness(const std::vector<bool> &index, merkle_authentication_path &witness)
    {
        bool result = false;

        // If this node is a leaf: do nothing and return success
        if (this->isLeaf()) {
            return true;
        }

		// If this node is pruned, we can't fetch a witness. Return failure.
		if (this->isPruned()) {
			return false;
		}

        // If the index path leads to the left, we grab the hash value on the
        // right -- then recurse on the left node.
        if (index.at(nodeDepth) == false) {

			// Make sure there is a value on the right. If not we put the 'null' hash (0) into that element.
			if (this->right == NULL) {
				witness.at(nodeDepth).resize(SHA256_BLOCK_SIZE * 8);
				std::fill (witness.at(nodeDepth).begin(), witness.at(nodeDepth).end(), false);
			} else {
				this->right->getValue(witness.at(nodeDepth));
				//printVectorAsHex(witness.at(nodeDepth));
			}

            // Recurse on the left node
            if (this->left) {
                result = this->left->getWitness(index, witness);
            }
        }

        // If the index path leads to the right, we grab the hash value on the
        // left -- then recurse on the right node.
        if (index.at(nodeDepth) == true) {
            this->left->getValue(witness.at(nodeDepth));

            // Recurse on the right node
            if (this->right) {
                result = this->right->getWitness(index, witness);
            }
        }

        return result;
    }

    bool
    IncrementalMerkleNode::prune()
    {
        bool result = true;

        // If we're already pruned, return.
        if (this->isPruned() == true) {
            return true;
        }

        // Check to see if this node is full. If so, delete the subtrees.
        if (this->subtreeFull == true) {
            if (this->left) {
                delete this->left;
                this->left = NULL;
            }

            if (this->right) {
                delete this->right;
                this->right = NULL;
            }

            this->subtreePruned = true;
        } else {
            // Node is not full. Recurse on left and right.
            if (this->left) {
                result &= this->left->prune();
            }

            if (this->right) {
                result &= this->right->prune();
            }
        }

        return result;
    }

    void
    IncrementalMerkleNode::updateHashValue()
    {
        // Take no action on leaves or pruned nodes.
        if (this->isLeaf() || this->isPruned()) {
            return;
        }

        // Obtain the hash of the two subtrees and hash the
        // concatenation of the two.
        std::vector<bool> hash(SHA256_BLOCK_SIZE * 8);
        std::vector<bool> zero(SHA256_BLOCK_SIZE * 8);
        std::fill (zero.begin(), zero.end(), false);

		// The following code is ugly and should be refactored. It runs
		// four special cases depending on whether left/right is NULL.
		// It also ensures that the "hash" of (0 || 0) is 0.
        if (this->left && !(this->right)) {
			if (VectorIsZero(this->left->getValue())) {
				hash = zero;
			} else {
				hashVectors(&ctx256, this->left->getValue(), zero, hash);
			}
        } else if (!(this->left) && this->right) {
			if (VectorIsZero(this->right->getValue())) {
				hash = zero;
			} else {
				hashVectors(&ctx256, zero, this->left->getValue(), hash);
			}
        } else if (this->left && this->right) {
			if (VectorIsZero(this->left->getValue()) && VectorIsZero(this->right->getValue())) {
				hash = zero;
			} else {
				hashVectors(&ctx256, this->left->getValue(), this->right->getValue(), hash);
			}
        } else {
            hash = zero;
        }

        this->value = hash;
    }

    bool
    IncrementalMerkleNode::checkIfNodeFull()
    {
        if (this->isPruned()) {
            return true;
        }

        if (this->left == NULL || this->right == NULL) {
            return false;
        }

        return (this->left->subtreeFull && this->right->subtreeFull);
    }

    void
    IncrementalMerkleNode::getCompactRepresentation(IncrementalMerkleTreeCompact &rep) const
    {
        // Do nothing at the bottom level
        if (this->isLeaf()) {
            return;
        }

        // There's no content below us. We're done
        if (!this->left) {
            return;
        }

        // If we have no right elements, don't include any hashes. Recurse to the left.
        if (this->hasRightChildren() == false && this->left->isLeaf() == false && this->left->subtreeFull == false) {
            rep.hashList.at(this->nodeDepth) = false;
            this->left->getCompactRepresentation(rep);
            return;
        }

        // Otherwise: Add our left child hash to the tree.
        rep.hashList.at(this->nodeDepth) = true;
        std::vector<unsigned char> hash(SHA256_BLOCK_SIZE, 0);
        convertVectorToBytesVector(this->left->getValue(), hash);
        rep.hashVec.push_back(hash);

        // If we have a right child, recurse to the right
        if (this->hasRightChildren()) {
            this->right->getCompactRepresentation(rep);
            return;
        }

        // We get here in one of the following cases:
        // 1. Our left child is a leaf, and there's no right child.
        // 2. Our left child is a full tree, and there's no right child.

        // We've gone right for the last time, now we go left until we reach the
        // bottom.
        for (uint32_t i = this->nodeDepth + 1; i < this->treeHeight; i++) {
            rep.hashList.at(i) = false;
        }
    }

    bool
    IncrementalMerkleNode::fromCompactRepresentation(IncrementalMerkleTreeCompact &rep, uint32_t pos)
    {
        bool result = false;

        // Do nothing at the bottom level
        if (this->isLeaf()) {
            return true;
        }

        // If we have any subtrees (or this tree already has stuff in it), clean it out.
        if (this->left) {
            // XXX memory leak: left might have the only pointers to its heap
            // allocated children!
            delete this->left;
            this->left = NULL;
        }
        if (this->right) {
            // XXX memory leak: right might have the only pointers to its heap
            // allocated children!
            delete this->right;
            this->right = NULL;
        }
        this->subtreeFull = this->subtreePruned = false;

        // If the hashList[nodeDepth] is true, insert the next hash into the left tree
        // and mark it full AND pruned. Then recurse to the right.
        if (rep.hashList.at(this->nodeDepth) == true) {
			// Create a left node
			this->left = new IncrementalMerkleNode(this->nodeDepth + 1, this->treeHeight);

            // Fill the left node with the value and mark it full/pruned
            std::vector<bool> hash(SHA256_BLOCK_SIZE * 8, 0);
            convertBytesVectorToVector(rep.hashVec.at(pos), hash);
            this->left->value = hash;
            this->left->subtreePruned = this->left->subtreeFull = true;

            // Create a right node and recurse on it (incrementing pos)
            this->right = new IncrementalMerkleNode(this->nodeDepth + 1, this->treeHeight);
            result = this->right->fromCompactRepresentation(rep, pos + 1);
        } else if (this->nodeDepth < (this->treeHeight - 1)) {
			// Otherwise --
			// * If we're about to create a leaf level, do nothing.
			// * Else create a left node and recurse on it.
			this->left = new IncrementalMerkleNode(this->nodeDepth + 1, this->treeHeight);

            // Otherwise recurse on the left node. Do not increment pos.
            result = this->left->fromCompactRepresentation(rep, pos);
        }

        // Update the hash value of this node
        this->updateHashValue();

        return result;
    }

	IncrementalMerkleNode
	IncrementalMerkleNode::operator=(const IncrementalMerkleNode &rhs) {
		IncrementalMerkleNode dup(rhs);
		return dup;
	}

} /* namespace libzerocash */
