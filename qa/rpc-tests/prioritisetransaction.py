#!/usr/bin/env python3
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal, assert_true, initialize_chain_clean,
    start_node, connect_nodes, wait_and_assert_operationid_status,
    get_coinbase_address
)
from test_framework.mininode import COIN

import time
from decimal import Decimal

class PrioritiseTransactionTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        self.nodes = []
        # Start nodes with tiny block size of 11kb
        self.nodes.append(start_node(0, self.options.tmpdir, ["-blockshieldedsize=7000", "-blockmaxsize=11000", "-maxorphantx=1000", "-relaypriority=true", "-printpriority=1"]))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-blockshieldedsize=7000", "-blockmaxsize=11000", "-maxorphantx=1000", "-relaypriority=true", "-printpriority=1"]))
        connect_nodes(self.nodes[1], 0)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):

        print("Mining 11kb blocks...")
        self.nodes[0].generate(501)

        base_fee = self.nodes[0].getnetworkinfo()['relayfee']

        # 11 kb blocks will only hold about 50 txs, so this will fill mempool with older txs
        taddr = self.nodes[1].getnewaddress()
        for _ in range(900):
            self.nodes[0].sendtoaddress(taddr, 0.1)
        self.nodes[0].generate(1)
        self.sync_all()

        # Create free shielded tx (should be included in block template)
        sapling_addr0 = self.nodes[0].z_getnewaddress('sapling')
        recipients = [{"address": sapling_addr0, "amount": Decimal('0.1')}]
        shielded_tx_opid = self.nodes[1].z_sendmany(taddr, recipients, 1, 0)
        shielded_tx = wait_and_assert_operationid_status(self.nodes[1], shielded_tx_opid)

        # Check that shielded_tx is in block_template()
        block_template = self.nodes[1].getblocktemplate()
        in_block_template = False
        for tx in block_template['transactions']:
            if tx['hash'] == shielded_tx:
                in_block_template = True
                break
        assert_equal(in_block_template, True)

        # Mine block on node 1
        blk_hash = self.nodes[1].generate(1)
        block = self.nodes[1].getblock(blk_hash[0])
        self.sync_all()

        # Check that shielded_tx was mined
        mempool = self.nodes[1].getrawmempool()
        assert_equal(shielded_tx in block['tx'], True)
        assert_equal(shielded_tx in mempool, False)

        # Create free tx to be prioritized on node 0
        utxo_list_0 = self.nodes[0].listunspent()
        assert(len(utxo_list_0) > 0)
        utxo_0 = utxo_list_0[0]
        inputs_0 = [{"txid": utxo_0["txid"], "vout": utxo_0["vout"]}]
        outputs_0 = {self.nodes[1].getnewaddress(): utxo_0["amount"]}
        raw_tx_0 = self.nodes[0].createrawtransaction(inputs_0, outputs_0)
        tx_hex_0 = self.nodes[0].signrawtransaction(raw_tx_0)["hex"]
        priority_tx_0 = self.nodes[0].decoderawtransaction(tx_hex_0)["txid"]
        print("priority_tx_0: ", priority_tx_0)
        assert_equal(self.nodes[0].sendrawtransaction(tx_hex_0), priority_tx_0)

        # self.nodes[0].settxfee(Decimal('0'))
        # priority_tx_0 = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.1, "", "", False)
        # print("priority_tx_0: ", priority_tx_0)

        # Check that priority_tx_0 is not in block_template() prior to prioritisation
        block_template = self.nodes[0].getblocktemplate()
        in_block_template = False
        for tx in block_template['transactions']:
            if tx['hash'] == priority_tx_0:
                in_block_template = True
                break
        assert_equal(in_block_template, False)

        priority_success = self.nodes[0].prioritisetransaction(priority_tx_0, int(100 * base_fee * COIN))
        assert(priority_success)
		
        # Check that prioritized transaction is not in getblocktemplate()
        # (not updated because no new txns)
        in_block_template = False
        block_template = self.nodes[0].getblocktemplate()
        for tx in block_template['transactions']:
            if tx['hash'] == priority_tx_0:
                in_block_template = True
                break
        assert_equal(in_block_template, False)

        # Sending a new transaction will make getblocktemplate refresh within 10s
        self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.1)

        # Check that prioritized transaction is not in getblocktemplate()
        # (too soon)
        in_block_template = False
        block_template = self.nodes[0].getblocktemplate()
        for tx in block_template['transactions']:
            if tx['hash'] == priority_tx_0:
                in_block_template = True
                break
        assert_equal(in_block_template, False)

        # Check that prioritized transaction is in getblocktemplate()
        # getblocktemplate() will refresh after 1 min, or after 10 sec if new transaction is added to mempool
        # Mempool is probed every 10 seconds. We'll give getblocktemplate() a maximum of 30 seconds to refresh
        block_template = self.nodes[0].getblocktemplate()
        start = time.time()
        in_block_template = False
        while in_block_template == False:
            for tx in block_template['transactions']:
                if tx['hash'] == priority_tx_0:
                    in_block_template = True
                    break
            if time.time() - start > 30:
                raise AssertionError("Test timed out because prioritised transaction was not returned by getblocktemplate within 30 seconds.")
            time.sleep(1)
            block_template = self.nodes[0].getblocktemplate()

        assert(in_block_template)

        # Node 1 doesn't get the next block, so this *shouldn't* be mined despite being prioritized on node 1
        utxo_list_1 = self.nodes[1].listunspent()
        assert(len(utxo_list_1) > 0)
        utxo_1 = utxo_list_1[0]
        inputs_1 = [{"txid": utxo_1["txid"], "vout": utxo_1["vout"]}]
        outputs_1 = {self.nodes[0].getnewaddress(): utxo_1["amount"]}
        raw_tx_1 = self.nodes[1].createrawtransaction(inputs_1, outputs_1)
        tx_hex_1 = self.nodes[1].signrawtransaction(raw_tx_1)["hex"]
        priority_tx_1 = self.nodes[1].decoderawtransaction(tx_hex_1)["txid"]
        print("priority_tx_1: ", priority_tx_1)
        assert_equal(self.nodes[1].sendrawtransaction(tx_hex_1), priority_tx_1)

        priority_success = self.nodes[1].prioritisetransaction(priority_tx_1, int(100 * base_fee * COIN))
        assert(priority_success)

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
