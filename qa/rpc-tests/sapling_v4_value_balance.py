#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Regression test for the Sapling v4 valueBalanceSapling silent-normalization
# bug.
#
# Per §7.1.2 (consensus rule, Sapling onward):
#
#   If effectiveVersion = 4 and there are no Spend descriptions or Output
#   descriptions, then valueBalanceSapling MUST be 0.
#
# Before this fix, zcashd silently normalized a non-zero valueBalanceSapling
# to 0 in that case (at the Rust side of the v4 Sapling bundle deserialization
# path), so a malformed transaction would be accepted and re-serialized in
# canonical form. The consensus check at
# https://github.com/zcash/zcash/blob/v6.12.1/src/main.cpp#L1549-L1554
# was dead code because it observed the normalized (zero) value rather than
# the original wire bytes. Zebra rejects such transactions per spec, so the
# two implementations would diverge on a transaction that reached them
# unchanged from a malicious/custom block producer.
#
# The fix (in BundleAssembler::parse_v4_components) rejects such
# transactions at deserialization time, matching Zebra's behavior.
#
# This test constructs the malformed wire format directly in Python, which
# keeps the unit of interest (the deserialization check) isolated from
# wallet state, signatures, and consensus-layer checks that would otherwise
# reject a transaction with no source of funds. It verifies that both
# decoderawtransaction and sendrawtransaction reject it.
#
# The transaction includes a dummy transparent input and output (referencing a
# non-existent prior output) purely to satisfy the Sapling-onward "tx must
# have some input/output" structural rules from §7.1.2, so that the only
# consensus rule the transaction violates is the one under test. A well-formed
# (zero valueBalanceSapling) version of this transaction would still fail
# sendrawtransaction at the input-missing check, but that check never runs for
# the valueBalanceSapling test cases because deserialization rejects first.
#

import struct

from test_framework.authproxy import JSONRPCException
from test_framework.mininode import MAX_MONEY, SAPLING_VERSION_GROUP_ID, ser_compactsize
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_message,
    bytes_to_hex_str,
    start_nodes,
)


# Transaction version encoding:
#   bit 31 (fOverwintered) = 1, remaining bits = nVersion = 4.
V4_VERSION_HEADER = 0x80000004


def build_minimal_v4_tx(value_balance_sapling):
    """
    Build a minimal structurally well-formed v4 transaction whose Sapling
    bundle is empty, with the specified valueBalanceSapling (a signed 64-bit
    little-endian integer). It contains a single dummy transparent input and
    output (referencing a non-existent prior output) only to satisfy the
    Sapling-onward "tx must have some input/output" structural rules; no
    Sapling spends or outputs; no JoinSplits.

    Because nSpendsSapling + nOutputsSapling = 0, the bindingSigSapling
    field is absent. The only "live" field we care about for the fix is
    valueBalanceSapling itself.
    """
    tx = b''
    tx += struct.pack('<I', V4_VERSION_HEADER)          # nVersion | fOverwintered
    tx += struct.pack('<I', SAPLING_VERSION_GROUP_ID)   # nVersionGroupId
    tx += ser_compactsize(1)                            # tx_in count
    tx += b'\x00' * 32                                  #   prevout.hash (null)
    tx += struct.pack('<I', 0)                          #   prevout.n
    tx += ser_compactsize(0)                            #   scriptSig (empty)
    tx += struct.pack('<I', 0xFFFFFFFF)                 #   nSequence
    tx += ser_compactsize(1)                            # tx_out count
    tx += struct.pack('<q', 0)                          #   nValue
    tx += ser_compactsize(0)                            #   scriptPubKey (empty)
    tx += struct.pack('<I', 0)                          # nLockTime
    tx += struct.pack('<I', 0)                          # nExpiryHeight
    tx += struct.pack('<q', value_balance_sapling)      # valueBalanceSapling (i64 LE)
    tx += ser_compactsize(0)                            # nSpendsSapling
    tx += ser_compactsize(0)                            # nOutputsSapling
    # bindingSigSapling omitted: present only if nSpendsSapling + nOutputsSapling > 0.
    tx += ser_compactsize(0)                            # nJoinSplit
    return tx


class SaplingV4ValueBalanceTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 1

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir)

    def setup_network(self, split=False, do_mempool_sync=True):
        self.nodes = self.setup_nodes()
        self.is_network_split = False

    def run_test(self):
        node = self.nodes[0]

        # Control: valueBalanceSapling = 0 is well-formed. decoderawtransaction
        # must accept it (even though higher-level validity checks would
        # reject a transaction with no source of funds, deserialization is
        # the layer under test here).
        print("Control: valueBalanceSapling = 0 deserializes successfully")
        control_hex = bytes_to_hex_str(build_minimal_v4_tx(0))
        decoded = node.decoderawtransaction(control_hex)
        assert_equal(decoded['version'], 4)
        assert_equal(decoded['valueBalance'], 0)

        # Malformed: valueBalanceSapling != 0 with no Sapling spends or outputs
        # violates §7.1.2. The fix rejects this at deserialization time. Test a
        # range of non-zero values (positive, negative, near the boundaries of
        # the valid range).
        for bad_value in (1, -1, 42, -42, MAX_MONEY, -MAX_MONEY):
            print("Malformed: valueBalanceSapling = %d must be rejected" % bad_value)
            malformed_hex = bytes_to_hex_str(build_minimal_v4_tx(bad_value))

            # decoderawtransaction must reject. The specific Rust error
            # message propagates as a consensus_rule_failure exception
            # through DecodeHexTx; its what() is included in the
            # JSONRPCError.
            assert_raises_message(
                JSONRPCException,
                "nonzero valueBalanceSapling has no sources or sinks",
                node.decoderawtransaction,
                malformed_hex,
            )

            # sendrawtransaction uses the same DecodeHexTx path and must also
            # reject before any consensus check runs.
            assert_raises_message(
                JSONRPCException,
                "nonzero valueBalanceSapling has no sources or sinks",
                node.sendrawtransaction,
                malformed_hex,
            )


if __name__ == '__main__':
    SaplingV4ValueBalanceTest().main()
