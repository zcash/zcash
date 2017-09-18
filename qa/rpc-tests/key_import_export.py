#!/usr/bin/env python2
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from time import sleep
from decimal import Decimal
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, sync_blocks

import logging

logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.INFO)


class KeyImportExportTest (BitcoinTestFramework):

    def run_test(self):
        [alice, bob, charlie, miner] = self.nodes

        def wait_until_miner_sees(txid):
            attempts = 123
            delay = 0.5

            for _ in range(attempts):
                try:
                    miner.getrawtransaction(txid)
                except Exception:
                    logging.debug(
                        'txid %r not yet seen by miner; sleep %r',
                        txid,
                        delay,
                    )
                    sleep(delay)
                else:
                    return

            raise Exception(
                'miner failed to see txid {!r} after {!r} attempts...'.format(
                    txid,
                    attempts,
                ),
            )

        def alice_to_bob(amount):
            txid = alice.sendtoaddress(addr, Decimal(amount))
            wait_until_miner_sees(txid)
            miner.generate(1)
            sync_blocks(self.nodes)

        def verify_utxos(node, amounts):
            utxos = node.listunspent(1, 10**9, [addr])

            def cmp_confirmations_high_to_low(a, b):
                return cmp(b["confirmations"], a["confirmations"])

            utxos.sort(cmp_confirmations_high_to_low)

            try:
                assert_equal(amounts, [utxo["amount"] for utxo in utxos])
            except AssertionError:
                logging.error(
                    'Expected amounts: %r; utxos: %r',
                    amounts, utxos)
                raise

        # The first address already mutated by the startup process, ignore it:
        # BUG: Why is this necessary?
        bob.getnewaddress()

        # Now get a pristine address for receiving transfers:
        addr = bob.getnewaddress()
        verify_utxos(bob, [])

        # the amounts of each txn embodied which generates a single UTXO:
        amounts = map(Decimal, ['2.3', '3.7', '0.1', '0.5', '1.0', '0.19'])

        # Internal test consistency assertion:
        assert_greater_than(
            alice.getbalance(),
            reduce(Decimal.__add__, amounts))

        logging.info("Sending pre-export txns...")
        for amount in amounts[0:2]:
            alice_to_bob(amount)

        logging.info("Exporting privkey from bob...")
        privkey = bob.dumpprivkey(addr)

        logging.info("Sending post-export txns...")
        for amount in amounts[2:4]:
            alice_to_bob(amount)

        verify_utxos(bob, amounts[:4])

        logging.info("Importing privkey into charlie...")
        ipkaddr = charlie.importprivkey(privkey, '', True)
        assert_equal(addr, ipkaddr)

        # Verify idempotent behavior:
        ipkaddr2 = charlie.importprivkey(privkey, '', True)
        assert_equal(addr, ipkaddr2)

        # importprivkey should have rescanned, so this should pass:
        verify_utxos(charlie, amounts[:4])

        logging.info("Sending post-import txns...")
        for amount in amounts[4:]:
            alice_to_bob(amount)

        verify_utxos(bob, amounts)
        verify_utxos(charlie, amounts)


if __name__ == '__main__':
    KeyImportExportTest().main()
