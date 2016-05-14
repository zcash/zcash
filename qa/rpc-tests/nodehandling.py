#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Test node handling
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, connect_nodes_bi, p2p_port

import time
import urllib.parse

class NodeHandlingTest (BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 4
        self.setup_clean_chain = False

    def run_test(self):
        ###########################
        # setban/listbanned tests #
        ###########################
        assert_equal(len(self.nodes[2].getpeerinfo()), 4) #we should have 4 nodes at this point
        self.nodes[2].setban("127.0.0.1", "add")
        time.sleep(3) #wait till the nodes are disconected
        assert_equal(len(self.nodes[2].getpeerinfo()), 0) #all nodes must be disconnected at this point
        assert_equal(len(self.nodes[2].listbanned()), 1)
        self.nodes[2].clearbanned()
        assert_equal(len(self.nodes[2].listbanned()), 0)
        self.nodes[2].setban("127.0.0.0/24", "add")
        assert_equal(len(self.nodes[2].listbanned()), 1)
        try:
            self.nodes[2].setban("127.0.0.1", "add") #throws exception because 127.0.0.1 is within range 127.0.0.0/24
        except:
            pass
        assert_equal(len(self.nodes[2].listbanned()), 1) #still only one banned ip because 127.0.0.1 is within the range of 127.0.0.0/24
        try:
            self.nodes[2].setban("127.0.0.1", "remove")
        except:
            pass
        assert_equal(len(self.nodes[2].listbanned()), 1)
        self.nodes[2].setban("127.0.0.0/24", "remove")
        assert_equal(len(self.nodes[2].listbanned()), 0)
        self.nodes[2].clearbanned()
        assert_equal(len(self.nodes[2].listbanned()), 0)
        
        ###########################
        # RPC disconnectnode test #
        ###########################
        url = urllib.parse.urlparse(self.nodes[1].url)
        self.nodes[0].disconnectnode(url.hostname+":"+str(p2p_port(1)))
        time.sleep(2) #disconnecting a node needs a little bit of time
        for node in self.nodes[0].getpeerinfo():
            assert(node['addr'] != url.hostname+":"+str(p2p_port(1)))

        connect_nodes_bi(self.nodes,0,1) #reconnect the node
        found = False
        for node in self.nodes[0].getpeerinfo():
            if node['addr'] == url.hostname+":"+str(p2p_port(1)):
                found = True
        assert(found)

if __name__ == '__main__':
    NodeHandlingTest ().main ()
