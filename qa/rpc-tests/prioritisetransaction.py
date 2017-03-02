#!/usr/bin/env python2
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from time import *
from test_framework.mininode import COIN

class PrioritiseTransactionTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        self.nodes = []
        # Start nodes with tiny block size of 11kb
        self.nodes.append(start_node(0, self.options.tmpdir, ["-blockprioritysize=7000", "-blockmaxsize=11000", "-maxorphantx=1000", "-relaypriority=true", "-printpriority=1"]))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-blockprioritysize=7000", "-blockmaxsize=11000", "-maxorphantx=1000", "-relaypriority=true", "-printpriority=1"]))
        connect_nodes(self.nodes[1], 0)
        self.is_network_split=False
        self.sync_all()

    # Returns txid if operation was a success or None
    def wait_and_assert_operationid_status(self, myopid, in_status='success', in_errormsg=None):
        print('waiting for async operation {}'.format(myopid))
        opids = []
        opids.append(myopid)
        timeout = 300
        status = None
        errormsg = None
        txid = None
        for x in xrange(1, timeout):
            results = self.nodes[0].z_getoperationresult(opids)
            if len(results)==0:
                sleep(1)
            else:
                status = results[0]["status"]
                if status == "failed":
                    errormsg = results[0]['error']['message']
                elif status == "success":
                    txid = results[0]['result']['txid']
                break
        print('...returned status: {}'.format(status))
        assert_equal(in_status, status)
        if errormsg is not None:
            assert(in_errormsg is not None)
            assert_equal(in_errormsg in errormsg, True)
            print('...returned error: {}'.format(errormsg))
        return txid

    def run_test (self):
        # tx priority is calculated: priority = sum(input_value_in_base_units * input_age)/size_in_bytes

        print "Mining 11kb blocks..."
        self.nodes[0].generate(501)

        base_fee = self.nodes[0].getnetworkinfo()['relayfee']

        # 11 kb blocks will only hold about 50 txs, so this will fill mempool with older txs
        taddr = self.nodes[1].getnewaddress()
        for _ in range(900):
            self.nodes[0].sendtoaddress(taddr, 0.1)
        self.nodes[0].generate(1)
        self.sync_all()

        # Create tx of lower value to be prioritized on node 0
        # Older transactions get mined first, so this lower value, newer tx is unlikely to be mined without prioritization
        priority_tx_0 = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.1)

        # Check that priority_tx_0 is not in block_template() prior to prioritisation
        block_template = self.nodes[0].getblocktemplate()
        in_block_template = False
        for tx in block_template['transactions']:
            if tx['hash'] == priority_tx_0:
                in_block_template = True
                break
        assert_equal(in_block_template, False)

        priority_result = self.nodes[0].prioritisetransaction(priority_tx_0, 1000, int(3 * base_fee * COIN))

        # Check that prioritized transaction is in getblocktemplate()
        in_block_template = False
        block_template = self.nodes[0].getblocktemplate()
        for tx in block_template['transactions']:
            if tx['hash'] == priority_tx_0:
                in_block_template = True
                break
        # NOTE: getblocktemplate() should return prioritized transaction, but is not
        # Noted by user in issue #1884
        assert_equal(in_block_template, False)

        # Node 1 doesn't get the next block, so this *shouldn't* be mined despite being prioritized on node 1
        priority_tx_1 = self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 0.1)
        self.nodes[1].prioritisetransaction(priority_tx_1, 1000, int(3 * base_fee * COIN))

        # Mine block on node 0
        blk_hash = self.nodes[0].generate(1)
        block = self.nodes[0].getblock(blk_hash[0])
        self.sync_all()

        # Check that priority_tx_0 was mined
        mempool = self.nodes[0].getrawmempool()
        assert_equal(priority_tx_0 in block['tx'], True)
        assert_equal(priority_tx_0 in mempool, False)

        # Check that priority_tx_1 was not mined
        assert_equal(priority_tx_1 in mempool, True)
        assert_equal(priority_tx_1 in block['tx'], False)

        # Mine a block on node 1 and sync
        blk_hash_1 = self.nodes[1].generate(1)
        block_1 = self.nodes[1].getblock(blk_hash_1[0])
        self.sync_all()

        # Check to see if priority_tx_1 is now mined
        mempool_1 = self.nodes[1].getrawmempool()
        assert_equal(priority_tx_1 in mempool_1, False)
        assert_equal(priority_tx_1 in block_1['tx'], True)

if __name__ == '__main__':
    PrioritiseTransactionTest().main()
