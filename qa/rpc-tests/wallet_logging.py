#!/usr/bin/env python3
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import start_nodes, stop_nodes, wait_and_assert_operationid_status, wait_bitcoinds, get_coinbase_address, check_node_log
from decimal import Decimal

class WalletNotifyTest (BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, extra_args=[['-debug=balanceunsafe', '-debug=stateunsafe'], [], [], []])

    def run_test (self):
        # generate some address of different types
        zaddr_sprout = self.nodes[0].z_getnewaddress('sprout')
        zaddr_sapling = self.nodes[0].z_getnewaddress('sapling')
        taddr = self.nodes[0].getnewaddress()

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # funds are in coinbase
        taddr_coinbase = get_coinbase_address(self.nodes[0])
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # send from coinbase to sprout
        recipients = []
        recipients.append({"address":zaddr_sprout, "amount":Decimal('10.0')-Decimal('0.0001')})
        tx = wait_and_assert_operationid_status(self.nodes[0], self.nodes[0].z_sendmany(taddr_coinbase, recipients), timeout=120)

        # generate
        self.sync_all()

        # check tx generated
        string_to_find = "Transaction generated: " + tx
        check_node_log(self, 0, string_to_find, False)

        # mine
        self.nodes[0].generate(1)
        self.sync_all()

        # check tx mined
        string_to_find = "Transaction mined: " + tx
        check_node_log(self, 0, string_to_find, False)

        # check balance
        string_to_find = "Balance changed in address " + zaddr_sprout + " from 0.00000000 to 9.99990000"
        check_node_log(self, 0, string_to_find, False)

        # send from sprout to taddr
        recipients = []
        recipients.append({"address":taddr, "amount":Decimal('5.0')-Decimal('0.0001')})
        tx = wait_and_assert_operationid_status(self.nodes[0], self.nodes[0].z_sendmany(zaddr_sprout, recipients), timeout=120)

        # generate
        self.sync_all()

        # check tx generated
        string_to_find = "Transaction generated: " + tx
        check_node_log(self, 0, string_to_find, False)

        # mine
        self.nodes[0].generate(1)
        self.sync_all()

        # check tx mined
        string_to_find = "Transaction mined: " + tx
        check_node_log(self, 0, string_to_find, False)

        # check balances
        string_to_find = "Balance changed in address " + zaddr_sprout + " from 9.99990000 to 4.99990000"
        check_node_log(self, 0, string_to_find, False)
        string_to_find = "Balance changed in address " + taddr + " from 0.00000000 to 4.99990000"
        check_node_log(self, 0, string_to_find, False)

        # send from coinbase to taddr
        tx = self.nodes[0].sendtoaddress(taddr, Decimal('1.0'))

        # generate
        self.sync_all()

        # check tx generated
        string_to_find = "Transaction generated: " + tx
        check_node_log(self, 0, string_to_find, False)

        # mine
        self.nodes[0].generate(1)
        self.sync_all()

        # check tx mined
        string_to_find = "Transaction mined: " + tx
        check_node_log(self, 0, string_to_find, False)

        # check balance
        string_to_find = "Balance changed in address " + taddr + " from 4.99990000 to 5.99990000"
        check_node_log(self, 0, string_to_find, False)

        # send from taddr to sapling
        recipients = []
        recipients.append({"address":zaddr_sapling, "amount":Decimal('0.5')-Decimal('0.0001')})
        tx = wait_and_assert_operationid_status(self.nodes[0], self.nodes[0].z_sendmany(taddr, recipients), timeout=120)

        # generate
        self.sync_all()

        # check tx generated
        string_to_find = "Transaction generated: " + tx
        check_node_log(self, 0, string_to_find, False)

        # mine
        self.nodes[0].generate(1)
        self.sync_all()

        # check tx mined
        string_to_find = "Transaction mined: " + tx
        check_node_log(self, 0, string_to_find, False)

        # check balances
        string_to_find = "Balance changed in address " + taddr + " from 5.99990000 to 4.99990000"
        check_node_log(self, 0, string_to_find, False)
        string_to_find = "Balance changed in address " + zaddr_sapling + " from 0.00000000 to 0.49990000"
        check_node_log(self, 0, string_to_find, False)

        # get a new sapling address
        zaddr_sapling2 = self.nodes[0].z_getnewaddress('sapling')
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # send from sapling to sapling2
        recipients = []
        recipients.append({"address":zaddr_sapling2, "amount":Decimal('0.3')-Decimal('0.0001')})
        tx = wait_and_assert_operationid_status(self.nodes[0], self.nodes[0].z_sendmany(zaddr_sapling, recipients), timeout=120)

        # generate
        self.sync_all()

        # check tx generated
        string_to_find = "Transaction generated: " + tx
        check_node_log(self, 0, string_to_find, False)

        # mine
        self.nodes[0].generate(1)
        self.sync_all()

        # check tx mined
        string_to_find = "Transaction mined: " + tx
        check_node_log(self, 0, string_to_find, False)

        # check balances
        string_to_find = "Balance changed in address " + zaddr_sapling + " from 0.49990000 to 0.19990000"
        check_node_log(self, 0, string_to_find, False)
        string_to_find = "Balance changed in address " + zaddr_sapling2 + " from 0.00000000 to 0.29990000"
        check_node_log(self, 0, string_to_find, False)

        # send from sapling2 to taddr
        recipients = []
        recipients.append({"address":taddr, "amount":Decimal('0.1')-Decimal('0.0001')})
        tx = wait_and_assert_operationid_status(self.nodes[0], self.nodes[0].z_sendmany(zaddr_sapling2, recipients), timeout=120)

        # generate
        self.sync_all()

        # check tx generated
        string_to_find = "Transaction generated: " + tx
        check_node_log(self, 0, string_to_find, False)

        # mine
        self.nodes[0].generate(1)
        self.sync_all()

        # check tx mined
        string_to_find = "Transaction mined: " + tx
        check_node_log(self, 0, string_to_find, False)

        # check balances
        string_to_find = "Balance changed in address " + taddr + " from 4.99990000 to 5.09980000"
        check_node_log(self, 0, string_to_find, False)
        string_to_find = "Balance changed in address " + zaddr_sapling2 + " from 0.29990000 to 0.19990000"
        check_node_log(self, 0, string_to_find, False)

        # restart all nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        # send from sprout to taddr again
        recipients = []
        recipients.append({"address":taddr, "amount":Decimal('1.0')-Decimal('0.0001')})
        tx = wait_and_assert_operationid_status(self.nodes[0], self.nodes[0].z_sendmany(zaddr_sprout, recipients), timeout=120)

        # generate
        self.sync_all()

        # check tx generated
        string_to_find = "Transaction generated: " + tx
        check_node_log(self, 0, string_to_find, False)

        # mine
        self.nodes[0].generate(1)
        self.sync_all()

        # check tx mined
        string_to_find = "Transaction mined: " + tx
        check_node_log(self, 0, string_to_find, False)

        # check balances
        string_to_find = "Balance changed in address " + zaddr_sprout + " from 9.99990000 to 4.99990000"
        check_node_log(self, 0, string_to_find, False)
        string_to_find = "Balance changed in address " + taddr + " from 5.09980000 to 6.09970000"
        check_node_log(self, 0, string_to_find, False)

        # send from saling2 to sapling
        recipients = []
        recipients.append({"address":zaddr_sapling, "amount":Decimal('0.1')-Decimal('0.00001')})
        tx = wait_and_assert_operationid_status(self.nodes[0], self.nodes[0].z_sendmany(zaddr_sapling2, recipients), timeout=120)

        # generate
        self.sync_all()

        # check tx generated
        string_to_find = "Transaction generated: " + tx
        check_node_log(self, 0, string_to_find, False)

        # mine
        self.nodes[0].generate(1)
        self.sync_all()

        # check tx mined
        string_to_find = "Transaction mined: " + tx
        check_node_log(self, 0, string_to_find, False)

        # check balances
        string_to_find = "Balance changed in address " + taddr + " from 5.99990000 to 4.99990000"
        #check_node_log(self, 0, string_to_find, False)
        #string_to_find = "Balance changed in address " + zaddr_sapling + " from 0.00000000 to 0.49990000"
        #check_node_log(self, 0, string_to_find, False)


if __name__ == '__main__':
    WalletNotifyTest().main ()