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
#include "ZerocashParams.h"
#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_gadget.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_ppzksnark.hpp"

using namespace libzerocash;

int main(int argc, char **argv)
{
    if(argc != 4) {
        std::cerr << "Usage: " << argv[0] << " treeDepth provingKeyFileName verificationKeyFileName" << std::endl;
        return 1;
    }

    unsigned int tree_depth = atoi(argv[1]);
    std::string pkFile = argv[2];
    std::string vkFile = argv[3];

    auto keypair = libzerocash::ZerocashParams::GenerateNewKeyPair(tree_depth);
    libzerocash::ZerocashParams p(
        tree_depth,
        &keypair
    );

    libzerocash::ZerocashParams::SaveProvingKeyToFile(&p.getProvingKey(), pkFile);
    libzerocash::ZerocashParams::SaveVerificationKeyToFile(&p.getVerificationKey(), vkFile);

    return 0;
}
