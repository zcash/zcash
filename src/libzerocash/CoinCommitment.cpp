/**
 * @file       CoinCommitment.cpp
 *
 * @brief      CoinCommitment class for the Zerocash library.
 *
 * @author     Christina Garman, Matthew Green, and Ian Miers
 * @date       April 2014
 *
 * @copyright  Copyright 2014 Christina Garman, Matthew Green, and Ian Miers
 * @license    This project is released under the MIT license.
 **/

#include <stdexcept>
#include <stdint.h>

#include "Zerocash.h"
#include "CoinCommitment.h"


namespace libzerocash {

CoinCommitment::CoinCommitment() : commitmentValue(cm_size)
{ }

CoinCommitment::CoinCommitment(const std::vector<unsigned char>& val, const std::vector<unsigned char>& k) : commitmentValue(cm_size)
{
	this->constructCommitment(val, k);
}
	  
void
CoinCommitment::constructCommitment(const std::vector<unsigned char>& val, const std::vector<unsigned char>& k)
{
	std::vector<bool> zeros_192(192, 0);
    std::vector<bool> cm_internal;
    std::vector<bool> value_bool(v_size * 8, 0);
    std::vector<bool> k_bool(k_size * 8, 0);
	
	// Sanity check: make sure the vectors are the right size
	if (val.size() > v_size || k.size() > k_size) {
		throw std::runtime_error("CoinCommitment: inputs are too large");
	}
	
    libzerocash::convertBytesVectorToVector(val, value_bool);
    libzerocash::convertBytesVectorToVector(k, k_bool);
	
    libzerocash::concatenateVectors(k_bool, zeros_192, value_bool, cm_internal);
    std::vector<bool> cm_bool(cm_size * 8);
    libzerocash::hashVector(cm_internal, cm_bool);
    libzerocash::convertVectorToBytesVector(cm_bool, this->commitmentValue);
}

bool CoinCommitment::operator==(const CoinCommitment& rhs) const {
	return (this->commitmentValue == rhs.commitmentValue);
}

bool CoinCommitment::operator!=(const CoinCommitment& rhs) const {
	return !(*this == rhs);
}
	
const std::vector<unsigned char>& CoinCommitment::getCommitmentValue() const {
    return this->commitmentValue;
}

} /* namespace libzerocash */
