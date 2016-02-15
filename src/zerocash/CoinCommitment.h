/** @file
 *****************************************************************************

 Declaration of interfaces for the class CoinCommitment.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef COINCOMMITMENT_H_
#define COINCOMMITMENT_H_

#include <vector>

namespace libzerocash {

/****************************** Coin commitment ******************************/

class CoinCommitment {

friend class PourTransaction;
friend class PourProver;

public:
	CoinCommitment();

	CoinCommitment(const std::vector<unsigned char>& val,
                   const std::vector<unsigned char>& k);

    const std::vector<unsigned char>& getCommitmentValue() const;

	bool operator==(const CoinCommitment& rhs) const;
	bool operator!=(const CoinCommitment& rhs) const;


private:
    std::vector<unsigned char> commitmentValue;
};

} /* namespace libzerocash */

#endif /* COINCOMMITMENT_H_ */
