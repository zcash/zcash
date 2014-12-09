/**
 * @file       CoinCommitment.h
 *
 * @brief      CoinCommitment class for the Zerocash library.
 *
 * @author     Christina Garman, Matthew Green, and Ian Miers
 * @date       April 2014
 *
 * @copyright  Copyright 2014 Christina Garman, Matthew Green, and Ian Miers
 * @license    This project is released under the MIT license.
 **/

#ifndef COINCOMMITMENT_H_
#define COINCOMMITMENT_H_

#include <vector>

#include "../serialize.h"

namespace libzerocash {

class CoinCommitment {

//friend class Coin;
//friend class MintTransaction;
friend class PourTransaction;

public:
	CoinCommitment();

	CoinCommitment(const std::vector<unsigned char>& val, const std::vector<unsigned char>& k);

	void constructCommitment(const std::vector<unsigned char>& val, const std::vector<unsigned char>& k);

    const std::vector<unsigned char>& getCommitmentValue() const;

	bool operator==(const CoinCommitment& rhs) const;
	bool operator!=(const CoinCommitment& rhs) const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(commitmentValue);
    }

private:    
    std::vector<unsigned char> commitmentValue;
};

} /* namespace libzerocash */
#endif /* COINCOMMITMENT_H_ */
