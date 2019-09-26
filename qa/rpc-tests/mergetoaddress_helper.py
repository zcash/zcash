# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Common code for testing z_mergetoaddress before and after sapling activation
#

from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, connect_nodes_bi, fail, \
    initialize_chain_clean, start_node, sync_blocks, sync_mempools, \
    wait_and_assert_operationid_status

from decimal import Decimal


def assert_mergetoaddress_exception(expected_error_msg, merge_to_address_lambda):
    try:
        merge_to_address_lambda()
    except JSONRPCException as e:
        assert_equal(expected_error_msg, e.error['message'])
    except Exception as e:
        fail("Expected JSONRPCException. Found %s" % repr(e))
    else:
        fail("Expected exception: %s" % expected_error_msg)


class MergeToAddressHelper:

    def __init__(self, addr_type, any_zaddr, utxos_to_generate, utxos_in_tx1, utxos_in_tx2):
        self.addr_type = addr_type
        self.any_zaddr = [any_zaddr]
        self.any_zaddr_or_utxo = [any_zaddr, "ANY_TADDR"]
        # utxos_to_generate, utxos_in_tx1, utxos_in_tx2 have to do with testing transaction size limits
        self.utxos_to_generate = utxos_to_generate
        self.utxos_in_tx1 = utxos_in_tx1
        self.utxos_in_tx2 = utxos_in_tx2

    def setup_chain(self, test):
        print("Initializing test directory "+test.options.tmpdir)
        initialize_chain_clean(test.options.tmpdir, 4)

    def setup_network(self, test, additional_args=[]):
        args = ['-debug=zrpcunsafe', '-experimentalfeatures', '-zmergetoaddress']
        args += additional_args
        test.nodes = []
        test.nodes.append(start_node(0, test.options.tmpdir, args))
        test.nodes.append(start_node(1, test.options.tmpdir, args))
        args2 = ['-debug=zrpcunsafe', '-experimentalfeatures', '-zmergetoaddress', '-mempooltxinputlimit=7']
        args2 += additional_args
        test.nodes.append(start_node(2, test.options.tmpdir, args2))
        connect_nodes_bi(test.nodes, 0, 1)
        connect_nodes_bi(test.nodes, 1, 2)
        connect_nodes_bi(test.nodes, 0, 2)
        test.is_network_split = False
        test.sync_all()

    def run_test(self, test):
        print "Mining blocks..."

        test.nodes[0].generate(1)
        do_not_shield_taddr = test.nodes[0].getnewaddress()

        test.nodes[0].generate(4)
        walletinfo = test.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 50)
        assert_equal(walletinfo['balance'], 0)
        test.sync_all()
        test.nodes[2].generate(1)
        test.nodes[2].getnewaddress()
        test.nodes[2].generate(1)
        test.nodes[2].getnewaddress()
        test.nodes[2].generate(1)
        test.sync_all()
        test.nodes[1].generate(101)
        test.sync_all()
        assert_equal(test.nodes[0].getbalance(), 50)
        assert_equal(test.nodes[1].getbalance(), 10)
        assert_equal(test.nodes[2].getbalance(), 30)

        # Shield the coinbase
        myzaddr = test.nodes[0].z_getnewaddress(self.addr_type)
        result = test.nodes[0].z_shieldcoinbase("*", myzaddr, 0)
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        test.nodes[1].generate(1)
        test.sync_all()

        # Prepare some UTXOs and notes for merging
        mytaddr = test.nodes[0].getnewaddress()
        mytaddr2 = test.nodes[0].getnewaddress()
        mytaddr3 = test.nodes[0].getnewaddress()
        result = test.nodes[0].z_sendmany(myzaddr, [
            {'address': do_not_shield_taddr, 'amount': 10},
            {'address': mytaddr, 'amount': 10},
            {'address': mytaddr2, 'amount': 10},
            {'address': mytaddr3, 'amount': 10},
            ], 1, 0)
        wait_and_assert_operationid_status(test.nodes[0], result)
        test.sync_all()
        test.nodes[1].generate(1)
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

        # Merging will fail because fee is larger than sum of UTXOs
        assert_mergetoaddress_exception(
            "Insufficient funds, have 50.00, which is less than miners fee 999.00",
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

        # Merge UTXOs from node 0 of value 30, standard fee of 0.00010000
        result = test.nodes[0].z_mergetoaddress([mytaddr, mytaddr2, mytaddr3], myzaddr)
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        test.nodes[1].generate(1)
        test.sync_all()

        # Confirm balances and that do_not_shield_taddr containing funds of 10 was left alone
        assert_equal(test.nodes[0].getbalance(), 10)
        assert_equal(test.nodes[0].z_getbalance(do_not_shield_taddr), Decimal('10.0'))
        assert_equal(test.nodes[0].z_getbalance(myzaddr), Decimal('39.99990000'))
        assert_equal(test.nodes[1].getbalance(), 40)
        assert_equal(test.nodes[2].getbalance(), 30)

        # Shield all notes to another z-addr
        myzaddr2 = test.nodes[0].z_getnewaddress(self.addr_type)
        result = test.nodes[0].z_mergetoaddress(self.any_zaddr, myzaddr2, 0)
        assert_equal(result["mergingUTXOs"], Decimal('0'))
        assert_equal(result["remainingUTXOs"], Decimal('0'))
        assert_equal(result["mergingNotes"], Decimal('2'))
        assert_equal(result["remainingNotes"], Decimal('0'))
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        blockhash = test.nodes[1].generate(1)
        test.sync_all()

        assert_equal(len(test.nodes[0].getblock(blockhash[0])['tx']), 2)
        assert_equal(test.nodes[0].z_getbalance(myzaddr), 0)
        assert_equal(test.nodes[0].z_getbalance(myzaddr2), Decimal('39.99990000'))

        # Shield coinbase UTXOs from any node 2 taddr, and set fee to 0
        result = test.nodes[2].z_shieldcoinbase("*", myzaddr, 0)
        wait_and_assert_operationid_status(test.nodes[2], result['opid'])
        test.sync_all()
        test.nodes[1].generate(1)
        test.sync_all()

        assert_equal(test.nodes[0].getbalance(), 10)
        assert_equal(test.nodes[0].z_getbalance(myzaddr), Decimal('30'))
        assert_equal(test.nodes[0].z_getbalance(myzaddr2), Decimal('39.99990000'))
        assert_equal(test.nodes[1].getbalance(), 60)
        assert_equal(test.nodes[2].getbalance(), 0)

        # Merge all notes from node 0 into a node 0 taddr, and set fee to 0
        result = test.nodes[0].z_mergetoaddress(self.any_zaddr, mytaddr, 0)
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        test.nodes[1].generate(1)
        test.sync_all()

        assert_equal(test.nodes[0].getbalance(), Decimal('79.99990000'))
        assert_equal(test.nodes[0].z_getbalance(do_not_shield_taddr), Decimal('10.0'))
        assert_equal(test.nodes[0].z_getbalance(mytaddr), Decimal('69.99990000'))
        assert_equal(test.nodes[0].z_getbalance(myzaddr), 0)
        assert_equal(test.nodes[0].z_getbalance(myzaddr2), 0)
        assert_equal(test.nodes[1].getbalance(), 70)
        assert_equal(test.nodes[2].getbalance(), 0)

        # Merge all node 0 UTXOs together into a node 1 taddr, and set fee to 0
        test.nodes[1].getnewaddress()  # Ensure we have an empty address
        n1taddr = test.nodes[1].getnewaddress()
        result = test.nodes[0].z_mergetoaddress(["ANY_TADDR"], n1taddr, 0)
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        test.sync_all()
        test.nodes[1].generate(1)
        test.sync_all()

        assert_equal(test.nodes[0].getbalance(), 0)
        assert_equal(test.nodes[0].z_getbalance(do_not_shield_taddr), 0)
        assert_equal(test.nodes[0].z_getbalance(mytaddr), 0)
        assert_equal(test.nodes[0].z_getbalance(myzaddr), 0)
        assert_equal(test.nodes[1].getbalance(), Decimal('159.99990000'))
        assert_equal(test.nodes[1].z_getbalance(n1taddr), Decimal('79.99990000'))
        assert_equal(test.nodes[2].getbalance(), 0)

        # Generate self.utxos_to_generate regular UTXOs on node 0, and 20 regular UTXOs on node 2
        mytaddr = test.nodes[0].getnewaddress()
        n2taddr = test.nodes[2].getnewaddress()
        test.nodes[1].generate(1000)
        test.sync_all()
        for i in range(self.utxos_to_generate):
            test.nodes[1].sendtoaddress(mytaddr, 1)
        for i in range(20):
            test.nodes[1].sendtoaddress(n2taddr, 1)
        test.nodes[1].generate(1)
        test.sync_all()

        # Merging the UTXOs will conditionally occur over two transactions, since max tx size is 100,000 bytes before Sapling and 2,000,000 after.
        # We don't verify mergingTransparentValue as UTXOs are not selected in any specific order, so value can change on each test run.
        # We set an unrealistically high limit parameter of 99999, to verify that max tx size will constrain the number of UTXOs.
        result = test.nodes[0].z_mergetoaddress([mytaddr], myzaddr, 0, 99999)
        assert_equal(result["mergingUTXOs"], self.utxos_in_tx1)
        assert_equal(result["remainingUTXOs"], self.utxos_in_tx2)
        assert_equal(result["mergingNotes"], Decimal('0'))
        assert_equal(result["mergingShieldedValue"], Decimal('0'))
        assert_equal(result["remainingNotes"], Decimal('0'))
        assert_equal(result["remainingShieldedValue"], Decimal('0'))
        remainingTransparentValue = result["remainingTransparentValue"]
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])

        # For sapling we do not check that this occurs over two transactions because of the time that it would take
        if self.utxos_in_tx2 > 0:
            # Verify that UTXOs are locked (not available for selection) by queuing up another merging operation
            result = test.nodes[0].z_mergetoaddress([mytaddr], myzaddr, 0, 0)
            assert_equal(result["mergingUTXOs"], self.utxos_in_tx2)
            assert_equal(result["mergingTransparentValue"], Decimal(remainingTransparentValue))
            assert_equal(result["remainingUTXOs"], Decimal('0'))
            assert_equal(result["remainingTransparentValue"], Decimal('0'))
            assert_equal(result["mergingNotes"], Decimal('0'))
            assert_equal(result["mergingShieldedValue"], Decimal('0'))
            assert_equal(result["remainingNotes"], Decimal('0'))
            assert_equal(result["remainingShieldedValue"], Decimal('0'))
            wait_and_assert_operationid_status(test.nodes[0], result['opid'])

        # sync_all() invokes sync_mempool() but node 2's mempool limit will cause tx1 and tx2 to be rejected.
        # So instead, we sync on blocks and mempool for node 0 and node 1, and after a new block is generated
        # which mines tx1 and tx2, all nodes will have an empty mempool which can then be synced.
        sync_blocks(test.nodes[:2])
        sync_mempools(test.nodes[:2])
        # Generate enough blocks to ensure all transactions are mined
        while test.nodes[1].getmempoolinfo()['size'] > 0:
            test.nodes[1].generate(1)
        test.sync_all()

        # Verify maximum number of UTXOs which node 2 can shield is not limited
        # when the limit parameter is set to 0.
        expected_to_merge = 20
        expected_remaining = 0

        result = test.nodes[2].z_mergetoaddress([n2taddr], myzaddr, Decimal('0.0001'), 0)
        assert_equal(result["mergingUTXOs"], expected_to_merge)
        assert_equal(result["remainingUTXOs"], expected_remaining)
        assert_equal(result["mergingNotes"], Decimal('0'))
        assert_equal(result["remainingNotes"], Decimal('0'))
        wait_and_assert_operationid_status(test.nodes[2], result['opid'])
        test.sync_all()
        test.nodes[1].generate(1)
        test.sync_all()

        # Verify maximum number of UTXOs which node 0 can shield is set by default limit parameter of 50
        mytaddr = test.nodes[0].getnewaddress()
        for i in range(100):
            test.nodes[1].sendtoaddress(mytaddr, 1)
        test.nodes[1].generate(1)
        test.sync_all()
        result = test.nodes[0].z_mergetoaddress([mytaddr], myzaddr, Decimal('0.0001'))
        assert_equal(result["mergingUTXOs"], Decimal('50'))
        assert_equal(result["remainingUTXOs"], Decimal('50'))
        assert_equal(result["mergingNotes"], Decimal('0'))
        # Remaining notes are only counted if we are trying to merge any notes
        assert_equal(result["remainingNotes"], Decimal('0'))
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])

        # Verify maximum number of UTXOs which node 0 can shield can be set by the limit parameter
        result = test.nodes[0].z_mergetoaddress([mytaddr], myzaddr, Decimal('0.0001'), 33)
        assert_equal(result["mergingUTXOs"], Decimal('33'))
        assert_equal(result["remainingUTXOs"], Decimal('17'))
        assert_equal(result["mergingNotes"], Decimal('0'))
        # Remaining notes are only counted if we are trying to merge any notes
        assert_equal(result["remainingNotes"], Decimal('0'))
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        # Don't sync node 2 which rejects the tx due to its mempooltxinputlimit
        sync_blocks(test.nodes[:2])
        sync_mempools(test.nodes[:2])
        test.nodes[1].generate(1)
        test.sync_all()

        # Verify maximum number of notes which node 0 can shield can be set by the limit parameter
        # Also check that we can set off a second merge before the first one is complete

        # myzaddr will have 5 notes if testing before to Sapling activation and 4 otherwise
        num_notes = len(test.nodes[0].z_listunspent(0))
        result1 = test.nodes[0].z_mergetoaddress([myzaddr], myzaddr, 0.0001, 50, 2)
        result2 = test.nodes[0].z_mergetoaddress([myzaddr], myzaddr, 0.0001, 50, 2)

        # First merge should select from all notes
        assert_equal(result1["mergingUTXOs"], Decimal('0'))
        # Remaining UTXOs are only counted if we are trying to merge any UTXOs
        assert_equal(result1["remainingUTXOs"], Decimal('0'))
        assert_equal(result1["mergingNotes"], Decimal('2'))
        assert_equal(result1["remainingNotes"], num_notes - 2)

        # Second merge should ignore locked notes
        assert_equal(result2["mergingUTXOs"], Decimal('0'))
        assert_equal(result2["remainingUTXOs"], Decimal('0'))
        assert_equal(result2["mergingNotes"], Decimal('2'))
        assert_equal(result2["remainingNotes"], num_notes - 4)
        wait_and_assert_operationid_status(test.nodes[0], result1['opid'])
        wait_and_assert_operationid_status(test.nodes[0], result2['opid'])

        test.sync_all()
        test.nodes[1].generate(1)
        test.sync_all()

        # Shield both UTXOs and notes to a z-addr
        result = test.nodes[0].z_mergetoaddress(self.any_zaddr_or_utxo, myzaddr, 0, 10, 2)
        assert_equal(result["mergingUTXOs"], Decimal('10'))
        assert_equal(result["remainingUTXOs"], Decimal('7'))
        assert_equal(result["mergingNotes"], Decimal('2'))
        assert_equal(result["remainingNotes"], num_notes - 4)
        wait_and_assert_operationid_status(test.nodes[0], result['opid'])
        # Don't sync node 2 which rejects the tx due to its mempooltxinputlimit
        sync_blocks(test.nodes[:2])
        sync_mempools(test.nodes[:2])
        test.nodes[1].generate(1)
        test.sync_all()
