#include "gmock/gmock.h"
#include "crypto/common.h"
#include "pubkey.h"
#include "zcash/JoinSplit.hpp"
#include "util.h"

#include <libsnark/common/default_types/r1cs_ppzksnark_pp.hpp>
#include <libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp>

#include "librustzcash.h"

struct ECCryptoClosure
{
    ECCVerifyHandle handle;
};

ECCryptoClosure instance_of_eccryptoclosure;

ZCJoinSplit* params;

int main(int argc, char **argv) {
  assert(init_and_check_sodium() != -1);
  libsnark::default_r1cs_ppzksnark_pp::init_public_params();
  libsnark::inhibit_profiling_info = true;
  libsnark::inhibit_profiling_counters = true;
  boost::filesystem::path pk_path = ZC_GetParamsDir() / "sprout-proving.key";
  boost::filesystem::path vk_path = ZC_GetParamsDir() / "sprout-verifying.key";
  params = ZCJoinSplit::Prepared(vk_path.string(), pk_path.string());

  boost::filesystem::path sapling_spend = ZC_GetParamsDir() / "sapling-spend-testnet.params";
  boost::filesystem::path sapling_output = ZC_GetParamsDir() / "sapling-output-testnet.params";
  boost::filesystem::path sprout_groth16 = ZC_GetParamsDir() / "sprout-groth16-testnet.params";

  std::string sapling_spend_str = sapling_spend.string();
  std::string sapling_output_str = sapling_output.string();
  std::string sprout_groth16_str = sprout_groth16.string();

  librustzcash_init_zksnark_params(
      sapling_spend_str.c_str(),
      sapling_output_str.c_str(),
      sprout_groth16_str.c_str()
  );
  
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
