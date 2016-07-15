#include "zcash/JoinSplit.hpp"

#include <iostream>
#include "sodium.h"

int main(int argc, char **argv)
{
    if (sodium_init() == -1) {
        return 1;
    }

    if(argc != 3) {
        std::cerr << "Usage: " << argv[0] << " provingKeyFileName verificationKeyFileName" << std::endl;
        return 1;
    }

    std::string pkFile = argv[1];
    std::string vkFile = argv[2];

    auto p = ZCJoinSplit::Generate();

    p->saveProvingKey(pkFile);
    p->saveVerifyingKey(vkFile);

    return 0;
}
