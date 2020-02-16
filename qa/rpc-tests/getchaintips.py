#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

# Exercise the getchaintips API.  We introduce a network split, work
# on chains of different lengths, and join the network together again.
# This gives us two tips, verify that it works.


from test_framework.test_framework import ZcashTestFramework

class GetChainTipsTest(ZcashTestFramework):

    def run_test(self):
        ZcashTestFramework.run_test (self)

        tips = self.nodes[0].getchaintips ()
        self.assertEqual(len (tips), 1)
        self.assertEqual(tips[0]['branchlen'], 0)
        self.assertEqual(tips[0]['height'], 200)
        self.assertEqual(tips[0]['status'], 'active')

        # Split the network and build two chains of different lengths.
        self.split_network ()
        self.nodes[0].generate(10);
        self.nodes[2].generate(20);
        self.sync_all ()

        tips = self.nodes[1].getchaintips ()
        self.assertEqual(len (tips), 1)
        shortTip = tips[0]
        self.assertEqual(shortTip['branchlen'], 0)
        self.assertEqual(shortTip['height'], 210)
        self.assertEqual(tips[0]['status'], 'active')

        tips = self.nodes[3].getchaintips ()
        self.assertEqual(len (tips), 1)
        longTip = tips[0]
        self.assertEqual(longTip['branchlen'], 0)
        self.assertEqual(longTip['height'], 220)
        self.assertEqual(tips[0]['status'], 'active')

        # Join the network halves and check that we now have two tips
        # (at least at the nodes that previously had the short chain).
        self.join_network()

        tips = self.nodes[0].getchaintips()
        self.assertEqual(len(tips), 2)
        self.assertEqual(tips[0], longTip)

        self.assertEqual(tips[1]['branchlen'], 10)
        self.assertEqual(tips[1]['status'], 'valid-fork')
        tips[1]['branchlen'] = 0
        tips[1]['status'] = 'active'
        self.assertEqual(tips[1], shortTip)

if __name__ == '__main__':
    GetChainTipsTest ().main ()
