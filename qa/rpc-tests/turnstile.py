#!/usr/bin/env python
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Test Sprout and Sapling turnstile violations
#
# Experimental feature -developersetpoolsizezero will, upon node launch,
# set the in-memory size of shielded pools to zero.
#
# An unshielding operation can then be used to verify:
# 1. Turnstile violating transactions are excluded by the miner
# 2. Turnstile violating blocks are rejected by nodes
#
# By default, ZIP209 support is disabled in regtest mode, but gets enabled
# when experimental feature -developersetpoolsizezero is switched on.
#
# To perform a manual turnstile test on testnet:
# 1. Launch zcashd
# 2. Shield transparent funds
# 3. Wait for transaction to be mined
# 4. Restart zcashd, enabling experimental feature -developersetpoolsizezero
# 5. Unshield funds
# 6. Wait for transaction to be mined (using testnet explorer or another node)
# 7. Verify zcashd rejected the block
#

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    get_coinbase_address,
    start_node, start_nodes,
    sync_blocks, sync_mempools,
    initialize_chain_clean, connect_nodes_bi,
    wait_and_assert_operationid_status,
    bitcoind_processes
)
from decimal import Decimal

TURNSTILE_ARGS = ['-experimentalfeatures',
                  '-developersetpoolsizezero']

class TurnstileTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        self.is_network_split=False
        self.sync_all()

    # Helper method to verify the size of a shielded value pool for a given node
    def assert_pool_balance(self, node, name, balance):
        pools = node.getblockchaininfo()['valuePools']
        for pool in pools:
            if pool['id'] == name:
                assert_equal(pool['chainValue'], balance, message="for pool named %r" % (name,))
                return
        assert False, "pool named %r not found" % (name,)

    # Helper method to start a single node with extra args and sync to the network
    def start_and_sync_node(self, index, args=[]):
        self.nodes[index] = start_node(index, self.options.tmpdir, extra_args=args)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.sync_all()

    # Helper method to stop and restart a single node with extra args and sync to the network
    def restart_and_sync_node(self, index, args=[]):
        self.nodes[index].stop()
        bitcoind_processes[index].wait()
        self.start_and_sync_node(index, args)

    def run_test(self):
        # Sanity-check the test harness
        self.nodes[0].generate(101)
        assert_equal(self.nodes[0].getblockcount(), 101)
        self.sync_all()

        # Node 0 shields some funds
        dest_addr = self.nodes[0].z_getnewaddress(POOL_NAME.lower())
        taddr0 = get_coinbase_address(self.nodes[0])
        recipients = []
        recipients.append({"address": dest_addr, "amount": Decimal('10')})
        myopid = self.nodes[0].z_sendmany(taddr0, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].z_getbalance(dest_addr), Decimal('10'))

        # Verify size of shielded pool
        self.assert_pool_balance(self.nodes[0], POOL_NAME.lower(), Decimal('10'))
        self.assert_pool_balance(self.nodes[1], POOL_NAME.lower(), Decimal('10'))
        self.assert_pool_balance(self.nodes[2], POOL_NAME.lower(), Decimal('10'))

        # Relaunch node 0 with in-memory size of value pools set to zero.
        self.restart_and_sync_node(0, TURNSTILE_ARGS)

        # Verify size of shielded pool
        self.assert_pool_balance(self.nodes[0], POOL_NAME.lower(), Decimal('0'))
        self.assert_pool_balance(self.nodes[1], POOL_NAME.lower(), Decimal('10'))
        self.assert_pool_balance(self.nodes[2], POOL_NAME.lower(), Decimal('10'))

        # Node 0 creates an unshielding transaction
        recipients = []
        recipients.append({"address": taddr0, "amount": Decimal('1')})
        myopid = self.nodes[0].z_sendmany(dest_addr, recipients, 1, 0)
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)

        # Verify transaction appears in mempool of nodes
        self.sync_all()
        assert(mytxid in self.nodes[0].getrawmempool())
        assert(mytxid in self.nodes[1].getrawmempool())
        assert(mytxid in self.nodes[2].getrawmempool())

        # Node 0 mines a block
        count = self.nodes[0].getblockcount()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify the mined block does not contain the unshielding transaction
        block = self.nodes[0].getblock(self.nodes[0].getbestblockhash())
        assert_equal(len(block["tx"]), 1)
        assert_equal(block["height"], count + 1)

        # Stop node 0 and check logs to verify the miner excluded the transaction from the block
        self.nodes[0].stop()
        bitcoind_processes[0].wait()
        logpath = self.options.tmpdir + "/node0/regtest/debug.log"
        foundErrorMsg = False
        with open(logpath, "r") as myfile:
            logdata = myfile.readlines()
        for logline in logdata:
            if "CreateNewBlock(): tx " + mytxid + " appears to violate " + POOL_NAME.capitalize() + " turnstile" in logline:
                foundErrorMsg = True
                break
        assert(foundErrorMsg)

        # Launch node 0 with in-memory size of value pools set to zero.
        self.start_and_sync_node(0, TURNSTILE_ARGS)

        # Node 1 mines a block
        oldhash = self.nodes[0].getbestblockhash()
        self.nodes[1].generate(1)
        newhash = self.nodes[1].getbestblockhash()

        # Verify block contains the unshielding transaction 
        assert(mytxid in self.nodes[1].getblock(newhash)["tx"])

        # Verify nodes 1 and 2 have accepted the block as valid
        sync_blocks(self.nodes[1:3])
        sync_mempools(self.nodes[1:3])
        assert_equal(len(self.nodes[1].getrawmempool()), 0)
        assert_equal(len(self.nodes[2].getrawmempool()), 0)

        # Verify node 0 has not accepted the block
        assert_equal(oldhash, self.nodes[0].getbestblockhash())
        assert(mytxid in self.nodes[0].getrawmempool())
        self.assert_pool_balance(self.nodes[0], POOL_NAME.lower(), Decimal('0'))

        # Verify size of shielded pool
        self.assert_pool_balance(self.nodes[0], POOL_NAME.lower(), Decimal('0'))
        self.assert_pool_balance(self.nodes[1], POOL_NAME.lower(), Decimal('9'))
        self.assert_pool_balance(self.nodes[2], POOL_NAME.lower(), Decimal('9'))

        # Stop node 0 and check logs to verify the block was rejected as a turnstile violation
        self.nodes[0].stop()
        bitcoind_processes[0].wait()
        logpath = self.options.tmpdir + "/node0/regtest/debug.log"
        foundConnectBlockErrorMsg = False
        foundInvalidBlockErrorMsg = False
        foundConnectTipErrorMsg = False
        with open(logpath, "r") as myfile:
            logdata = myfile.readlines()
        for logline in logdata:
            if "ConnectBlock(): turnstile violation in " + POOL_NAME.capitalize() + " shielded value pool" in logline:
                foundConnectBlockErrorMsg = True
            elif "InvalidChainFound: invalid block=" + newhash in logline:
                foundInvalidBlockErrorMsg = True
            elif "ConnectTip(): ConnectBlock " + newhash + " failed" in logline:
                foundConnectTipErrorMsg = True
        assert(foundConnectBlockErrorMsg and foundInvalidBlockErrorMsg and foundConnectTipErrorMsg)

        # Launch node 0 without overriding the pool size, so the node can sync with rest of network.
        self.start_and_sync_node(0)
        assert_equal(newhash, self.nodes[0].getbestblockhash())

if __name__ == '__main__':
    POOL_NAME = "SPROUT"
    TurnstileTest().main()
    POOL_NAME = "SAPLING"
    TurnstileTest().main()
