#!/usr/bin/env python3
# Copyright (c) 2016 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    BLOSSOM_BRANCH_ID,
    CANOPY_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    NU5_BRANCH_ID,
    nuparams,
    start_nodes,
)

class GetBlockTemplateTest(BitcoinTestFramework):
    '''
    Test getblocktemplate.
    '''

    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.setup_clean_chain = True

    def setup_network(self, split=False):
        args = [nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 1),
            nuparams(CANOPY_BRANCH_ID, 1),
            nuparams(NU5_BRANCH_ID, 1),
        ]
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, [args] * self.num_nodes)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        node = self.nodes[0]
        node.generate(1) # Mine a block to leave initial block download

        # Test 1: Default to coinbasetxn
        tmpl = node.getblocktemplate()
        assert('coinbasetxn' in tmpl)
        assert('coinbasevalue' not in tmpl)

        # Test 2: Get coinbasetxn if requested
        tmpl = node.getblocktemplate({'capabilities': ['coinbasetxn']})
        assert('coinbasetxn' in tmpl)
        assert('coinbasevalue' not in tmpl)

        # Test 3: coinbasevalue not supported if requested
        tmpl = node.getblocktemplate({'capabilities': ['coinbasevalue']})
        assert('coinbasetxn' in tmpl)
        assert('coinbasevalue' not in tmpl)

        # Test 4: coinbasevalue not supported if both requested
        tmpl = node.getblocktemplate({'capabilities': ['coinbasetxn', 'coinbasevalue']})
        assert('coinbasetxn' in tmpl)
        assert('coinbasevalue' not in tmpl)

        # Test 5: General checks
        tmpl = node.getblocktemplate()
        assert_equal(16, len(tmpl['noncerange']))
        # should be proposing height 2, since current tip is height 1
        assert_equal(2, tmpl['height'])

        # Test 6: coinbasetxn checks
        assert(tmpl['coinbasetxn']['required'])

        # Test 7: blockcommitmentshash checks
        assert('blockcommitmentshash' in tmpl)
        assert('00' * 32 != tmpl['finalsaplingroothash'])

if __name__ == '__main__':
    GetBlockTemplateTest().main()
