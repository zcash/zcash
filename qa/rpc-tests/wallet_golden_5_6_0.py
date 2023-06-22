#!/usr/bin/env python3
#
# Copyright (c) 2023 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    nuparams,
    start_nodes,
    stop_nodes,
    wait_bitcoinds,
    wait_and_assert_operationid_status,
    persistent_cache_exists,
    persist_node_caches,
)
from decimal import Decimal


class WalletGoldenV5_6_0Test(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4
        # Start from the cache if it exists
        if persistent_cache_exists('golden-v5.6.0'):
            self.cache_behavior = 'golden-v5.6.0'

    def setup_nodes(self):
        default_extra_args = [
            nuparams(NU5_BRANCH_ID, 201),
            # The following was required for correct persistence of updated wallet state,
            # but not required for subsequent test runs.
            #"-regtestwalletsetbestchaineveryblock",
        ]
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[
            default_extra_args,
            default_extra_args,
            default_extra_args,
            default_extra_args + ["-mineraddress=uregtest124msndx0mcxnjx7l9vxjswqeear2nvp3sg8u6uuvmxxnfmvmxkyjnx4qkxasfszez8m5z9trsgva0s6rzxn4fx96wf0lphfj0yu9s63w",
                                  "-minetolocalwallet=0"] # this address doesn't exist in node 3's wallet at the time of node start
            ])

    def run_test(self):
        # Establish the persistent cache state before running the main behavior
        # of the test.
        if self.cache_behavior != 'golden-v5.6.0':
            # Sanity-check the test harness
            assert_equal(self.nodes[0].getblockcount(), 200)

            # Activate nu5
            self.nodes[0].generate(1)
            self.sync_all()

            # Set up an Orchard account on node 0
            n0account0 = self.nodes[0].z_getnewaccount()['account']
            assert(n0account0 == 0)
            n0ua0_0 = self.nodes[0].z_getaddressforaccount(n0account0, ['orchard'], 0)['address']
            assert_equal(
                    {'pools': {}, 'minimum_confirmations': 1},
                    self.nodes[0].z_getbalanceforaccount(n0account0))

            # Fund the Orchard wallet on node 0 from Node 2 coinbase
            n2taddr = get_coinbase_address(self.nodes[2])
            recipients = [{"address": n0ua0_0, 'amount': Decimal('9.99985')}]
            # TODO: since we can't have transparent change, this should only require
            # `AllowRevealedSenders`
            opid = self.nodes[2].z_sendmany(n2taddr, recipients, 1, None, 'AllowFullyTransparent')
            wait_and_assert_operationid_status(self.nodes[2], opid)

            self.nodes[2].generate(1)
            self.sync_all()

            assert_equal(
                    {
                        'pools': {'orchard': {'valueZat': Decimal('999985000')}},
                        'minimum_confirmations': 1
                    },
                    self.nodes[0].z_getbalanceforaccount(n0account0))

            # Send funds from Orchard on node 0 to Orchard on node 2
            n2account0 = self.nodes[2].z_getnewaccount()['account']
            assert(n2account0 == 0)
            n2ua0_0 = self.nodes[2].z_getaddressforaccount(n2account0, ['orchard'], 0)['address']
            assert_equal(
                    {'pools': {}, 'minimum_confirmations': 1},
                    self.nodes[2].z_getbalanceforaccount(n2account0))

            recipients = [{"address":n2ua0_0, "amount": Decimal('5.0')}]
            opid = self.nodes[0].z_sendmany(n0ua0_0, recipients, 1)
            wait_and_assert_operationid_status(self.nodes[0], opid)

            self.nodes[0].generate(1)
            self.sync_all()

            assert_equal(
                    {
                        'pools': {'orchard': {'valueZat': Decimal('499975000')}},
                        'minimum_confirmations': 1
                    },
                    self.nodes[0].z_getbalanceforaccount(n0account0))

            # Set up a second account and a couple more addresses on node 0 &
            # send the funds back and forth to establish more note commitment
            # tree structure.
            n0ua0_1 = self.nodes[0].z_getaddressforaccount(n0account0, ['orchard'], 1)['address']
            n0account1 = self.nodes[0].z_getnewaccount()['account']
            assert(n0account1 == 1)
            n0ua1_0 = self.nodes[0].z_getaddressforaccount(n0account1, ['orchard'], 0)['address']

            # We space out transactions at 15 block increments so that some of the
            # oldest checkpoints get dropped.
            for i in range(10):
                if i % 2 == 0:
                    recipients = [
                        {"address": n0ua0_0, "amount": Decimal('0.1')},
                        {"address": n0ua0_1, "amount": Decimal('0.2')},
                        {"address": n0ua1_0, "amount": Decimal('0.3')},
                    ]
                    opid = self.nodes[2].z_sendmany(n2ua0_0, recipients, 1)
                    wait_and_assert_operationid_status(self.nodes[2], opid)

                    self.nodes[2].generate(15)
                    self.sync_all()
                else:
                    recipients = [{"address": n2ua0_0, "amount": Decimal('0.7')}]
                    opid = self.nodes[0].z_sendmany(n0ua0_0, recipients, 1)
                    wait_and_assert_operationid_status(self.nodes[0], opid)

                    self.nodes[0].generate(15)
                    self.sync_all()

            # Shut down the nodes
            stop_nodes(self.nodes)
            wait_bitcoinds()

            # persist the node state to the cache
            persist_node_caches(self.options.tmpdir, 'golden-v5.6.0', 4)

            # Restart the network
            self.setup_network()

        tarnish = False
        if tarnish:
            # We need to add another Orchard note to the note commitment tree. The mineraddress used when starting
            # node 3 is the default address for account 0, so here we just mine a block to shielded coinbase to
            # add another Orchard note commitment.
            n3account0 = self.nodes[3].z_getnewaccount()['account']
            n3ua0_0 = self.nodes[3].z_getaddressforaccount(n3account0, ['orchard'], 0)['address']
            assert_equal(n3ua0_0, "uregtest124msndx0mcxnjx7l9vxjswqeear2nvp3sg8u6uuvmxxnfmvmxkyjnx4qkxasfszez8m5z9trsgva0s6rzxn4fx96wf0lphfj0yu9s63w");

            self.nodes[3].generate(1)
            self.sync_all()

            # Shut down the nodes
            stop_nodes(self.nodes)
            wait_bitcoinds()

            # persist the node state to the cache
            persist_node_caches(self.options.tmpdir, 'tarnished-v5.6.0', 4)
        else:
            golden_check_spendability(self, 0)

def golden_check_spendability(test_instance, block_offset):
    # Since the cache has already been loaded, reproduce the previously-derived
    # addresses.
    n0account0 = 0
    n0ua0_0 = test_instance.nodes[0].z_getaddressforaccount(n0account0, ['orchard'], 0)['address']
    n0ua0_1 = test_instance.nodes[0].z_getaddressforaccount(n0account0, ['orchard'], 1)['address']

    n0account1 = 1
    n0ua1_0 = test_instance.nodes[0].z_getaddressforaccount(n0account1, ['orchard'], 0)['address']

    n2account0 = 0
    n2ua0_0 = test_instance.nodes[2].z_getaddressforaccount(n2account0, ['orchard'], 0)['address']

    # Sanity-check the test harness
    assert_equal(test_instance.nodes[0].getblockcount(), 353 + block_offset)

    assert_equal(
            {
                'pools': {'orchard': {'valueZat': Decimal(499975000 + (5 * 30000000) - (5 * 70010000))}},
                'minimum_confirmations': 1
            },
            test_instance.nodes[0].z_getbalanceforaccount(n0account0))
    assert_equal(
            {
                'pools': {'orchard': {'valueZat': Decimal(5 * 30000000)}},
                'minimum_confirmations': 1
            },
            test_instance.nodes[0].z_getbalanceforaccount(n0account1))
    assert_equal(
            {
                'pools': {'orchard': {'valueZat': Decimal(500000000 + (5 * 70000000) - (5 * 60020000))}},
                'minimum_confirmations': 1
            },
            test_instance.nodes[2].z_getbalanceforaccount(n2account0))

    # Send a bunch of notes back and forth again.
    for i in range(10):
        if i % 2 == 0:
            recipients = [
                {"address": n0ua0_0, "amount": Decimal('0.1')},
                {"address": n0ua0_1, "amount": Decimal('0.2')},
                {"address": n0ua1_0, "amount": Decimal('0.3')},
            ]
            opid = test_instance.nodes[2].z_sendmany(n2ua0_0, recipients, 1)
            wait_and_assert_operationid_status(test_instance.nodes[2], opid)

            test_instance.nodes[2].generate(1)
            test_instance.sync_all()
        else:
            recipients = [{"address": n2ua0_0, "amount": Decimal('0.1')}]
            opid = test_instance.nodes[0].z_sendmany(n0ua0_0, recipients, 1)
            wait_and_assert_operationid_status(test_instance.nodes[0], opid)

            test_instance.nodes[0].generate(1)
            test_instance.sync_all()

    assert_equal(test_instance.nodes[0].getblockcount(), 363 + block_offset)

    recipients = [{"address": n0ua0_0, "amount": Decimal('2.9')}]
    opid = test_instance.nodes[0].z_sendmany(n0ua1_0, recipients, 1)
    wait_and_assert_operationid_status(test_instance.nodes[0], opid)

    test_instance.nodes[0].generate(1)
    test_instance.sync_all()

    assert_equal(
            {
                'pools': {'orchard': {'valueZat': Decimal(299925000 + (5 * 30000000) - (5 * 10010000) + 290000000)}},
                'minimum_confirmations': 1
            },
            test_instance.nodes[0].z_getbalanceforaccount(n0account0))
    assert_equal(
            {
                'pools': {'orchard': {'valueZat': Decimal(150000000 + (5 * 30000000) - 290050000)}},
                'minimum_confirmations': 1
            },
            test_instance.nodes[0].z_getbalanceforaccount(n0account1))
    assert_equal(
            {
                'pools': {'orchard': {'valueZat': Decimal(549900000 + (5 * 10000000) - (5 * 60020000))}},
                'minimum_confirmations': 1
            },
            test_instance.nodes[2].z_getbalanceforaccount(n2account0))

if __name__ == '__main__':
    WalletGoldenV5_6_0Test().main()


