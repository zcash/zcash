/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/
#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/common/profiling.hpp"
#include "zerocash_pour_ppzksnark/zerocash_pour_gadget.hpp"

using namespace libsnark;

int main(int argc, const char* argv[])
{
    start_profiling();
    default_r1cs_ppzksnark_pp::init_public_params();
#ifndef DEBUG
    printf("this program needs to be compiled with constraint annotations (make debug/make cppdebug)\n");
    return 2;
#else
    if (argc != 4)
    {
        printf("usage: %s num_old_coins num_new_coins tree_depth\n", argv[0]);
        return 1;
    }


    int num_old_coins = atoi(argv[1]);
    int num_new_coins = atoi(argv[2]);
    int tree_depth = atoi(argv[3]);
    protoboard<Fr<default_r1cs_ppzksnark_pp> > pb;
    libzerocash::zerocash_pour_gadget<Fr<default_r1cs_ppzksnark_pp> > g(pb, num_old_coins, num_new_coins, tree_depth, "pour");
    g.generate_r1cs_constraints();
    for (auto it : pb.get_constraint_system().constraint_annotations)
    {
        printf("%s\n", it.second.c_str());
    }
#endif
}
