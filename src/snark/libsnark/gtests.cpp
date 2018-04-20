#include <gtest/gtest.h>

#include "common/profiling.hpp"

int main(int argc, char **argv) {
  libsnark::inhibit_profiling_info = true;
  libsnark::inhibit_profiling_counters = true;

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

