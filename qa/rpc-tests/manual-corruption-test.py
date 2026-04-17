#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
End-to-end test for CheckRecomputedPoolDeltas corruption detection.

Demonstrates that CheckRecomputedPoolDeltas correctly detects
corrupted persisted pool deltas from the duplicate-header attack.

Prerequisites:
  1. Save the current commit and build the vulnerable (v6.12.0) binary:
       export FIXED_COMMIT=$(git rev-parse HEAD)
       git checkout v6.12.0 && ./zcutil/build.sh -j$(nproc)
       cp src/zcashd src/zcashd-vulnerable

  2. Build the fixed binary:
       git checkout "$FIXED_COMMIT" && ./zcutil/build.sh -j$(nproc)

  3. Run this test:
       qa/rpc-tests/manual-corruption-test.py --vulnerable-binary src/zcashd-vulnerable

Or run qa/rpc-tests/test-delta-corruption.sh to do all steps automatically.
"""

import os
import sys
import tempfile
import time

from decimal import Decimal
from io import BytesIO

from test_framework.mininode import (
    CBlock,
    mininode_lock,
    msg_block,
    msg_ping,
    msg_pong,
    NodeConn,
    NodeConnCB,
    NetworkThread,
    NU5_PROTO_VERSION,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    hex_str_to_bytes,
    nuparams,
    p2p_port,
    start_node,
    start_nodes,
    stop_node,
    stop_nodes,
    wait_and_assert_operationid_status,
    wait_bitcoinds,
)
from test_framework.zip317 import conventional_fee


class TestNode(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.create_callback_map()
        self.connection = None
        self.ping_counter = 1
        self.last_pong = msg_pong()

    def add_connection(self, conn):
        self.connection = conn

    def on_pong(self, conn, message):
        self.last_pong = message

    def wait_for_verack(self):
        while True:
            with mininode_lock:
                if self.verack_received:
                    return
            time.sleep(0.05)

    def send_message(self, message):
        self.connection.send_message(message)

    def sync_with_ping(self, timeout=30):
        self.connection.send_message(msg_ping(nonce=self.ping_counter))
        received_pong = False
        sleep_time = 0.05
        while not received_pong and timeout > 0:
            time.sleep(sleep_time)
            timeout -= sleep_time
            with mininode_lock:
                if self.last_pong.nonce == self.ping_counter:
                    received_pong = True
        self.ping_counter += 1
        return received_pong


class CorruptionDetectionTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 1

    def add_options(self, parser):
        parser.add_option('--vulnerable-binary', dest='vulnerable_binary',
                          default='src/zcashd-vulnerable',
                          help='Path to the vulnerable zcashd binary')

    def setup_network(self, split=False):
        # Start with the vulnerable binary
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            nuparams(NU5_BRANCH_ID, 210),
        ]], binary=[self.options.vulnerable_binary])

    def run_test(self):
        node = self.nodes[0]
        assert_equal(node.getblockcount(), 200)

        # Activate NU5 and mine an Orchard shielding transaction.
        node.generate(11)
        assert_equal(node.getblockcount(), 211)

        acct = node.z_getnewaccount()['account']
        addrRes = node.z_getaddressforaccount(acct, ['orchard'])
        ua = addrRes['address']

        coinbase_fee = conventional_fee(3)
        shield_amount = Decimal('10') - coinbase_fee
        recipients = [{"address": ua, "amount": shield_amount}]
        myopid = node.z_sendmany(
            get_coinbase_address(node), recipients, 1, coinbase_fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(node, myopid)

        node.generate(1)
        target_height = node.getblockcount()
        target_hash = node.getbestblockhash()
        print(f"Target block: height={target_height} hash={target_hash}")

        # Verify the block has non-zero Orchard activity.
        target_info = node.getblock(target_hash)
        pools_before = {p['id']: p for p in target_info['valuePools']}
        orchard_delta = pools_before['orchard'].get('valueDeltaZat')
        print(f"Orchard valueDelta: {orchard_delta}")
        assert orchard_delta is not None and orchard_delta != 0

        # Mine a couple more blocks so the target isn't the tip.
        node.generate(2)

        # Get the target block as raw hex.
        target_hex = node.getblock(target_hash, 0)

        def fresh_target_block():
            b = CBlock()
            b.deserialize(BytesIO(hex_str_to_bytes(target_hex)))
            b.calc_sha256()
            return b

        # Connect P2P and do Scenario B: replay with duplicated tx.
        test_node = TestNode()
        conn = NodeConn('127.0.0.1', p2p_port(0), node, test_node,
                        protocol_version=NU5_PROTO_VERSION)
        test_node.add_connection(conn)
        NetworkThread().start()
        test_node.wait_for_verack()

        print("Sending Scenario B: replay with duplicated shielding tx...")
        scenario_b_block = fresh_target_block()
        assert len(scenario_b_block.vtx) >= 2
        scenario_b_block.vtx.append(scenario_b_block.vtx[-1])
        assert_equal(scenario_b_block.sha256, int(target_hash, 16))

        test_node.send_message(msg_block(scenario_b_block))
        test_node.sync_with_ping()

        # On the vulnerable binary, the delta should be corrupted.
        # Verify corruption the same way as Scenario C in
        # p2p_duplicate_block_clobbers_chain_value.py: compare the full
        # pool state before and after the attack.
        target_info_after = node.getblock(target_hash)
        target_state_before = {p['id']: (p['monitored'], p.get('chainValueZat'), p.get('valueDeltaZat'))
                               for p in target_info['valuePools']}
        target_state_after = {p['id']: (p['monitored'], p.get('chainValueZat'), p.get('valueDeltaZat'))
                              for p in target_info_after['valuePools']}
        print(f"Post-attack pool state: {target_state_after}")
        assert target_state_before != target_state_after, \
            "Pool state unchanged after attack - binary may not be vulnerable"

        # Force flush via invalidateblock/reconsiderblock.
        print("Forcing flush to disk...")
        node.invalidateblock(target_hash)
        node.reconsiderblock(target_hash)

        # Disconnect P2P and stop the node.
        conn.disconnect_node()
        time.sleep(1)
        stop_nodes(self.nodes)
        wait_bitcoinds()

        # Phase 2: restart with the FIXED binary.
        print("\n=== Restarting with fixed binary ===")
        node_stderr = tempfile.SpooledTemporaryFile(max_size=2**16)
        node_started = False
        try:
            self.nodes[0] = start_node(0, self.options.tmpdir, extra_args=[
                nuparams(NU5_BRANCH_ID, 210),
                '-regtestenablezip209',
            ], stderr=node_stderr)
            node_started = True
        except Exception as e:
            print(f"Node failed to start (expected): {e}")

        # Check the debug log for the corruption detection message.
        debug_log = os.path.join(self.options.tmpdir, 'node0', 'regtest',
                                 'debug.log')
        found_mismatch = False
        with open(debug_log, encoding='utf-8') as f:
            for line in f:
                if 'mismatch' in line and 'recomputed' in line:
                    print(f"  DETECTED: {line.rstrip()}")
                    found_mismatch = True

        if node_started:
            stop_node(self.nodes[0], 0)

        if found_mismatch and not node_started:
            print("\nTEST PASSED: CheckRecomputedPoolDeltas detected the "
                  "corrupted delta and aborted.")
        else:
            if found_mismatch and node_started:
                print("\nTEST FAILED: mismatch detected but node did not abort.")
            else:
                print("\nTEST FAILED: corruption was not detected.")
            sys.exit(1)


if __name__ == '__main__':
    CorruptionDetectionTest().main()
