#!/usr/bin/env python2
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_nodes, connect_nodes_bi, sync_blocks, sync_mempools, \
    wait_and_assert_operationid_status

from decimal import Decimal

class WalletOverwinterTxTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        self.nodes = start_nodes(4, self.options.tmpdir, extra_args=[["-nuparams=5ba81b19:200", "-debug=zrpcunsafe", "-txindex"]] * 4 )
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        connect_nodes_bi(self.nodes,0,3)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        self.nodes[0].generate(100)
        self.sync_all()
        self.nodes[1].generate(98)
        self.sync_all()
        # Node 0 has reward from blocks 1 to 98 which are spendable.

        #
        # Sprout
        #
        # Currently at block 198. The next block to be mined 199 is a Sprout block
        #
        taddr0 = self.nodes[0].getnewaddress()
        taddr2 = self.nodes[2].getnewaddress()
        zaddr2 = self.nodes[2].z_getnewaddress()
        taddr3 = self.nodes[3].getnewaddress()
        zaddr3 = self.nodes[3].z_getnewaddress()

        # Send taddr to taddr
        tsendamount = Decimal('1.23')
        txid_transparent = self.nodes[0].sendtoaddress(taddr2, tsendamount)

        # Send one coinbase utxo of value 10.0 less a fee of 0.00010000, with no change.
        zsendamount = Decimal('10.0') - Decimal('0.0001')
        recipients = []
        recipients.append({"address":zaddr2, "amount": zsendamount})
        myopid = self.nodes[0].z_sendmany(taddr0, recipients)
        txid_shielded = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify balance
        assert_equal(self.nodes[2].getbalance(), tsendamount)
        assert_equal(self.nodes[2].z_getbalance(zaddr2), zsendamount)

        # Verify transaction version is 1 or 2 (intended for Sprout)
        result = self.nodes[0].getrawtransaction(txid_transparent, 1)
        assert_equal(result["version"], 1)
        assert_equal(result["overwintered"], False)
        result = self.nodes[0].getrawtransaction(txid_shielded, 1)
        assert_equal(result["version"], 2)
        assert_equal(result["overwintered"], False)

        #
        # Overwinter
        #
        # Currently at block 199. The next block to be mined 200 is an Overwinter block
        #

        # Send taddr to taddr
        tsendamount = Decimal('4.56')
        txid_transparent = self.nodes[0].sendtoaddress(taddr3, tsendamount)

        # Send one coinbase utxo of value 20.0 less a fee of 0.00010000, with no change.
        zsendamount = Decimal('20.0') - Decimal('0.0001')
        recipients = []
        recipients.append({"address":zaddr3, "amount": zsendamount})
        myopid = self.nodes[0].z_sendmany(taddr0, recipients)
        txid_shielded = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify balance
        assert_equal(self.nodes[3].getbalance(), tsendamount)
        assert_equal(self.nodes[3].z_getbalance(zaddr3), zsendamount)

        # Verify transaction version is 3 (intended for Overwinter)
        result = self.nodes[0].getrawtransaction(txid_transparent, 1)
        assert_equal(result["version"], 3)
        assert_equal(result["overwintered"], True)
        assert_equal(result["versiongroupid"], "03c48270")
        result = self.nodes[0].getrawtransaction(txid_shielded, 1)
        assert_equal(result["version"], 3)
        assert_equal(result["overwintered"], True)
        assert_equal(result["versiongroupid"], "03c48270")

if __name__ == '__main__':
    WalletOverwinterTxTest().main()
