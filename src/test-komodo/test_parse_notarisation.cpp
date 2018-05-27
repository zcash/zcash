#include <gtest/gtest.h>

#include "cc/eval.h"
#include "core_io.h"
#include "key.h"

#include "testutils.h"


namespace TestParseNotarisation {

class TestParseNotarisation : public ::testing::Test, public Eval {};


TEST(TestParseNotarisation, test_ee2fa)
{
    // ee2fa47820a31a979f9f21cb3fedbc484bf9a8957cb6c9acd0af28ced29bdfe1
    std::vector<uint8_t> opret = ParseHex("c349ff90f3bce62c1b7b49d1da0423b1a3d9b733130cce825b95b9e047c729066e020d00743a06fdb95ad5775d032b30bbb3680dac2091a0f800cf54c79fd3461ce9b31d4b4d4400");
    NotarisationData nd;
    ASSERT_TRUE(E_UNMARSHAL(opret, ss >> nd));
}

}
