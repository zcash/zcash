#!/usr/bin/env python3
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
from test_framework.test_framework import ZcashTestFramework
from test_framework.util import assert_equal, assert_greater_than, start_nodes,\
    initialize_chain_clean, connect_nodes_bi, wait_and_assert_operationid_status
from functools import reduce
import logging

logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.INFO)

fee = Decimal('0.0001') # constant (but can be changed within reason)

class ZkeyImportExportTest(ZcashTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 5)

    def setup_network(self, split=False):
        self.nodes = start_nodes(5, self.options.tmpdir)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        connect_nodes_bi(self.nodes,0,3)
        connect_nodes_bi(self.nodes,0,4)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        [alice, bob, charlie, david, miner] = self.nodes

        # the sender loses 'amount' plus fee; to_addr receives exactly 'amount'
        def z_send(from_node, from_addr, to_addr, amount):
            global fee
            
            opid = from_node.z_sendmany(from_addr,
                [{"address": to_addr, "amount": Decimal(amount)}], 1, int(fee))
            wait_and_assert_operationid_status(from_node, opid)
            self.sync_all()
            miner.generate(1)
            self.sync_all()

        def verify_utxos(node, amts, zaddr):
            amts.sort(reverse=True)
            txs = node.z_listreceivedbyaddress(zaddr)
            txs.sort(key=lambda x: x["amount"], reverse=True)
            print("Sorted txs", txs)
            print("amts", amts)

            try:
                assert_equal(amts, [tx["amount"] for tx in txs])
                for tx in txs:
                    # make sure JoinSplit keys exist and have valid values
                    assert_equal("jsindex" in tx, True)
                    assert_equal("jsoutindex" in tx, True)
                    assert_greater_than(tx["jsindex"], -1)
                    assert_greater_than(tx["jsoutindex"], -1)
            except AssertionError:
                logging.error(
                    'Expected amounts: %r; txs: %r',
                    amts, txs)
                raise

        def get_private_balance(node):
            balance = node.z_gettotalbalance()
            return balance['private']

        # Seed Alice with some funds
        alice.generate(10)
        self.sync_all()
        miner.generate(100)
        self.sync_all()
        # Shield Alice's coinbase funds to her zaddr
        alice_zaddr = alice.z_getnewaddress('sprout')
        res = alice.z_shieldcoinbase("*", alice_zaddr)
        wait_and_assert_operationid_status(alice, res['opid'])
        self.sync_all()
        miner.generate(1)
        self.sync_all()

        # Now get a pristine z-address for receiving transfers:
        bob_zaddr = bob.z_getnewaddress('sprout')
        verify_utxos(bob, [], bob_zaddr)
        # TODO: Verify that charlie doesn't have funds in addr
        # verify_utxos(charlie, [])

        # the amounts of each txn embodied which generates a single UTXO:
        amounts = list(map(Decimal, ['2.3', '3.7', '0.1', '0.5', '1.0', '0.19']))

        # Internal test consistency assertion:
        assert_greater_than(
            Decimal(get_private_balance(alice)),
            reduce(Decimal.__add__, amounts))

        logging.info("Sending pre-export txns...")
        for amount in amounts[0:2]:
            z_send(alice, alice_zaddr, bob_zaddr, amount)

        logging.info("Exporting privkey from bob...")
        bob_privkey = bob.z_exportkey(bob_zaddr)

        logging.info("Sending post-export txns...")
        for amount in amounts[2:4]:
            z_send(alice, alice_zaddr, bob_zaddr, amount)

        verify_utxos(bob, amounts[:4], bob_zaddr)
        # verify_utxos(charlie, [])

        logging.info("Importing bob_privkey into charlie...")
        # z_importkey rescan defaults to "whenkeyisnew", so should rescan here
        ipk_zaddr = charlie.z_importkey(bob_privkey)

        # z_importkey should have rescanned for new key, so this should pass:
        verify_utxos(charlie, amounts[:4], ipk_zaddr["address"])

        # address is sprout
        assert_equal(ipk_zaddr["type"], "sprout")

        # Verify idempotent behavior:
        ipk_zaddr2 = charlie.z_importkey(bob_privkey)
        assert_equal(ipk_zaddr["address"], ipk_zaddr2["address"])

        # amounts should be unchanged
        verify_utxos(charlie, amounts[:4], ipk_zaddr2["address"])

        logging.info("Sending post-import txns...")
        for amount in amounts[4:]:
            z_send(alice, alice_zaddr, bob_zaddr, amount)

        verify_utxos(bob, amounts, bob_zaddr)
        verify_utxos(charlie, amounts, ipk_zaddr["address"])
        verify_utxos(charlie, amounts, ipk_zaddr2["address"])

        # keep track of the fees incurred by bob (his sends)
        bob_fee = Decimal("0")

        # Try to reproduce zombie balance reported in #1936
        # At generated zaddr, receive ZEC, and send ZEC back out. bob -> alice
        for amount in amounts[:2]:
            print("Sending amount from bob to alice: ", amount)
            z_send(bob, bob_zaddr, alice_zaddr, amount)
            bob_fee += fee

        bob_balance = sum(amounts[2:]) - int(bob_fee)
        
        assert_equal(bob.z_getbalance(bob_zaddr), bob_balance)

        # z_import onto new node "david" (blockchain rescan, default or True?)
        d_ipk_zaddr = david.z_importkey(bob_privkey)

        # Check if amt bob spent is deducted for charlie and david
        assert_equal(charlie.z_getbalance(ipk_zaddr["address"]), bob_balance)
        assert_equal(david.z_getbalance(d_ipk_zaddr["address"]), bob_balance)

if __name__ == '__main__':
    ZkeyImportExportTest().main()
