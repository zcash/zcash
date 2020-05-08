#!/usr/bin/env python3
#
# Copyright (c) 2020 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (assert_equal, assert_true,
    initialize_chain_clean, start_nodes, start_node, connect_nodes_bi,
    bitcoind_processes)
from test_framework.mininode import (
    nuparams,
    OVERWINTER_BRANCH_ID, SAPLING_BRANCH_ID, BLOSSOM_BRANCH_ID, HEARTWOOD_BRANCH_ID)

import re
import logging
import tarfile
import os.path

HAS_SAPLING   = [nuparams(OVERWINTER_BRANCH_ID, 10), nuparams(SAPLING_BRANCH_ID, 20)]
HAS_BLOSSOM   = HAS_SAPLING + [nuparams(BLOSSOM_BRANCH_ID, 30)]
HAS_HEARTWOOD = HAS_BLOSSOM + [nuparams(HEARTWOOD_BRANCH_ID, 40)]

class Upgrade():
    def __init__(self, h, p, a):
        self.gen_height = h
        self.tgz_path = p
        self.extra_args = a

class UpgradeGoldenTest(BitcoinTestFramework):
    def setup_chain(self):
        self.upgrades = [ Upgrade(35, os.path.dirname(os.path.realpath(__file__))+"/golden/blossom.tar.gz", HAS_BLOSSOM)
                        , Upgrade(45, os.path.dirname(os.path.realpath(__file__))+"/golden/heartwood.tar.gz", HAS_HEARTWOOD)
                        ]

        logging.info("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, len(self.upgrades) + 1)

    # This mirrors how the network was setup in the bash test
    def setup_network(self, split=False):
        logging.info("Initializing the network in "+self.options.tmpdir)
        upgrade_args = [self.upgrades[-1].extra_args] + [u.extra_args for u in self.upgrades]
        self.nodes = start_nodes(len(self.upgrades) + 1, self.options.tmpdir, extra_args=upgrade_args)

    def capture_blocks(self, to_height, tgz_path):
        # Generate past Blossom activation.
        self.nodes[0].generate(to_height)
        self.nodes[0].stop()
        bitcoind_processes[0].wait()

        node_path = self.options.tmpdir + "/node0/regtest"
        with tarfile.open(tgz_path, "w:gz") as tgz:
            tgz.add(node_path, arcname="")

        logging.info("Captured node state to "+tgz_path)

    def run_test(self):
        last_upgrade = self.upgrades[-1]
        if not os.path.isfile(last_upgrade.tgz_path):
            self.capture_blocks(last_upgrade.gen_height, last_upgrade.tgz_path)
            # restart the node after capturing the state
            self.nodes[0] = start_node(0, self.options.tmpdir, extra_args=last_upgrade.extra_args)

        i = 1
        for upgrade in self.upgrades:
            if os.path.isfile(upgrade.tgz_path):
                # shut down the node so we can replace its data dir(s)
                self.nodes[i].stop()
                bitcoind_processes[i].wait()

                with tarfile.open(upgrade.tgz_path, "r:gz") as tgz:
                    tgz.extractall(path = self.options.tmpdir + "/node" + str(i)+"/regtest")

                    # Upgrade each node to the latest network version. If any fails to
                    # start, this will fail the test.
                    try:
                        self.nodes[i] = start_node(i, self.options.tmpdir, extra_args=last_upgrade.extra_args)
                    except:
                        logging.error("An error occurred attempting to start node "+str(i))
                        raise



if __name__ == '__main__':
    UpgradeGoldenTest().main()

