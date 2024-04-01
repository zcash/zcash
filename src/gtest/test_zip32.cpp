#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <zcash/address/zip32.h>

// From https://github.com/zcash/zcash-test-vectors/blob/master/zcash_test_vectors/sapling/zip32.py
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

    auto m_1h = m.Derive(1 | HARDENED_KEY_LIMIT);
    EXPECT_EQ(m_1h.depth, 1);
    EXPECT_EQ(m_1h.parentFVKTag, 0x3a71c214);
    EXPECT_EQ(m_1h.childIndex, 1 | HARDENED_KEY_LIMIT);
    EXPECT_EQ(
        m_1h.chaincode,
        uint256S("dbaeca68fd2ef8b45ec23ee91bd694aa2759e010c668bb3e066b20a845aacc6f"));
    EXPECT_EQ(
        m_1h.expsk.ask,
        uint256S("04bd31e1a6218db693ff0802f029043ec20f3b0b8b148cdc04be7afb2ee9f7d5"));
    EXPECT_EQ(
        m_1h.expsk.nsk,
        uint256S("0a75e557f6fcbf672e0134d4ec2d51a3f358659b4b5c46f303e6cb22687c2a37"));
    EXPECT_EQ(
        m_1h.expsk.ovk,
        uint256S("691c33ec470a1697ca37ceb237bb7f1691d2a833543514cf1f8c343319763025"));
    EXPECT_EQ(
        m_1h.dk,
        uint256S("26d53444cbe2e9929f619d810a0d05ae0deece0a72c3a7e3df9a5fd60f4088f2"));
    EXPECT_THAT(
        m_1h.ToXFVK().DefaultAddress().d,
        testing::ElementsAreArray({ 0xbc, 0xc3, 0x23, 0xe8, 0xda, 0x39, 0xb4, 0x96, 0xc0, 0x50, 0x51 }));

    auto m_1h_2h = m_1h.Derive(2 | HARDENED_KEY_LIMIT);
    EXPECT_EQ(m_1h_2h.depth, 2);
    EXPECT_EQ(m_1h_2h.parentFVKTag, 0xcb238476);
    EXPECT_EQ(m_1h_2h.childIndex, 2 | HARDENED_KEY_LIMIT);
    EXPECT_EQ(
        m_1h_2h.chaincode,
        uint256S("daf7be6f80503ab34f14f236da9de2cf540ae3c100f520607980d0756c087944"));
    EXPECT_EQ(
        m_1h_2h.expsk.ask,
        uint256S("06512f33a6f9ae4b42fd71f9cfa08d3727522dd3089cad596fc3139eb65df37f"));
    EXPECT_EQ(
        m_1h_2h.expsk.nsk,
        uint256S("00debf5999f564a3e05a0d418cf40714399a32c1bdc98ba2eb4439a0e46e9c77"));
    EXPECT_EQ(
        m_1h_2h.expsk.ovk,
        uint256S("ac85619305763dc29b67b75e305e5323bda7d6a530736a88417f90bf0171fcd9"));
    EXPECT_EQ(
        m_1h_2h.dk,
        uint256S("d148325ff6faa682558de97a9fec61dd8dc10a96d0cd214bc531e0869a9e69e4"));
    EXPECT_THAT(
        m_1h_2h.ToXFVK().DefaultAddress().d,
        testing::ElementsAreArray({ 0x98, 0x82, 0x40, 0xce, 0xa4, 0xdb, 0xc3, 0x0a, 0x73, 0x75, 0x50 }));

    auto m_1_2hv = m_1h_2h.ToXFVK();
    EXPECT_EQ(m_1_2hv.depth, 2);
    EXPECT_EQ(m_1_2hv.parentFVKTag, 0xcb238476);
    EXPECT_EQ(m_1_2hv.childIndex, 2 | HARDENED_KEY_LIMIT);
    EXPECT_EQ(
        m_1_2hv.chaincode,
        uint256S("daf7be6f80503ab34f14f236da9de2cf540ae3c100f520607980d0756c087944"));
    EXPECT_EQ(
        m_1_2hv.fvk.ak,
        uint256S("4eab7275725c76ee4247ae8d941d4b53682e39da641785e097377144953f859a"));
    EXPECT_EQ(
        m_1_2hv.fvk.nk,
        uint256S("be4f5d4f36018511d23a1b9a9c87af8c6dbd20212da84121c1ce884f8aa266f1"));
    EXPECT_EQ(
        m_1_2hv.fvk.ovk,
        uint256S("ac85619305763dc29b67b75e305e5323bda7d6a530736a88417f90bf0171fcd9"));
    EXPECT_EQ(
        m_1_2hv.dk,
        uint256S("d148325ff6faa682558de97a9fec61dd8dc10a96d0cd214bc531e0869a9e69e4"));
    EXPECT_EQ(m_1_2hv.DefaultAddress(), m_1h_2h.ToXFVK().DefaultAddress());

    auto m_1h_2h_3h = m_1h_2h.Derive(3 | HARDENED_KEY_LIMIT);
    EXPECT_EQ(m_1h_2h_3h.depth, 3);
    EXPECT_EQ(m_1h_2h_3h.parentFVKTag, 0x2b2ddc0b);
    EXPECT_EQ(m_1h_2h_3h.childIndex, 3 | HARDENED_KEY_LIMIT);
    EXPECT_EQ(
        m_1h_2h_3h.chaincode,
        uint256S("4bc7bd5b38fcbdb259be013bef298c8de263e4c32ccb2bcdd2ce90762d01dc33"));
    EXPECT_EQ(
        m_1h_2h_3h.expsk.ask,
        uint256S("07afec4df829ace083d68145103c50692f331c4690cf52f13759e3214dd29345"));
    EXPECT_EQ(
        m_1h_2h_3h.expsk.nsk,
        uint256S("0bbba7ff6efab6fbabb5b727ed3d55e40efa0de858f8c0e357503f12c27ec81a"));
    EXPECT_EQ(
        m_1h_2h_3h.expsk.ovk,
        uint256S("e39e4b1b9c5c1b31988beffb515522a95de718afa880e36c9d2ebef20cea361e"));
    EXPECT_EQ(
        m_1h_2h_3h.dk,
        uint256S("6abb7b109c7270edaa3a36dc140d3f70bf8cd271b69d606f5aadf3a4596cfc57"));
    EXPECT_THAT(
        m_1h_2h_3h.ToXFVK().DefaultAddress().d,
        testing::ElementsAreArray({ 0x5a, 0x75, 0xbe, 0x14, 0x00, 0x53, 0x0b, 0x4b, 0x7a, 0xdd, 0x52 }));
}

TEST(ZIP32, ParseHDKeypathAccount) {
    auto expect_account = [](std::string sAccount, uint32_t coinType, long expected) {
        auto result = libzcash::ParseHDKeypathAccount(32, coinType, sAccount);
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), expected);
    };

    std::string sAccount = "m/32'/1234'/5'";
    expect_account(sAccount, 1234, 5);

    sAccount = "m/32'/1234'/50'";
    expect_account(sAccount, 1234, 50);

    sAccount = "m/32'/1234'/5'/0";
    expect_account(sAccount, 1234, 5);

    sAccount = "m/32'/133'/2147483646'/1";
    expect_account(sAccount, 133, 2147483646);
}

TEST(ZIP32, diversifier_index_t_increment)
{
    libzcash::diversifier_index_t d_zero(0);
    libzcash::diversifier_index_t d_one(1);
    EXPECT_TRUE(d_zero.increment());
    EXPECT_EQ(d_zero, d_one);
}

TEST(ZIP32, diversifier_index_t_lt)
{
    EXPECT_TRUE(libzcash::diversifier_index_t(0) < libzcash::diversifier_index_t(1));
    EXPECT_FALSE(libzcash::diversifier_index_t(1) < libzcash::diversifier_index_t(0));
    EXPECT_FALSE(libzcash::diversifier_index_t(0) < libzcash::diversifier_index_t(0));
    EXPECT_TRUE(libzcash::diversifier_index_t(0xfffffffe) < libzcash::diversifier_index_t(0xffffffff));
    EXPECT_FALSE(libzcash::diversifier_index_t(0xffffffff) < libzcash::diversifier_index_t(0xfffffffe));
    EXPECT_TRUE(libzcash::diversifier_index_t(0x01) < libzcash::diversifier_index_t(0xffffffff));
    EXPECT_FALSE(libzcash::diversifier_index_t(0xffffffff) < libzcash::diversifier_index_t(0x01));
}

TEST(ZIP32, DeriveChangeAddress)
{
    std::vector<unsigned char, secure_allocator<unsigned char>> rawSeed {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    HDSeed seed(rawSeed);

    auto accountSk = libzcash::SaplingExtendedSpendingKey::ForAccount(seed, 1, 0);
    auto extfvk = accountSk.first.ToXFVK();
    auto changeSk = accountSk.first.DeriveInternalKey();

    EXPECT_EQ(changeSk.ToXFVK().DefaultAddress(), extfvk.GetChangeAddress());
}

