#include "zcash/History.hpp"

#include <stdexcept>


#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "librustzcash.h"

namespace libzcash {

void HistoryCache::Extend(const HistoryNode &leaf) {
    appends[length++] = leaf;
}

void HistoryCache::Truncate(HistoryIndex newLength) {
    // Remove any to-be-appended nodes beyond the new length. The array representation is
    // zero-indexed, and HistoryIndex is unsigned, so we handle the truncate-to-zero case
    // separately.
    if (newLength > 0) {
        for (HistoryIndex idx = length; idx >= newLength; idx--) {
            appends.erase(idx);
        }
    } else {
        appends.clear();
    }

    length = newLength;
    // we track how deep updates go back in the tree, so we could later
    // update everything starting from `updateDepth`
    //
    // imagine we rolled two blocks back and then put another 3 blocks on top
    // of the rolled back state. In that case `updateDepth` will be H-3, while length
    // will be H (where H is a final chain height after such operation). So we know that
    // history entries in the range of H-3..H are expected to be pushed into the database
    // to replace/append to the persistent nodes there.
    if (updateDepth > length) updateDepth = length;
}

HistoryNode NewNode(
        uint256 subtreeCommitment,
        uint32_t startTime,
        uint32_t endTime,
        uint32_t startTarget,
        uint32_t endTarget,
        uint256 startSaplingRoot,
        uint256 endSaplingRoot,
        uint256 subtreeTotalWork,
        uint64_t startHeight,
        uint64_t endHeight,
        uint64_t saplingTxCount
    )
{
    CDataStream buf(SER_DISK, 0);
    HistoryNode result = {};

    buf << subtreeCommitment;
    buf << startTime;
    buf << endTime;
    buf << startTarget;
    buf << endTarget;
    buf << startSaplingRoot;
    buf << endSaplingRoot;
    buf << subtreeTotalWork;
    buf << COMPACTSIZE(startHeight);
    buf << COMPACTSIZE(endHeight);
    buf << COMPACTSIZE(saplingTxCount);

    assert(buf.size() <= NODE_SERIALIZED_LENGTH);
    std::copy(std::begin(buf), std::end(buf), result.bytes);
    return result;
}

HistoryNode NewLeaf(
    uint256 commitment,
    uint32_t time,
    uint32_t target,
    uint256 saplingRoot,
    uint256 totalWork,
    uint64_t height,
    uint64_t saplingTxCount
) {
    return NewNode(
        commitment,
        time,
        time,
        target,
        target,
        saplingRoot,
        saplingRoot,
        totalWork,
        height,
        height,
        saplingTxCount
    );
}

HistoryEntry NodeToEntry(const HistoryNode node, uint32_t left, uint32_t right) {
    CDataStream buf(SER_DISK, 0);
    HistoryEntry result;

    uint8_t code = 0;
    buf << code;
    buf << left;
    buf << right;

    std::array<unsigned char, NODE_SERIALIZED_LENGTH> tmpMmrNode;
    std::copy(node.bytes, node.bytes + NODE_SERIALIZED_LENGTH, std::begin(tmpMmrNode));

    buf << tmpMmrNode;

    assert(buf.size() <= ENTRY_SERIALIZED_LENGTH);
    std::copy(std::begin(buf), std::end(buf), result.bytes);

    return result;
}

HistoryEntry LeafToEntry(const HistoryNode node) {
    CDataStream buf(SER_DISK, 0);
    HistoryEntry result;

    uint8_t code = 1;
    buf << code;

    std::array<unsigned char, NODE_SERIALIZED_LENGTH> tmpMmrNode;
    std::copy(node.bytes, node.bytes + NODE_SERIALIZED_LENGTH, std::begin(tmpMmrNode));

    buf << tmpMmrNode;

    assert(buf.size() <= ENTRY_SERIALIZED_LENGTH);
    std::copy(std::begin(buf), std::end(buf), result.bytes);

    return result;
}

}
