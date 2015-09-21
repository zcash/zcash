/** @file
 *****************************************************************************

 Functionality to generate files containing the Zerocash public parameters.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include <fstream>

#include "Zerocash.h"
#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_gadget.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_ppzksnark.hpp"

using namespace libzerocash;

int main(int argc, char **argv)
{
    if(argc < 2 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " treeDepth provingKeyFileName verificationKeyFileName" << std::endl;
        return 1;
    }

    unsigned int tree_depth = atoi(argv[1]);
    std::string pkFile = argv[2];
    std::string vkFile = argv[3];

    default_r1cs_ppzksnark_pp::init_public_params();
    zerocash_pour_keypair<default_r1cs_ppzksnark_pp> kp = zerocash_pour_keypair<default_r1cs_ppzksnark_pp>(zerocash_pour_ppzksnark_generator<default_r1cs_ppzksnark_pp>(2, 2, tree_depth));

    std::stringstream ssProving;
    ssProving << kp.pk.r1cs_pk;
    std::ofstream pkFilePtr;
    pkFilePtr.open(pkFile, std::ios::binary);
    ssProving.rdbuf()->pubseekpos(0, std::ios_base::out);
    pkFilePtr << ssProving.rdbuf();
    pkFilePtr.flush();
    pkFilePtr.close();

    std::stringstream ssVerification;
    ssVerification << kp.vk.r1cs_vk;
    std::ofstream vkFilePtr;
    vkFilePtr.open(vkFile, std::ios::binary);
    ssVerification.rdbuf()->pubseekpos(0, std::ios_base::out);
    vkFilePtr << ssVerification.rdbuf();
    vkFilePtr.flush();
    vkFilePtr.close();

    return 0;
}
