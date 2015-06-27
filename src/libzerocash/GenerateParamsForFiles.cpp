#include <fstream>

#include "Zerocash.h"
#include "libsnark/common/types.hpp"
#include "libsnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "libzerocash/pour_ppzksnark/pour_gadget.hpp"
#include "libzerocash/pour_ppzksnark/zerocash_ppzksnark.hpp"

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

    init_public_params<default_pp>();
	pour_keypair<default_pp> kp = pour_keypair<default_pp>(zerocash_pour_ppzksnark_generator<default_pp>(tree_depth));

    std::stringstream ssProving;
    ssProving << kp.pk.r1cs_pk;
    std::ofstream pkFilePtr;
    pkFilePtr.open(pkFile, std::ios::binary);
    ssProving.rdbuf()->pubseekpos(0, std::ios_base::out);
    pkFilePtr << ssProving.rdbuf();
    pkFilePtr.flush();
    pkFilePtr.close();

    std::stringstream ssVerification;
    ssVerification << kp.vk;
    std::ofstream vkFilePtr;
    vkFilePtr.open(vkFile, std::ios::binary);
    ssVerification.rdbuf()->pubseekpos(0, std::ios_base::out);
    vkFilePtr << ssVerification.rdbuf();
    vkFilePtr.flush();
    vkFilePtr.close();

    return 0;   
}
