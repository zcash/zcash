/** @file
 *****************************************************************************

 Implementation of interfaces for the class ZerocashParams.

 See ZerocashParams.h .

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include <fstream>

#include "Zerocash.h"
#include "ZerocashParams.h"

namespace libzerocash {

ZerocashParams::ZerocashParams(zerocash_pour_proving_key<ZerocashParams::zerocash_pp>* p_pk_1,
                               zerocash_pour_verification_key<ZerocashParams::zerocash_pp>* p_vk_1) :
    params_pk_v1(p_pk_1), params_vk_v1(p_vk_1)
{
	kp_v1 = NULL;
    params_pk_v1 = NULL;
    params_vk_v1 = NULL;
}

ZerocashParams::ZerocashParams(const unsigned int tree_depth) :
    treeDepth(tree_depth)
{
	kp_v1 = NULL;
    params_pk_v1 = NULL;
    params_vk_v1 = NULL;
}

ZerocashParams::ZerocashParams(const unsigned int tree_depth,
                               std::string pathToProvingParams="",
                               std::string pathToVerificationParams="") :
    treeDepth(tree_depth)
{
	kp_v1 = NULL;

    ZerocashParams::zerocash_pp::init_public_params();

    if(pathToProvingParams != "") {
        std::stringstream ssProving;
        std::ifstream fileProving(pathToProvingParams, std::ios::binary);

        if(!fileProving.is_open()) {
            throw ZerocashException("Could not open proving key file.");
        }

        ssProving << fileProving.rdbuf();
        fileProving.close();

        ssProving.rdbuf()->pubseekpos(0, std::ios_base::in);

        r1cs_ppzksnark_proving_key<ZerocashParams::zerocash_pp> pk_temp;
        ssProving >> pk_temp;

        params_pk_v1 = new zerocash_pour_proving_key<ZerocashParams::zerocash_pp>(this->numPourInputs,
                                                                                  this->numPourOutputs,
                                                                                  this->treeDepth,
                                                                                  std::move(pk_temp));
    }
    else {
        params_pk_v1 = NULL;
    }

    if(pathToVerificationParams != "") {
        std::stringstream ssVerification;
        std::ifstream fileVerification(pathToVerificationParams, std::ios::binary);

        if(!fileVerification.is_open()) {
            throw ZerocashException("Could not open verification key file.");
        }

        ssVerification << fileVerification.rdbuf();
        fileVerification.close();

        ssVerification.rdbuf()->pubseekpos(0, std::ios_base::in);

        r1cs_ppzksnark_verification_key<ZerocashParams::zerocash_pp> vk_temp2;
        ssVerification >> vk_temp2;

        params_vk_v1 = new zerocash_pour_verification_key<ZerocashParams::zerocash_pp>(this->numPourInputs,
                                                                                       this->numPourOutputs,
                                                                                       std::move(vk_temp2));
    }
    else {
        params_vk_v1 = NULL;
    }
}

void ZerocashParams::initV1Params()
{
    ZerocashParams::zerocash_pp::init_public_params();
    kp_v1 = new zerocash_pour_keypair<ZerocashParams::zerocash_pp>(zerocash_pour_ppzksnark_generator<ZerocashParams::zerocash_pp>(this->numPourInputs,
                                                                                                                                  this->numPourOutputs,
                                                                                                                                  this->treeDepth));

	params_pk_v1 = &kp_v1->pk;
	params_vk_v1 = &kp_v1->vk;
}

ZerocashParams::~ZerocashParams()
{
	delete kp_v1;
}

const zerocash_pour_proving_key<ZerocashParams::zerocash_pp>& ZerocashParams::getProvingKey(const int version)
{
    switch(version) {
        case 1:
            if(params_pk_v1 == NULL) {
                this->initV1Params();
                return *(this->params_pk_v1);
            }
            else {
                return *(this->params_pk_v1);
            }
            break;
    }

    throw ZerocashException("Invalid version number");
}

const zerocash_pour_verification_key<ZerocashParams::zerocash_pp>& ZerocashParams::getVerificationKey(const int version)
{
    switch(version) {
        case 1:
            if(params_vk_v1 == NULL) {
                this->initV1Params();
                return *(this->params_vk_v1);
            }
            else {
                return *(this->params_vk_v1);
            }
            break;
    }

    throw ZerocashException("Invalid version number");
}

} /* namespace libzerocash */
