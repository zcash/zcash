#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.mininode import COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal, assert_true,
    get_coinbase_address,
    start_nodes, stop_nodes,
    initialize_chain_clean, connect_nodes_bi, wait_bitcoinds,
    wait_and_assert_operationid_status
)
from decimal import Decimal

class WalletPersistenceTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        self.nodes = start_nodes(4, self.options.tmpdir, extra_args=[[
            '-allowdeprecated=z_getnewaddress',
            '-allowdeprecated=z_getbalance',
            '-allowdeprecated=z_gettotalbalance',
            '-allowdeprecated=z_listaddresses',
        ]] * 4)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,2,3)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        # Slow start is not enabled for regtest, so the expected subsidy starts at the
        # maximum, but the hardcoded genesis block for regtest does not consume the
        # available subsidy.
        pre_halving_blocks = 143
        pre_halving_subsidy = Decimal('12.5')
        post_halving_blocks = 57
        post_halving_subsidy = pre_halving_subsidy / 2
        expected_supply = (pre_halving_blocks * pre_halving_subsidy +
                           post_halving_blocks * post_halving_subsidy)

        blocks_to_mine = pre_halving_blocks + post_halving_blocks

        # Sanity-check the test harness
        # Note that the genesis block is not counted in the result of `getblockcount`
        self.nodes[0].generate(blocks_to_mine)
        assert_equal(self.nodes[0].getblockcount(), blocks_to_mine)
        self.sync_all()

        # Verify Sapling address is persisted in wallet
        sapling_addr = self.nodes[0].z_getnewaddress('sapling')

        # Make sure the node has the address
        addresses = self.nodes[0].z_listaddresses()
        assert_true(sapling_addr in addresses, "Should contain address before restart")

        def check_chain_value(pool, expected_id, expected_value):
            assert_equal(pool.get('id', None), expected_id)
            assert_equal(pool['monitored'], True)
            assert_equal(pool['chainValue'], expected_value)
            assert_equal(pool['chainValueZat'], expected_value * COIN)

        # Verify size of pools
        chainInfo = self.nodes[0].getblockchaininfo()
        pools = chainInfo['valuePools']
        check_chain_value(chainInfo['chainSupply'], None, expected_supply)
        check_chain_value(pools[0], 'transparent', expected_supply)
        check_chain_value(pools[1], 'sprout',  Decimal('0'))
        check_chain_value(pools[2], 'sapling', Decimal('0'))
        check_chain_value(pools[3], 'orchard', Decimal('0'))

        # Restart the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # Make sure we still have the address after restarting
        addresses = self.nodes[0].z_listaddresses()
        assert_true(sapling_addr in addresses, "Should contain address after restart")

        # Verify size of pools after restarting
        chainInfo = self.nodes[0].getblockchaininfo()
        pools = chainInfo['valuePools']
        check_chain_value(chainInfo['chainSupply'], None, expected_supply)  # Supply
        check_chain_value(pools[0], 'transparent', expected_supply)
        check_chain_value(pools[1], 'sprout',  Decimal('0'))
        check_chain_value(pools[2], 'sapling', Decimal('0'))
        check_chain_value(pools[3], 'orchard', Decimal('0'))

        # Node 0 shields funds to Sapling address
        taddr0 = get_coinbase_address(self.nodes[0])
        recipients = []
        recipients.append({"address": sapling_addr, "amount": Decimal('20')})
        myopid = self.nodes[0].z_sendmany(taddr0, recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        expected_supply += post_halving_subsidy
        self.sync_all()

        # Verify shielded balance
        assert_equal(self.nodes[0].z_getbalance(sapling_addr), Decimal('20'))

        # Verify size of pools
        chainInfo = self.nodes[0].getblockchaininfo()
        pools = chainInfo['valuePools']
        check_chain_value(chainInfo['chainSupply'], None, expected_supply)  # Supply
        check_chain_value(pools[0], 'transparent', expected_supply - Decimal('20'))  # Transparent
        check_chain_value(pools[1], 'sprout',  Decimal('0'))  
        check_chain_value(pools[2], 'sapling', Decimal('20'))
        check_chain_value(pools[3], 'orchard', Decimal('0'))

        # Restart the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # Verify size of pools
        chainInfo = self.nodes[0].getblockchaininfo()
        pools = chainInfo['valuePools']
        check_chain_value(chainInfo['chainSupply'], None, expected_supply)  # Supply
        check_chain_value(pools[0], 'transparent', expected_supply - Decimal('20'))  # Transparent
        check_chain_value(pools[1], 'sprout',  Decimal('0')) 
        check_chain_value(pools[2], 'sapling', Decimal('20'))
        check_chain_value(pools[3], 'orchard', Decimal('0'))

        # Node 0 sends some shielded funds to Node 1
        dest_addr = self.nodes[1].z_getnewaddress('sapling')
        recipients = []
        recipients.append({"address": dest_addr, "amount": Decimal('15')})
        myopid = self.nodes[0].z_sendmany(sapling_addr, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify balances
        assert_equal(self.nodes[0].z_getbalance(sapling_addr), Decimal('5'))
        assert_equal(self.nodes[1].z_getbalance(dest_addr), Decimal('15'))

        # Restart the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # Verify balances
        assert_equal(self.nodes[0].z_getbalance(sapling_addr), Decimal('5'))
        assert_equal(self.nodes[1].z_getbalance(dest_addr), Decimal('15'))

        # Verify importing a spending key will update and persist the nullifiers and witnesses correctly
        sk0 = self.nodes[0].z_exportkey(sapling_addr)
        self.nodes[2].z_importkey(sk0, "yes")
        assert_equal(self.nodes[2].z_getbalance(sapling_addr), Decimal('5'))

        # Verify importing a viewing key will update and persist the nullifiers and witnesses correctly
        extfvk0 = self.nodes[0].z_exportviewingkey(sapling_addr)
        self.nodes[3].z_importviewingkey(extfvk0, "yes")
        assert_equal(self.nodes[3].z_getbalance(sapling_addr), Decimal('5'))
        assert_equal(self.nodes[3].z_gettotalbalance()['private'], '0.00')
        assert_equal(self.nodes[3].z_gettotalbalance(1, True)['private'], '5.00')

        # Restart the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # Verify nullifiers persisted correctly by checking balance
        # Prior to PR #3590, there will be an error as spent notes are considered unspent:
        #    Assertion failed: expected: <25.00000000> but was: <5>
        assert_equal(self.nodes[2].z_getbalance(sapling_addr), Decimal('5'))
        assert_equal(self.nodes[3].z_getbalance(sapling_addr), Decimal('5'))
        assert_equal(self.nodes[3].z_gettotalbalance()['private'], '0.00')
        assert_equal(self.nodes[3].z_gettotalbalance(1, True)['private'], '5.00')

        # Verity witnesses persisted correctly by sending shielded funds
        recipients = []
        recipients.append({"address": dest_addr, "amount": Decimal('1')})
        myopid = self.nodes[2].z_sendmany(sapling_addr, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[2], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify balances
        assert_equal(self.nodes[2].z_getbalance(sapling_addr), Decimal('4'))
        assert_equal(self.nodes[1].z_getbalance(dest_addr), Decimal('16'))

if __name__ == '__main__':
    WalletPersistenceTest().main()
