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

TEST(TestParseNotarisation, test__)
{
    // 576e910a1f704207bcbcf724124ff9adc5237f45cb6919589cd0aa152caec424
    std::vector<uint8_t> opret = ParseHex("b3ed7fbbfbc027caeeeec81e65489ec5d9cd47cda675a5cbb75b4a845e67cf0ef6330300b5a6bd8385feb833f3be961c9d8a46fcecd36dcdfa42ad81a20a892433722f0b4b4d44004125a06024eae24c11f36ea110acd707b041d5355b6e1b42de5e2614357999c6aa02000d26ad0300000000404b4c000000000005130300500d000061f22ba7d19fe29ac3baebd839af8b7127d1f90755534400");
    NotarisationData nd;
    // We can't parse this one
    ASSERT_FALSE(E_UNMARSHAL(opret, ss >> nd));
}

// for l in `g 'parse notarisation' ~/.komodo/debug.log | pyline 'l.split()[8]'`; do hoek decodeTx '{"hex":"'`src/komodo-cli getrawtransaction "$l"`'"}' | jq '.outputs[1].script.op_return' | pyline 'import base64; print base64.b64decode(l).encode("hex")'; done

}
