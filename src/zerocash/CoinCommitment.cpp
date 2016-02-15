/** @file
 *****************************************************************************

 Implementation of interfaces for the class CoinCommitment.

 See CoinCommitment.h .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include <stdexcept>
#include <stdint.h>

#include "Zerocash.h"
#include "CoinCommitment.h"

namespace libzerocash {

CoinCommitment::CoinCommitment() : commitmentValue(ZC_CM_SIZE)
{ }

CoinCommitment::CoinCommitment(const std::vector<unsigned char>& val,
                               const std::vector<unsigned char>& k) : commitmentValue(ZC_CM_SIZE)
{
	std::vector<bool> zeros_192(192, 0);
    std::vector<bool> cm_internal;
    std::vector<bool> value_bool(ZC_V_SIZE * 8, 0);
    std::vector<bool> k_bool(ZC_K_SIZE * 8, 0);

	if (val.size() > ZC_V_SIZE || k.size() > ZC_K_SIZE) {
		throw std::runtime_error("CoinCommitment: inputs are too large");
	}

    libzerocash::convertBytesVectorToVector(val, value_bool);
    libzerocash::convertBytesVectorToVector(k, k_bool);

    libzerocash::concatenateVectors(k_bool, zeros_192, value_bool, cm_internal);
    std::vector<bool> cm_bool(ZC_CM_SIZE * 8);
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
