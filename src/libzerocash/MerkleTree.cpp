#include "MerkleTree.h"
#include "Zerocash.h"

#include <cmath>
#include <iostream>

namespace libzerocash {

MerkleTree::MerkleTree() {
    sha256_init(&ctx256);

    root = new Node();

    depth = 64;

    actualDepth = 0;

    fullRoot = std::vector<bool>(SHA256_BLOCK_SIZE * 8, 0);

    coinlist.clear();
}

MerkleTree::MerkleTree(unsigned int d) {
	sha256_init(&ctx256);

    root = new Node();

    depth = d;

    actualDepth = 0;

    fullRoot = std::vector<bool>(SHA256_BLOCK_SIZE * 8, 0);

    coinlist.clear();
}

MerkleTree::MerkleTree(std::vector< std::vector<bool> > coinList, unsigned int d) {
	sha256_init(&ctx256);

    root = new Node();

    depth = d;

    coinlist.clear();

    fullRoot = std::vector<bool>(SHA256_BLOCK_SIZE * 8, 0);

    unsigned int size = coinList.size();
    actualSize = size;
    actualDepth = ceil(log(actualSize)/log(2));

    if(size == 1) {
        coinList.resize(2, std::vector<bool>(cm_size * 8, 0));
        constructTree(root, coinList, 0, coinList.size()-1);
    }
    else if(!(size == 0) && !(size & (size - 1))){
        constructTree(root, coinList, 0, coinList.size()-1);
    }
    else {
        unsigned int newSize = ceil(log(size)/log(2));
        coinList.resize(pow(2, newSize), std::vector<bool>(cm_size * 8, 0));
        constructTree(root, coinList, 0, coinList.size()-1);
    }

    if(ceil(log(actualSize)/log(2)) == 0) {
        std::vector<bool> hash(SHA256_BLOCK_SIZE * 8);
        std::vector<bool> zeros(cm_size * 8, 0);
        hashVectors(&ctx256, root->value, zeros, hash);

        for(int i = 0; i < depth-ceil(log(actualSize)/log(2))-2; i++) {
            hashVectors(&ctx256, hash, zeros, hash);
        }
        fullRoot = hash;
    }
    else if(ceil(log(actualSize)/log(2)) != depth) {
        std::vector<bool> hash(SHA256_BLOCK_SIZE * 8);
        std::vector<bool> zeros(cm_size * 8, 0);
        hashVectors(&ctx256, root->value, zeros, hash);

        for(int i = 0; i < depth-ceil(log(actualSize)/log(2))-1; i++) {
            hashVectors(&ctx256, hash, zeros, hash);
        }
        fullRoot = hash;
    }
    else {
        fullRoot = root->value;
    }
}

void MerkleTree::constructTree(Node* curr, std::vector< std::vector<bool> > &coinList, size_t left, size_t right) {
    if((right == left)) {
        if(left < actualSize)
            addCoinMapping(coinList.at(left), left);

        curr->value = coinList.at(left);
    }
    else {
        curr->left = new Node();
        curr->right = new Node();

        constructTree(curr->left, coinList, left, int((left+right)/2));
        constructTree(curr->right, coinList, int((left+right)/2)+1, right);

        std::vector<bool> hash(SHA256_BLOCK_SIZE * 8);
        hashVectors(&ctx256, curr->left->value, curr->right->value, hash);

        curr->value = hash;
    }
}

void MerkleTree::addCoinMapping(const std::vector<bool> &hashV, int index) {
    unsigned char hash[SHA256_BLOCK_SIZE + 1];
    convertVectorToBytes(hashV, hash);
    hash[SHA256_BLOCK_SIZE] = '\0';

    coinlist[hashV] = index;
}

int MerkleTree::getCoinMapping(const std::vector<bool> &hashV) {
    unsigned char hash[SHA256_BLOCK_SIZE + 1];
    convertVectorToBytes(hashV, hash);
    hash[SHA256_BLOCK_SIZE] = '\0';

    std::map<std::vector<bool>, int>::iterator it = coinlist.find(hashV);
    if(it != coinlist.end()) {
        return it->second;
    }   

    return -1;
}

void MerkleTree::getWitness(const std::vector<bool> &coin, auth_path &witness) {
    int index = getCoinMapping(coin);

    if(index != -1) {
        if(ceil(log(actualSize)/log(2)) != 0) {
            constructWitness(0, pow(2, ceil(log(actualSize)/log(2)))-1, root, depth-ceil(log(actualSize)/log(2)), index, witness);
        }

        if(actualDepth != depth) {
            std::vector<bool> zeros(cm_size * 8, 0);
            for(int i = 0; i < depth-ceil(log(actualSize)/log(2)); i++) {
                witness[i].aux_digest = zeros;
                witness[i].computed_is_right = false;
            }
        }
    }
    else {
        std::cout << "Coin does not exist in the tree!" << std::endl;
        exit(-1);
    }
}

void MerkleTree::constructWitness(int left, int right, Node* curr, int currentLevel, int index, auth_path &witness) {
    if((right - left) == 1) {
        if((index % 2) == 0) {
            witness[currentLevel].aux_digest = curr->right->value;
            witness[currentLevel].computed_is_right = false;
        }
        else {
            witness[currentLevel].aux_digest = curr->left->value;
            witness[currentLevel].computed_is_right = true;
        }
    }
    else {  
        if(index <= ((right+left)/2)) {
            witness[currentLevel].computed_is_right = false;
            constructWitness(left, (right+left)/2, curr->left, currentLevel+1, index, witness);
            witness[currentLevel].aux_digest = curr->right->value;
        }
        else {
            witness[currentLevel].computed_is_right = true;
            constructWitness((right+left)/2 + 1, right, curr->right, currentLevel+1, index, witness);
            witness[currentLevel].aux_digest = curr->left->value;
        }
    }
}

void MerkleTree::getRootValue(std::vector<bool>& r) {
    r = fullRoot;
}

void MerkleTree::getSubtreeRootValue(std::vector<bool>& r) {
    r = this->root->value;
}

} /* namespace libzerocash */
