#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_true, start_nodes

class WalletImportExportTest (BitcoinTestFramework):
    def setup_network(self, split=False):
        num_nodes = 3
        extra_args = [["-exportdir={}/export{}".format(self.options.tmpdir, i)] for i in range(num_nodes)]
        self.nodes = start_nodes(num_nodes, self.options.tmpdir, extra_args)

    def run_test(self):
        sapling_address2 = self.nodes[2].z_getnewaddress('sapling')
        privkey2 = self.nodes[2].z_exportkey(sapling_address2)
        self.nodes[0].z_importkey(privkey2)

        sprout_address0 = self.nodes[0].z_getnewaddress('sprout')
        sapling_address0 = self.nodes[0].z_getnewaddress('sapling')

        # node 0 should have the keys
        dump_path0 = self.nodes[0].z_exportwallet('walletdump')
        (t_keys0, sprout_keys0, sapling_keys0) = parse_wallet_file(dump_path0)

        sapling_line_lengths = [len(sapling_key0.split(' #')[0].split()) for sapling_key0 in sapling_keys0.splitlines()]
        assert_equal(2, len(sapling_line_lengths), "Should have 2 sapling keys")
        assert_true(2 in sapling_line_lengths, "Should have a key with 2 parameters")
        assert_true(4 in sapling_line_lengths, "Should have a key with 4 parameters")

        assert_true(sprout_address0 in sprout_keys0)
        assert_true(sapling_address0 in sapling_keys0)
        assert_true(sapling_address2 in sapling_keys0)

        # node 1 should not have the keys
        dump_path1 = self.nodes[1].z_exportwallet('walletdumpbefore')
        (t_keys1, sprout_keys1, sapling_keys1) = parse_wallet_file(dump_path1)
        
        assert_true(sprout_address0 not in sprout_keys1)
        assert_true(sapling_address0 not in sapling_keys1)

        # import wallet to node 1
        self.nodes[1].z_importwallet(dump_path0)

        # node 1 should now have the keys
        dump_path1 = self.nodes[1].z_exportwallet('walletdumpafter')
        (t_keys1, sprout_keys1, sapling_keys1) = parse_wallet_file(dump_path1)
        
        assert_true(sprout_address0 in sprout_keys1)
        assert_true(sapling_address0 in sapling_keys1)
        assert_true(sapling_address2 in sapling_keys1)

        # make sure we have perserved the metadata
        for sapling_key0 in sapling_keys0.splitlines():
            assert_true(sapling_key0 in sapling_keys1)

# Helper functions
def parse_wallet_file(dump_path):
    file_lines = open(dump_path, "r").readlines()
    # We expect information about the HDSeed and fingerpring in the header
    assert_true("HDSeed" in file_lines[4], "Expected HDSeed")
    assert_true("fingerprint" in file_lines[4], "Expected fingerprint")
    seed_comment_line = file_lines[4][2:].split()  # ["HDSeed=...", "fingerprint=..."]
    assert_true(seed_comment_line[0].split("=")[1] != seed_comment_line[1].split("=")[1], "The seed should not equal the fingerprint")
    (t_keys, i) = parse_wallet_file_lines(file_lines, 0)
    (sprout_keys, i) = parse_wallet_file_lines(file_lines, i)
    (sapling_keys, i) = parse_wallet_file_lines(file_lines, i)

    return (t_keys, sprout_keys, sapling_keys)

def parse_wallet_file_lines(file_lines, i):
    keys = []
    # skip blank lines and comments
    while i < len(file_lines) and (file_lines[i] == '\n' or file_lines[i].startswith("#")):
        i += 1
    # add keys until we hit another blank line or comment
    while  i < len(file_lines) and not (file_lines[i] == '\n' or file_lines[i].startswith("#")):
        keys.append(file_lines[i])
        i += 1
    return ("".join(keys), i)

if __name__ == '__main__':
    WalletImportExportTest().main()