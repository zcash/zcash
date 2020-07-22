#include <gtest/gtest.h>

#include "main.h"
#include "utiltest.h"
#include "zcash/History.hpp"

// Fake an empty view
class FakeCoinsViewDB : public CCoinsView {
public:
    FakeCoinsViewDB() {}

    bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const {
        return false;
    }

    bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
        return false;
    }

    bool GetNullifier(const uint256 &nf, ShieldedType type) const {
        return false;
    }

    bool GetCoins(const uint256 &txid, CCoins &coins) const {
        return false;
    }

    bool HaveCoins(const uint256 &txid) const {
        return false;
    }

    uint256 GetBestBlock() const {
        uint256 a;
        return a;
    }

    uint256 GetBestAnchor(ShieldedType type) const {
        uint256 a;
        return a;
    }

    bool BatchWrite(CCoinsMap &mapCoins,
                    const uint256 &hashBlock,
                    const uint256 &hashSproutAnchor,
                    const uint256 &hashSaplingAnchor,
                    CAnchorsSproutMap &mapSproutAnchors,
                    CAnchorsSaplingMap &mapSaplingAnchors,
                    CNullifiersMap &mapSproutNullifiers,
                    CNullifiersMap saplingNullifiersMap) {
        return false;
    }

    bool GetStats(CCoinsStats &stats) const {
        return false;
    }

    HistoryIndex GetHistoryLength(uint32_t branchId) const {
        return 0;
    }

    HistoryNode GetHistoryAt(uint32_t branchId, HistoryIndex index) const {
        return HistoryNode();
    }
};

HistoryNode getLeafN(uint64_t block_num) {
    HistoryNode node = libzcash::NewLeaf(
        uint256(),
        block_num*10,
        block_num*13,
        uint256(),
        uint256(),
        block_num,
        3
    );
    return node;
}

TEST(History, Smoky) {
    // Fake an empty view
    FakeCoinsViewDB fakeDB;
    CCoinsViewCache view(&fakeDB);

    // Test initial value
    EXPECT_EQ(view.GetHistoryLength(0), 0);

    view.PushHistoryNode(1, getLeafN(1));

    EXPECT_EQ(view.GetHistoryLength(1), 1);

    view.PushHistoryNode(1, getLeafN(2));

    EXPECT_EQ(view.GetHistoryLength(1), 3);

    view.PushHistoryNode(1, getLeafN(3));

    EXPECT_EQ(view.GetHistoryLength(1), 4);

    view.PushHistoryNode(1, getLeafN(4));

    uint256 h4Root = view.GetHistoryRoot(1);

    EXPECT_EQ(view.GetHistoryLength(1), 7);

    view.PushHistoryNode(1, getLeafN(5));
    EXPECT_EQ(view.GetHistoryLength(1), 8);

    view.PopHistoryNode(1);

    EXPECT_EQ(view.GetHistoryLength(1), 7);
    EXPECT_EQ(h4Root, view.GetHistoryRoot(1));
}


TEST(History, EpochBoundaries) {
    // Fake an empty view
    FakeCoinsViewDB fakeDB;
    CCoinsViewCache view(&fakeDB);

    view.PushHistoryNode(1, getLeafN(1));

    EXPECT_EQ(view.GetHistoryLength(1), 1);

    view.PushHistoryNode(1, getLeafN(2));

    EXPECT_EQ(view.GetHistoryLength(1), 3);

    view.PushHistoryNode(1, getLeafN(3));

    EXPECT_EQ(view.GetHistoryLength(1), 4);

    view.PushHistoryNode(1, getLeafN(4));

    uint256 h4Root = view.GetHistoryRoot(1);

    EXPECT_EQ(view.GetHistoryLength(1), 7);

    view.PushHistoryNode(1, getLeafN(5));
    EXPECT_EQ(view.GetHistoryLength(1), 8);


    // New Epoch(2)
    view.PushHistoryNode(2, getLeafN(6));
    EXPECT_EQ(view.GetHistoryLength(1), 8);
    EXPECT_EQ(view.GetHistoryLength(2), 1);

    view.PushHistoryNode(2, getLeafN(7));
    EXPECT_EQ(view.GetHistoryLength(1), 8);
    EXPECT_EQ(view.GetHistoryLength(2), 3);

    view.PushHistoryNode(2, getLeafN(8));
    EXPECT_EQ(view.GetHistoryLength(1), 8);
    EXPECT_EQ(view.GetHistoryLength(2), 4);

    // Rolling epoch back to 1
    view.PopHistoryNode(2);
    EXPECT_EQ(view.GetHistoryLength(2), 3);

    view.PopHistoryNode(2);
    EXPECT_EQ(view.GetHistoryLength(2), 1);
    EXPECT_EQ(view.GetHistoryLength(1), 8);

    // And even rolling epoch 1 back a bit
    view.PopHistoryNode(1);
    EXPECT_EQ(view.GetHistoryLength(1), 7);

    // And also rolling epoch 2 back to 0
    view.PopHistoryNode(2);
    EXPECT_EQ(view.GetHistoryLength(2), 0);

}

TEST(History, WinHistoryFailure) {
    FakeCoinsViewDB fakeDB;
    CCoinsViewCache view(&fakeDB);

    const int NODE_SERIALIZED_LENGTH = 171;
    const auto consensusBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_HEARTWOOD].nBranchId;

    std::array<unsigned char, NODE_SERIALIZED_LENGTH> node0 = {
        0x43, 0x56, 0xbe, 0x39, 0x71, 0x06, 0x06, 0x76, 0xfe, 0xe2, 0xe9, 0x8d, 0x84, 0x5e, 0xd0, 0xb4, 0xf8, 0xec, 0x35, 0x3c, 0xa9, 0x64, 0x89, 0x69, 0xc8, 0xd1, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x32, 0x10, 0x5f, 0x48, 0x32, 0x10, 0x5f, 0x4a, 0x95, 0x02, 0x1c, 0x4a, 0x95, 0x02, 0x1c, 0x79, 0x6c, 0x97, 0x96, 0x73, 0x30, 0xbe, 0x9e, 0x95, 0x4e, 0x5f, 0x15, 0x55, 0x11, 0xda, 0xda, 0xed, 0x26, 0x64, 0xb2, 0x40, 0x33, 0x41, 0x5c, 0x6d, 0x29, 0xe2, 0xf0, 0x00, 0x83, 0xe4, 0x11, 0x79, 0x6c, 0x97, 0x96, 0x73, 0x30, 0xbe, 0x9e, 0x95, 0x4e, 0x5f, 0x15, 0x55, 0x11, 0xda, 0xda, 0xed, 0x26, 0x64, 0xb2, 0x40, 0x33, 0x41, 0x5c, 0x6d, 0x29, 0xe2, 0xf0, 0x00, 0x83, 0xe4, 0x11, 0x44, 0xf8, 0x78, 0x1a, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x58, 0xc7, 0x0d, 0x00, 0xfe, 0x58, 0xc7, 0x0d, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0xd7, 0xdf, 0x35, 0x1d, 0x00, 0x00, 0x00, 0xf0, 0x2c, 0x8f
    };
    std::array<unsigned char, NODE_SERIALIZED_LENGTH> node1 = {
        0xac, 0x21, 0x11, 0xdb, 0x8b, 0xe0, 0xf5, 0xa3, 0xe4, 0x4b, 0x72, 0xa0, 0x76, 0x9c, 0x4f, 0x74, 0x09, 0xae, 0x43, 0x1b, 0xad, 0xdd, 0x0a, 0x8d, 0x34, 0x4d, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x99, 0x32, 0x10, 0x5f, 0x99, 0x32, 0x10, 0x5f, 0x50, 0x8a, 0x02, 0x1c, 0x50, 0x8a, 0x02, 0x1c, 0xce, 0x9c, 0x38, 0x9e, 0x15, 0x65, 0xf8, 0x8b, 0xf5, 0xed, 0x0d, 0x67, 0xa6, 0xe1, 0xd5, 0x92, 0x42, 0x82, 0x20, 0x3c, 0x57, 0x86, 0x74, 0xe1, 0xc4, 0x9b, 0x23, 0xaf, 0xb8, 0xc2, 0xe3, 0x14, 0xce, 0x9c, 0x38, 0x9e, 0x15, 0x65, 0xf8, 0x8b, 0xf5, 0xed, 0x0d, 0x67, 0xa6, 0xe1, 0xd5, 0x92, 0x42, 0x82, 0x20, 0x3c, 0x57, 0x86, 0x74, 0xe1, 0xc4, 0x9b, 0x23, 0xaf, 0xb8, 0xc2, 0xe3, 0x14, 0xf9, 0xc4, 0xb2, 0xc6, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x59, 0xc7, 0x0d, 0x00, 0xfe, 0x59, 0xc7, 0x0d, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x27, 0xb2, 0xa0, 0x7a, 0x79, 0x7e, 0x1a, 0xf5, 0xfc, 0x7b
    };
    std::array<unsigned char, NODE_SERIALIZED_LENGTH> node2 = {
        0xea, 0x48, 0x12, 0x5b, 0xd5, 0xac, 0x19, 0xb0, 0xb2, 0xc5, 0x68, 0xe6, 0x6f, 0xb3, 0xfe, 0x43, 0x75, 0x5c, 0x25, 0x96, 0x2c, 0x82, 0x1c, 0xd6, 0x29, 0x25, 0xbd, 0xaa, 0x2e, 0x4e, 0x13, 0x5e, 0x48, 0x32, 0x10, 0x5f, 0x99, 0x32, 0x10, 0x5f, 0x4a, 0x95, 0x02, 0x1c, 0x50, 0x8a, 0x02, 0x1c, 0x79, 0x6c, 0x97, 0x96, 0x73, 0x30, 0xbe, 0x9e, 0x95, 0x4e, 0x5f, 0x15, 0x55, 0x11, 0xda, 0xda, 0xed, 0x26, 0x64, 0xb2, 0x40, 0x33, 0x41, 0x5c, 0x6d, 0x29, 0xe2, 0xf0, 0x00, 0x83, 0xe4, 0x11, 0xce, 0x9c, 0x38, 0x9e, 0x15, 0x65, 0xf8, 0x8b, 0xf5, 0xed, 0x0d, 0x67, 0xa6, 0xe1, 0xd5, 0x92, 0x42, 0x82, 0x20, 0x3c, 0x57, 0x86, 0x74, 0xe1, 0xc4, 0x9b, 0x23, 0xaf, 0xb8, 0xc2, 0xe3, 0x14, 0x3d, 0xbd, 0x2b, 0xe1, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x58, 0xc7, 0x0d, 0x00, 0xfe, 0x59, 0xc7, 0x0d, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x08, 0x00, 0x00, 0x00
    };

    view.PushHistoryNode(consensusBranchId, node0);
    view.PushHistoryNode(consensusBranchId, node1);
    // view.PushHistoryNode(1, node2);

    EXPECT_EQ(view.GetHistoryRoot(consensusBranchId), uint256S(
        "4864e541c5952db2edf875a14f21964a1228bbca17c0417c8f800a2274c261c0"
    ));
}

TEST(History, WinHistoryPassing) {
    FakeCoinsViewDB fakeDB;
    CCoinsViewCache view(&fakeDB);

    const int NODE_SERIALIZED_LENGTH = 171;
    const auto consensusBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_HEARTWOOD].nBranchId;

    std::array<unsigned char, NODE_SERIALIZED_LENGTH> node0 = {
        0x43, 0x56, 0xbe, 0x39, 0x71, 0x06, 0x06, 0x76, 0xfe, 0xe2, 0xe9, 0x8d, 0x84, 0x5e, 0xd0, 0xb4, 0xf8, 0xec, 0x35, 0x3c, 0xa9, 0x64, 0x89, 0x69, 0xc8, 0xd1, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x32, 0x10, 0x5f, 0x48, 0x32, 0x10, 0x5f, 0x4a, 0x95, 0x02, 0x1c, 0x4a, 0x95, 0x02, 0x1c, 0x79, 0x6c, 0x97, 0x96, 0x73, 0x30, 0xbe, 0x9e, 0x95, 0x4e, 0x5f, 0x15, 0x55, 0x11, 0xda, 0xda, 0xed, 0x26, 0x64, 0xb2, 0x40, 0x33, 0x41, 0x5c, 0x6d, 0x29, 0xe2, 0xf0, 0x00, 0x83, 0xe4, 0x11, 0x79, 0x6c, 0x97, 0x96, 0x73, 0x30, 0xbe, 0x9e, 0x95, 0x4e, 0x5f, 0x15, 0x55, 0x11, 0xda, 0xda, 0xed, 0x26, 0x64, 0xb2, 0x40, 0x33, 0x41, 0x5c, 0x6d, 0x29, 0xe2, 0xf0, 0x00, 0x83, 0xe4, 0x11, 0x44, 0xf8, 0x78, 0x1a, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x58, 0xc7, 0x0d, 0x00, 0xfe, 0x58, 0xc7, 0x0d, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0xd7, 0xdf, 0x35, 0x1d, 0x00, 0x00, 0x00, 0xf0, 0x2c, 0x8f
    };
    std::array<unsigned char, NODE_SERIALIZED_LENGTH> node1 = {
        0xac, 0x21, 0x11, 0xdb, 0x8b, 0xe0, 0xf5, 0xa3, 0xe4, 0x4b, 0x72, 0xa0, 0x76, 0x9c, 0x4f, 0x74, 0x09, 0xae, 0x43, 0x1b, 0xad, 0xdd, 0x0a, 0x8d, 0x34, 0x4d, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x99, 0x32, 0x10, 0x5f, 0x99, 0x32, 0x10, 0x5f, 0x50, 0x8a, 0x02, 0x1c, 0x50, 0x8a, 0x02, 0x1c, 0xce, 0x9c, 0x38, 0x9e, 0x15, 0x65, 0xf8, 0x8b, 0xf5, 0xed, 0x0d, 0x67, 0xa6, 0xe1, 0xd5, 0x92, 0x42, 0x82, 0x20, 0x3c, 0x57, 0x86, 0x74, 0xe1, 0xc4, 0x9b, 0x23, 0xaf, 0xb8, 0xc2, 0xe3, 0x14, 0xce, 0x9c, 0x38, 0x9e, 0x15, 0x65, 0xf8, 0x8b, 0xf5, 0xed, 0x0d, 0x67, 0xa6, 0xe1, 0xd5, 0x92, 0x42, 0x82, 0x20, 0x3c, 0x57, 0x86, 0x74, 0xe1, 0xc4, 0x9b, 0x23, 0xaf, 0xb8, 0xc2, 0xe3, 0x14, 0xf9, 0xc4, 0xb2, 0xc6, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x59, 0xc7, 0x0d, 0x00, 0xfe, 0x59, 0xc7, 0x0d, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0xd7, 0xdf, 0x35, 0x1d, 0x00, 0x00, 0x00, 0xf0, 0x2c, 0x8f
    };
    std::array<unsigned char, NODE_SERIALIZED_LENGTH> node2 = {
        0xea, 0x48, 0x12, 0x5b, 0xd5, 0xac, 0x19, 0xb0, 0xb2,
        0xc5, 0x68, 0xe6, 0x6f, 0xb3, 0xfe, 0x43, 0x75, 0x5c,
        0x25, 0x96, 0x2c, 0x82, 0x1c, 0xd6, 0x29, 0x25, 0xbd,
        0xaa, 0x2e, 0x4e, 0x13, 0x5e, 0x48, 0x32, 0x10, 0x5f,
        0x99, 0x32, 0x10, 0x5f, 0x4a, 0x95, 0x02, 0x1c, 0x50,
        0x8a, 0x02, 0x1c, 0x79, 0x6c, 0x97, 0x96, 0x73, 0x30,
        0xbe, 0x9e, 0x95, 0x4e, 0x5f, 0x15, 0x55, 0x11, 0xda,
        0xda, 0xed, 0x26, 0x64, 0xb2, 0x40, 0x33, 0x41, 0x5c,
        0x6d, 0x29, 0xe2, 0xf0, 0x00, 0x83, 0xe4, 0x11, 0xce,
        0x9c, 0x38, 0x9e, 0x15, 0x65, 0xf8, 0x8b, 0xf5, 0xed,
        0x0d, 0x67, 0xa6, 0xe1, 0xd5, 0x92, 0x42, 0x82, 0x20,
        0x3c, 0x57, 0x86, 0x74, 0xe1, 0xc4, 0x9b, 0x23, 0xaf,
        0xb8, 0xc2, 0xe3, 0x14, 0x3d, 0xbd, 0x2b, 0xe1, 0xc7,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xfe, 0x58, 0xc7, 0x0d, 0x00, 0xfe, 0x59, 0xc7, 0x0d,
        0x00, 0x05, 0x00, 0x00, 0x20, 0x00, 0x00, 0xb0, 0x7b,
        0x7f, 0x00, 0x00, 0xd0, 0x79, 0x00, 0xb0, 0x7b, 0x7f
    };

    view.PushHistoryNode(consensusBranchId, node0);
    view.PushHistoryNode(consensusBranchId, node1);
    // view.PushHistoryNode(1, node2);

    EXPECT_EQ(view.GetHistoryRoot(consensusBranchId), uint256S(
        "4864e541c5952db2edf875a14f21964a1228bbca17c0417c8f800a2274c261c0"
    ));
}

