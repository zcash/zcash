#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Common code for testing z_mergetoaddress before and after sapling activation
#

from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, connect_nodes_bi, fail, \
    initialize_chain_clean, start_node, \
    wait_and_assert_operationid_status, DEFAULT_FEE
from test_framework.zip317 import conventional_fee

from decimal import Decimal

def assert_mergetoaddress_exception(expected_error_msg, merge_to_address_lambda):
    try:
        merge_to_address_lambda()
    except JSONRPCException as e:
        assert_equal(expected_error_msg, e.error['message'])
    except Exception as e:
        fail("Expected JSONRPCException. Found %s" % repr(e))
    else:
        fail("Expected exception: “%s”, but didn’t fail" % expected_error_msg)


class MergeToAddressHelper:

    def __init__(self, new_address, any_zaddr):
        self.new_address = new_address
        self.any_zaddr = [any_zaddr]
        self.any_zaddr_or_utxo = [any_zaddr, "ANY_TADDR"]

    def setup_chain(self, test):
        print("Initializing test directory "+test.options.tmpdir)
        initialize_chain_clean(test.options.tmpdir, 4)

    def setup_network(self, test, additional_args=[]):
        args = [
            '-minrelaytxfee=0',
            '-debug=zrpcunsafe',
            '-allowdeprecated=getnewaddress',
            '-allowdeprecated=z_getnewaddress',
            '-allowdeprecated=z_getbalance',
        ]
        args += additional_args
        test.nodes = []
        test.nodes.append(start_node(0, test.options.tmpdir, args))
        test.nodes.append(start_node(1, test.options.tmpdir, args))
        test.nodes.append(start_node(2, test.options.tmpdir, args))
        connect_nodes_bi(test.nodes, 0, 1)
        connect_nodes_bi(test.nodes, 1, 2)
        connect_nodes_bi(test.nodes, 0, 2)
        test.is_network_split = False
        test.sync_all()

    def run_test(self, test):
        def generate_and_check(node, expected_transactions):
            [blockhash] = node.generate(1)
            assert_equal(len(node.getblock(blockhash)['tx']), expected_transactions)

        print("Mining blocks...")

        test.nodes[0].generate(1)
        do_not_shield_taddr = test.nodes[0].getnewaddress()

        test.nodes[0].generate(4)
        test.sync_all()
        walletinfo = test.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], Decimal('50'))
        assert_equal(walletinfo['balance'], Decimal('0'))
        test.sync_all()
        test.nodes[2].generate(1)
        test.nodes[2].getnewaddress()
        test.nodes[2].generate(1)
        test.nodes[2].getnewaddress()
        test.nodes[2].generate(1)
        test.sync_all()
        test.nodes[1].generate(101)
        test.sync_all()
        assert_equal(test.nodes[0].getbalance(), Decimal('50'))
        assert_equal(test.nodes[1].getbalance(), Decimal('10'))
        assert_equal(test.nodes[2].getbalance(), Decimal('30'))

        # Shield the coinbase
        myzaddr = self.new_address(test, 0)
        result = test.nodes[0].z_shieldcoinbase("*", myzaddr, 0)
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        generate_and_check(test.nodes[1], 2)
        test.sync_all()

        # Prepare some UTXOs and notes for merging
        mytaddr = test.nodes[0].getnewaddress()
        mytaddr2 = test.nodes[0].getnewaddress()
        mytaddr3 = test.nodes[0].getnewaddress()
        result = test.nodes[0].z_sendmany(myzaddr, [
            {'address': do_not_shield_taddr, 'amount': Decimal('10')},
            {'address': mytaddr, 'amount': Decimal('10')},
            {'address': mytaddr2, 'amount': Decimal('10')},
            {'address': mytaddr3, 'amount': Decimal('10')},
            ], 1, 0, 'AllowRevealedRecipients')
        wait_and_assert_operationid_status(test.nodes[0], result)
        test.sync_all()
        generate_and_check(test.nodes[1], 2)
        test.sync_all()

        # Merging will fail because from arguments need to be in an array
        assert_mergetoaddress_exception(
            "JSON value is not an array as expected",
            lambda: test.nodes[0].z_mergetoaddress("notanarray", myzaddr))

        # Merging will fail when trying to spend from watch-only address
        test.nodes[2].importaddress(mytaddr)
        assert_mergetoaddress_exception(
            "Could not find any funds to merge.",
            lambda: test.nodes[2].z_mergetoaddress([mytaddr], myzaddr))

        # Merging will fail because fee is negative
        assert_mergetoaddress_exception(
            "Amount out of range",
            lambda: test.nodes[0].z_mergetoaddress(self.any_zaddr_or_utxo, myzaddr, -1))

        # Merging will fail because fee is larger than MAX_MONEY
        assert_mergetoaddress_exception(
            "Amount out of range",
            lambda: test.nodes[0].z_mergetoaddress(self.any_zaddr_or_utxo, myzaddr, Decimal('21000000.00000001')))

        # Merging will fail because fee is larger than `-maxtxfee`
        assert_mergetoaddress_exception(
            "Fee (999.00 ZEC) is greater than the maximum fee allowed by this instance (0.10 ZEC). Run zcashd with `-maxtxfee` to adjust this limit.",
            lambda: test.nodes[0].z_mergetoaddress(self.any_zaddr_or_utxo, myzaddr, 999))

        # Merging will fail because transparent limit parameter must be at least 0
        assert_mergetoaddress_exception(
            "Limit on maximum number of UTXOs cannot be negative",
            lambda: test.nodes[0].z_mergetoaddress(self.any_zaddr_or_utxo, myzaddr, Decimal('0.001'), -1))

        # Merging will fail because transparent limit parameter is absurdly large
        assert_mergetoaddress_exception(
            "JSON integer out of range",
            lambda: test.nodes[0].z_mergetoaddress(self.any_zaddr_or_utxo, myzaddr, Decimal('0.001'), 99999999999999))

        # Merging will fail because shielded limit parameter must be at least 0
        assert_mergetoaddress_exception(
            "Limit on maximum number of notes cannot be negative",
            lambda: test.nodes[0].z_mergetoaddress(self.any_zaddr_or_utxo, myzaddr, Decimal('0.001'), 50, -1))

        # Merging will fail because shielded limit parameter is absurdly large
        assert_mergetoaddress_exception(
            "JSON integer out of range",
            lambda: test.nodes[0].z_mergetoaddress(self.any_zaddr_or_utxo, myzaddr, Decimal('0.001'), 50, 99999999999999))

        # Merging will fail for this specific case where it would spend a fee and do nothing
        assert_mergetoaddress_exception(
            "Destination address is also the only source address, and all its funds are already merged.",
            lambda: test.nodes[0].z_mergetoaddress([mytaddr], mytaddr))

        # Merging will fail if we try to specify from Sprout AND Sapling
        assert_mergetoaddress_exception(
            "Cannot send from both Sprout and Sapling addresses using z_mergetoaddress",
            lambda: test.nodes[0].z_mergetoaddress(["ANY_SPROUT", "ANY_SAPLING"], mytaddr))

        # Merge UTXOs from node 0 of value 30, default fee
        result = test.nodes[0].z_mergetoaddress([mytaddr, mytaddr2, mytaddr3], myzaddr, None, None, None, None, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        generate_and_check(test.nodes[1], 2)
        test.sync_all()

        # Confirm balances and that do_not_shield_taddr containing funds of 10 was left alone
        assert_equal(test.nodes[0].getbalance(), Decimal('10'))
        assert_equal(test.nodes[0].z_getbalance(do_not_shield_taddr), Decimal('10'))
        assert_equal(test.nodes[0].z_getbalance(myzaddr), Decimal('40') - conventional_fee(4))
        assert_equal(test.nodes[1].getbalance(), Decimal('40'))
        assert_equal(test.nodes[2].getbalance(), Decimal('30'))

        # Shield all notes to another z-addr
        myzaddr2 = self.new_address(test, 0)
        result = test.nodes[0].z_mergetoaddress(self.any_zaddr, myzaddr2, 0)
        assert_equal(result["mergingUTXOs"], 0)
        assert_equal(result["remainingUTXOs"], 0)
        assert_equal(result["mergingNotes"], 2)
        assert_equal(result["remainingNotes"], 0)
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        generate_and_check(test.nodes[1], 2)
        test.sync_all()

        assert_equal(test.nodes[0].z_getbalance(myzaddr), Decimal('0'))
        assert_equal(test.nodes[0].z_getbalance(myzaddr2), Decimal('40') - conventional_fee(4))

        # Shield coinbase UTXOs from any node 2 taddr, and set fee to 0
        result = test.nodes[2].z_shieldcoinbase("*", myzaddr, 0)
        wait_and_assert_operationid_status(test.nodes[2], result['opid'])
        test.sync_all()
        generate_and_check(test.nodes[1], 2)
        test.sync_all()

        assert_equal(test.nodes[0].getbalance(), Decimal('10'))
        assert_equal(test.nodes[0].z_getbalance(myzaddr), Decimal('30'))
        assert_equal(test.nodes[0].z_getbalance(myzaddr2), Decimal('40') - conventional_fee(4))
        assert_equal(test.nodes[1].getbalance(), Decimal('60'))
        assert_equal(test.nodes[2].getbalance(), Decimal('0'))

        # Merge all notes from node 0 into a node 0 taddr, and set fee to 0
        result = test.nodes[0].z_mergetoaddress(self.any_zaddr, mytaddr, 0, None, None, None, 'AllowRevealedRecipients')
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        generate_and_check(test.nodes[1], 2)
        test.sync_all()

        assert_equal(test.nodes[0].getbalance(), Decimal('80') - conventional_fee(4))
        assert_equal(test.nodes[0].z_getbalance(do_not_shield_taddr), Decimal('10'))
        assert_equal(test.nodes[0].z_getbalance(mytaddr), Decimal('70') - conventional_fee(4))
        assert_equal(test.nodes[0].z_getbalance(myzaddr), Decimal('0'))
        assert_equal(test.nodes[0].z_getbalance(myzaddr2), Decimal('0'))
        assert_equal(test.nodes[1].getbalance(), Decimal('70'))
        assert_equal(test.nodes[2].getbalance(), Decimal('0'))

        # Merge all node 0 UTXOs together into a node 1 taddr, and set fee to 0
        test.nodes[1].getnewaddress()  # Ensure we have an empty address
        n1taddr = test.nodes[1].getnewaddress()
        result = test.nodes[0].z_mergetoaddress(["ANY_TADDR"], n1taddr, 0, None, None, None, 'AllowFullyTransparent')
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        generate_and_check(test.nodes[1], 2)
        test.sync_all()
        assert_equal(0, len(test.nodes[0].z_listunspent(0)))

        assert_equal(test.nodes[0].getbalance(), Decimal('0'))
        assert_equal(test.nodes[0].z_getbalance(do_not_shield_taddr), Decimal('0'))
        assert_equal(test.nodes[0].z_getbalance(mytaddr), Decimal('0'))
        assert_equal(test.nodes[0].z_getbalance(myzaddr), Decimal('0'))
        assert_equal(test.nodes[1].getbalance(), Decimal('160') - conventional_fee(4))
        assert_equal(test.nodes[1].z_getbalance(n1taddr), Decimal('80') - conventional_fee(4))
        assert_equal(test.nodes[2].getbalance(), Decimal('0'))

        # Generate 5 regular UTXOs on node 0, and 20 regular UTXOs on node 2.
        mytaddr = test.nodes[0].getnewaddress()
        n2taddr = test.nodes[2].getnewaddress()
        test.nodes[1].generate(20)
        test.sync_all()
        for i in range(5):
            test.nodes[1].sendtoaddress(mytaddr, Decimal('1'))
        for i in range(20):
            test.nodes[1].sendtoaddress(n2taddr, Decimal('1'))
        generate_and_check(test.nodes[1], 26)
        test.sync_all()

        # This z_mergetoaddress and the one below result in two notes in myzaddr.
        result = test.nodes[0].z_mergetoaddress([mytaddr], myzaddr, DEFAULT_FEE, None, None, None, 'AllowRevealedSenders')
        assert_equal(result["mergingUTXOs"], 5)
        assert_equal(result["remainingUTXOs"], 0)
        assert_equal(result["mergingNotes"], 0)
        assert_equal(result["remainingNotes"], 0)
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])

        # Verify maximum number of UTXOs is not limited when the limit parameter is set to 0.
        result = test.nodes[2].z_mergetoaddress([n2taddr], myzaddr, DEFAULT_FEE, 0, None, None, 'AllowRevealedSenders')
        assert_equal(result["mergingUTXOs"], 20)
        assert_equal(result["remainingUTXOs"], 0)
        assert_equal(result["mergingNotes"], 0)
        assert_equal(result["remainingNotes"], 0)
        wait_and_assert_operationid_status(test.nodes[2], result['opid'])
        test.sync_all()
        generate_and_check(test.nodes[1], 3)
        test.sync_all()
        assert_equal(2, len(test.nodes[0].z_listunspent()))

        # Verify maximum number of UTXOs which node 0 can shield is set by default limit parameter of 50
        mytaddr = test.nodes[0].getnewaddress()
        for i in range(100):
            test.nodes[1].sendtoaddress(mytaddr, 1)
        generate_and_check(test.nodes[1], 101)
        test.sync_all()
        result = test.nodes[0].z_mergetoaddress([mytaddr], myzaddr, conventional_fee(52), None, None, None, 'AllowRevealedSenders')
        assert_equal(result["mergingUTXOs"], 50)
        assert_equal(result["remainingUTXOs"], 50)
        assert_equal(result["mergingNotes"], 0)
        # Remaining notes are only counted if we are trying to merge any notes
        assert_equal(result["remainingNotes"], 0)
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])

        assert_equal(2, len(test.nodes[0].z_listunspent()))
        assert_equal(3, len(test.nodes[0].z_listunspent(0)))

        # Verify maximum number of UTXOs which node 0 can shield can be set by the limit parameter
        result = test.nodes[0].z_mergetoaddress([mytaddr], myzaddr, conventional_fee(35), 33, None, None, 'AllowRevealedSenders')
        assert_equal(result["mergingUTXOs"], 33)
        assert_equal(result["remainingUTXOs"], 17)
        assert_equal(result["mergingNotes"], 0)
        # Remaining notes are only counted if we are trying to merge any notes
        assert_equal(result["remainingNotes"], 0)
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        generate_and_check(test.nodes[1], 3)
        test.sync_all()
        assert_equal(4, len(test.nodes[0].z_listunspent()))

        # NB: We can’t yet merge from UAs, so ensure we’re not before running these cases
        if (myzaddr[0] != 'u'):
            # Also check that we can set off a second merge before the first one is complete
            result1 = test.nodes[0].z_mergetoaddress([myzaddr], myzaddr, DEFAULT_FEE, 50, 2)

            # First merge should select from all notes
            assert_equal(result1["mergingUTXOs"], 0)
            # Remaining UTXOs are only counted if we are trying to merge any UTXOs
            assert_equal(result1["remainingUTXOs"], 0)
            assert_equal(result1["mergingNotes"], 2)
            assert_equal(result1["remainingNotes"], 2)

            # Second merge should ignore locked notes
            result2 = test.nodes[0].z_mergetoaddress([myzaddr], myzaddr, DEFAULT_FEE, 50, 2)
            assert_equal(result2["mergingUTXOs"], 0)
            assert_equal(result2["remainingUTXOs"], 0)
            assert_equal(result2["mergingNotes"], 2)
            assert_equal(result2["remainingNotes"], 0)
            wait_and_assert_operationid_status(test.nodes[0], result1['opid'])
            wait_and_assert_operationid_status(test.nodes[0], result2['opid'])

            test.sync_all()
            generate_and_check(test.nodes[1], 3)
            test.sync_all()

            # Shield both UTXOs and notes to a z-addr
            result = test.nodes[0].z_mergetoaddress(self.any_zaddr_or_utxo, myzaddr, DEFAULT_FEE, 10, 2, None, 'AllowRevealedSenders')
            assert_equal(result["mergingUTXOs"], 10)
            assert_equal(result["remainingUTXOs"], 7)
            assert_equal(result["mergingNotes"], 2)
            assert_equal(result["remainingNotes"], 0)
            wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        else:
            # Shield both UTXOs and notes to a z-addr
            result = test.nodes[0].z_mergetoaddress(self.any_zaddr_or_utxo, myzaddr, DEFAULT_FEE, 10, 2, None, 'AllowRevealedSenders')
            assert_equal(result["mergingUTXOs"], 10)
            assert_equal(result["remainingUTXOs"], 7)
            assert_equal(result["mergingNotes"], 2)
            assert_equal(result["remainingNotes"], 2)
            wait_and_assert_operationid_status(test.nodes[0], result['opid'])

        test.sync_all()
        generate_and_check(test.nodes[1], 2)
        test.sync_all()
