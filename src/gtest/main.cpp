#include "gtest/gtest.h"
#include "sodium.h"

int main(int argc, char **argv) {
  assert(sodium_init() != -1);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
