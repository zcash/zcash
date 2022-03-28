#include "gmock/gmock.h"
#include "init.h"
#include "key.h"
#include "pubkey.h"
#include "util.h"
#include "utiltest.h"

#include "librustzcash.h"
#include <sodium.h>
#include <tracing.h>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

struct ECCryptoClosure
{
    ECCVerifyHandle handle;
};

ECCryptoClosure instance_of_eccryptoclosure;

int main(int argc, char **argv) {
  assert(sodium_init() != -1);
  ECC_Start();

  // Log all errors to stdout so we see them in test output.
  std::string initialFilter = "error";
  pTracingHandle = tracing_init(
      nullptr, 0,
      initialFilter.c_str(),
      false);

  testing::InitGoogleMock(&argc, argv);

  // The "threadsafe" style is necessary for correct operation of death/exit
  // tests on macOS (https://github.com/zcash/zcash/issues/4802).
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  auto ret = RUN_ALL_TESTS();

  ECC_Stop();
  return ret;
}
