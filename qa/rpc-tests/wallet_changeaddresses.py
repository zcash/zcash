#!/usr/bin/env python
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    connect_nodes_bi,
    get_coinbase_address,
    initialize_chain_clean,
    start_node,
    wait_and_assert_operationid_status,
)

from decimal import Decimal

# Test wallet change address behaviour
class WalletChangeAddressesTest(BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 2)

    def setup_network(self):
        args = [
            '-nuparams=5ba81b19:1', # Overwinter
            '-nuparams=76b809bb:1', # Sapling
            '-txindex',             # Avoid JSONRPC error: No information available about transaction
            '-experimentalfeatures', '-zmergetoaddress',
        ]
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, args))
        self.nodes.append(start_node(1, self.options.tmpdir, args))
        connect_nodes_bi(self.nodes,0,1)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        self.nodes[0].generate(110)

        # Obtain some transparent funds
        midAddr = self.nodes[0].z_getnewaddress('sapling')
        myopid = self.nodes[0].z_shieldcoinbase(get_coinbase_address(self.nodes[0]), midAddr, 0)['opid']
        wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.nodes[1].generate(1)
        self.sync_all()
        taddrSource = self.nodes[0].getnewaddress()
        for _ in range(6):
            recipients = [{"address": taddrSource, "amount": Decimal('2')}]
            myopid = self.nodes[0].z_sendmany(midAddr, recipients, 1, 0)
            wait_and_assert_operationid_status(self.nodes[0], myopid)
            self.nodes[1].generate(1)
            self.sync_all()

        def check_change_taddr_reuse(target):
            recipients = [{"address": target, "amount": Decimal('1')}]

            # Send funds to recipient address twice
            myopid = self.nodes[0].z_sendmany(taddrSource, recipients, 1, 0)
            txid1 = wait_and_assert_operationid_status(self.nodes[0], myopid)
            self.nodes[1].generate(1)
            self.sync_all()
            myopid = self.nodes[0].z_sendmany(taddrSource, recipients, 1, 0)
            txid2 = wait_and_assert_operationid_status(self.nodes[0], myopid)
            self.nodes[1].generate(1)
            self.sync_all()

            # Verify that the two transactions used different change addresses
            tx1 = self.nodes[0].getrawtransaction(txid1, 1)
            tx2 = self.nodes[0].getrawtransaction(txid2, 1)
            for i in range(len(tx1['vout'])):
                tx1OutAddrs = tx1['vout'][i]['scriptPubKey']['addresses']
                tx2OutAddrs = tx2['vout'][i]['scriptPubKey']['addresses']
                if tx1OutAddrs != [target]:
                    print('Source address:     %s' % taddrSource)
                    print('TX1 change address: %s' % tx1OutAddrs[0])
                    print('TX2 change address: %s' % tx2OutAddrs[0])
                    assert(tx1OutAddrs != tx2OutAddrs)

        taddr = self.nodes[0].getnewaddress()
        saplingAddr = self.nodes[0].z_getnewaddress('sapling')
        sproutAddr = self.nodes[0].z_getnewaddress('sprout')

        print
        print('Checking z_sendmany(taddr->Sapling)')
        check_change_taddr_reuse(saplingAddr)
        print
        print('Checking z_sendmany(taddr->Sprout)')
        check_change_taddr_reuse(sproutAddr)
        print
        print('Checking z_sendmany(taddr->taddr)')
        check_change_taddr_reuse(taddr)

if __name__ == '__main__':
    WalletChangeAddressesTest().main()
