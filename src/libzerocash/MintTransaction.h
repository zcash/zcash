/**
 * @file       MintTransaction.h
 *
 * @brief      MintTransaction class for the Zerocash library.
 *
 * @author     Christina Garman, Matthew Green, and Ian Miers
 * @date       April 2014
 *
 * @copyright  Copyright 2014 Christina Garman, Matthew Green, and Ian Miers
 * @license    This project is released under the MIT license.
 **/

#ifndef MINTTRANSACTION_H_
#define MINTTRANSACTION_H_

#include "../serialize.h"
#include "CoinCommitment.h"
#include "Coin.h"
typedef std::vector<unsigned char> CoinCommitmentValue;

namespace libzerocash {

class MintTransaction {
public:
    MintTransaction();

    /** Creates a transaction minting the provided coin.
     *
     * @param c the coin to mint.
     */
    MintTransaction(const Coin& c);

    /**Verifies the MintTransaction.
     * In particular, this checks that output coin commitment
     * actually is to a coin of the claimed value.
     *
     * @return true if valid, false otherwise.
     */
    bool verify() const;

    /**Gets the commitment to the coin that was minted by this transaction.
     *
     * @return the commitment
     */
    const CoinCommitmentValue& getMintedCoinCommitmentValue() const;

    /**Gets the monetary value of the minted coin.
     *
     * @return the value
     */
    uint64_t getMonetaryValue() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(externalCommitment);
        READWRITE(coinValue);
        READWRITE(internalCommitment);
    }

private:
	std::vector<unsigned char>	coinValue;			// coin value
	std::vector<unsigned char>	internalCommitment; // "k" in paper notation
    CoinCommitment				externalCommitment; // "cm" in paper notation



	const CoinCommitment& getCoinCommitment() const { return this->externalCommitment; }
};

} /* namespace libzerocash */
#endif /* MINTTRANSACTION_H_ */
