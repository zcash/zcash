#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

# Exercise the getchaintips API.  We introduce a network split, work
# on chains of different lengths, and join the network together again.
# This gives us two tips, verify that it works.


from test_framework.test_framework import ZcashTestFramework
from test_framework.util import 

class GetChainTipsTest (ZcashTestFramework):

    def run_test (self):
        ZcashTestFramework.run_test (self)

        tips = self.nodes[0].getchaintips ()
         (len (tips), 1)
         (tips[0]['branchlen'], 0)
         (tips[0]['height'], 200)
         (tips[0]['status'], 'active')

        # Split the network and build two chains of different lengths.
        self.split_network ()
        self.nodes[0].generate(10);
        self.nodes[2].generate(20);
        self.sync_all ()

        tips = self.nodes[1].getchaintips ()
         (len (tips), 1)
        shortTip = tips[0]
         (shortTip['branchlen'], 0)
         (shortTip['height'], 210)
         (tips[0]['status'], 'active')

        tips = self.nodes[3].getchaintips ()
         (len (tips), 1)
        longTip = tips[0]
         (longTip['branchlen'], 0)
         (longTip['height'], 220)
         (tips[0]['status'], 'active')

        # Join the network halves and check that we now have two tips
        # (at least at the nodes that previously had the short chain).
        self.join_network ()

        tips = self.nodes[0].getchaintips ()
         (len (tips), 2)
         (tips[0], longTip)

         (tips[1]['branchlen'], 10)
         (tips[1]['status'], 'valid-fork')
        tips[1]['branchlen'] = 0
        tips[1]['status'] = 'active'
         (tips[1], shortTip)

if __name__ == '__main__':
    GetChainTipsTest ().main ()
