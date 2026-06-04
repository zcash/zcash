#!/usr/bin/env python3
# Copyright (c) 2026 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""
Regression test for https://github.com/zcash/zcash/issues/5960.

If a wallet's Orchard note commitment tree silently diverges from consensus, the
*only* place that divergence was historically detected was a hard assertion on the
block-disconnect path in `CWallet::DecrementNoteWitnesses`, which aborted the whole
node (SIGABRT) on the next reorg — a restart-proof crash loop, since the diverged
tree persists and re-crashes on every subsequent reorg.

We reproduce the divergence deterministically with a regtest-only test hook,
`-regtestcorruptorchardtree=<height>`, which makes the wallet skip appending one
block's Orchard commitments (while still checkpointing) — exactly the silent
undercount from the bug — and suppresses the forward consistency check so the
divergence is only caught on the disconnect path, as in the original report.

We then force a reorg with `invalidateblock` above the undercounted block, which
disconnects blocks back across it and triggers the disconnect-path check.

  - Before the fix: the node aborts with
    `Assertion 'pindex->pprev->hashFinalOrchardRoot == orchardWallet.GetLatestAnchor()' failed`.
  - After the fix: the node does NOT abort; it logs that the Orchard tree has
    diverged and shuts down cleanly with an operator-actionable -rescan message,
    and restarting with -rescan recovers the wallet.
"""

import os
import time
import tempfile

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    BLOSSOM_BRANCH_ID,
    HEARTWOOD_BRANCH_ID,
    CANOPY_BRANCH_ID,
    NU5_BRANCH_ID,
    assert_equal,
    assert_true,
    bitcoind_processes,
    get_coinbase_address,
    nuparams,
    start_node,
    wait_and_assert_operationid_status,
)
from test_framework.zip317 import ZIP_317_FEE

# The first block that will contain an Orchard commitment (the shielding tx is
# mined here, after activating NU5 at height 10 and maturing coinbase); the test
# hook skips appending this block's commitments.
CORRUPT_HEIGHT = 111

# Logged by CWallet when it detects the divergence (the fixed behaviour).
DIVERGENCE_MESSAGE = "Orchard note commitment tree has diverged from the consensus tree"
# The assertion that fires in the unfixed code (printed to stderr on abort).
ASSERT_FRAGMENT = "hashFinalOrchardRoot == orchardWallet.GetLatestAnchor()"


class WalletOrchardTreeDivergenceTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.cache_behavior = 'clean'

    def base_args(self):
        return [
            '-debug=1',
            '-nurejectoldversions=false',
            nuparams(BLOSSOM_BRANCH_ID, 1),
            nuparams(HEARTWOOD_BRANCH_ID, 5),
            nuparams(CANOPY_BRANCH_ID, 5),
            nuparams(NU5_BRANCH_ID, 10),
        ]

    def setup_network(self, split=False):
        # Capture the node's stderr so that, against unfixed code, the abort
        # message does not propagate to the framework (which would otherwise
        # treat the test as failed before we can inspect it).
        self.node_stderr = tempfile.SpooledTemporaryFile(max_size=2**20)
        self.nodes = [start_node(
            0, self.options.tmpdir,
            extra_args=self.base_args() + ['-regtestcorruptorchardtree=%d' % CORRUPT_HEIGHT],
            stderr=self.node_stderr)]
        self.is_network_split = False

    def debug_log(self):
        with open(os.path.join(self.options.tmpdir, "node0", "regtest", "debug.log"), encoding="utf8") as f:
            return f.read()

    def captured_stderr(self):
        self.node_stderr.seek(0)
        return self.node_stderr.read().decode("utf8", "replace")

    def wait_for_wallet_connect(self, height, timeout=30):
        # The wallet is notified of block connects asynchronously (once-per-second
        # poll in ThreadNotifyWallets), so wait until it has processed `height`
        # before forcing a reorg, otherwise the reorg unwinds blocks the wallet
        # never saw and no disconnect is observed.
        target = "height=%d kind=connect" % height
        for _ in range(timeout):
            if target in self.debug_log():
                return
            time.sleep(1)
        raise AssertionError("wallet did not process the connect of height %d" % height)

    def run_test(self):
        node = self.nodes[0]
        assert_equal(node.getblockcount(), 0)

        # Activate NU5 (height 10) and mature coinbase, then create an
        # Orchard-only address to receive the shielded note.
        node.generate(CORRUPT_HEIGHT - 1)
        assert_equal(node.getblockcount(), CORRUPT_HEIGHT - 1)
        acct = node.z_getnewaccount()['account']
        addr = node.z_getaddressforaccount(acct, ['orchard'])
        assert_equal(set(addr['receiver_types']), set(['orchard']))
        ua = addr['address']

        # Shield coinbase into the Orchard address; mining it produces the block
        # at CORRUPT_HEIGHT whose Orchard commitment the hook will drop, leaving
        # the wallet's note commitment tree undercounted relative to consensus.
        res = node.z_shieldcoinbase(get_coinbase_address(node), ua, ZIP_317_FEE, None, None, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(node, res['opid'])
        node.generate(1)
        assert_equal(node.getblockcount(), CORRUPT_HEIGHT)

        # Mine two more (Orchard-free) blocks on top; the tree stays undercounted.
        node.generate(2)
        assert_equal(node.getblockcount(), CORRUPT_HEIGHT + 2)
        # Make sure the wallet has actually connected these blocks before we
        # reorg them away (otherwise there is nothing for it to disconnect).
        self.wait_for_wallet_connect(CORRUPT_HEIGHT + 2)

        # Force a reorg that disconnects blocks back across the undercounted one.
        # The disconnect runs on the wallet notification thread, where the
        # divergence is detected. Before the fix this aborts the node; after the
        # fix the node shuts down cleanly.
        reorg_from = node.getblockhash(CORRUPT_HEIGHT + 1)
        try:
            node.invalidateblock(reorg_from)
        except (JSONRPCException, ConnectionError, BrokenPipeError):
            pass  # the node may already be shutting down/aborting

        # Wait for the node process to exit (clean shutdown or abort).
        proc = bitcoind_processes.pop(0)
        proc.wait(timeout=60)

        log = self.debug_log()
        stderr = self.captured_stderr()

        assert_true(
            ASSERT_FRAGMENT not in log and ASSERT_FRAGMENT not in stderr,
            "node aborted on the Orchard-root assertion instead of recovering (#5960):\n%s" % stderr)
        assert_true(
            DIVERGENCE_MESSAGE in log,
            "expected the node to detect and log the Orchard tree divergence; it did not.\n"
            "debug.log tail:\n%s" % "\n".join(log.splitlines()[-40:]))
        assert_equal(0, proc.returncode)
        print("Node detected the divergence and shut down cleanly (no abort).")

        # Recovery: restarting with -rescan (and without the corruption hook)
        # must rebuild the Orchard tree and bring the node back up — i.e. the
        # crash is not a restart-proof loop.
        self.node_stderr = tempfile.SpooledTemporaryFile(max_size=2**20)
        self.nodes = [start_node(
            0, self.options.tmpdir,
            extra_args=self.base_args() + ['-rescan'],
            stderr=self.node_stderr)]
        # Give the rescan a moment, then confirm the node is healthy and serving.
        for _ in range(60):
            try:
                height = self.nodes[0].getblockcount()
                break
            except (JSONRPCException, ConnectionError, BrokenPipeError):
                time.sleep(1)
        else:
            raise AssertionError("node did not come back up after -rescan recovery")
        assert_true(height >= CORRUPT_HEIGHT, "recovered node is at an unexpected height %d" % height)
        recovered_stderr = self.captured_stderr()
        assert_true(
            ASSERT_FRAGMENT not in self.debug_log() and ASSERT_FRAGMENT not in recovered_stderr,
            "node re-aborted on the Orchard-root assertion after -rescan recovery (#5960)")
        print("Node recovered after restarting with -rescan; height=%d." % height)


if __name__ == '__main__':
    WalletOrchardTreeDivergenceTest().main()
