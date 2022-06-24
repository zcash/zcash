#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal
import json
import os.path
import shutil
import tarfile
import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    start_nodes, stop_nodes,
    get_coinbase_address, assert_equal,
    set_node_times,
    wait_and_assert_operationid_status,
    wait_bitcoinds,
    PRE_BLOSSOM_BLOCK_TARGET_SPACING
)

class CreateSproutChainsTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4
        self.is_network_split = False
        self.setup_clean_chain = True

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            '-nuparams=5ba81b19:1', # Overwinter
            '-nuparams=76b809bb:1', # Sapling
        ]] * self.num_nodes)

    def run_test (self):
        block_time = int(time.time()) - (200 * PRE_BLOSSOM_BLOCK_TARGET_SPACING)
        for i in range(2):
            for peer in range(4):
                for j in range(25 - i):
                    set_node_times(self.nodes, block_time)
                    self.nodes[peer].generate(1)
                    self.sync_all()
                    block_time += PRE_BLOSSOM_BLOCK_TARGET_SPACING

                self.sync_all()

                # On the second iteration, shield 50 mature ZEC to Sprout on each node
                if i == 1:
                    set_node_times(self.nodes, block_time)
                    sprout_addr = self.nodes[peer].z_getnewaddress('sprout')
                    op_result = self.nodes[peer].z_shieldcoinbase('*', sprout_addr, 0, 5)
                    wait_and_assert_operationid_status(self.nodes[peer], op_result['opid'])

                    self.nodes[peer].generate(1)
                    self.sync_all()

                    assert_equal(self.nodes[peer].z_getbalance(sprout_addr), Decimal('50'))

        # Shut down the nodes
        stop_nodes(self.nodes)
        wait_bitcoinds()

        # Clean up, zip, and persist the generated datadirs. Record the generation
        # time so that we can correctly set the system clock offset in tests that
        # use these files.
        tmpdir = self.options.tmpdir
        sprout_cache_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'cache', 'sprout')
        for i in range(4):
            node_path = os.path.join(tmpdir, 'node' + str(i), 'regtest')
            os.remove(os.path.join(node_path, 'debug.log'))
            os.remove(os.path.join(node_path, 'db.log'))
            os.remove(os.path.join(node_path, 'peers.dat'))
            os.remove(os.path.join(node_path, 'fee_estimates.dat'))

            wallet_tgz_path = os.path.join(sprout_cache_path, 'node' + str(i) + '_wallet.tar.gz')
            if not os.path.isfile(wallet_tgz_path):
                with tarfile.open(wallet_tgz_path, "w:gz") as tgz:
                    tgz.add(os.path.join(node_path, 'wallet.dat'), arcname="")

            os.remove(os.path.join(node_path, 'wallet.dat'))

            if i == 0:
                cache_config = {
                    "cache_time": time.time()
                }

                with open(os.path.join(node_path, 'cache_config.json'), "w", encoding="utf8") as cache_conf_file:
                    cache_conf_json = json.dumps(cache_config, indent=4)
                    cache_conf_file.write(cache_conf_json)

                cache_path = os.path.join(sprout_cache_path, 'chain_cache.tar.gz')
                if not os.path.isfile(cache_path):
                    with tarfile.open(cache_path, "w:gz") as tgz:
                        tgz.add(node_path, arcname="")

if __name__ == '__main__':
    CreateSproutChainsTest().main()
