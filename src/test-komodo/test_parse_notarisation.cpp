#include <gtest/gtest.h>

#include "cc/eval.h"
#include "core_io.h"
#include "key.h"

#include "testutils.h"

#include "komodo_structs.h"
komodo_state *komodo_stateptr(char *symbol,char *dest);
void komodo_notarized_update(struct komodo_state *sp,int32_t nHeight,int32_t notarized_height,
        uint256 notarized_hash,uint256 notarized_desttxid,uint256 MoM,int32_t MoMdepth);
const notarized_checkpoint *komodo_npptr(int32_t height);
int32_t komodo_prevMoMheight();
int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp);

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

TEST(TestParseNotarisation, test__a)
{
    // be55101e6c5a93fb3611a44bd66217ad8714d204275ea4e691cfff9d65dff85c TXSCL
    std::vector<uint8_t> opret = ParseHex("fb9ea2818eec8b07f8811bab49d64379db074db478997f8114666f239bd79803cc460000d0fac4e715b7e2b917a5d79f85ece0c423d27bd3648fd39ac1dc7db8e1bd4b16545853434c00a69eab9f23d7fb63c4624973e7a9079d6ada2f327040936356d7af5e849f6d670a0003001caf7b7b9e1c9bc59d0c7a619c9683ab1dd0794b6f3ea184a19f8fda031150e700000000");
    NotarisationData nd(1);
    bool res = E_UNMARSHAL(opret, ss >> nd);
    ASSERT_TRUE(res);
}

TEST(TestParseNotarisation, test__b)
{
    // 03085dafed656aaebfda25bf43ffe9d1fb72565bb1fc8b2a12a631659f28f877 TXSCL
    std::vector<uint8_t> opret = ParseHex("48c71a10aa060eab1a43f52acefac3b81fb2a2ce310186b06141884c0501d403c246000052e6d49afd82d9ab3d97c996dd9b6a78a554ffa1625e8dadf0494bd1f8442e3e545853434c007cc5c07e3b67520fd14e23cd5b49f2aa022f411500fd3326ff91e6dc0544a1c90c0003008b69117bb1376ac8df960f785d8c208c599d3a36248c98728256bb6d4737e59600000000");
    NotarisationData nd(1);
    bool res = E_UNMARSHAL(opret, ss >> nd);
    ASSERT_TRUE(res);
}

void clear_npoints(komodo_state *sp)
{
    if (sp->NPOINTS != nullptr)
        free(sp->NPOINTS);
    sp->NPOINTS = nullptr;
    sp->NUM_NPOINTS = 0;
    sp->last_NPOINTSi = 0;
}

TEST(TestParseNotarisation, test_notarized_update)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    clear_npoints(sp);
    // height lower than notarized_height
    komodo_notarized_update(sp, 9, 10, uint256(), uint256(), uint256(), 1);
    EXPECT_EQ(sp->NUM_NPOINTS, 0);
    auto npptr = komodo_npptr(11);
    EXPECT_EQ(npptr, nullptr);

    // 1 inserted with height 10
    komodo_notarized_update(sp, 10, 8, uint256(), uint256(), uint256(), 2);
    EXPECT_EQ(sp->NUM_NPOINTS, 1);
    notarized_checkpoint *tmp = &sp->NPOINTS[0];
    EXPECT_EQ(tmp->nHeight, 10);
    EXPECT_EQ(tmp->notarized_height, 8);
    EXPECT_EQ(tmp->MoMdepth, 2);
    clear_npoints(sp);
}

TEST(TestParseNotarisation, test_npptr)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    // empty NPOINTS
    clear_npoints(sp);
    komodo_notarized_update(sp, 9, 10, uint256(), uint256(), uint256(), 1);
    EXPECT_EQ(sp->NUM_NPOINTS, 0);
    auto npptr = komodo_npptr(11);
    EXPECT_EQ(npptr, nullptr);

    // 1 inserted with height 10
    komodo_notarized_update(sp, 10, 8, uint256(), uint256(), uint256(), 2);
    EXPECT_EQ(sp->NUM_NPOINTS, 1);
    notarized_checkpoint *tmp = &sp->NPOINTS[0];
    EXPECT_EQ(tmp->nHeight, 10);
    EXPECT_EQ(tmp->notarized_height, 8);
    EXPECT_EQ(tmp->MoMdepth, 2);
    // test komodo_npptr
    npptr = komodo_npptr(-1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(0); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(6); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(7); // one found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 8);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(9); // none found with a notarized_height so high
    EXPECT_EQ(npptr, nullptr);

    // add another with the same index
    komodo_notarized_update(sp, 10, 9, uint256(), uint256(), uint256(), 2);
    EXPECT_EQ(sp->NUM_NPOINTS, 2); 

    npptr = komodo_npptr(-1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(0); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(6); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(7); // original found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 8);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(8); // new one found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 9);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(9); // new one found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 9);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(10); // none found with a notarized_height so high
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(11); // none found with a notarized_height so high
    EXPECT_EQ(npptr, nullptr);
    clear_npoints(sp);
}

TEST(TestParseNotarisation, test_prevMoMheight)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    // empty NPOINTS
    clear_npoints(sp);
    EXPECT_EQ(komodo_prevMoMheight(), 0);
    uint256 mom;
    mom.SetHex("A0");
    komodo_notarized_update(sp, 10, 9, uint256(), uint256(), mom, 1);
    EXPECT_EQ(komodo_prevMoMheight(), 9);
    komodo_notarized_update(sp, 11, 10, uint256(), uint256(), mom, 1);
    EXPECT_EQ(komodo_prevMoMheight(), 10);
    komodo_notarized_update(sp, 9, 8, uint256(), uint256(), mom, 1); // we're not sorted by anything other than chronological
    EXPECT_EQ(komodo_prevMoMheight(), 8);

}

TEST(TestParseNotarisation, test_notarizeddata)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    // empty NPOINTS
    clear_npoints(sp);
    uint256 hash;
    uint256 expected_hash;
    uint256 txid;
    uint256 expected_txid;
    auto rslt = komodo_notarizeddata(0, &hash, &txid);
    EXPECT_EQ(rslt, 0);
    EXPECT_EQ(hash, expected_hash);
    EXPECT_EQ(txid, expected_txid);

    // now add a notarization
    expected_hash.SetHex("0A");
    expected_txid.SetHex("0B");
    komodo_notarized_update(sp, 10, 9, expected_hash, expected_txid, uint256(), 1);
    rslt = komodo_notarizeddata(0, &hash, &txid); // too low
    EXPECT_EQ(rslt, 0);
    rslt = komodo_notarizeddata(9, &hash, &txid); // too low
    EXPECT_EQ(rslt, 0);
    rslt = komodo_notarizeddata(10, &hash, &txid); // just right, but will return nothing (still too low)
    EXPECT_EQ(rslt, 0);
    rslt = komodo_notarizeddata(11, &hash, &txid); // over the height in the array, so should find the one below
    EXPECT_EQ(rslt, 9);
    EXPECT_EQ(hash, expected_hash);
    EXPECT_EQ(txid, expected_txid);
 }

// for l in `g 'parse notarisation' ~/.komodo/debug.log | pyline 'l.split()[8]'`; do hoek decodeTx '{"hex":"'`src/komodo-cli getrawtransaction "$l"`'"}' | jq '.outputs[1].script.op_return' | pyline 'import base64; print base64.b64decode(l).encode("hex")'; done

}
