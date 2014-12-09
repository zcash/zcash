/**
* @file       Params.h
*
* @brief      Parameter class for Zerocash.
*
* @author     Christina Garman, Matthew Green, and Ian Miers
* @date       April 2014
*
* @copyright  Copyright 2014 Christina Garman, Matthew Green, and Ian Miers
* @license    This project is released under the MIT license.
**/

#ifndef PARAMS_H_
#define PARAMS_H_
#include "Zerocash.h"
#include "libsnark/common/types.hpp"
#include "libsnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "pour_ppzksnark/zerocash_ppzksnark.hpp"

namespace libzerocash {
class ZerocashParams {
public:
	ZerocashParams(const unsigned int tree_depth);
	ZerocashParams(pour_proving_key<default_pp>* p_pk_1, r1cs_ppzksnark_verification_key<default_pp>* p_vk_1);

    ZerocashParams(const unsigned int tree_depth, std::string pathToProvingParams, std::string pathToVerificationParams);

    const pour_proving_key<default_pp>& getProvingKey(const int version);

    const r1cs_ppzksnark_verification_key<default_pp>& getVerificationKey(const int version);
    ~ZerocashParams();
private:
    void initV1Params();
    pour_keypair<default_pp>* kp_v1;
    pour_proving_key<default_pp>* params_pk_v1;
    r1cs_ppzksnark_verification_key<default_pp>* params_vk_v1;
    int treeDepth;
};

} /* namespace libzerocash */

#endif /* PARAMS_H_ */
