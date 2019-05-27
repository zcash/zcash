#!/usr/bin/env python2
# Copyright (c) 2018 The Hush developers
# Copyright (c) 2019 The SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
import time

class DPoWConfsTest(BitcoinTestFramework):
    def debug_info(self):
        rpc = self.nodes[0]
        print "-- DEBUG --"
        getinfo               = rpc.getinfo()
        getwalletinfo         = rpc.getwalletinfo()
        listreceivedbyaddress = rpc.listreceivedbyaddress()
        print "notarized=", getinfo['notarized'], " blocks=", getinfo['blocks']
        #print "getinfo=", getinfo
        print "balance=", getwalletinfo['balance']
        #print "getwalletinfo=", getwalletinfo
        print "listreceivedbyaddress=", listreceivedbyaddress
        print "-- DEBUG --"

    def setup_chain(self):
        self.num_nodes = 1
        print("Initializing DPoWconfs test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, self.num_nodes)

    def setup_network(self):
        print("Setting up network...")
        self.nodes            = []
        self.is_network_split = False
        self.addr             = "RWPg8B91kfK5UtUN7z6s6TeV9cHSGtVY8D"
        self.pubkey           = "02676d00110c2cd14ae24f95969e8598f7ccfaa675498b82654a5b5bd57fc1d8cf"
        self.nodes            = start_nodes( self.num_nodes, self.options.tmpdir,
                extra_args=[[
                    '-ac_name=REGTEST',
                    '-conf='+self.options.tmpdir+'/node0/REGTEST.conf',
                    '-port=64367',
                    '-rpcport=64368',
                    '-regtest',
                    '-addressindex=1',
                    '-spentindex=1',
                    '-ac_supply=5555555',
                    '-ac_reward=10000000000000',
                    #'-pubkey=' + self.pubkey,
                    '-ac_cc=2',
                    '-whitelist=127.0.0.1',
                    '-debug',
                    '--daemon',
                    '-rpcuser=rt',
                    '-rpcpassword=rt'
                    ]]
                )
        self.sync_all()

    def run_test(self):
        rpc = self.nodes[0]
        # 98 is notarized, next will be 105. Must mine at least 101
        # blocks for 100 block maturity rule
        blockhashes = rpc.generate(101)
        # block 98, this is 0 indexed
        notarizedhash = blockhashes[97]
        self.debug_info()

        taddr = rpc.getnewaddress()
        txid  = rpc.sendtoaddress(taddr, 5.55)
        # blocks 102,103
        rpc.generate(2)
        self.debug_info()

        info = rpc.getinfo()
        print "notarizedhash=", notarizedhash, "\n"
        print "info[notarizedhash]", info['notarizedhash'], "\n"
        assert_equal( info['notarizedhash'], notarizedhash)

        result = rpc.listunspent()

        # this xtn has 2 raw confs, but not in a notarized block,
        # so dpowconfs holds it at 1
        for res in result:
            if (res['address'] == taddr and res['generated'] == 'false'):
                assert_equal( result[0]['confirmations'], 1 )
                assert_equal( result[0]['rawconfirmations'], 2 )

        # we will now have 3 rawconfs but confirmations=1 because not notarized
        # block 104
        rpc.generate(1)
        self.debug_info()
        minconf = 2
        result  = rpc.listreceivedbyaddress(minconf)
        print "listreceivedbyaddress(2)=", result, "\n"

        # nothing is notarized, so we should see no results for minconf=2
        assert len(result) == 0

        print "getreceivedaddress"
        received = rpc.getreceivedbyaddress(taddr, minconf)
        assert_equal( received, 0.00000000)

        #received = rpc.getreceivedbyaddress(taddr)
        #assert_equal( received, "5.55000000")
	taddr = rpc.getnewaddress()
	zaddr = rpc.z_getnewaddress()
	# should get insufficient funds error
	recipients = [ { "amount" : Decimal('4.20'), "address" : zaddr } ]
	txid  = rpc.z_sendmany( taddr, recipients, minconf)

        # generate a notarized block, block 105 and block 106
        # only generating the notarized block seems to have
        # race conditions about whether the block is notarized
        txids = rpc.generate(2)
        self.debug_info()

        getinfo = rpc.getinfo()
        # try to allow notarization data to update
        print "Sleeping"
        while (getinfo['blocks'] != 106) or (getinfo['notarized'] != 105):
            printf(".")
            time.sleep(1)
            getinfo = rpc.getinfo()

        # make sure this block was notarized as we expect
        #assert_equal(getinfo['blocks'], getinfo['notarized'])
        #assert_equal(getinfo['notarizedhash'], txids[0])

        result = rpc.listreceivedbyaddress(minconf)
        print "listreceivedbyaddress(2)=", result

        assert_equal( len(result), 1,  'got one xtn with minconf=2' )

        # verify we see the correct dpowconfs + rawconfs
        assert_greater_than( result[0]['confirmations'], 1)
        assert_greater_than( result[0]['rawconfirmations'], 1)

        print "listtransactions"
        xtns = rpc.listtransactions()
        # verify this rpc agrees with listreceivedbyaddress
        assert_greater_than(xtns[0]['confirmations'], 1)
        assert_greater_than(xtns[0]['rawconfirmations'], 1)

        print "getreceivedaddress"
        received = rpc.getreceivedbyaddress(taddr, minconf)
        assert_equal( "%.8f" % received, "5.55000000")

        received = rpc.getreceivedbyaddress(taddr)
        assert_equal( "%.8f" % received, "5.55000000")

if __name__ == '__main__':
    DPoWConfsTest().main()
