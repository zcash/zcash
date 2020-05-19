#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import json
from util import assert_success, assert_error, check_if_mined, send_and_mine,\
    rpc_connect, wait_some_blocks, generate_random_string, komodo_teardown


@pytest.mark.usefixtures("proxy_connection")
def test_rewards(test_params):

    # test params inits
    rpc = test_params.get('node1').get('rpc')
    rpc1 = test_params.get('node2').get('rpc')

    pubkey = test_params.get('node1').get('pubkey')
    pubkey1 = test_params.get('node2').get('pubkey')

    is_fresh_chain = test_params.get("is_fresh_chain")

    global proxy
    proxy = [rpc, rpc1]

    result = rpc.rewardsaddress()
    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    result = rpc.rewardsaddress(pubkey)
    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    # no rewards yet
    if is_fresh_chain:
        result = rpc.rewardslist()
        assert result == []

    # looking up non-existent reward should return error
    result = rpc.rewardsinfo("none")
    assert_error(result)

    # creating rewards plan with name > 8 chars, should return error
    result = rpc.rewardscreatefunding("STUFFSTUFF", "7777", "25", "0", "10", "10")
    assert_error(result)

    # creating rewards plan with 0 funding
    result = rpc.rewardscreatefunding("STUFF", "0", "25", "0", "10", "10")
    assert_error(result)

    # creating rewards plan with 0 maxdays
    result = rpc.rewardscreatefunding("STUFF", "7777", "25", "0", "10", "0")
    assert_error(result)

    # creating rewards plan with > 25% APR
    result = rpc.rewardscreatefunding("STUFF", "7777", "30", "0", "10", "10")
    assert_error(result)

    # creating valid rewards plan
    plan_name = generate_random_string(6)
    result = rpc.rewardscreatefunding(plan_name, "7777", "25", "0", "10", "10")
    assert result['hex'], 'got raw xtn'
    fundingtxid = send_and_mine(result['hex'], rpc)
    assert fundingtxid, 'got txid'
    result = rpc.rewardsinfo(fundingtxid)
    assert_success(result)
    assert result['name'] == plan_name
    assert result['APR'] == "25.00000000"
    assert result['minseconds'] == 0
    assert result['maxseconds'] == 864000
    assert result['funding'] == "7777.00000000"
    assert result['mindeposit'] == "10.00000000"
    assert result['fundingtxid'] == fundingtxid

    # checking if new plan in rewardslist
    result = rpc.rewardslist()
    assert fundingtxid in result

    # creating reward plan with already existing name, should return error
    result = rpc.rewardscreatefunding(plan_name, "7777", "25", "0", "10", "10")
    assert_error(result)

    # add funding amount must be positive
    result = rpc.rewardsaddfunding(plan_name, fundingtxid, "-1")
    assert_error(result)

    # add funding amount must be positive
    result = rpc.rewardsaddfunding(plan_name, fundingtxid, "0")
    assert_error(result)

    # adding valid funding
    result = rpc.rewardsaddfunding(plan_name, fundingtxid, "555")
    addfundingtxid = send_and_mine(result['hex'], rpc)
    assert addfundingtxid, 'got funding txid'

    # checking if funding added to rewardsplan
    result = rpc.rewardsinfo(fundingtxid)
    assert result['funding'] == "8332.00000000"

    # trying to lock funds, locking funds amount must be positive
    result = rpc.rewardslock(plan_name, fundingtxid, "-5")
    assert_error(result)

    # trying to lock funds, locking funds amount must be positive
    result = rpc.rewardslock(plan_name, fundingtxid, "0")
    assert_error(result)

    # trying to lock less than the min amount is an error
    result = rpc.rewardslock(plan_name, fundingtxid, "7")
    assert_error(result)

    # locking funds in rewards plan
    result = rpc.rewardslock(plan_name, fundingtxid, "10")
    assert_success(result)
    locktxid = result['hex']
    assert locktxid, "got lock txid"

    # locktxid has not been broadcast yet
    result = rpc.rewardsunlock(plan_name, fundingtxid, locktxid)
    assert_error(result)

    # broadcast xtn
    txid = rpc.sendrawtransaction(locktxid)
    assert txid, 'got txid from sendrawtransaction'

    # confirm the xtn above
    wait_some_blocks(rpc, 1)

    # will not unlock since reward amount is less than tx fee
    result = rpc.rewardsunlock(plan_name, fundingtxid, locktxid)
    assert_error(result)
