#include "gtest/gtest.h"
#include "crypto/common.h"
#include "wallet/db.h"

#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"

#include <boost/filesystem.hpp>

int main(int argc, char **argv) {
  assert(init_and_check_sodium() != -1);
  libsnark::default_r1cs_ppzksnark_pp::init_public_params();
  libsnark::inhibit_profiling_info = true;
  libsnark::inhibit_profiling_counters = true;

  // Get temporary and unique path for walletdb.
  // bitdb.Open() only intialises once, so this dir is used for all tests.
  // Note: / operator to append paths
  boost::filesystem::path pathTemp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  boost::filesystem::create_directories(pathTemp);
  bitdb.Open(pathTemp);
  
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
