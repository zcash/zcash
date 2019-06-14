#!/usr/bin/env python2
# Copyright (c) 2018 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import CryptoconditionsTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, initialize_chain, start_nodes, start_node, connect_nodes_bi, \
    stop_nodes, sync_blocks, sync_mempools, wait_bitcoinds, rpc_port, assert_raises
from cryptoconditions import assert_success, assert_error, generate_random_string

class CryptoconditionsOraclesTest(CryptoconditionsTestFramework):

    def run_oracles_tests(self):
        rpc = self.nodes[0]
        rpc1 = self.nodes[1]
        result = rpc1.oraclesaddress()

        result = rpc.oraclesaddress()
        assert_success(result)

        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        result = rpc.oraclesaddress(self.pubkey)
        assert_success(result)

        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        # there are no oracles created yet
        result = rpc.oracleslist()
        assert_equal(result, [])

        # looking up non-existent oracle should return error.
        result = rpc.oraclesinfo("none")
        assert_error(result)

        # attempt to create oracle with not valid data type should return error
        result = rpc.oraclescreate("Test", "Test", "Test")
        assert_error(result)

        # attempt to create oracle with description > 32 symbols should return error
        too_long_name = generate_random_string(33)
        result = rpc.oraclescreate(too_long_name, "Test", "s")

        # attempt to create oracle with description > 4096 symbols should return error
        too_long_description = generate_random_string(4100)
        result = rpc.oraclescreate("Test", too_long_description, "s")
        assert_error(result)

        # need uxtos to create oracle? Crashes if without generate
        rpc.generate(2)

        # valid creating oracles of different types
        # using such naming to re-use it for data publishing / reading (e.g. oracle_s for s type)

        valid_formats = ["s", "S", "d", "D", "c", "C", "t", "T", "i", "I", "l", "L", "h", "Ihh"]
        for f in valid_formats:
            result = rpc.oraclescreate("Test_"+f, "Test_"+f, f)
            assert_success(result)
            globals()["oracle_{}".format(f)] = self.send_and_mine(result['hex'], rpc)

        # trying to register with negative datafee
            result = rpc.oraclesregister(globals()["oracle_{}".format(f)], "-100")
            assert_error(result)

        # trying to register with zero datafee
            result = rpc.oraclesregister(globals()["oracle_{}".format(f)], "0")
            assert_error(result)

        # trying to register with datafee less than txfee
            result = rpc.oraclesregister(globals()["oracle_{}".format(f)], "500")
            assert_error(result)

        # trying to register valid (unfunded)
            result = rpc.oraclesregister(globals()["oracle_{}".format(f)], "10000")
            assert_error(result)

        # Fund the oracles
            result = rpc.oraclesfund(globals()["oracle_{}".format(f)])
            assert_success(result)
            fund_txid = self.send_and_mine(result["hex"], rpc)
            assert fund_txid, "got txid"

        # trying to register valid (funded)
            result = rpc.oraclesregister(globals()["oracle_{}".format(f)], "10000")
            print(f)
            assert_success(result)
            register_txid = self.send_and_mine(result["hex"], rpc)
            assert register_txid, "got txid"

        # TODO: for most of the non valid oraclesregister and oraclessubscribe transactions generating and broadcasting now
        # so trying only valid oraclessubscribe atm
            result = rpc.oraclessubscribe(globals()["oracle_{}".format(f)], self.pubkey, "1")
            assert_success(result)
            subscribe_txid = self.send_and_mine(result["hex"], rpc)
            assert register_txid, "got txid"

            rpc.generate(1)

        # now lets publish and read valid data for each oracle type

        # s type
        result = rpc.oraclesdata(globals()["oracle_{}".format("s")], "05416e746f6e")
        assert_success(result)
        oraclesdata_s = self.send_and_mine(result["hex"], rpc)        
        info = rpc.oraclesinfo(globals()["oracle_{}".format("s")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("s")], batonaddr, "1")
        assert_equal("[u'Anton']", str(result["samples"][0]['data']), "Data match")

        # S type
        result = rpc.oraclesdata(globals()["oracle_{}".format("S")], "000161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161")
        assert_success(result)
        oraclesdata_S = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("S")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("S")], batonaddr, "1")
        assert_equal("[u'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa']", str(result["samples"][0]['data']), "Data match")

        # d type
        result = rpc.oraclesdata(globals()["oracle_{}".format("d")], "0101")
        assert_success(result)
        # baton
        oraclesdata_d = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("d")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("d")], batonaddr, "1")
        assert_equal("[u'01']", str(result["samples"][0]['data']), "Data match")

        # D type
        result = rpc.oraclesdata(globals()["oracle_{}".format("D")], "010001")
        assert_success(result)
        # baton
        oraclesdata_D = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("D")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("D")], batonaddr, "1")
        assert_equal("[u'01']", str(result["samples"][0]['data']), "Data match")

        # c type
        result = rpc.oraclesdata(globals()["oracle_{}".format("c")], "ff")
        assert_success(result)
        # baton
        oraclesdata_c = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("c")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("c")], batonaddr, "1")
        assert_equal("[u'-1']", str(result["samples"][0]['data']), "Data match")

        # C type
        result = rpc.oraclesdata(globals()["oracle_{}".format("C")], "ff")
        assert_success(result)
        # baton
        oraclesdata_C = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("C")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("C")], batonaddr, "1")
        assert_equal("[u'255']", str(result["samples"][0]['data']), "Data match")

        # t type
        result = rpc.oraclesdata(globals()["oracle_{}".format("t")], "ffff")
        assert_success(result)
        # baton
        oraclesdata_t = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("t")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("t")], batonaddr, "1")
        assert_equal("[u'-1']", str(result["samples"][0]['data']), "Data match")

        # T type
        result = rpc.oraclesdata(globals()["oracle_{}".format("T")], "ffff")
        assert_success(result)
        # baton
        oraclesdata_T = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("T")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("T")], batonaddr, "1")
        assert_equal("[u'65535']", str(result["samples"][0]['data']), "Data match")

        # i type
        result = rpc.oraclesdata(globals()["oracle_{}".format("i")], "ffffffff")
        assert_success(result)
        # baton
        oraclesdata_i = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("i")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("i")], batonaddr, "1")
        assert_equal("[u'-1']", str(result["samples"][0]['data']), "Data match")

        # I type
        result = rpc.oraclesdata(globals()["oracle_{}".format("I")], "ffffffff")
        assert_success(result)
        # baton
        oraclesdata_I = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("I")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("I")], batonaddr, "1")
        assert_equal("[u'4294967295']", str(result["samples"][0]['data']), "Data match")

        # l type
        result = rpc.oraclesdata(globals()["oracle_{}".format("l")], "00000000ffffffff")
        assert_success(result)
        # baton
        oraclesdata_l = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("l")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("l")], batonaddr, "1")
        assert_equal("[u'-4294967296']", str(result["samples"][0]['data']), "Data match")

        # L type
        result = rpc.oraclesdata(globals()["oracle_{}".format("L")], "00000000ffffffff")
        assert_success(result)
        # baton
        oraclesdata_L = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("L")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("L")], batonaddr, "1")
        assert_equal("[u'18446744069414584320']", str(result["samples"][0]['data']), "Data match")

        # h type
        result = rpc.oraclesdata(globals()["oracle_{}".format("h")], "00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff")
        assert_success(result)
        # baton
        oraclesdata_h = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("h")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("h")], batonaddr, "1")
        assert_equal("[u'ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000']", str(result["samples"][0]['data']), "Data match")

        # Ihh type
        result = rpc.oraclesdata(globals()["oracle_{}".format("Ihh")], "ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff")
        assert_success(result)
        # baton
        oraclesdata_Ihh = self.send_and_mine(result["hex"], rpc)
        info = rpc.oraclesinfo(globals()["oracle_{}".format("Ihh")])
        batonaddr = info['registered'][0]['baton']
        result = rpc.oraclessamples(globals()["oracle_{}".format("Ihh")], batonaddr, "1")
        print(result)
        assert_equal("[u'4294967295', u'ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000', u'ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000']", str(result["samples"][0]['data']), "Data match")

    def run_test(self):
        print("Mining blocks...")
        rpc = self.nodes[0]
        rpc1 = self.nodes[1]
        # utxos from block 1 become mature in block 101
        if not self.options.noshutdown:
            rpc.generate(101)
        self.sync_all()
        rpc.getinfo()
        rpc1.getinfo()
        # this corresponds to -pubkey above
        print("Importing privkeys")
        rpc.importprivkey(self.privkey)
        rpc1.importprivkey(self.privkey1)
        self.run_oracles_tests()


if __name__ == '__main__':
    CryptoconditionsOraclesTest().main()
