/** @file
 *****************************************************************************

 Implementation of interfaces for the class PourInput.

 See PourInput.h .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "IncrementalMerkleTree.h"
#include "PourInput.h"

namespace libzerocash {

PourInput::PourInput(int tree_depth): old_coin(), merkle_index(), path() {
	this->old_address = Address::CreateNewRandomAddress();

	this->old_coin = Coin(this->old_address.getPublicAddress(), 0);

	// dummy merkle tree
	IncrementalMerkleTree merkleTree(tree_depth);

	// commitment from coin
	std::vector<bool> commitment(ZC_CM_SIZE * 8);
	convertBytesVectorToVector(this->old_coin.getCoinCommitment().getCommitmentValue(), commitment);

	// insert commitment into the merkle tree
	std::vector<bool> index;
	merkleTree.insertElement(commitment, index);

	merkleTree.getWitness(index, this->path);

	this->merkle_index = 1;
}

PourInput::PourInput(Coin old_coin,
          Address old_address,
          size_t merkle_index,
          merkle_authentication_path path) : old_coin(old_coin), merkle_index(merkle_index), path(path) {
		this->old_address = old_address;
};

} /* namespace libzerocash */