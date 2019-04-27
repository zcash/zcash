#!/usr/bin/env python
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from decimal import Decimal
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_true, get_coinbase_address, \
    initialize_chain_clean, start_nodes, wait_and_assert_operationid_status, \
    wait_and_assert_operationid_status_result


class SproutSaplingMigration(BitcoinTestFramework):
    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, [[
            '-nuparams=5ba81b19:100',  # Overwinter
            '-nuparams=76b809bb:100',  # Sapling
        ]] * 4)

    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def run_test(self):
        print "Mining blocks..."
        self.nodes[0].generate(101)
        self.sync_all()

        # Send some ZEC to a Sprout address
        tAddr = get_coinbase_address(self.nodes[0])
        sproutAddr = self.nodes[0].z_getnewaddress('sprout')
        saplingAddr = self.nodes[0].z_getnewaddress('sapling')

        opid = self.nodes[0].z_sendmany(tAddr, [{"address": sproutAddr, "amount": Decimal('10')}], 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], opid)
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].z_getbalance(sproutAddr), Decimal('10'))
        assert_equal(self.nodes[0].z_getbalance(saplingAddr), Decimal('0'))

        # Migrate
        self.nodes[0].z_setmigration(True)
        print "Mining to block 494..."
        self.nodes[0].generate(392)  # 102 -> 494
        self.sync_all()

        # At 494 we should have no async operations
        assert_equal(0, len(self.nodes[0].z_getoperationstatus()), "num async operations at 494")

        self.nodes[0].generate(1)
        self.sync_all()

        # At 495 we should have an async operation
        operationstatus = self.nodes[0].z_getoperationstatus()
        print "migration operation: {}".format(operationstatus)
        assert_equal(1, len(operationstatus), "num async operations at 495")
        assert_equal('saplingmigration', operationstatus[0]['method'])
        assert_equal(500, operationstatus[0]['target_height'])

        result = wait_and_assert_operationid_status_result(self.nodes[0], operationstatus[0]['id'])
        print "result: {}".format(result)
        assert_equal('saplingmigration', result['method'])
        assert_equal(500, result['target_height'])
        assert_equal(1, result['result']['num_tx_created'])

        assert_equal(0, len(self.nodes[0].getrawmempool()), "mempool size at 495")

        self.nodes[0].generate(3)
        self.sync_all()

        # At 498 the mempool will be empty and no funds will have moved
        assert_equal(0, len(self.nodes[0].getrawmempool()), "mempool size at 498")
        assert_equal(self.nodes[0].z_getbalance(sproutAddr), Decimal('10'))
        assert_equal(self.nodes[0].z_getbalance(saplingAddr), Decimal('0'))

        self.nodes[0].generate(1)
        self.sync_all()

        # At 499 there will be a transaction in the mempool and the note will be locked
        assert_equal(1, len(self.nodes[0].getrawmempool()), "mempool size at 499")
        assert_equal(self.nodes[0].z_getbalance(sproutAddr), Decimal('0'))
        assert_equal(self.nodes[0].z_getbalance(saplingAddr), Decimal('0'))
        assert_true(self.nodes[0].z_getbalance(saplingAddr, 0) > Decimal('0'), "Unconfirmed sapling")

        self.nodes[0].generate(1)
        self.sync_all()

        # At 500 funds will have moved
        sprout_balance = self.nodes[0].z_getbalance(sproutAddr)
        sapling_balance = self.nodes[0].z_getbalance(saplingAddr)
        print "sprout balance: {}, sapling balance: {}".format(sprout_balance, sapling_balance)
        assert_true(sprout_balance < Decimal('10'), "Should have less Sprout funds")
        assert_true(sapling_balance > Decimal('0'), "Should have more Sapling funds")
        assert_true(sprout_balance + sapling_balance, Decimal('9.9999'))


if __name__ == '__main__':
    SproutSaplingMigration().main()
