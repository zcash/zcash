/** @file
 *****************************************************************************

 Declaration of interfaces for the class PourProver.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef POURPROVER_H_
#define POURPROVER_H_

#include "ZerocashParams.h"
#include "boost/array.hpp"
#include "PourTransaction.h"
#include "CoinCommitment.h"

namespace libzerocash {

class PourProver {
public:
	static bool VerifyProof(
		ZerocashParams& params,
		const std::vector<unsigned char>& pubkeyHash,
		const std::vector<unsigned char>& rt,
		const uint64_t vpub_old,
        const uint64_t vpub_new,
		const boost::array<std::vector<unsigned char>, 2> serials,
		const boost::array<std::vector<unsigned char>, 2> commitments,
		const boost::array<std::vector<unsigned char>, 2> macs,
		const std::string &zkSNARK
	) {
		PourTransaction pourtx;

		pourtx.version = 1;
		pourtx.publicOldValue.resize(8);
		pourtx.publicNewValue.resize(8);
		convertIntToBytesVector(vpub_old, pourtx.publicOldValue);
    	convertIntToBytesVector(vpub_new, pourtx.publicNewValue);
    	pourtx.serialNumber_1 = serials[0];
    	pourtx.serialNumber_2 = serials[1];
    	{
    		CoinCommitment cm;
    		cm.commitmentValue = commitments[0];
    		pourtx.cm_1 = cm;
    	}
    	{
    		CoinCommitment cm;
    		cm.commitmentValue = commitments[1];
    		pourtx.cm_2 = cm;
    	}
    	pourtx.MAC_1 = macs[0];
    	pourtx.MAC_2 = macs[1];
    	pourtx.zkSNARK = zkSNARK;

    	return pourtx.verify(params, pubkeyHash, rt);
	}
};


}

#endif /* POURPROVER_H_ */
