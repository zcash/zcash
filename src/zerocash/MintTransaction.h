/** @file
 *****************************************************************************

 Declaration of interfaces for the class MintTransaction.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MINTTRANSACTION_H_
#define MINTTRANSACTION_H_

#include "CoinCommitment.h"
#include "Coin.h"

typedef std::vector<unsigned char> CoinCommitmentValue;

namespace libzerocash{

/***************************** Mint transaction ******************************/

class MintTransaction {
public:
    MintTransaction();

    /**
     * Creates a transaction minting the provided coin.
     *
     * @param c the coin to mint.
     */
    MintTransaction(const Coin& c);

    /**
     * Verifies the MintTransaction.
     * In particular, this checks that output coin commitment
     * actually is to a coin of the claimed value.
     *
     * @return true if valid, false otherwise.
     */
    bool verify() const;

    /**
     *Gets the commitment to the coin that was minted by this transaction.
     *
     * @return the commitment
     */
    const CoinCommitmentValue& getMintedCoinCommitmentValue() const;

    /**
     * Gets the monetary value of the minted coin.
     *
     * @return the value
     */
    uint64_t getMonetaryValue() const;


private:
	std::vector<unsigned char>	coinValue;			// coin value
	std::vector<unsigned char>	internalCommitment; // "k" in paper notation
    CoinCommitment				externalCommitment; // "cm" in paper notation

	const CoinCommitment& getCoinCommitment() const { return this->externalCommitment; }
};

} /* namespace libzerocash */

#endif /* MINTTRANSACTION_H_ */
