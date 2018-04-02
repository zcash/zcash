#include "key.h"
#include "gtest/gtest.h"
#include "crypto/common.h"


int main(int argc, char **argv) {
  assert(init_and_check_sodium() != -1);
  ECC_Start();

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
