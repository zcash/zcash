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
#include <boost/format.hpp>

#include "Zerocash.h"
#include "ZerocashParams.h"

static void throw_missing_param_file_exception(std::string paramtype, std::string path) {
    /* paramtype should be either "proving" or "verifying". */
    const char* tmpl = ("Could not open %s key file: %s\n"
                        "Please refer to user documentation for installing this file.");
    throw std::runtime_error((boost::format(tmpl) % paramtype % path).str());
}

namespace libzerocash {

int ZerocashParams::getTreeDepth()
{
    return treeDepth;
}

zerocash_pour_keypair<ZerocashParams::zerocash_pp> ZerocashParams::GenerateNewKeyPair(const unsigned int tree_depth)
{
    libzerocash::ZerocashParams::zerocash_pp::init_public_params();
    libzerocash::zerocash_pour_keypair<libzerocash::ZerocashParams::zerocash_pp> kp_v1 =
        libzerocash::zerocash_pour_ppzksnark_generator<libzerocash::ZerocashParams::zerocash_pp>(
            libzerocash::ZerocashParams::numPourInputs,
            libzerocash::ZerocashParams::numPourOutputs,
            tree_depth
        );
    return kp_v1;
}

void ZerocashParams::SaveProvingKeyToFile(const zerocash_pour_proving_key<ZerocashParams::zerocash_pp>* p_pk_1, std::string path)
{
    std::stringstream ssProving;
    ssProving << p_pk_1->r1cs_pk;
    std::ofstream pkFilePtr;
    pkFilePtr.open(path, std::ios::binary);
    ssProving.rdbuf()->pubseekpos(0, std::ios_base::out);
    pkFilePtr << ssProving.rdbuf();
    pkFilePtr.flush();
    pkFilePtr.close();
}


void ZerocashParams::SaveVerificationKeyToFile(const zerocash_pour_verification_key<ZerocashParams::zerocash_pp>* p_vk_1, std::string path)
{
    std::stringstream ssVerification;
    ssVerification << p_vk_1->r1cs_vk;
    std::ofstream vkFilePtr;
    vkFilePtr.open(path, std::ios::binary);
    ssVerification.rdbuf()->pubseekpos(0, std::ios_base::out);
    vkFilePtr << ssVerification.rdbuf();
    vkFilePtr.flush();
    vkFilePtr.close();
}

zerocash_pour_proving_key<ZerocashParams::zerocash_pp> ZerocashParams::LoadProvingKeyFromFile(std::string path, const unsigned int tree_depth)
{
    std::stringstream ssProving;
    std::ifstream fileProving(path, std::ios::binary);

    if(!fileProving.is_open()) {
        throw_missing_param_file_exception("proving", path);
    }

    ssProving << fileProving.rdbuf();
    fileProving.close();

    ssProving.rdbuf()->pubseekpos(0, std::ios_base::in);

    r1cs_ppzksnark_proving_key<ZerocashParams::zerocash_pp> pk_temp;
    ssProving >> pk_temp;

    return zerocash_pour_proving_key<ZerocashParams::zerocash_pp>(
        libzerocash::ZerocashParams::numPourInputs,
        libzerocash::ZerocashParams::numPourOutputs,
        tree_depth,
        std::move(pk_temp)
    );
}

zerocash_pour_verification_key<ZerocashParams::zerocash_pp> ZerocashParams::LoadVerificationKeyFromFile(std::string path, const unsigned int tree_depth)
{
    std::stringstream ssVerification;
    std::ifstream fileVerification(path, std::ios::binary);

    if(!fileVerification.is_open()) {
        throw_missing_param_file_exception("verification", path);
    }

    ssVerification << fileVerification.rdbuf();
    fileVerification.close();

    ssVerification.rdbuf()->pubseekpos(0, std::ios_base::in);

    r1cs_ppzksnark_verification_key<ZerocashParams::zerocash_pp> vk_temp;
    ssVerification >> vk_temp;

    return zerocash_pour_verification_key<ZerocashParams::zerocash_pp>(
        libzerocash::ZerocashParams::numPourInputs,
        libzerocash::ZerocashParams::numPourOutputs,
        std::move(vk_temp)
    );
}

ZerocashParams::ZerocashParams(
    const unsigned int tree_depth,
    zerocash_pour_keypair<ZerocashParams::zerocash_pp> *keypair
) :
    treeDepth(tree_depth)
{
    params_pk_v1 = new zerocash_pour_proving_key<ZerocashParams::zerocash_pp>(keypair->pk);
    params_vk_v1 = new zerocash_pour_verification_key<ZerocashParams::zerocash_pp>(keypair->vk);
}

ZerocashParams::ZerocashParams(
    const unsigned int tree_depth,
    zerocash_pour_proving_key<ZerocashParams::zerocash_pp>* p_pk_1,
    zerocash_pour_verification_key<ZerocashParams::zerocash_pp>* p_vk_1
) :
    treeDepth(tree_depth)
{
    assert(p_pk_1 != NULL || p_vk_1 != NULL);

    if (p_pk_1 == NULL) {
        params_pk_v1 = NULL;
    } else {
        params_pk_v1 = new zerocash_pour_proving_key<ZerocashParams::zerocash_pp>(*p_pk_1);
    }

    if (p_vk_1 == NULL) {
        params_vk_v1 = NULL;
    } else {
        params_vk_v1 = new zerocash_pour_verification_key<ZerocashParams::zerocash_pp>(*p_vk_1);
    }
}

ZerocashParams::~ZerocashParams()
{
    if (params_pk_v1 != NULL) {
        delete params_pk_v1;
    }
    if (params_vk_v1 != NULL) {
        delete params_vk_v1;
    }
}

const zerocash_pour_proving_key<ZerocashParams::zerocash_pp>& ZerocashParams::getProvingKey()
{
    if (params_pk_v1 != NULL) {
        return *params_pk_v1;
    } else {
        throw std::runtime_error("Pour proving key not set.");
    }
}

const zerocash_pour_verification_key<ZerocashParams::zerocash_pp>& ZerocashParams::getVerificationKey()
{
    if (params_vk_v1 != NULL) {
        return *params_vk_v1;
    } else {
        throw std::runtime_error("Pour verification key not set.");
    }
}

} /* namespace libzerocash */
