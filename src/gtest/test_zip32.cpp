#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <zcash/zip32.h>

// From https://github.com/zcash-hackworks/zcash-test-vectors/blob/master/sapling_zip32.py
// Sapling consistently uses little-endian encoding, but uint256S takes its input in
// big-endian byte order, so the test vectors below are byte-reversed.
TEST(ZIP32, TestVectors) {
    std::vector<unsigned char, secure_allocator<unsigned char>> rawSeed {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    HDSeed seed(rawSeed);

    auto m = libzcash::SaplingExtendedSpendingKey::Master(seed);
    EXPECT_EQ(m.depth, 0);
    EXPECT_EQ(m.parentFVKTag, 0);
    EXPECT_EQ(m.childIndex, 0);
    EXPECT_EQ(
        m.chaincode,
        uint256S("8e661820750d557e8b34733ebf7ecdfdf31c6d27724fb47aa372bf034b7c94d0"));
    EXPECT_EQ(
        m.expsk.ask,
        uint256S("06257454c907f6510ba1c1830ebf60657760a8869ee968a2b93260d3930cc0b6"));
    EXPECT_EQ(
        m.expsk.nsk,
        uint256S("06ea21888a749fd38eb443d20a030abd2e6e997f5db4f984bd1f2f3be8ed0482"));
    EXPECT_EQ(
        m.expsk.ovk,
        uint256S("21fb4adfa42183848306ffb27719f27d76cf9bb81d023c93d4b9230389845839"));
    EXPECT_EQ(
        m.dk,
        uint256S("72a196f93e8abc0935280ea2a96fa57d6024c9913e0f9fb3af96775bb77cc177"));
    EXPECT_THAT(
        m.ToXFVK().DefaultAddress().d,
        testing::ElementsAreArray({ 0xd8, 0x62, 0x1b, 0x98, 0x1c, 0xf3, 0x00, 0xe9, 0xd4, 0xcc, 0x89 }));

    auto m_1 = m.Derive(1);
    EXPECT_EQ(m_1.depth, 1);
    EXPECT_EQ(m_1.parentFVKTag, 0x3a71c214);
    EXPECT_EQ(m_1.childIndex, 1);
    EXPECT_EQ(
        m_1.chaincode,
        uint256S("e6bcda05678a43fad229334ef0b795a590e7c50590baf0d9b9031a690c114701"));
    EXPECT_EQ(
        m_1.expsk.ask,
        uint256S("0c357a2655b4b8d761794095df5cb402d3ba4a428cf6a88e7c2816a597c12b28"));
    EXPECT_EQ(
        m_1.expsk.nsk,
        uint256S("01ba6bff1018fd4eac04da7e3f2c6be9c229e662c5c4d1d6fc1ecafd8829a3e7"));
    EXPECT_EQ(
        m_1.expsk.ovk,
        uint256S("7474a4c518551bd82f14a7f7365a8ffa403c50cfeffedf026ada8688fc81135f"));
    EXPECT_EQ(
        m_1.dk,
        uint256S("dcb4c170d878510e96c4a74192d7eecde9c9912b00b99a12ec91d7a232e84de0"));
    EXPECT_THAT(
        m_1.ToXFVK().DefaultAddress().d,
        testing::ElementsAreArray({ 0x8b, 0x41, 0x38, 0x32, 0x0d, 0xfa, 0xfd, 0x7b, 0x39, 0x97, 0x81 }));

    auto m_1_2h = m_1.Derive(2 | ZIP32_HARDENED_KEY_LIMIT);
    EXPECT_EQ(m_1_2h.depth, 2);
    EXPECT_EQ(m_1_2h.parentFVKTag, 0x079e99db);
    EXPECT_EQ(m_1_2h.childIndex, 2 | ZIP32_HARDENED_KEY_LIMIT);
    EXPECT_EQ(
        m_1_2h.chaincode,
        uint256S("35d4a883737742ca41a4baa92323bdb3c93dcb3b462a26b039971bedf415ce97"));
    EXPECT_EQ(
        m_1_2h.expsk.ask,
        uint256S("0dc6e4fe846bda925c82e632980434e17b51dac81fc4821fa71334ee3c11e88b"));
    EXPECT_EQ(
        m_1_2h.expsk.nsk,
        uint256S("0c99a63a275c1c66734761cfb9c62fe9bd1b953f579123d3d0e769c59d057837"));
    EXPECT_EQ(
        m_1_2h.expsk.ovk,
        uint256S("bc1328fc5eb693e18875c5149d06953b11d39447ebd6e38c023c22962e1881cf"));
    EXPECT_EQ(
        m_1_2h.dk,
        uint256S("377bb062dce7e0dcd8a0054d0ca4b4d1481b3710bfa1df12ca46ff9e9fa1eda3"));
    EXPECT_THAT(
        m_1_2h.ToXFVK().DefaultAddress().d,
        testing::ElementsAreArray({ 0xe8, 0xd0, 0x37, 0x93, 0xcd, 0xd2, 0xba, 0xcc, 0x9c, 0x70, 0x41 }));

    auto m_1_2hv = m_1_2h.ToXFVK();
    EXPECT_EQ(m_1_2hv.depth, 2);
    EXPECT_EQ(m_1_2hv.parentFVKTag, 0x079e99db);
    EXPECT_EQ(m_1_2hv.childIndex, 2 | ZIP32_HARDENED_KEY_LIMIT);
    EXPECT_EQ(
        m_1_2hv.chaincode,
        uint256S("35d4a883737742ca41a4baa92323bdb3c93dcb3b462a26b039971bedf415ce97"));
    EXPECT_EQ(
        m_1_2hv.fvk.ak,
        uint256S("4138cffdf7200e52d4e9f4384481b4a4c4d070493a5e401e4ffa850f5a92c5a6"));
    EXPECT_EQ(
        m_1_2hv.fvk.nk,
        uint256S("11eee22577304f660cc036bc84b3fc88d1ec50ae8a4d657beb6b211659304e30"));
    EXPECT_EQ(
        m_1_2hv.fvk.ovk,
        uint256S("bc1328fc5eb693e18875c5149d06953b11d39447ebd6e38c023c22962e1881cf"));
    EXPECT_EQ(
        m_1_2hv.dk,
        uint256S("377bb062dce7e0dcd8a0054d0ca4b4d1481b3710bfa1df12ca46ff9e9fa1eda3"));
    EXPECT_EQ(m_1_2hv.DefaultAddress(), m_1_2h.ToXFVK().DefaultAddress());

    // Hardened derivation from an xfvk fails
    EXPECT_FALSE(m_1_2hv.Derive(3 | ZIP32_HARDENED_KEY_LIMIT));

    // Non-hardened derivation succeeds
    auto maybe_m_1_2hv_3 = m_1_2hv.Derive(3);
    EXPECT_TRUE(maybe_m_1_2hv_3);

    auto m_1_2hv_3 = maybe_m_1_2hv_3.get();
    EXPECT_EQ(m_1_2hv_3.depth, 3);
    EXPECT_EQ(m_1_2hv_3.parentFVKTag, 0x7583c148);
    EXPECT_EQ(m_1_2hv_3.childIndex, 3);
    EXPECT_EQ(
        m_1_2hv_3.chaincode,
        uint256S("e8e7d6a74a5a1c05be41baec7998d91f7b3603a4c0af495b0d43ba81cf7b938d"));
    EXPECT_EQ(
        m_1_2hv_3.fvk.ak,
        uint256S("a3a697bdda9d648d32a97553de4754b2fac866d726d3f2c436259c507bc585b1"));
    EXPECT_EQ(
        m_1_2hv_3.fvk.nk,
        uint256S("4f66c0814b769963f3bf1bc001270b50edabb27de042fc8a5607d2029e0488db"));
    EXPECT_EQ(
        m_1_2hv_3.fvk.ovk,
        uint256S("f61a699934dc78441324ef628b4b4721611571e8ee3bd591eb3d4b1cfae0b969"));
    EXPECT_EQ(
        m_1_2hv_3.dk,
        uint256S("6ee53b1261f2c9c0f7359ab236f87b52a0f1b0ce43305cdad92ebb63c350cbbe"));
    EXPECT_THAT(
        m_1_2hv_3.DefaultAddress().d,
        testing::ElementsAreArray({ 0x03, 0x0f, 0xfb, 0x26, 0x3a, 0x93, 0x9e, 0x23, 0x0e, 0x96, 0xdd }));
}
