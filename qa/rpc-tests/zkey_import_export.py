#!/usr/bin/env python2
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import Decimal
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, start_nodes, initialize_chain_clean, connect_nodes_bi

import logging
import time
import math

logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.INFO)


class ZkeyImportExportTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 5)

    def setup_network(self, split=False):
        self.nodes = start_nodes(5, self.options.tmpdir )
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        connect_nodes_bi(self.nodes,0,3)
        connect_nodes_bi(self.nodes,0,4)
        self.is_network_split=False
        self.sync_all()

    # TODO: Refactor in z_addr test_framework file
    # Returns txid if operation was a success or None
    def wait_and_assert_operationid_status(self, node, myopid, in_status='success', in_errormsg=None):
        print('waiting for async operation {}'.format(myopid))
        opids = []
        opids.append(myopid)
        timeout = 300
        status = None
        errormsg = None
        txid = None
        for x in xrange(1, timeout):
            results = node.z_getoperationresult(opids)
            if len(results)==0:
                time.sleep(1)
            else:
                print("Results", results[0])
                status = results[0]["status"]
                if status == "failed":
                    errormsg = results[0]['error']['message']
                elif status == "success":
                    txid = results[0]['result']['txid']
                break
        print('...returned status: {}'.format(status))
        assert_equal(in_status, status)
        if errormsg is not None:
            assert(in_errormsg is not None)
            assert_equal(in_errormsg in errormsg, True)
            print('...returned error: {}'.format(errormsg))
        return txid

    def run_test(self):
        [alice, bob, charlie, david, miner] = self.nodes

        def z_send(from_node, from_addr, to_addr, amount):
            opid = from_node.z_sendmany(from_addr, [{"address": to_addr, "amount": Decimal(amount)}])
            self.wait_and_assert_operationid_status(from_node, opid)
            self.sync_all()
            miner.generate(1)
            self.sync_all()

        def z_getbalance(node, zaddr):
            bal = node.z_getbalance(zaddr)
            # Ignore fees for sake of comparison
            round_balance = math.ceil(bal*100)/100
            return round_balance

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

        def find_imported_key(node, import_zaddr):
            zaddrs = node.z_listaddresses()
            assert(import_zaddr in zaddrs)
            return import_zaddr

        # Seed Alice with some funds
        alice.generate(10)
        self.sync_all()
        miner.generate(100)
        self.sync_all()
        # Shield Alice's coinbase funds to her zaddr
        alice_zaddr = alice.z_getnewaddress()
        res = alice.z_shieldcoinbase("*", alice_zaddr)
        self.wait_and_assert_operationid_status(alice, res['opid'])
        miner.generate(6)
        self.sync_all()

        # Now get a pristine z-address for receiving transfers:
        bob_zaddr = bob.z_getnewaddress()
        verify_utxos(bob, [], bob_zaddr)
        # TODO: Verify that charlie doesn't have funds in addr
        # verify_utxos(charlie, [])

        # the amounts of each txn embodied which generates a single UTXO:
        amounts = map(Decimal, ['2.3', '3.7', '0.1', '0.5', '1.0', '0.19'])

        # Internal test consistency assertion:
        assert_greater_than(
            get_private_balance(alice),
            reduce(Decimal.__add__, amounts))

        logging.info("Sending pre-export txns...")
        for amount in amounts[0:2]:
            z_send(alice, alice_zaddr, bob_zaddr, amount)

        logging.info("Exporting privkey from bob...")
        privkey = bob.z_exportkey(bob_zaddr)

        logging.info("Sending post-export txns...")
        for amount in amounts[2:4]:
            z_send(alice, alice_zaddr, bob_zaddr, amount)

        print("Bob amounts:", amounts[:4])
        verify_utxos(bob, amounts[:4], bob_zaddr)
        # verify_utxos(charlie, [])

        logging.info("Importing privkey into charlie...")
        # z_importkey rescan defaults to "whenkeyisnew", so should rescan here
        charlie.z_importkey(privkey)
        ipk_zaddr = find_imported_key(charlie, bob_zaddr)

        # z_importkey should have rescanned for new key, so this should pass:
        verify_utxos(charlie, amounts[:4], ipk_zaddr)

        # Verify idempotent behavior:
        charlie.z_importkey(privkey)
        ipk_zaddr2 = find_imported_key(charlie, bob_zaddr)

        # amounts should be unchanged
        verify_utxos(charlie, amounts[:4], ipk_zaddr2)

        logging.info("Sending post-import txns...")
        for amount in amounts[4:]:
            z_send(alice, alice_zaddr, bob_zaddr, amount)

        verify_utxos(bob, amounts, bob_zaddr)
        verify_utxos(charlie, amounts, ipk_zaddr)
        verify_utxos(charlie, amounts, ipk_zaddr2)

        # Try to reproduce zombie balance reported in #1936
        # At generated zaddr, receive ZEC, and send ZEC back out. bob -> alice
        for amount in amounts[:2]:
            print("Sending amount from bob to alice: ", amount)
            z_send(bob, bob_zaddr, alice_zaddr, amount)

        balance = float(sum(amounts) - sum(amounts[:2]))
        assert_equal(z_getbalance(bob, bob_zaddr), balance)

        # z_import onto new node "david" (blockchain rescan, default or True?)
        david.z_importkey(privkey)
        d_ipk_zaddr = find_imported_key(david, bob_zaddr)

        # Check if amt bob spent is deducted for charlie and david
        assert_equal(z_getbalance(charlie, ipk_zaddr), balance)
        assert_equal(z_getbalance(david, d_ipk_zaddr), balance)

if __name__ == '__main__':
    ZkeyImportExportTest().main()
