#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Test node handling
#

from test_framework.test_framework import ZcashTestFramework
from test_framework.util import  connect_nodes_bi, p2p_port

import time
from urllib.parse import urlparse

class NodeHandlingTest(ZcashTestFramework):
    def run_test(self):
        ###########################
        # setban/listbanned tests #
        ###########################
        self.assertEqual(len(self.nodes[2].getpeerinfo()), 4) #we should have 4 nodes at this point
        self.nodes[2].setban("127.0.0.1", "add")
        time.sleep(3) #wait till the nodes are disconected
        self.assertEqual(len(self.nodes[2].getpeerinfo()), 0) #all nodes must be disconnected at this point
        self.assertEqual(len(self.nodes[2].listbanned()), 1)
        self.nodes[2].clearbanned()
        self.assertEqual(len(self.nodes[2].listbanned()), 0)
        self.nodes[2].setban("127.0.0.0/24", "add")
        self.assertEqual(len(self.nodes[2].listbanned()), 1)
        try:
            self.nodes[2].setban("127.0.0.1", "add") #throws exception because 127.0.0.1 is within range 127.0.0.0/24
        except:
            pass
        self.assertEqual(len(self.nodes[2].listbanned()), 1) #still only one banned ip because 127.0.0.1 is within the range of 127.0.0.0/24
        try:
            self.nodes[2].setban("127.0.0.1", "remove")
        except:
            pass
        self.assertEqual(len(self.nodes[2].listbanned()), 1)
        self.nodes[2].setban("127.0.0.0/24", "remove")
        self.assertEqual(len(self.nodes[2].listbanned()), 0)
        self.nodes[2].clearbanned()
        self.assertEqual(len(self.nodes[2].listbanned()), 0)
        
        ###########################
        # RPC disconnectnode test #
        ###########################
        url = urlparse(self.nodes[1].url)
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
