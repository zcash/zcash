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

TEST(History, GarbageMemoryHash) {
    const auto consensusBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_HEARTWOOD].nBranchId;

    FakeCoinsViewDB fakeDB;
    CCoinsViewCache view(&fakeDB);

    // Hash two history nodes
    HistoryNode node0 = getLeafN(1);
    HistoryNode node1 = getLeafN(2);

    view.PushHistoryNode(consensusBranchId, node0);
    view.PushHistoryNode(consensusBranchId, node1);

    uint256 historyRoot = view.GetHistoryRoot(consensusBranchId);

    // Change garbage memory and re-hash nodes
    FakeCoinsViewDB fakeDBGarbage;
    CCoinsViewCache viewGarbage(&fakeDBGarbage);

    HistoryNode node0Garbage = getLeafN(1);
    HistoryNode node1Garbage = getLeafN(2);

    EXPECT_NE(node0.bytes, node0Garbage.bytes);
    EXPECT_NE(node1.bytes, node1Garbage.bytes);

    viewGarbage.PushHistoryNode(consensusBranchId, node0Garbage);
    viewGarbage.PushHistoryNode(consensusBranchId, node1Garbage);

    uint256 historyRootGarbage = viewGarbage.GetHistoryRoot(consensusBranchId);

    // Check history root and garbage history root are equal
    EXPECT_EQ(historyRoot, historyRootGarbage);
}
