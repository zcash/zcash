#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .


from test_framework.blocktools import derive_block_commitments_hash
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    BLOSSOM_BRANCH_ID,
    CANOPY_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    NU5_BRANCH_ID,
    assert_equal,
    bytes_to_hex_str,
    hex_str_to_bytes,
    nuparams,
    start_nodes,
)

TERMINATOR = b'\x00' * 32

# Verify block header field 'hashLightClientRoot' is set correctly for NU5 blocks.
class AuthDataRootTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 4

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(BLOSSOM_BRANCH_ID, 201),
            nuparams(HEARTWOOD_BRANCH_ID, 201),
            nuparams(CANOPY_BRANCH_ID, 201),
            nuparams(NU5_BRANCH_ID, 205),
            '-nurejectoldversions=false',
        ]] * self.num_nodes)

    def run_test(self):
        # Generate a block so we are on Canopy rules.
        self.nodes[0].generate(2)
        self.sync_all()

        # For blocks prior to NU5 activation, the hashBlockCommitments field of
        # the block header contains the root of the ZIP 221 history tree.
        for i in range(3):
            block_before = self.nodes[0].getblock('%d' % (202 + i))
            assert_equal(block_before['blockcommitments'], block_before['chainhistoryroot'])

            self.nodes[0].generate(1)
            self.sync_all()

        # From NU5 activation, the hashBlockCommitments field of the block
        # header contains a hash of various block commitments (per ZIP 244).
        for i in range(2):
            block_after = self.nodes[0].getblock('%d' % (205 + i))
            block_commitments = bytes_to_hex_str(derive_block_commitments_hash(
                hex_str_to_bytes(block_after['chainhistoryroot'])[::-1],
                hex_str_to_bytes(block_after['authdataroot'])[::-1],
            )[::-1])
            assert_equal(block_after['blockcommitments'], block_commitments)

            self.nodes[0].generate(1)
            self.sync_all()


if __name__ == '__main__':
    AuthDataRootTest().main()
