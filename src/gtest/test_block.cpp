#include <gtest/gtest.h>

#include "primitives/block.h"
#include "streams.h"
#include "version.h"


TEST(BlockTests, HeaderSizeIsExpected) {
    // Dummy header with an empty Equihash solution.
    CBlockHeader header;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << header;

    ASSERT_EQ(ss.size(), CBlockHeader::HEADER_SIZE);
}
