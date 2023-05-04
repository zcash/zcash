#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
    get_coinbase_address,
    start_nodes,
    start_node,
    stop_node,
    wait_and_assert_operationid_status,
)
from test_framework.mininode import COIN

import os
from decimal import Decimal

class WalletRestoreTest (BitcoinTestFramework):
    def setup_network(self, split=False):
        self.num_nodes = 2
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            "-exportdir={}/export{}".format(self.options.tmpdir, i)
        ] for i in range(self.num_nodes)])

        connect_nodes_bi(self.nodes, 0, 1)

        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        # Create account 0 on node 0
        acct0 = self.nodes[0].z_getnewaccount()['account']
        ua0 = self.nodes[0].z_getaddressforaccount(acct0)['address']

        # Send funds from node 1 to ua0 on node 0
        recipients = []
        recipients.append({"address": ua0, "amount": Decimal('10')})
        myopid = self.nodes[1].z_sendmany(get_coinbase_address(self.nodes[1]), recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[1], myopid)

        self.nodes[1].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].z_getbalanceforaccount(acct0, 0)['pools']['sapling']['valueZat'], 10 * COIN)

        dump_path0 = self.nodes[0].z_exportwallet('walletdump')
        stop_node(self.nodes[0], 0)
        os.remove(self.options.tmpdir + "/node0/regtest/wallet.dat")

        self.nodes[0] = start_node(0, self.options.tmpdir, ["-recoverwallet=%s" % dump_path0])
        connect_nodes_bi(self.nodes,0,1)

        # Check that all notes from previous wallet were successfully decrypted.
        assert_equal(self.nodes[0].z_getbalanceforaccount(acct0, 0)['pools']['sapling']['valueZat'], 10 * COIN)

if __name__ == '__main__':
    WalletRestoreTest().main()
