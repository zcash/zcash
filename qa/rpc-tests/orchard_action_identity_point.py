#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Regression test for the Orchard identity-point bugs:
#
# 1. An Orchard action with rk encoding the Pallas identity point (all zeros)
#    would cause a crash in proof verification. This is now rejected by
#    CheckTransactionWithoutProofVerification before proofs are checked.
#
# 2. The Zcash protocol specification (section 5.4.9.4) requires that the
#    ephemeral public key (epk) in each Orchard action encode a non-identity
#    Pallas point. Zebra enforced this but zcashd did not, creating a potential
#    consensus split. This is now also rejected by
#    CheckTransactionWithoutProofVerification.
#
# The test creates a valid Orchard transaction, locates the rk and epk fields
# by byte offset in the serialized v5 format (ZIP 225), zeroes them out, and
# verifies that the modified transaction is rejected with the expected error.

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID, nuparams,
    assert_equal, assert_raises_message,
    bytes_to_hex_str, hex_str_to_bytes,
    get_coinbase_address,
    start_nodes, wait_and_assert_operationid_status,
)
from test_framework.zip317 import ZIP_317_FEE, conventional_fee

from decimal import Decimal
import struct

# Per ZIP 225, each Orchard action is 820 bytes:
#   cv(32) + nullifier(32) + rk(32) + cmx(32) + ephemeralKey(32) + encCiphertext(580) + outCiphertext(80)
RK_OFFSET_IN_ACTION = 64     # cv(32) + nullifier(32)
EPK_OFFSET_IN_ACTION = 128   # cv(32) + nullifier(32) + rk(32) + cmx(32)
IDENTITY_BYTES = b'\x00' * 32


def read_compact_size(data, pos):
    """Read a Bitcoin CompactSize uint, returning (value, new_pos)."""
    first = data[pos]
    if first < 253:
        return first, pos + 1
    elif first == 253:
        return struct.unpack_from('<H', data, pos + 1)[0], pos + 3
    elif first == 254:
        return struct.unpack_from('<I', data, pos + 1)[0], pos + 5
    else:
        return struct.unpack_from('<Q', data, pos + 1)[0], pos + 9

def skip_empty_vector(tx, pos):
    """Assert that a vector is empty and return new pos."""
    n_vec, pos = read_compact_size(tx, pos)
    assert_equal(n_vec, 0)
    return pos

def find_orchard_actions_offset(tx):
    """
    Parse a v5 Orchard-only transaction to find the byte offset of the
    first Orchard action and the number of actions.
    Returns (n_actions, first_action_offset).
    """
    # v5 header: nVersion(4) + nVersionGroupId(4) + nConsensusBranchId(4) +
    #            nLockTime(4) + nExpiryHeight(4) = 20 bytes
    pos = 20

    # Skip four empty vectors (vin, vout, Sapling spends, Sapling outputs)
    for _ in range(4):
        pos = skip_empty_vector(tx, pos)

    # Now at Orchard bundle
    return read_compact_size(tx, pos)


def tamper_field(tx, offset):
    return tx[:offset] + IDENTITY_BYTES + tx[offset + 32:]

def get_field(tx, offset):
    return tx[offset:offset + 32]

def rk_offset(action_offset): 
    return action_offset + RK_OFFSET_IN_ACTION

def epk_offset(action_offset):
    return action_offset + EPK_OFFSET_IN_ACTION

def tamper_rk(tx, action_offset):
    """Set the rk field of an Orchard action to all zeros."""
    return tamper_field(tx, rk_offset(action_offset))

def tamper_epk(tx, action_offset):
    """Set the epk field of an Orchard action to all zeros."""
    return tamper_field(tx, epk_offset(action_offset))

def get_rk(tx, action_offset):
    return get_field(tx, rk_offset(action_offset))

def get_epk(tx, action_offset):
    return get_field(tx, epk_offset(action_offset))


class OrchardActionIdentityPointTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 2

    def add_options(self, parser):
        # Allow running an individual scenario without editing the test, e.g.
        # for re-verifying that each scenario fails independently against a
        # pre-fix binary.
        parser.add_option("--only-rk", dest="only_rk", default=False, action="store_true",
                          help="Run only the identity rk test.")
        parser.add_option("--only-epk", dest="only_epk", default=False, action="store_true",
                          help="Run only the identity epk test.")
        parser.add_option("--only-both", dest="only_both", default=False, action="store_true",
                          help="Run only the identity rk + epk test.")

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(NU5_BRANCH_ID, 210),
        ]] * self.num_nodes)

    def run_test(self):
        # Mine blocks to activate NU5 (at height 210)
        assert_equal(self.nodes[0].getblockcount(), 200)
        self.nodes[0].generate(10)
        self.sync_all()

        # Get an Orchard-only unified address on node 1
        acct = self.nodes[1].z_getnewaccount()['account']
        ua = self.nodes[1].z_getaddressforaccount(acct, ['orchard'])['address']

        # Shield coinbase to the Orchard address.
        # Send the full coinbase amount minus fee to avoid transparent change.
        coinbase_addr = get_coinbase_address(self.nodes[0])
        coinbase_fee = conventional_fee(3)
        coinbase_amount = Decimal('10') - coinbase_fee
        opid = self.nodes[0].z_sendmany(
            coinbase_addr,
            [{'address': ua, 'amount': coinbase_amount}],
            1,
            coinbase_fee,
            'AllowRevealedSenders',
        )
        wait_and_assert_operationid_status(self.nodes[0], opid)
        self.nodes[0].generate(1)
        self.sync_all()

        # Create an Orchard-to-Orchard spend to get a transaction with
        # Orchard actions that we can tamper with
        acct2 = self.nodes[1].z_getnewaccount()['account']
        ua2 = self.nodes[1].z_getaddressforaccount(acct2, ['orchard'])['address']
        opid = self.nodes[1].z_sendmany(
            ua,
            [{'address': ua2, 'amount': Decimal('0.5')}],
            1,
            ZIP_317_FEE,
        )
        txid = wait_and_assert_operationid_status(self.nodes[1], opid)

        # Get the raw transaction hex from the mempool
        tx_untampered_hex = self.nodes[1].getrawtransaction(txid)
        tx_untampered = hex_str_to_bytes(tx_untampered_hex)

        # Parse the v5 format to locate the Orchard action fields by byte offset
        n_actions, action_pos = find_orchard_actions_offset(tx_untampered)
        assert n_actions > 0, "Transaction should have Orchard actions, got %d" % n_actions
        print("Found %d Orchard action(s) starting at byte offset %d" % (n_actions, action_pos))

        # Verify our parsing is correct by checking the rk and epk match decoderawtransaction
        action_untampered_decoded = self.nodes[1].decoderawtransaction(tx_untampered_hex)['orchard']['actions'][0]
        rk_untampered_decoded = hex_str_to_bytes(action_untampered_decoded['rk'])
        epk_untampered_decoded = hex_str_to_bytes(action_untampered_decoded['ephemeralKey'])
        assert_equal(get_rk(tx_untampered, action_pos), rk_untampered_decoded)
        assert_equal(get_epk(tx_untampered, action_pos), epk_untampered_decoded)

        if not self.options.only_epk and not self.options.only_both:
            self.test_identity_rk(tx_untampered, action_pos)
        if not self.options.only_rk and not self.options.only_both:
            self.test_identity_epk(tx_untampered, action_pos)
        if not self.options.only_rk and not self.options.only_epk:
            self.test_identity_both(tx_untampered, action_pos)

    def _test_identity_case(self, name, tx_untampered, action_pos,
                            rk_identity=False, epk_identity=False):
        print("Testing identity %s rejection..." % name)

        tx = tx_untampered
        if rk_identity:
            tx = tamper_rk(tx, action_pos)
        if epk_identity:
            tx = tamper_epk(tx, action_pos)
        tx_hex = bytes_to_hex_str(tx)

        # Sanity check: the tampered byte ranges read as identity directly
        # and via decoderawtransaction (validates the byte-offset arithmetic).
        action_decoded = self.nodes[0].decoderawtransaction(tx_hex)['orchard']['actions'][0]
        if rk_identity:
            assert_equal(get_rk(tx, action_pos), IDENTITY_BYTES)
            assert_equal(hex_str_to_bytes(action_decoded['rk']), IDENTITY_BYTES)
        if epk_identity:
            assert_equal(get_epk(tx, action_pos), IDENTITY_BYTES)
            assert_equal(hex_str_to_bytes(action_decoded['ephemeralKey']), IDENTITY_BYTES)

        assert_raises_message(
            JSONRPCException, "bad-orchard-action-identity-point",
            self.nodes[0].sendrawtransaction, tx_hex,
        )
        print("  PASS: identity %s correctly rejected" % name)

    def test_identity_rk(self, tx_untampered, action_pos):
        self._test_identity_case('rk', tx_untampered, action_pos, rk_identity=True)

    def test_identity_epk(self, tx_untampered, action_pos):
        self._test_identity_case('epk', tx_untampered, action_pos, epk_identity=True)

    def test_identity_both(self, tx_untampered, action_pos):
        self._test_identity_case('rk+epk', tx_untampered, action_pos,
                                 rk_identity=True, epk_identity=True)

if __name__ == '__main__':
    OrchardActionIdentityPointTest().main()
