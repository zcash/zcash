#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
Integration test for the soft fork that temporarily disables Orchard actions.

The soft fork activates at a configured height (here 220, set via
`-regtesttemporaryorcharddisablingsoftforkheight`). It enforces two things:

  1. At the boundary: when the last block *before* the activation height is
     connected, `ConnectTip` calls `CTxMemPool::removeContainingOrchard`, which
     drops every mempool transaction containing Orchard actions while leaving
     all other transactions untouched.

  2. After activation: `ContextualCheckTransaction` rejects any transaction
     containing Orchard actions with `bad-tx-has-orchard-actions`. This applies
     both to mempool acceptance and to transactions inside a connected block.

Node layout:

  - Nodes 0, 1, 2 enforce the soft fork at height 220.
  - Node 3 does NOT enforce the soft fork (the regtest default is no
    activation). It is used to build an otherwise-valid block that contains a
    transaction with Orchard actions, which an enforcing node must reject.
"""

from io import BytesIO

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.blocktools import create_block
from test_framework.mininode import COIN, CTransaction
from test_framework.util import (
    NU6_1_BRANCH_ID,
    NU6_2_BRANCH_ID,
    assert_equal,
    assert_raises_message,
    assert_true,
    connect_nodes_bi,
    get_coinbase_address,
    hex_str_to_bytes,
    nuparams,
    start_nodes,
    sync_blocks,
    sync_mempools,
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import conventional_fee

from decimal import Decimal

# The height at which the soft fork that disables Orchard actions activates.
ORCHARD_DISABLED_HEIGHT = 220

# Test the soft fork disabling Orchard
class SoftForkDisablingOrchardTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4

    def setup_nodes(self):
        common = [
            nuparams(NU6_1_BRANCH_ID, 210),
            nuparams(NU6_2_BRANCH_ID, 230),
        ]
        enforcing = common + [
            '-regtesttemporaryorcharddisablingsoftforkheight=%d' % ORCHARD_DISABLED_HEIGHT,
        ]
        # Node 3 does not enforce the soft fork, so that it can build a block
        # containing Orchard actions for the block-rejection test. The regtest
        # default is no activation, so we simply omit the argument for it.
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[
            enforcing,  # node 0
            enforcing,  # node 1
            enforcing,  # node 2
            common,     # node 3 (soft fork not enforced)
        ])

    def build_block_from_template(self, node):
        """Build the canonical next block from `node`'s current block template.

        The block includes every transaction the node's template selects from
        its mempool. When `node` does not enforce the Orchard soft fork, its
        template will include a mempool transaction containing Orchard actions,
        producing an otherwise-valid block that an enforcing node must reject.
        """
        gbt = node.getblocktemplate()

        prevhash = int(gbt['previousblockhash'], 16)
        nTime = gbt['mintime']
        nBits = int(gbt['bits'], 16)
        block_commitments_hash = int(gbt['defaultroots']['blockcommitmentshash'], 16)

        f = BytesIO(hex_str_to_bytes(gbt['coinbasetxn']['data']))
        coinbase = CTransaction()
        coinbase.deserialize(f)
        coinbase.calc_sha256()

        block = create_block(prevhash, coinbase, nTime, nBits, block_commitments_hash)

        for gbt_tx in gbt['transactions']:
            f = BytesIO(hex_str_to_bytes(gbt_tx['data']))
            tx = CTransaction()
            tx.deserialize(f)
            tx.calc_sha256()
            block.vtx.append(tx)
        block.hashMerkleRoot = int(gbt['defaultroots']['merkleroot'], 16)
        block.hashAuthDataRoot = int(gbt['defaultroots']['authdataroot'], 16)
        block.solve()
        block.calc_sha256()
        return block

    def run_test(self):
        # Sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Get a new orchard-only unified address on node 1; this is the source
        # of the Orchard note we will spend later.
        acct1 = self.nodes[1].z_getnewaccount()['account']
        addrRes1 = self.nodes[1].z_getaddressforaccount(acct1, ['orchard'])
        assert_equal(acct1, addrRes1['account'])
        assert_equal(addrRes1['receiver_types'], ['orchard'])
        ua1 = addrRes1['address']

        # Verify that we have only an Orchard component
        receiver_types = self.nodes[0].z_listunifiedreceivers(ua1)
        assert_equal(set(['orchard']), set(receiver_types))

        # Verify balance
        assert_equal({'pools': {}, 'minimum_confirmations': 1}, self.nodes[1].z_getbalanceforaccount(acct1))

        # Destination addresses used by the two transactions we will create
        # in the mempool just before the soft fork activates:
        #
        # - `ua0_orchard` receives the Orchard-containing transaction (which
        #   must be dropped at the boundary).
        # - `ua0_sapling` receives the non-Orchard transaction (which must be
        #   left alone). Shielding a coinbase to Sapling produces a v5
        #   transaction with no Orchard actions.
        acct0_orchard = self.nodes[0].z_getnewaccount()['account']
        ua0_orchard = self.nodes[0].z_getaddressforaccount(acct0_orchard, ['orchard'])['address']
        acct0_sapling = self.nodes[0].z_getnewaccount()['account']
        ua0_sapling = self.nodes[0].z_getaddressforaccount(acct0_sapling, ['sapling'])['address']

        # Activate NU6.1
        self.sync_all()
        self.nodes[0].generate(10)
        self.sync_all()

        # Node 0 shields some funds
        # t-coinbase -> Orchard
        coinbase_fee = conventional_fee(3)
        coinbase_amount = Decimal('10') - coinbase_fee
        recipients = [{"address": ua1, "amount": coinbase_amount}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, coinbase_fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], myopid)
        acct1_balance = coinbase_amount

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(
                {'pools': {'orchard': {'valueZat': acct1_balance * COIN}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        # Advance to the block immediately below the one whose connection
        # triggers the boundary cleanup. The cleanup runs in `ConnectTip` when
        # the block at height `ORCHARD_DISABLED_HEIGHT - 1` is connected, so we
        # must have the relevant transactions in the mempool when that block is
        # connected. Stop one block earlier than that.
        self.nodes[0].generate(ORCHARD_DISABLED_HEIGHT - 2 - self.nodes[0].getblockcount())
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), ORCHARD_DISABLED_HEIGHT - 2)

        self.test_mempool_cleared_at_boundary(ua1, ua0_orchard, ua0_sapling)
        self.test_rejected_after_activation()
        self.test_accepted_after_nu6_2(ua1, ua0_orchard)

    def test_mempool_cleared_at_boundary(self, ua1, ua0_orchard, ua0_sapling):
        """When the last block before the soft fork is connected, Orchard
        transactions are dropped from the mempool while others are left alone.

        We split the network so that one half (nodes 0/1) holds both an
        Orchard transaction and a non-Orchard transaction in its mempool, while
        the other half (nodes 2/3) mines the boundary block without them. After
        rejoining, the half holding the transactions connects the boundary
        block, which must drop only the Orchard transaction.
        """
        print("Testing that Orchard transactions are dropped from the mempool at the boundary")

        # Split into halves 0/1 and 2/3, both at height ORCHARD_DISABLED_HEIGHT - 2.
        self.split_network()
        assert_equal(self.nodes[0].getblockcount(), ORCHARD_DISABLED_HEIGHT - 2)
        assert_equal(self.nodes[2].getblockcount(), ORCHARD_DISABLED_HEIGHT - 2)

        # In the 0/1 half, create a transaction containing Orchard actions by
        # spending the funded Orchard note (Orchard -> Orchard). The soft fork
        # is not yet active at the next height (ORCHARD_DISABLED_HEIGHT - 1), so
        # this is accepted into the mempool.
        orchard_opid = self.nodes[1].z_sendmany(
            ua1, [{"address": ua0_orchard, "amount": Decimal('1')}], 1, None)
        orchard_txid = wait_and_assert_operationid_status(self.nodes[1], orchard_opid)

        # Capture the raw transaction now, while it is still in the mempool, so
        # we can re-broadcast it after activation.
        orchard_tx_hex = self.nodes[1].getrawtransaction(orchard_txid)

        # Create a non-Orchard transaction in the same half (coinbase ->
        # Sapling). This produces a v5 transaction with a Sapling output and no
        # Orchard actions, so it must survive the boundary. Shielding coinbase
        # funds does not allow any change, so we send the full coinbase value
        # minus the fee.
        sapling_fee = conventional_fee(3)
        sapling_amount = Decimal('10') - sapling_fee
        sapling_opid = self.nodes[0].z_sendmany(
            get_coinbase_address(self.nodes[0]),
            [{"address": ua0_sapling, "amount": sapling_amount}], 1, sapling_fee, 'AllowRevealedSenders')
        sapling_txid = wait_and_assert_operationid_status(self.nodes[0], sapling_opid)

        # Both transactions should now be in both nodes of the 0/1 half.
        sync_mempools(self.nodes[:2])
        assert_true(orchard_txid in self.nodes[0].getrawmempool(),
                    "Orchard transaction should be in the mempool before the boundary")
        assert_true(sapling_txid in self.nodes[0].getrawmempool(),
                    "non-Orchard transaction should be in the mempool before the boundary")

        # In the 2/3 half, mine the boundary block (height
        # ORCHARD_DISABLED_HEIGHT - 1). This half has an empty mempool, so the
        # block contains neither of our transactions.
        self.nodes[2].generate(1)
        sync_blocks(self.nodes[2:])
        assert_equal(self.nodes[2].getblockcount(), ORCHARD_DISABLED_HEIGHT - 1)

        # Rejoin the halves. The 0/1 half adopts the boundary block from the
        # 2/3 half; connecting it triggers the mempool cleanup.
        connect_nodes_bi(self.nodes, 1, 2)
        self.is_network_split = False
        sync_blocks(self.nodes)
        assert_equal(self.nodes[0].getblockcount(), ORCHARD_DISABLED_HEIGHT - 1)

        # The Orchard transaction has been dropped; the non-Orchard one remains.
        mempool0 = self.nodes[0].getrawmempool()
        assert_true(orchard_txid not in mempool0,
                    "Orchard transaction should have been dropped at the boundary")
        assert_true(sapling_txid in mempool0,
                    "non-Orchard transaction should have been left in the mempool")

        # The same holds on node 1, which also crossed the boundary.
        mempool1 = self.nodes[1].getrawmempool()
        assert_true(orchard_txid not in mempool1,
                    "Orchard transaction should have been dropped at the boundary (node 1)")
        assert_true(sapling_txid in mempool1,
                    "non-Orchard transaction should have been left in the mempool (node 1)")

        # Save state needed by the post-activation tests.
        self.orchard_tx_hex = orchard_tx_hex
        self.orchard_txid = orchard_txid

    def test_rejected_after_activation(self):
        """Once the soft fork has activated, the mempool rejects transactions
        containing Orchard actions, and a block containing such a transaction
        is rejected.
        """
        print("Testing that Orchard transactions are rejected after activation")

        # The chain is already synced at height ORCHARD_DISABLED_HEIGHT - 1 from
        # the boundary test. The mempool is already be rejecting transactions
        # that contain Orchard actions: re-broadcasting the Orchard transaction
        # we saved earlier (its input note is still unspent, as it was never
        # mined) is rejected by an enforcing node.
        assert_raises_message(
            JSONRPCException,
            "bad-tx-has-orchard-actions",
            self.nodes[0].sendrawtransaction,
            self.orchard_tx_hex)

        # Block rejection: node 3 does not enforce the soft fork, so it accepts
        # the Orchard transaction into its mempool and will include it in a
        # block template.
        node3_orchard_txid = self.nodes[3].sendrawtransaction(self.orchard_tx_hex)
        assert_true(node3_orchard_txid in self.nodes[3].getrawmempool(),
                    "node 3 (not enforcing) should accept the Orchard transaction")

        # Build an otherwise-valid block on node 3 containing the Orchard
        # transaction, then submit it to an enforcing node.
        block = self.build_block_from_template(self.nodes[3])
        assert_true(len(block.vtx) >= 2,
                    "block template should include the Orchard transaction")

        height_before = self.nodes[0].getblockcount()
        best_before = self.nodes[0].getbestblockhash()

        assert_equal(
            self.nodes[0].submitblock(block.serialize().hex()),
            "bad-tx-has-orchard-actions")

        # The enforcing node did not advance onto the rejected block.
        assert_equal(self.nodes[0].getblockcount(), height_before)
        assert_equal(self.nodes[0].getbestblockhash(), best_before)

        # Mine the activation block on node 0. This also mines the surviving
        # non-Orchard transaction, so after the block propagates, the mempool is
        # empty for every node following the soft fork. However, node 3 still
        # has the Orchard tx we submitted above, so the mempools cannot converge
        # (only the blocks).
        self.nodes[0].generate(1)
        self.sync_all(False)
        assert_equal(self.nodes[0].getblockcount(), ORCHARD_DISABLED_HEIGHT)

        # Confirm that mempool rejection is still enforced.
        assert_raises_message(
            JSONRPCException,
            "bad-tx-has-orchard-actions",
            self.nodes[0].sendrawtransaction,
            self.orchard_tx_hex)

        # Confirm that block rejection is still enforced
        block = self.build_block_from_template(self.nodes[3])
        assert_true(len(block.vtx) >= 2,
                    "block template should include the Orchard transaction")

        height_before = self.nodes[0].getblockcount()
        best_before = self.nodes[0].getbestblockhash()

        assert_equal(
            self.nodes[0].submitblock(block.serialize().hex()),
            "bad-tx-has-orchard-actions")

        # The enforcing node did not advance onto the rejected block.
        assert_equal(self.nodes[0].getblockcount(), height_before)
        assert_equal(self.nodes[0].getbestblockhash(), best_before)

    def test_accepted_after_nu6_2(self, ua1, ua0_orchard):
        """Once NU 6.2 has activated, the Orchard rejection logic from the soft
        fork is disabled, the mempool accepts transactions containing Orchard
        actions, and a block containing such a transaction is accepted.
        """
        print("Testing that Orchard transactions are accepted after NU 6.2")

        # Activate NU 6.2
        self.nodes[0].generate(10)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 230)

        # The transaction rejected earlier is still rejected, but now because it
        # is invalid for this consensus branch ID (and expired due to how zcashd
        # constructs txs).
        assert_raises_message(
            JSONRPCException,
            "tx-expiring-soon",
            self.nodes[0].sendrawtransaction,
            self.orchard_tx_hex)

        # Try again to spend the funded Orchard note (Orchard -> Orchard).
        orchard_opid = self.nodes[1].z_sendmany(
            ua1, [{"address": ua0_orchard, "amount": Decimal('1')}], 1, None)
        orchard_txid = wait_and_assert_operationid_status(self.nodes[1], orchard_opid)

        # Other nodes now accept it.
        sync_mempools(self.nodes)
        assert_true(orchard_txid in self.nodes[0].getrawmempool(),
                    "Orchard transaction should be in the mempool after NU 6.2 activation")

if __name__ == '__main__':
    SoftForkDisablingOrchardTest().main()
