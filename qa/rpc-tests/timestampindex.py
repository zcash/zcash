#!/usr/bin/env python
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
#
# Test timestampindex generation and fetching for insightexplorer

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

import time

from test_framework.test_framework import BitcoinTestFramework

from test_framework.util import (
    assert_equal,
    initialize_chain_clean,
    start_nodes,
    stop_nodes,
    connect_nodes,
    wait_bitcoinds,
)


class TimestampIndexTest(BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self):
        # -insightexplorer causes spentindex to be enabled (fSpentIndex = true)

        self.nodes = start_nodes(
            3, self.options.tmpdir,
            [['-debug', '-txindex', '-experimentalfeatures', '-insightexplorer']]*3)
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[0], 2)

        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        blockhashes = []
        print "Mining blocks..."
        for _ in range(8):
            blockhashes.extend(self.nodes[0].generate(1))
            time.sleep(1)
        self.sync_all()
        times = [self.nodes[1].getblock(b)['time'] for b in blockhashes]
        assert_equal(blockhashes, self.nodes[1].getblockhashes(times[0]+100, 0))
        # test various ranges; the api returns blocks have times LESS THAN
        # 'high' (first argument), not less than or equal, hence the +1
        assert_equal(blockhashes[0:8], self.nodes[1].getblockhashes(times[8-1]+1, times[0]))
        assert_equal(blockhashes[2:6], self.nodes[1].getblockhashes(times[6-1]+1, times[2]))
        assert_equal(blockhashes[5:8], self.nodes[1].getblockhashes(times[8-1]+1, times[5]))
        assert_equal(blockhashes[6:7], self.nodes[1].getblockhashes(times[7-1]+1, times[6]))
        assert_equal(blockhashes[4:6], self.nodes[1].getblockhashes(times[6-1]+1, times[4]))
        assert_equal(blockhashes[1:1], self.nodes[1].getblockhashes(times[1-1]+1, times[1]))

        # Restart all nodes to ensure indices are saved to disk and recovered
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # generating multiple blocks within the same second should
        # result in timestamp index entries with unique times
        # (not realistic but there is logic to ensure this)
        blockhashes = self.nodes[0].generate(10)
        self.sync_all()
        firsttime = self.nodes[1].getblock(blockhashes[0])['time']
        assert_equal(blockhashes, self.nodes[1].getblockhashes(firsttime+10+1, firsttime))

        # the api can also return 'logical' times, which is the key of the
        # timestamp index (the content being blockhash). Logical times are
        # block times when possible, but since keys must be unique, and the
        # previous 10 block were generated in much less than 10 seconds,
        # each logical time should be one greater than the previous.
        results = self.nodes[1].getblockhashes(
            firsttime+10+1, firsttime,
            {'logicalTimes': True})
        ltimes = [r['logicalts'] for r in results]
        assert_equal(ltimes, range(firsttime, firsttime+10))

        # there's also a flag to exclude orphaned blocks; results should
        # be the same in this test
        assert_equal(
            results,
            self.nodes[1].getblockhashes(
                firsttime+10+1, firsttime,
                {'logicalTimes': True, 'noOrphans': True}))


if __name__ == '__main__':
    TimestampIndexTest().main()
