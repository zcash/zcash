#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    connect_nodes_bi,
    get_coinbase_address,
    initialize_chain_clean,
    start_nodes,
    wait_and_assert_operationid_status,
)
from test_framework.authproxy import JSONRPCException

from decimal import Decimal

class WalletOverwinterTxTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        self.nodes = start_nodes(4, self.options.tmpdir, extra_args=[[
            "-nuparams=2bb40e60:200",
            "-debug=zrpcunsafe",
            "-txindex",
        ]] * 4 )
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        connect_nodes_bi(self.nodes,0,3)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        self.nodes[0].generate(100)
        self.sync_all()
        self.nodes[1].generate(95)
        self.sync_all()
        # Node 0 has reward from blocks 1 to 95 which are spendable.

        taddr0 = get_coinbase_address(self.nodes[0])
        taddr1 = self.nodes[1].getnewaddress()
        taddr2 = self.nodes[2].getnewaddress()
        zaddr2 = self.nodes[2].z_getnewaddress('sprout')
        taddr3 = self.nodes[3].getnewaddress()
        zaddr3 = self.nodes[3].z_getnewaddress('sprout')

        #
        # Currently at block 195. The next block to be mined 196 is a Sapling block
        #
        bci = self.nodes[0].getblockchaininfo()
        assert_equal(bci['consensus']['chaintip'], '76b809bb')
        assert_equal(bci['consensus']['nextblock'], '76b809bb')
        assert_equal(bci['upgrades']['2bb40e60']['status'], 'pending')

        # Node 0 sends transparent funds to Node 2
        tsendamount = Decimal('1.0')
        txid_transparent = self.nodes[0].sendtoaddress(taddr2, tsendamount)
        self.sync_all()

        # Node 2 sends the zero-confirmation transparent funds to Node 1 using z_sendmany
        recipients = []
        recipients.append({"address":taddr1, "amount": Decimal('0.5')})
        myopid = self.nodes[2].z_sendmany(taddr2, recipients, 0)
        txid_zsendmany = wait_and_assert_operationid_status(self.nodes[2], myopid)

        # Node 0 shields to Node 2, a coinbase utxo of value 10.0 less fee 0.00010000
        zsendamount = Decimal('10.0') - Decimal('0.0001')
        recipients = []
        recipients.append({"address":zaddr2, "amount": zsendamount})
        myopid = self.nodes[0].z_sendmany(taddr0, recipients)
        txid_shielded = wait_and_assert_operationid_status(self.nodes[0], myopid)

        # Skip over the three blocks prior to activation; no transactions can be mined
        # in them due to the nearly-expiring restrictions.
        self.sync_all()
        self.nodes[0].generate(4)
        self.sync_all()

        # Verify balance
        assert_equal(self.nodes[1].z_getbalance(taddr1), Decimal('0.5'))
        assert_equal(self.nodes[2].getbalance(), Decimal('0.4999'))
        assert_equal(self.nodes[2].z_getbalance(zaddr2), zsendamount)

        # Verify transaction version is 4 (intended for Sapling+)
        result = self.nodes[0].getrawtransaction(txid_transparent, 1)
        assert_equal(result["version"], 4)
        assert_equal(result["overwintered"], True)
        result = self.nodes[0].getrawtransaction(txid_zsendmany, 1)
        assert_equal(result["version"], 4)
        assert_equal(result["overwintered"], True)
        result = self.nodes[0].getrawtransaction(txid_shielded, 1)
        assert_equal(result["version"], 4)
        assert_equal(result["overwintered"], True)

        #
        # Currently at block 199. The next block to be mined 200 is a Blossom block
        #
        bci = self.nodes[0].getblockchaininfo()
        assert_equal(bci['consensus']['chaintip'], '76b809bb')
        assert_equal(bci['consensus']['nextblock'], '2bb40e60')
        assert_equal(bci['upgrades']['2bb40e60']['status'], 'pending')

        # Test using expiryheight parameter of createrawtransaction when Blossom is active in the next block
        errorString = ""
        try:
            self.nodes[0].createrawtransaction([], {}, 0, 499999999)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("", errorString)
        try:
            self.nodes[0].createrawtransaction([], {}, 0, -1)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Invalid parameter, expiryheight must be nonnegative and less than 500000000" in errorString, True)
        try:
            self.nodes[0].createrawtransaction([], {}, 0, 500000000)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Invalid parameter, expiryheight must be nonnegative and less than 500000000" in errorString, True)
        try:
            self.nodes[0].createrawtransaction([], {}, 0, 200)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Invalid parameter, expiryheight should be at least 203 to avoid transaction expiring soon" in errorString, True)

        # Node 0 sends transparent funds to Node 3
        tsendamount = Decimal('1.0')
        txid_transparent = self.nodes[0].sendtoaddress(taddr3, tsendamount)
        self.sync_all()

        # Node 3 sends the zero-confirmation transparent funds to Node 1 using z_sendmany
        recipients = []
        recipients.append({"address":taddr1, "amount": Decimal('0.5')})
        myopid = self.nodes[3].z_sendmany(taddr3, recipients, 0)
        txid_zsendmany = wait_and_assert_operationid_status(self.nodes[3], myopid)

        # Node 0 shields to Node 3, a coinbase utxo of value 10.0 less fee 0.00010000
        zsendamount = Decimal('10.0') - Decimal('0.0001')
        recipients = []
        recipients.append({"address":zaddr3, "amount": zsendamount})
        myopid = self.nodes[0].z_sendmany(taddr0, recipients)
        txid_shielded = wait_and_assert_operationid_status(self.nodes[0], myopid)

        # Mine the first Blossom block
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        bci = self.nodes[0].getblockchaininfo()

        # size_on_disk should be > 0
        assert_greater_than(bci['size_on_disk'], 0)

        assert_equal(bci['consensus']['chaintip'], '2bb40e60')
        assert_equal(bci['consensus']['nextblock'], '2bb40e60')
        assert_equal(bci['upgrades']['2bb40e60']['status'], 'active')

        # Verify balance
        assert_equal(self.nodes[1].z_getbalance(taddr1), Decimal('1.0'))
        assert_equal(self.nodes[3].getbalance(), Decimal('0.4999'))
        assert_equal(self.nodes[3].z_getbalance(zaddr3), zsendamount)

        # Verify transaction version is 4 (intended for Sapling+)
        result = self.nodes[0].getrawtransaction(txid_transparent, 1)
        assert_equal(result["version"], 4)
        assert_equal(result["overwintered"], True)
        assert_equal(result["versiongroupid"], "892f2085")
        result = self.nodes[0].getrawtransaction(txid_zsendmany, 1)
        assert_equal(result["version"], 4)
        assert_equal(result["overwintered"], True)
        assert_equal(result["versiongroupid"], "892f2085")
        result = self.nodes[0].getrawtransaction(txid_shielded, 1)
        assert_equal(result["version"], 4)
        assert_equal(result["overwintered"], True)
        assert_equal(result["versiongroupid"], "892f2085")

if __name__ == '__main__':
    WalletOverwinterTxTest().main()
