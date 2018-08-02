#include "gmock/gmock.h"
#include "crypto/common.h"
#include "key.h"
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
  ECC_Start();

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
      "35f6afd7d7514531aaa9fa529bdcddf116865f02abdd42164322bb1949227d82bdae295cad9c7b98d4bbbb00e045fa17aca79c90f53433a66bce4e82b6a1936d",
      sapling_output_str.c_str(),
      "f9d0b98ea51830c4974878f1b32bb68b2bf530e2e0ae09cd2a9b609d6fda37f1a1928e2d1ca91c31835c75dcc16057db53a807cc5cb37ebcfb753aa843a8ac21",
      sprout_groth16_str.c_str(),
      "7a6723311162cb0c664c742d2fa42278195ade98ba3f21ef4fa02b82c83aed696e107e389ac7b3b0f33f417aeefe5be775d117910a473a422b4a1b97489fbdd6"
  );
  
  testing::InitGoogleMock(&argc, argv);
  
  auto ret = RUN_ALL_TESTS();

  ECC_Stop();
  return ret;
}
