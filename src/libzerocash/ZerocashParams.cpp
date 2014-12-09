/**
* @file       Params.cpp
*
* @brief      Parameter class for Zerocash.
*
* @author     Christina Garman, Matthew Green, and Ian Miers
* @date       April 2014
*
* @copyright  Copyright 2014 Christina Garman, Matthew Green, and Ian Miers
* @license    This project is released under the MIT license.
**/

#include <fstream>

#include "Zerocash.h"
#include "ZerocashParams.h"

namespace libzerocash {

ZerocashParams::ZerocashParams(pour_proving_key<default_pp>* p_pk_1, r1cs_ppzksnark_verification_key<default_pp>* p_vk_1): params_pk_v1(p_pk_1), params_vk_v1(p_vk_1) {
	kp_v1=NULL;
    params_pk_v1 = NULL;
    params_vk_v1 = NULL;
}

ZerocashParams::ZerocashParams(const unsigned int tree_depth): treeDepth(tree_depth) {
	kp_v1=NULL;
    params_pk_v1 = NULL;
    params_vk_v1 = NULL;
}

ZerocashParams::ZerocashParams(const unsigned int tree_depth, std::string pathToProvingParams="", std::string pathToVerificationParams=""): treeDepth(tree_depth) {
	kp_v1=NULL;

    init_public_params<default_pp>();

    if(pathToProvingParams != "") {
        std::stringstream ssProving;
        std::ifstream fileProving(pathToProvingParams, std::ios::binary);

        if(!fileProving.is_open()) {
            throw ZerocashException("Could not open proving key file.");
        }

        ssProving << fileProving.rdbuf();
        fileProving.close();

        ssProving.rdbuf()->pubseekpos(0, std::ios_base::in);

        r1cs_ppzksnark_proving_key<default_pp> pk_temp;
        ssProving >> pk_temp;

        params_pk_v1 = new pour_proving_key<default_pp>(this->treeDepth, std::move(pk_temp));
        //kp_v1->pk = params_pk_v1;
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

        r1cs_ppzksnark_verification_key<default_pp> vk_temp2;
        ssVerification >> vk_temp2;

        params_vk_v1 = new r1cs_ppzksnark_verification_key<default_pp>(std::move(vk_temp2));
        //kp_v1->vk = params_pk_v1;
    }
    else {
        params_vk_v1 = NULL;
    }
}

void ZerocashParams::initV1Params(){
    init_public_params<default_pp>();
	kp_v1 = new pour_keypair<default_pp>(zerocash_pour_ppzksnark_generator<default_pp>(this->treeDepth));

	params_pk_v1 = &kp_v1->pk;
	params_vk_v1 = &kp_v1->vk;
}

ZerocashParams::~ZerocashParams(){
	delete kp_v1;
}

const pour_proving_key<default_pp>& ZerocashParams::getProvingKey(const int version) {
    switch(version) {
        case 1:
        	//if(kp_v1 == NULL){
        	//	this->initV1Params();
        	//}
            //return *(this->params_pk_v1);
            //break;
            if(params_pk_v1 == NULL) {
                this->initV1Params();
                return *(this->params_pk_v1);
                //throw ZerocashException("Proving key not initialized."); //TODO: always have this?
            }
            else {
                return *(this->params_pk_v1);
            }
            break;
    }

    throw ZerocashException("Invalid version number");
}

const pour_verification_key<default_pp>& ZerocashParams::getVerificationKey(const int version) {
    switch(version) {
        case 1:
        	//if(kp_v1 == NULL){
        	//	this->initV1Params();
        	//}
            //return *(this->params_vk_v1);
            //break;
            if(params_vk_v1 == NULL) {
                this->initV1Params();
                return *(this->params_vk_v1);
                //throw ZerocashException("Verification key not initialized."); //TODO: always have this?
            }
            else {
                return *(this->params_vk_v1);
            }
            break;
    }

    throw ZerocashException("Invalid version number");
}

} /* namespace libzerocash */
