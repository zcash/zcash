#include "zcash/JoinSplit.hpp"

#include <iostream>
#include "crypto/common.h"

int64_t MAX_MONEY = 200000000 * 100000000LL;

int main(int argc, char **argv)
{
    if (init_and_check_sodium() == -1) {
        return 1;
    }

    if(argc != 4) {
        std::cerr << "Usage: " << argv[0] << " provingKeyFileName verificationKeyFileName r1csFileName" << std::endl;
        return 1;
    }

    std::string pkFile = argv[1];
    std::string vkFile = argv[2];
    std::string r1csFile = argv[3];

    ZCJoinSplit::Generate(r1csFile, vkFile, pkFile);

    return 0;
}
