#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, connect_nodes_bi, \
    DEFAULT_FEE, start_nodes, wait_and_assert_operationid_status
from test_framework.authproxy import JSONRPCException
from test_framework.mininode import COIN
from decimal import Decimal

# Test wallet address behaviour across network upgrades
class WalletZSendmanyTest(BitcoinTestFramework):
    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir, [[
            '-experimentalfeatures',
            '-orchardwallet',
        ]] * self.num_nodes)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    # Check we only have balances in the expected pools.
    # Remember that empty pools are omitted from the output.
    def _check_balance_for_rpc(self, rpcmethod, node, account, expected, minconf):
        rpc = getattr(self.nodes[node], rpcmethod)
        actual = rpc(account) if minconf is None else rpc(account, minconf)
        assert_equal(set(expected), set(actual['pools']))
        for pool in expected:
            assert_equal(expected[pool] * COIN, actual['pools'][pool]['valueZat'])
        assert_equal(actual['minimum_confirmations'], 1 if minconf is None else minconf)

    def check_balance(self, node, account, address, expected, minconf=None):
        self._check_balance_for_rpc('z_getbalanceforaccount', node, account, expected, minconf)
        fvk = self.nodes[node].z_exportviewingkey(address)
        self._check_balance_for_rpc('z_getbalanceforviewingkey', node, fvk, expected, minconf)

    def run_test(self):
        # z_sendmany is expected to fail if tx size breaks limit
        myzaddr = self.nodes[0].z_getnewaddress()

        recipients = []
        num_t_recipients = 1000
        num_z_recipients = 2100
        amount_per_recipient = Decimal('0.00000001')
        errorString = ''
        for i in range(0,num_t_recipients):
            newtaddr = self.nodes[2].getnewaddress()
            recipients.append({"address":newtaddr, "amount":amount_per_recipient})
        for i in range(0,num_z_recipients):
            newzaddr = self.nodes[2].z_getnewaddress()
            recipients.append({"address":newzaddr, "amount":amount_per_recipient})

        # Issue #2759 Workaround START
        # HTTP connection to node 0 may fall into a state, during the few minutes it takes to process
        # loop above to create new addresses, that when z_sendmany is called with a large amount of
        # rpc data in recipients, the connection fails with a 'broken pipe' error.  Making a RPC call
        # to node 0 before calling z_sendmany appears to fix this issue, perhaps putting the HTTP
        # connection into a good state to handle a large amount of data in recipients.
        self.nodes[0].getinfo()
        # Issue #2759 Workaround END

        try:
            self.nodes[0].z_sendmany(myzaddr, recipients)
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("size of raw transaction would be larger than limit" in errorString)

        # add zaddr to node 2
        myzaddr = self.nodes[2].z_getnewaddress()

        # add taddr to node 2
        mytaddr = self.nodes[2].getnewaddress()

        # send from node 0 to node 2 taddr
        mytxid = self.nodes[0].sendtoaddress(mytaddr, 10.0)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # send node 2 taddr to zaddr
        recipients = []
        recipients.append({"address":myzaddr, "amount":7})
        opid = self.nodes[2].z_sendmany(mytaddr, recipients)
        mytxid = wait_and_assert_operationid_status(self.nodes[2], opid)

        self.sync_all()

        # check balances
        zsendmanynotevalue = Decimal('7.0')
        zsendmanyfee = DEFAULT_FEE
        node2utxobalance = Decimal('260.00000000') - zsendmanynotevalue - zsendmanyfee

        # check shielded balance status with getwalletinfo
        wallet_info = self.nodes[2].getwalletinfo()
        assert_equal(Decimal(wallet_info["shielded_unconfirmed_balance"]), zsendmanynotevalue)
        assert_equal(Decimal(wallet_info["shielded_balance"]), Decimal('0.0'))

        self.nodes[2].generate(1)
        self.sync_all()

        assert_equal(self.nodes[2].getbalance(), node2utxobalance)
        assert_equal(self.nodes[2].getbalance("*"), node2utxobalance)

        # check zaddr balance with z_getbalance
        zbalance = zsendmanynotevalue
        assert_equal(self.nodes[2].z_getbalance(myzaddr), zbalance)

        # check via z_gettotalbalance
        resp = self.nodes[2].z_gettotalbalance()
        assert_equal(Decimal(resp["transparent"]), node2utxobalance)
        assert_equal(Decimal(resp["private"]), zbalance)
        assert_equal(Decimal(resp["total"]), node2utxobalance + zbalance)

        # check confirmed shielded balance with getwalletinfo
        wallet_info = self.nodes[2].getwalletinfo()
        assert_equal(Decimal(wallet_info["shielded_unconfirmed_balance"]), Decimal('0.0'))
        assert_equal(Decimal(wallet_info["shielded_balance"]), zsendmanynotevalue)

        # there should be at least one Sapling output
        mytxdetails = self.nodes[2].getrawtransaction(mytxid, 1)
        assert_greater_than(len(mytxdetails["vShieldedOutput"]), 0)
        # the Sapling output should take in all the public value
        assert_equal(mytxdetails["valueBalance"], -zsendmanynotevalue)

        # send from private note to node 0 and node 2
        node0balance = self.nodes[0].getbalance()
        # The following assertion fails nondeterministically
        # assert_equal(node0balance, Decimal('25.99798873'))
        node2balance = self.nodes[2].getbalance()
        # The following assertion might fail nondeterministically
        # assert_equal(node2balance, Decimal('16.99799000'))

        recipients = []
        recipients.append({"address":self.nodes[0].getnewaddress(), "amount":1})
        recipients.append({"address":self.nodes[2].getnewaddress(), "amount":1.0})

        opid = self.nodes[2].z_sendmany(myzaddr, recipients)
        wait_and_assert_operationid_status(self.nodes[2], opid)
        zbalance -= Decimal('2.0') + zsendmanyfee

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()

        node0balance += Decimal('11.0')
        node2balance += Decimal('1.0')
        assert_equal(Decimal(self.nodes[0].getbalance()), node0balance)
        assert_equal(Decimal(self.nodes[0].getbalance("*")), node0balance)
        assert_equal(Decimal(self.nodes[2].getbalance()), node2balance)
        assert_equal(Decimal(self.nodes[2].getbalance("*")), node2balance)

        # Get a new unified account on node 2 & generate a UA
        n0account0 = self.nodes[0].z_getnewaccount()['account']
        n0ua0 = self.nodes[0].z_getaddressforaccount(n0account0)['unifiedaddress']

        # Change went to a fresh address, so use `ANY_TADDR` which
        # should hold the rest of our transparent funds.
        recipients = []
        recipients.append({"address":n0ua0, "amount":10})
        opid = self.nodes[2].z_sendmany('ANY_TADDR', recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[2], opid)

        self.nodes[2].generate(1)
        self.sync_all()

        node2balance -= Decimal('10.0')
        node0balance += Decimal('10.0')
        assert_equal(Decimal(self.nodes[2].getbalance()), node2balance)
        assert_equal(Decimal(self.nodes[0].getbalance()), node0balance)
        self.check_balance(0, 0, n0ua0, {'sapling': 10})

        # Send some funds to a specific legacy taddr that we can spend from
        recipients = []
        recipients.append({"address":mytaddr, "amount":5})
        opid = self.nodes[0].z_sendmany(n0ua0, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.nodes[0].generate(1)
        self.sync_all()

        node2balance += Decimal('5.0')
        self.check_balance(0, 0, n0ua0, {'sapling': 5})
        assert_equal(Decimal(self.nodes[2].getbalance()), node2balance)

        # Send some funds to a legacy sapling address that we can spend from
        recipients = []
        recipients.append({"address":myzaddr, "amount":3})
        opid = self.nodes[0].z_sendmany(n0ua0, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.nodes[0].generate(1)
        self.sync_all()

        zbalance += Decimal('3.0')
        self.check_balance(0, 0, n0ua0, {'sapling': 2})
        assert_equal(Decimal(self.nodes[2].z_getbalance(myzaddr)), zbalance)

        # Send funds back from the legacy taddr to the UA
        recipients = []
        recipients.append({"address":n0ua0, "amount":4})
        opid = self.nodes[2].z_sendmany(mytaddr, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[2], opid)

        self.nodes[2].generate(1)
        self.sync_all()

        node2balance -= Decimal('4.0')
        self.check_balance(0, 0, n0ua0, {'sapling': 6})
        assert_equal(Decimal(self.nodes[2].getbalance()), node2balance)

        # Send funds back from the legacy zaddr to the UA
        recipients = []
        recipients.append({"address":n0ua0, "amount":2})
        opid = self.nodes[2].z_sendmany(myzaddr, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[2], opid)

        self.nodes[2].generate(1)
        self.sync_all()

        zbalance -= Decimal('2.0')
        self.check_balance(0, 0, n0ua0, {'sapling': 8})
        assert_equal(Decimal(self.nodes[2].z_getbalance(myzaddr)), zbalance)

if __name__ == '__main__':
    WalletZSendmanyTest().main()
