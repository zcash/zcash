/** @file
 *****************************************************************************

 Declaration of interfaces for the class Coin.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef COIN_H_
#define COIN_H_

#include <vector>

#include "Address.h"
#include "CoinCommitment.h"

namespace libzerocash {

/********************************* Coin **************************************/

class Coin {

friend class MintTransaction;
friend class PourTransaction;

public:
    /* This constructor is to be used ONLY for deserialization. */
	Coin();
	/**
	 * @param addr the address the coin will belong to when minted or poured into
	 * @param value the monetary value of the coin
	 */
    Coin(const PublicAddress& addr,
         uint64_t value);

    Coin(const PublicAddress& addr,
         uint64_t value,
		 const std::vector<unsigned char>& rho,
         const std::vector<unsigned char>& r);

   	Coin(const std::string bucket, Address& addr);

	const PublicAddress& getPublicAddress() const;

	const CoinCommitment& getCoinCommitment() const;

	bool operator==(const Coin& rhs) const;
	bool operator!=(const Coin& rhs) const;

	uint64_t getValue() const;

    const std::vector<unsigned char>& getRho() const;

    const std::vector<unsigned char>& getR() const;

private:
	PublicAddress addr_pk;
    CoinCommitment cm;
	std::vector<unsigned char> rho;
    std::vector<unsigned char> r;
	std::vector<unsigned char> k;
	std::vector<unsigned char> coinValue;

	const std::vector<unsigned char>& getInternalCommitment() const;

    void computeCommitments(std::vector<unsigned char>& a_pk);
};

} /* namespace libzerocash */

#endif /* COIN_H_ */
