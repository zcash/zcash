/** @file
 *****************************************************************************

 Implementation of interfaces for the class MintTransaction.

 See MintTransaction.h .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "Zerocash.h"
#include "MintTransaction.h"

namespace libzerocash {

MintTransaction::MintTransaction(): coinValue(0), internalCommitment(), externalCommitment()
{ }

/**
 * Creates a transaction minting the coin c.
 *
 * @param c the coin to mint.
 */
MintTransaction::MintTransaction(const Coin& c): coinValue(ZC_V_SIZE)
{
    convertIntToBytesVector(c.getValue(), this->coinValue);

	internalCommitment = c.getInternalCommitment();
	externalCommitment = c.getCoinCommitment();
}

/**
 * Verify the correctness of a Mint transaction.
 *
 * @return true if correct, false otherwise.
 */
bool MintTransaction::verify() const{

	// Check that the internal commitment is the right size
	if (this->internalCommitment.size() != ZC_K_SIZE) {
		return false;
	}

	// The external commitment should formulated as:
	// H( internalCommitment || 0^192 || coinValue)
	//
	// To check the structure of our proof we simply reconstruct
	// a version of the external commitment and check that it's
	// equal to the value we store.
	//
	// We use the constructor for CoinCommitment to do this.

	try {
		CoinCommitment comp(this->coinValue, this->internalCommitment);

		return (comp == this->externalCommitment);
	} catch (std::runtime_error) {
		return false;
	}

	return false;
}

const CoinCommitmentValue& MintTransaction::getMintedCoinCommitmentValue() const{
	return this->externalCommitment.getCommitmentValue();
}

uint64_t MintTransaction::getMonetaryValue() const {
    return convertBytesVectorToInt(this->coinValue);
}

} /* namespace libzerocash */
