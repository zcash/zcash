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

SAPLING_ADDR = 'zregtestsapling1ssqj3f3majnl270985gqcdqedd9t4nlttjqskccwevj2v20sc25deqspv3masufnwcdy67cydyy'
SAPLING_KEY = 'secret-extended-key-regtest1qv62zt2fqyqqpqrh2qzc08h7gncf4447jh9kvnnnhjg959fkwt7mhw9j8e9at7attx8z6u3953u86vcnsujdc2ckdlcmztjt44x3uxpah5mxtncxd0mqcnz9eq8rghh5m4j44ep5d9702sdvvwawqassulktfegrcp4twxgqdxx4eww3lau0mywuaeztpla2cmvagr5nj98elt45zh6fjznadl6wz52n2uyhdwcm2wlsu8fnxstrk6s4t55t8dy6jkgx5g0cwpchh5qffp8x5'


def check_migration_status(
        node,
        enabled,
        destination_address,
        non_zero_unmigrated_amount,
        non_zero_unfinalized_migrated_amount,
        non_zero_finalized_migrated_amount,
        finalized_migration_transactions,
        len_migration_txids
):
    status = node.z_getmigrationstatus()
    assert_equal(enabled, status['enabled'])
    assert_equal(destination_address, status['destination_address'])
    assert_equal(non_zero_unmigrated_amount, Decimal(status['unmigrated_amount']) > Decimal('0.00'))
    assert_equal(non_zero_unfinalized_migrated_amount, Decimal(status['unfinalized_migrated_amount']) > Decimal('0'))
    assert_equal(non_zero_finalized_migrated_amount, Decimal(status['finalized_migrated_amount']) > Decimal('0'))
    assert_equal(finalized_migration_transactions, status['finalized_migration_transactions'])
    assert_equal(len_migration_txids, len(status['migration_txids']))


class SproutSaplingMigration(BitcoinTestFramework):
    def setup_nodes(self):
        # Activate overwinter/sapling on all nodes
        extra_args = [[
            '-nuparams=5ba81b19:100',  # Overwinter
            '-nuparams=76b809bb:100',  # Sapling
        ]] * 4
        # Add migration parameters to nodes[0]
        extra_args[0] = extra_args[0] + [
            '-migration',
            '-migrationdestaddress=' + SAPLING_ADDR,
            '-debug=zrpcunsafe'
        ]
        assert_equal(5, len(extra_args[0]))
        assert_equal(2, len(extra_args[1]))
        return start_nodes(4, self.options.tmpdir, extra_args)

    def setup_chain(self):
        print("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def run_migration_test(self, node, sproutAddr, saplingAddr, target_height):
        # Make sure we are in a good state to run the test
        assert_equal(102, node.getblockcount() % 500, "Should be at block 102 % 500")
        assert_equal(node.z_getbalance(sproutAddr), Decimal('10'))
        assert_equal(node.z_getbalance(saplingAddr), Decimal('0'))
        check_migration_status(node, False, saplingAddr, True, False, False, 0, 0)

        # Migrate
        node.z_setmigration(True)
        print("Mining to block 494 % 500...")
        node.generate(392)  # 102 % 500 -> 494 % 500
        self.sync_all()

        # At 494 % 500 we should have no async operations
        assert_equal(0, len(node.z_getoperationstatus()), "num async operations at 494 % 500")
        check_migration_status(node, True, saplingAddr, True, False, False, 0, 0)

        node.generate(1)
        self.sync_all()

        # At 495 % 500 we should have an async operation
        operationstatus = node.z_getoperationstatus()
        print("migration operation: {}".format(operationstatus))
        assert_equal(1, len(operationstatus), "num async operations at 495  % 500")
        assert_equal('saplingmigration', operationstatus[0]['method'])
        assert_equal(target_height, operationstatus[0]['target_height'])

        result = wait_and_assert_operationid_status_result(node, operationstatus[0]['id'])
        print("result: {}".format(result))
        assert_equal('saplingmigration', result['method'])
        assert_equal(target_height, result['target_height'])
        assert_equal(1, result['result']['num_tx_created'])
        assert_equal(1, len(result['result']['migration_txids']))
        assert_true(result['result']['amount_migrated'] > Decimal('0'))

        assert_equal(0, len(node.getrawmempool()), "mempool size at 495 % 500")

        node.generate(3)
        self.sync_all()

        # At 498 % 500 the mempool will be empty and no funds will have moved
        assert_equal(0, len(node.getrawmempool()), "mempool size at 498 % 500")
        assert_equal(node.z_getbalance(sproutAddr), Decimal('10'))
        assert_equal(node.z_getbalance(saplingAddr), Decimal('0'))

        node.generate(1)
        self.sync_all()

        # At 499 % 500 there will be a transaction in the mempool and the note will be locked
        mempool = node.getrawmempool()
        print("mempool: {}".format(mempool))
        assert_equal(1, len(mempool), "mempool size at 499 % 500")
        assert_equal(node.z_getbalance(sproutAddr), Decimal('0'))
        assert_equal(node.z_getbalance(saplingAddr), Decimal('0'))
        assert_true(node.z_getbalance(saplingAddr, 0) > Decimal('0'), "Unconfirmed sapling balance at 499 % 500")
        # Check that unmigrated amount + unfinalized = starting balance - fee
        status = node.z_getmigrationstatus()
        print("status: {}".format(status))
        assert_equal(Decimal('9.9999'), Decimal(status['unmigrated_amount']) + Decimal(status['unfinalized_migrated_amount']))

        # The transaction in the mempool should be the one listed in migration_txids,
        # and it should expire at the next 450 % 500.
        assert_equal(1, len(status['migration_txids']))
        txid = status['migration_txids'][0]
        assert_equal(txid, mempool[0])
        tx = node.getrawtransaction(txid, 1)
        assert_equal(target_height + 450, tx['expiryheight'])

        node.generate(1)
        self.sync_all()

        # At 0 % 500 funds will have moved
        sprout_balance = node.z_getbalance(sproutAddr)
        sapling_balance = node.z_getbalance(saplingAddr)
        print("sprout balance: {}, sapling balance: {}".format(sprout_balance, sapling_balance))
        assert_true(sprout_balance < Decimal('10'), "Should have less Sprout funds")
        assert_true(sapling_balance > Decimal('0'), "Should have more Sapling funds")
        assert_true(sprout_balance + sapling_balance, Decimal('9.9999'))

        check_migration_status(node, True, saplingAddr, True, True, False, 0, 1)
        # At 10 % 500 the transactions will be considered 'finalized'
        node.generate(10)
        self.sync_all()
        check_migration_status(node, True, saplingAddr, True, False, True, 1, 1)
        # Check exact migration status amounts to make sure we account for fee
        status = node.z_getmigrationstatus()
        assert_equal(sprout_balance, Decimal(status['unmigrated_amount']))
        assert_equal(sapling_balance, Decimal(status['finalized_migrated_amount']))

    def send_to_sprout_zaddr(self, tAddr, sproutAddr):
        # Send some ZEC to a Sprout address
        opid = self.nodes[0].z_sendmany(tAddr, [{"address": sproutAddr, "amount": Decimal('10')}], 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], opid)
        self.nodes[0].generate(1)
        self.sync_all()

    def run_test(self):
        # Check enabling via '-migration' and disabling via rpc
        check_migration_status(self.nodes[0], True, SAPLING_ADDR, False, False, False, 0, 0)
        self.nodes[0].z_setmigration(False)
        check_migration_status(self.nodes[0], False, SAPLING_ADDR, False, False, False, 0, 0)

        # 1. Test using self.nodes[0] which has the parameter
        print("Running test using '-migrationdestaddress'...")
        print("Mining blocks...")
        self.nodes[0].generate(101)
        self.sync_all()
        tAddr = get_coinbase_address(self.nodes[0])

        # Import a previously generated key to test '-migrationdestaddress'
        self.nodes[0].z_importkey(SAPLING_KEY)
        sproutAddr0 = self.nodes[0].z_getnewaddress('sprout')

        self.send_to_sprout_zaddr(tAddr, sproutAddr0)
        self.run_migration_test(self.nodes[0], sproutAddr0, SAPLING_ADDR, 500)
        # Disable migration so only self.nodes[1] has a transaction in the mempool at block 999
        self.nodes[0].z_setmigration(False)

        # 2. Test using self.nodes[1] which will use the default Sapling address
        print("Running test using default Sapling address...")
        # Mine more blocks so we start at 102 % 500
        print("Mining blocks...")
        self.nodes[1].generate(91)  # 511 -> 602
        self.sync_all()

        sproutAddr1 = self.nodes[1].z_getnewaddress('sprout')
        saplingAddr1 = self.nodes[1].z_getnewaddress('sapling')

        self.send_to_sprout_zaddr(tAddr, sproutAddr1)
        self.run_migration_test(self.nodes[1], sproutAddr1, saplingAddr1, 1000)


if __name__ == '__main__':
    SproutSaplingMigration().main()
