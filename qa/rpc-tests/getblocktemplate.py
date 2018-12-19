#!/usr/bin/env python
# Copyright (c) 2016 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, connect_nodes_bi, \
    initialize_chain_clean, start_nodes


class GetBlockTemplateTest(BitcoinTestFramework):
    '''
    Test getblocktemplate.
    '''

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        self.nodes = start_nodes(2, self.options.tmpdir)
        connect_nodes_bi(self.nodes,0,1)
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

        # Test 6: coinbasetxn checks
        assert('foundersreward' in tmpl['coinbasetxn'])
        assert(tmpl['coinbasetxn']['required'])

        # Test 7: hashFinalSaplingRoot checks
        assert('finalsaplingroothash' in tmpl)
        finalsaplingroothash = '3e49b5f954aa9d3545bc6c37744661eea48d7c34e3000d82b7f0010c30f4c2fb'
        assert_equal(finalsaplingroothash, tmpl['finalsaplingroothash'])

if __name__ == '__main__':
    GetBlockTemplateTest().main()
