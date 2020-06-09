#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import (
    assert_equal,
    get_coinbase_address,
    start_nodes,
    wait_and_assert_operationid_status,
    start_node,
    stop_node,
    initialize_chain_clean,
    connect_nodes_bi
)

from decimal import Decimal

class WalletRemoveAddressTest(BitcoinTestFramework):

    def setup_chain(self):
        initialize_chain_clean(self.options.tmpdir, 2)

    def setup_network(self):
        self.nodes = start_nodes(2, self.options.tmpdir)
        connect_nodes_bi(self.nodes, 0, 1)
        self.is_network_split = False
        self.sync_all()

    def fundzaddress(self, address, amount):
        recipients = []
        recipients.append({"address": address, "amount": amount})
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], myopid)
        
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].z_getbalance(address), amount)

    def send(self, from_, to, amount):
        recipients = []
        recipients.append({"address": to, "amount": amount})
        myopid = self.nodes[0].z_sendmany(from_, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

    def getbalance(self, address):
        try:
            return self.nodes[0].z_getbalance(address)
        except JSONRPCException as e:
            assert_equal("From address does not belong to this node, spending key or viewing key not found.", e.error['message'])
            return "e"

    def run_test(self):
        self.nodes[0].generate(200)
        self.sync_all()

        transparent = self.nodes[0].getnewaddress()
        sapling = self.nodes[0].z_getnewaddress('sapling')
        sprout = self.nodes[0].z_getnewaddress('sprout')

        self.fundzaddress(sapling, Decimal('10'))
        assert_equal(self.getbalance(sapling), 10)

        # transparent
        assert_equal(self.getbalance(transparent), 0)
        # remove transparent key
        self.nodes[0].z_removeaddress(transparent, False)
        # send to transparent
        self.send(sapling, transparent, Decimal('1'))
        # as we dont have the key anymore balance will not be reflected
        assert_equal(self.getbalance(transparent), 0)
        # however the coin left sapling
        assert_equal(self.getbalance(sapling), 9)

        # sprout
        assert_equal(self.getbalance(sprout), 0)
        # remove sprout key
        self.nodes[0].z_removeaddress(sprout, False)
        # exception
        assert_equal(self.getbalance(sprout), "e")

        # sapling
        assert_equal(self.getbalance(sapling), 9)
        # remove sapling
        self.nodes[0].z_removeaddress(sapling, False)
        # exception
        assert_equal(self.getbalance(sapling), "e")

        # lets restart the node
        stop_node(self.nodes[0],0)
        self.nodes[0] = start_node(0, self.options.tmpdir, [], timewait=60)

        # no exception should happen
        assert_equal(self.getbalance(transparent), 1)
        assert_equal(self.getbalance(sprout), 0)
        assert_equal(self.getbalance(sapling), 9)

        # now remove everything from ram and from disk
        self.nodes[0].z_removeaddress(transparent, True)
        self.nodes[0].z_removeaddress(sprout, True)
        self.nodes[0].z_removeaddress(sapling, True)

        # restart node
        stop_node(self.nodes[0],0)
        self.nodes[0] = start_node(0, self.options.tmpdir, [], timewait=60)

        assert_equal(self.nodes[0].z_getbalance(transparent), 0)
        assert_equal(self.getbalance(sprout), "e")
        assert_equal(self.getbalance(sapling), "e")

if __name__ == '__main__':
    WalletRemoveAddressTest().main()
