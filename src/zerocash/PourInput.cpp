/** @file
 *****************************************************************************

 Implementation of interfaces for the class PourInput.

 See PourInput.h .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "PourInput.h"

namespace libzerocash {

PourInput::PourInput(int tree_depth): old_coin(), merkle_index(), path() {
	this->old_address = Address::CreateNewRandomAddress();

	this->old_coin = Coin(this->old_address.getPublicAddress(), 0);

	ZCIncrementalMerkleTree merkleTree;
	merkleTree.append(uint256(this->old_coin.getCoinCommitment().getCommitmentValue()));

	auto witness = merkleTree.witness();
	auto merkle_path = witness.path();

	this->path = merkle_path.authentication_path;
	this->merkle_index = convertVectorToInt(merkle_path.index);
}

PourInput::PourInput(Coin old_coin,
          Address old_address,
          const libzcash::MerklePath &path) : old_address(old_address), old_coin(old_coin), path(path.authentication_path) {
		this->merkle_index = convertVectorToInt(path.index);
};

} /* namespace libzerocash */