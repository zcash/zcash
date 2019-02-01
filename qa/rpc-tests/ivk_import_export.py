#!/usr/bin/env python2
# Copyright (c) 2019 Bartlomiej Lisiecki
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import Decimal
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, start_nodes,\
    initialize_chain_clean, connect_nodes_bi, wait_and_assert_operationid_status

import logging

logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.INFO)

fee = Decimal('0.0001') # constant (but can be changed within reason)

class IVKImportExportTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        self.nodes = start_nodes(4, self.options.tmpdir, [[
            '-nuparams=5ba81b19:101', # Overwinter
            '-nuparams=76b809bb:102', # Sapling
        ]] * 4)

        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,0,2)
        connect_nodes_bi(self.nodes,0,3)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        [alice, bob, charlie, miner] = self.nodes

        # the sender loses 'amount' plus fee; to_addr receives exactly 'amount'
        def z_send(from_node, from_addr, to_addr, amount):
            global fee
            opid = from_node.z_sendmany(from_addr,
                [{"address": to_addr, "amount": Decimal(amount)}], 1, fee)
            wait_and_assert_operationid_status(from_node, opid)
            self.sync_all()
            miner.generate(1)
            self.sync_all()

        def verify_utxos(node, amts, zaddr):
            amts.sort(reverse=True)
            txs = node.z_listreceivedbyaddress(zaddr)

            def cmp_confirmations_high_to_low(a, b):
                return cmp(b["amount"], a["amount"])

            txs.sort(cmp_confirmations_high_to_low)
            print("Sorted txs", txs)
            print("amts", amts)

            try:
                assert_equal(amts, [tx["amount"] for tx in txs])
            except AssertionError:
                logging.error(
                    'Expected amounts: %r; txs: %r',
                    amts, txs)
                raise

        def get_private_balance(node):
            balance = node.z_gettotalbalance()
            return balance['private']

        def find_imported_zaddr(node, import_zaddr):
            zaddrs = node.z_listaddresses()
            assert(import_zaddr in zaddrs)
            return import_zaddr

        # activate sapling
        alice.generate(102)
        self.sync_all()

        # sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 102)

        # shield alice's coinbase funds to her zaddr
        alice_zaddr = alice.z_getnewaddress('sapling')
        res = alice.z_shieldcoinbase("*", alice_zaddr)
        wait_and_assert_operationid_status(alice, res['opid'])
        self.sync_all()
        miner.generate(1)
        self.sync_all()

        # the amounts of each txn embodied which generates a single utxo:
        amounts = map(Decimal, ['2.3', '3.7', '0.1', '0.5', '1.0', '0.19'])

        # internal test consistency assertion:
        assert_greater_than(
            get_private_balance(alice),
            reduce(Decimal.__add__, amounts))


        # now get a pristine z-address for receiving transfers:
        bob_zaddr = bob.z_getnewaddress('sapling')
        verify_utxos(bob, [], bob_zaddr)

        logging.info("sending pre-export txns...")
        for amount in amounts[0:2]:
            z_send(alice, alice_zaddr, bob_zaddr, amount)

        logging.info("exporting ivk from bob...")
        bob_ivk = bob.z_exportviewingkey(bob_zaddr)

        logging.info("sending post-export txns...")
        for amount in amounts[2:4]:
            z_send(alice, alice_zaddr, bob_zaddr, amount)

        verify_utxos(bob, amounts[:4], bob_zaddr)

        logging.info("importing bob_ivk into charlie...")
        # we need to pass bob_zaddr since it's a sapling address
        charlie.z_importviewingkey(bob_ivk, 'yes', 0, bob_zaddr)

        # z_importkey should have rescanned for new key, so this should pass:
        verify_utxos(charlie, amounts[:4], bob_zaddr)

        # verify idempotent behavior:
        charlie.z_importviewingkey(bob_ivk, 'yes', 0, bob_zaddr)
        verify_utxos(charlie, amounts[:4], bob_zaddr)


        logging.info("Sending post-import txns...")
        for amount in amounts[4:]:
            z_send(alice, alice_zaddr, bob_zaddr, amount)

        verify_utxos(bob, amounts, bob_zaddr)
        verify_utxos(charlie, amounts, bob_zaddr)

if __name__ == '__main__':
    IVKImportExportTest().main()
