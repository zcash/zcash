#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.mininode import (
    NodeConn,
    NodeConnCB,
    NetworkThread,
    msg_ping,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (initialize_chain_clean, start_nodes,
    p2p_port, assert_equal, nuparams, EPOCHS)

import time
import sys

#
# In this test we connect mininodes of each epoch to a Zcashd node that
# will activate each upgrade, as specified in the 'EPOCHS' list.
#
# We test:
# * the mininodes stay connected to Zcash with Sprout consensus rules;
# * for each upgrade,
#   * when the upgrade activates, mininodes that don't support it are dropped;
#   * new mininodes that do support the upgrade can connect;
#   * new mininodes that don't support the upgrade cannot connect.
#
# This test *does not* verify that prior to each activation, the Zcashd
# node will prefer connections with NU-aware nodes, with an eviction process
# that prioritizes non-NU-aware connections.
#


class TestManager(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.create_callback_map()

    def on_close(self, conn):
        pass

    def on_reject(self, conn, message):
        conn.rejectMessage = message


class NUPeerManagementTest(BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self):
        upgrade_params = [nuparams(EPOCHS[e].branch_id, self._height(e)) for e in range(1, len(EPOCHS))]

        self.nodes = start_nodes(1, self.options.tmpdir, extra_args=[upgrade_params + [
            '-debug',
            '-whitelist=127.0.0.1',
        ]])

    def _height(self, e):
        return e*5

    def _verify_correct_nodes_connected(self, e, n):
        peerinfo = self.nodes[0].getpeerinfo()
        versions = [x["version"] for x in peerinfo]
        for peer_epoch in EPOCHS[:e]:
            assert_equal(0, versions.count(peer_epoch.proto_version))
        for peer_epoch in EPOCHS[e:]:
            assert_equal(n, versions.count(peer_epoch.proto_version))

    def _until_it_works(self, f):
        for t in range(100):
            time.sleep(0.1)
            try:
                f()
                return
            except AssertionError:
                sys.stdout.write('.')
                sys.stdout.flush()
                continue
        f()

    def run_test(self):
        test = TestManager()

        # Launch mininodes, one per epoch
        nodes = []
        for x in range(10):
            for epoch in EPOCHS:
                nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0],
                             test, "regtest", epoch.proto_version))

        # Start up network handling in another thread
        NetworkThread().start()

        for e in range(1, len(EPOCHS)):
            epoch = EPOCHS[e]

            self.nodes[0].generate(self._height(e) - 1 - self.nodes[0].getblockcount())
            assert_equal(self._height(e) - 1, self.nodes[0].getblockcount())

            # Verify mininodes are still connected to zcashd node
            self._until_it_works(lambda: self._verify_correct_nodes_connected(e-1, 10+e-1))

            # Next consensus rules activate
            self.nodes[0].generate(1)
            assert_equal(self._height(e), self.nodes[0].getblockcount())
            print('Upgrade %s active' % (epoch.name))

            # Mininodes send ping message to zcashd node.
            pingCounter = 1
            for node in nodes:
                node.send_message(msg_ping(pingCounter))
                pingCounter = pingCounter + 1

            # Verify mininodes that don't support this upgrade have been dropped,
            # while mininodes that do are still connected.
            self._until_it_works(lambda: self._verify_correct_nodes_connected(e, 10+e-1))

            # Extend the upgraded chain with another block.
            self.nodes[0].generate(1)

            # Connect new mininodes supporting this upgrade to the zcashd node;
            # they should be accepted.
            for peer_epoch in EPOCHS[e:]:
                nodes.append(NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test,
                                      "regtest", peer_epoch.proto_version))

            def check_nodes_supporting_upgrade():
                # There are 10+e nodes connected per epoch that supports this upgrade.
                assert_equal((10+e)*len(EPOCHS[e:]), len(self.nodes[0].getpeerinfo()))

            self._until_it_works(check_nodes_supporting_upgrade)

            # Try to connect new mininodes that don't support the upgrade;
            # they should be rejected.
            for peer_epoch in EPOCHS[:e]:
                peer = NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test,
                                "regtest", peer_epoch.proto_version)
                nodes.append(peer)

            def check_version_reject_messages():
                for peer in nodes[-e:]:
                    expected = "Version must be %d or greater" % (epoch.proto_version,)
                    actual = str(getattr(peer, 'rejectMessage', None))
                    assert expected in actual, (expected, actual)

            self._until_it_works(check_version_reject_messages)

            # Verify that only mininodes that support the upgrade are connected.
            self._until_it_works(lambda: self._verify_correct_nodes_connected(e, 10+e))

        for node in nodes:
            node.disconnect_node()

if __name__ == '__main__':
    NUPeerManagementTest().main()
