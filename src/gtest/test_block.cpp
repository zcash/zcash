#include <gtest/gtest.h>

#include "primitives/block.h"


TEST(block_tests, header_size_is_expected) {
    // Dummy header with an empty Equihash solution.
    CBlockHeader header;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << header;

    ASSERT_EQ(ss.size(), CBlockHeader::HEADER_SIZE);
}
