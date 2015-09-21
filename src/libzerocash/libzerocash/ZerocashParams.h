/** @file
 *****************************************************************************

 Declaration of interfaces for the class ZerocashParams.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef PARAMS_H_
#define PARAMS_H_

#include "Zerocash.h"
#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_ppzksnark.hpp"

namespace libzerocash {

class ZerocashParams {

public:
    typedef default_r1cs_ppzksnark_pp zerocash_pp;

	ZerocashParams(const unsigned int tree_depth);
	ZerocashParams(zerocash_pour_proving_key<zerocash_pp>* p_pk_1,
                   zerocash_pour_verification_key<zerocash_pp>* p_vk_1);

    ZerocashParams(const unsigned int tree_depth,
                   std::string pathToProvingParams,
                   std::string pathToVerificationParams);

    const zerocash_pour_proving_key<zerocash_pp>& getProvingKey(const int version);

    const zerocash_pour_verification_key<zerocash_pp>& getVerificationKey(const int version);
    ~ZerocashParams();

private:
    void initV1Params();
    zerocash_pour_keypair<zerocash_pp>* kp_v1;
    zerocash_pour_proving_key<zerocash_pp>* params_pk_v1;
    zerocash_pour_verification_key<zerocash_pp>* params_vk_v1;
    int treeDepth;

    const size_t numPourInputs = 2;
    const size_t numPourOutputs = 2;

};

} /* namespace libzerocash */

#endif /* PARAMS_H_ */
