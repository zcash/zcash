/** @file
 *****************************************************************************

 Declaration of interfaces for the class MerkleTree.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MERKLETREE_H_
#define MERKLETREE_H_

#include "Node.h"
#include "libzerocash/utils/sha256.h"
#include "Zerocash.h"

#include <vector>
#include <iostream>
#include <map>
#include <cstring>

#include "libsnark/common/data_structures/merkle_tree.hpp"

namespace libzerocash {

/******************************* Merkle tree *********************************/

class MerkleTree {
private:
    struct cmp_str
    {
        bool operator() (unsigned char * const a, unsigned char * const b)
        {
            return memcmp(a, b, SHA256_BLOCK_SIZE) == 0;
        }
    };

	SHA256_CTX_mod ctx256;
    Node *root;
    std::map<std::vector<bool>, int> coinlist;
    unsigned int actualSize;
    unsigned int depth;
    unsigned int actualDepth;

    std::vector<bool> fullRoot;

    void addCoinMapping(const std::vector<bool> &hashV, int index);
    int getCoinMapping(const std::vector<bool> &hashV);
    void constructWitness(int left, int right, Node* curr, int currentLevel, int index, merkle_authentication_path &witness);

public:
    MerkleTree();
    MerkleTree(unsigned int d);
    MerkleTree(std::vector< std::vector<bool> > coinList, unsigned int d = 64);

    void constructTree(Node *curr, std::vector< std::vector<bool> > &coinList, size_t left, size_t right);
    void getWitness(const std::vector<bool> &coin, merkle_authentication_path &authentication_path);

    void getRootValue(std::vector<bool>& r);
    void getSubtreeRootValue(std::vector<bool>& r);
};

} /* namespace libzerocash */

#endif /* MERKLETREE_H_ */

