#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import os
import time
from util import assert_success, assert_error, check_if_mined,\
    send_and_mine, rpc_connect, wait_some_blocks, generate_random_string, komodo_teardown


@pytest.mark.usefixtures("proxy_connection")
def test_oracles(test_params):

    # test params inits
    rpc = test_params.get('node1').get('rpc')
    rpc1 = test_params.get('node2').get('rpc')

    pubkey = test_params.get('node1').get('pubkey')
    pubkey1 = test_params.get('node2').get('pubkey')

    is_fresh_chain = test_params.get("is_fresh_chain")

    result = rpc.oraclesaddress()
    assert_success(result)

    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    result = rpc.oraclesaddress(pubkey)
    assert_success(result)

    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    # there are no oracles created yet
    if is_fresh_chain:
        result = rpc.oracleslist()
        assert result == []

    # looking up non-existent oracle should return error.
    result = rpc.oraclesinfo("none")
    assert_error(result)

    # attempt to create oracle with not valid data type should return error
    result = rpc.oraclescreate("Test", "Test", "Test")
    assert_error(result)

    # attempt to create oracle with description > 32 symbols should return error
    too_long_name = generate_random_string(33)
    result = rpc.oraclescreate(too_long_name, "Test", "s")
    assert_error(result)

    # attempt to create oracle with description > 4096 symbols should return error
    too_long_description = generate_random_string(4100)
    result = rpc.oraclescreate("Test", too_long_description, "s")
    assert_error(result)

    # valid creating oracles of different types
    # using such naming to re-use it for data publishing / reading (e.g. oracle_s for s type)
    print(len(rpc.listunspent()))
    # enable mining
    valid_formats = ["s", "S", "d", "D", "c", "C", "t", "T", "i", "I", "l", "L", "h", "Ihh"]
    for f in valid_formats:
        result = rpc.oraclescreate("Test_" + f, "Test_" + f, f)
        assert_success(result)
        # globals()["oracle_{}".format(f)] = rpc.sendrawtransaction(result['hex'])
        globals()["oracle_{}".format(f)] = send_and_mine(result['hex'], rpc)

    list_fund_txid = []
    for f in valid_formats:
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
        fund_txid = rpc.sendrawtransaction(result["hex"])
        list_fund_txid.append(fund_txid)
        assert fund_txid, "got txid"

    wait_some_blocks(rpc, 2)

    for t in list_fund_txid:
        c = 0
        print("Waiting confiramtions for oraclesfund")
        while c < 2:
            try:
                c = rpc.getrawtransaction(t, 1)['confirmations']
            except KeyError:
                time.sleep(29)
        print("Oracles fund confirmed \n", t)

    for f in valid_formats:
        # trying to register valid (funded)
        result = rpc.oraclesregister(globals()["oracle_{}".format(f)], "100000")
        assert_success(result)
        print("Registering ", f)
        register_txid = rpc.sendrawtransaction(result["hex"])
        assert register_txid, "got txid"

        # TODO: for most of the non valid oraclesregister and oraclessubscribe transactions generating and broadcasting now
        # so trying only valid oraclessubscribe atm
        result = rpc.oraclessubscribe(globals()["oracle_{}".format(f)], pubkey, "1")
        assert_success(result)
        subscribe_txid = rpc.sendrawtransaction(result["hex"])
        assert register_txid, "got txid"

    wait_some_blocks(rpc, 2)

    # now lets publish and read valid data for each oracle type

    # recording data for s type
    result = rpc.oraclesdata(globals()["oracle_{}".format("s")], "05416e746f6e")
    assert_success(result)
    oraclesdata_s = rpc.sendrawtransaction(result["hex"])
    info_s = rpc.oraclesinfo(globals()["oracle_{}".format("s")])
    batonaddr_s = info_s['registered'][0]['baton']

    # recording data for S type
    result = rpc.oraclesdata(globals()["oracle_{}".format("S")],
                             "000161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161616161")
    assert_success(result)
    oraclesdata_S = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("S")])
    batonaddr_S = info['registered'][0]['baton']

    # recording data for d type
    result = rpc.oraclesdata(globals()["oracle_{}".format("d")], "0101")
    assert_success(result)
    # baton
    oraclesdata_d = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("d")])
    batonaddr_d = info['registered'][0]['baton']

    # recording data for D type
    result = rpc.oraclesdata(globals()["oracle_{}".format("D")], "010001")
    assert_success(result)
    # baton
    oraclesdata_D = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("D")])
    batonaddr_D = info['registered'][0]['baton']

    # recording data for c type
    result = rpc.oraclesdata(globals()["oracle_{}".format("c")], "ff")
    assert_success(result)
    # baton
    oraclesdata_c = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("c")])
    batonaddr_c = info['registered'][0]['baton']

    # recording data for C type
    result = rpc.oraclesdata(globals()["oracle_{}".format("C")], "ff")
    assert_success(result)
    # baton
    oraclesdata_C = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("C")])
    batonaddr_C = info['registered'][0]['baton']

    # recording data for t type
    result = rpc.oraclesdata(globals()["oracle_{}".format("t")], "ffff")
    assert_success(result)
    # baton
    oraclesdata_t = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("t")])
    batonaddr_t = info['registered'][0]['baton']

    # recording data for T type
    result = rpc.oraclesdata(globals()["oracle_{}".format("T")], "ffff")
    assert_success(result)
    # baton
    oraclesdata_T = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("T")])
    batonaddr_T = info['registered'][0]['baton']

    # recording data for i type
    result = rpc.oraclesdata(globals()["oracle_{}".format("i")], "ffffffff")
    assert_success(result)
    # baton
    oraclesdata_i = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("i")])
    batonaddr_i = info['registered'][0]['baton']

    # recording data for I type
    result = rpc.oraclesdata(globals()["oracle_{}".format("I")], "ffffffff")
    assert_success(result)
    # baton
    oraclesdata_I = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("I")])
    batonaddr_I = info['registered'][0]['baton']

    # recording data for l type
    result = rpc.oraclesdata(globals()["oracle_{}".format("l")], "00000000ffffffff")
    assert_success(result)
    # baton
    oraclesdata_l = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("l")])
    batonaddr_l = info['registered'][0]['baton']

    # recording data for L type
    result = rpc.oraclesdata(globals()["oracle_{}".format("L")], "00000000ffffffff")
    assert_success(result)
    # baton
    oraclesdata_L = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("L")])
    batonaddr_L = info['registered'][0]['baton']

    # recording data for h type
    result = rpc.oraclesdata(globals()["oracle_{}".format("h")],
                             "00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff")
    assert_success(result)
    # baton
    oraclesdata_h = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("h")])
    batonaddr_h = info['registered'][0]['baton']

    # recording data for Ihh type
    result = rpc.oraclesdata(globals()["oracle_{}".format("Ihh")],
                             "ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff")
    assert_success(result)
    # baton
    oraclesdata_Ihh = rpc.sendrawtransaction(result["hex"])
    info = rpc.oraclesinfo(globals()["oracle_{}".format("Ihh")])
    batonaddr_Ihh = info['registered'][0]['baton']

    wait_some_blocks(rpc, 1)

    # checking data for s type
    result = rpc.oraclessamples(globals()["oracle_{}".format("s")], batonaddr_s, "1")
    assert "['Anton']" == str(result["samples"][0]['data'])

    # checking data for S type
    result = rpc.oraclessamples(globals()["oracle_{}".format("S")], batonaddr_S, "1")
    assert "['aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa']" == str(result["samples"][0]['data'])

    # checking data for d type
    result = rpc.oraclessamples(globals()["oracle_{}".format("d")], batonaddr_d, "1")
    assert "['01']" == str(result["samples"][0]['data'])

    # checking data for D type
    result = rpc.oraclessamples(globals()["oracle_{}".format("D")], batonaddr_D, "1")
    assert "['01']" == str(result["samples"][0]['data'])

    # checking data for c type
    result = rpc.oraclessamples(globals()["oracle_{}".format("c")], batonaddr_c, "1")
    assert "['-1']" == str(result["samples"][0]['data'])

    # checking data for C type
    result = rpc.oraclessamples(globals()["oracle_{}".format("C")], batonaddr_C, "1")
    assert "['255']" == str(result["samples"][0]['data'])

    # checking data for t type
    result = rpc.oraclessamples(globals()["oracle_{}".format("t")], batonaddr_t, "1")
    assert "['-1']" == str(result["samples"][0]['data'])

    # checking data for T type
    result = rpc.oraclessamples(globals()["oracle_{}".format("T")], batonaddr_T, "1")
    assert "['65535']" == str(result["samples"][0]['data'])

    # checking data for i type
    result = rpc.oraclessamples(globals()["oracle_{}".format("i")], batonaddr_i, "1")
    assert "['-1']" == str(result["samples"][0]['data'])

    # checking data for I type
    result = rpc.oraclessamples(globals()["oracle_{}".format("I")], batonaddr_I, "1")
    assert "['4294967295']" == str(result["samples"][0]['data'])

    # checking data for l type
    result = rpc.oraclessamples(globals()["oracle_{}".format("l")], batonaddr_l, "1")
    assert "['-4294967296']" == str(result["samples"][0]['data'])

    # checking data for L type
    result = rpc.oraclessamples(globals()["oracle_{}".format("L")], batonaddr_L, "1")
    assert "['18446744069414584320']" == str(result["samples"][0]['data'])

    # checking data for h type
    result = rpc.oraclessamples(globals()["oracle_{}".format("h")], batonaddr_h, "1")
    assert "['ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000']" == str(result["samples"][0]['data'])

    # checking data for Ihh type
    result = rpc.oraclessamples(globals()["oracle_{}".format("Ihh")], batonaddr_Ihh, "1")
    assert "['4294967295', 'ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000', 'ffffffff00000000ffffffff00000000ffffffff00000000ffffffff00000000']" == str(result["samples"][0]['data'])
