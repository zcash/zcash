#include "gtest/gtest.h"
#include "crypto/common.h"

#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"

int main(int argc, char **argv) {
  assert(init_and_check_sodium() != -1);
  libsnark::default_r1cs_ppzksnark_pp::init_public_params();
  libsnark::inhibit_profiling_info = true;
  libsnark::inhibit_profiling_counters = true;
  
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
