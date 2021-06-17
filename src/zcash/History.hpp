#ifndef ZC_HISTORY_H_
#define ZC_HISTORY_H_

#include <stdexcept>
#include <unordered_map>
#include <boost/foreach.hpp>

#include "serialize.h"
#include "streams.h"
#include "uint256.h"

#include <rust/history.h>

#include "librustzcash.h"

namespace libzcash {

typedef uint64_t HistoryIndex;

class HistoryCache {
public:
    // updates to the persistent(db) layer
    std::unordered_map<HistoryIndex, HistoryNode> appends;
    // current length of the history
    HistoryIndex length;
    // how much back into the old state current update state
    // goes
    HistoryIndex updateDepth;
    // current root of the history
    uint256 root;
    // current epoch of this history state
    uint32_t epoch;

    HistoryCache(HistoryIndex initialLength, uint256 initialRoot, uint32_t initialEpoch) :
        length(initialLength), updateDepth(initialLength), root(initialRoot), epoch(initialEpoch) { };

    HistoryCache() { }

    // Extends current history update by one history node.
    void Extend(const HistoryNode &leaf);

    // Truncates current history to the new length.
    void Truncate(HistoryIndex newLength);
};

// New V1 history node with metadata based on block state.
HistoryNode NewV1Leaf(
    uint256 commitment,
    uint32_t time,
    uint32_t target,
    uint256 saplingRoot,
    uint256 totalWork,
    uint64_t height,
    uint64_t saplingTxCount
);

// New V2 history node with metadata based on block state.
HistoryNode NewV2Leaf(
    uint256 commitment,
    uint32_t time,
    uint32_t target,
    uint256 saplingRoot,
    uint256 orchardRoot,
    uint256 totalWork,
    uint64_t height,
    uint64_t saplingTxCount,
    uint64_t orchardTxCount
);

// Convert history node to tree node (with children references)
HistoryEntry NodeToEntry(const HistoryNode node, uint32_t left, uint32_t right);

// Convert history node to leaf node (end nodes without children)
HistoryEntry LeafToEntry(const HistoryNode node);

// Returns true if this epoch used the V1 history tree format.
bool IsV1HistoryTree(uint32_t epochId);

}

typedef libzcash::HistoryCache HistoryCache;
typedef libzcash::HistoryIndex HistoryIndex;

#endif /* ZC_HISTORY_H_ */