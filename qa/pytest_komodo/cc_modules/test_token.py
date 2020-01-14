#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import time

from util import assert_success, assert_error, check_if_mined,\
    send_and_mine, rpc_connect, wait_some_blocks, komodo_teardown


@pytest.mark.usefixtures("proxy_connection")
def test_token(test_params):

    # test params inits
    rpc = test_params.get('node1').get('rpc')
    rpc1 = test_params.get('node2').get('rpc')

    pubkey = test_params.get('node1').get('pubkey')
    pubkey1 = test_params.get('node2').get('pubkey')

    is_fresh_chain = test_params.get("is_fresh_chain")

    result = rpc.tokenaddress()
    assert_success(result)
    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    result = rpc.tokenaddress(pubkey)
    assert_success(result)
    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    result = rpc.assetsaddress()
    assert_success(result)
    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    result = rpc.assetsaddress(pubkey)
    assert_success(result)
    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    # there are no tokens created yet
    # TODO: this test conflicts with heir test because token creating for heir
#    if is_fresh_chain:
#        result = rpc.tokenlist()
#        assert result == []

    # trying to create token with negative supply
    result = rpc.tokencreate("NUKE", "-1987420", "no bueno supply")
    assert_error(result)

    # creating token with name more than 32 chars
    result = rpc.tokencreate("NUKE123456789012345678901234567890", "1987420", "name too long")
    assert_error(result)

    # creating valid token
    result = rpc.tokencreate("DUKE", "1987.420", "Duke's custom token")
    assert_success(result)

    tokenid = send_and_mine(result['hex'], rpc)

    result = rpc.tokenlist()
    assert tokenid in result

    # there are no token orders yet
    if is_fresh_chain:
        result = rpc.tokenorders(tokenid)
        assert result == []

    # getting token balance for non existing tokenid
    result = rpc.tokenbalance(pubkey)
    assert_error(result)

    # get token balance for token with pubkey
    result = rpc.tokenbalance(tokenid, pubkey)
    assert_success(result)
    assert result['balance'] == 198742000000
    assert result['tokenid'] ==  tokenid

    # get token balance for token without pubkey
    result = rpc.tokenbalance(tokenid)
    assert_success(result)
    assert result['balance'] == 198742000000
    assert result['tokenid'] == tokenid

    # this is not a valid assetid
    result = rpc.tokeninfo(pubkey)
    assert_error(result)

    # check tokeninfo for valid token
    result = rpc.tokeninfo(tokenid)
    assert_success(result)
    assert result['tokenid'] == tokenid
    assert result['owner'] == pubkey
    assert result['name'] == "DUKE"
    assert result['supply'] == 198742000000
    assert result['description'] == "Duke's custom token"

    # invalid numtokens ask
    result = rpc.tokenask("-1", tokenid, "1")
    assert_error(result)

    # invalid numtokens ask
    result = rpc.tokenask("0", tokenid, "1")
    assert_error(result)

    # invalid price ask
    result = rpc.tokenask("1", tokenid, "-1")
    assert_error(result)

    # invalid price ask
    result = rpc.tokenask("1", tokenid, "0")
    assert_error(result)

    # invalid tokenid ask
    result = rpc.tokenask("100", "deadbeef", "1")
    assert_error(result)

    # valid ask
    tokenask = rpc.tokenask("100", tokenid, "7.77")
    tokenaskhex = tokenask['hex']
    tokenaskid = send_and_mine(tokenask['hex'], rpc)
    result = rpc.tokenorders(tokenid)
    order = result[0]
    assert order, "found order"

    # invalid ask fillunits
    result = rpc.tokenfillask(tokenid, tokenaskid, "0")
    assert_error(result)

    # invalid ask fillunits
    result = rpc.tokenfillask(tokenid, tokenaskid, "-777")
    assert_error(result)

    # valid ask fillunits
    fillask = rpc.tokenfillask(tokenid, tokenaskid, "777")
    result = send_and_mine(fillask['hex'], rpc)
    txid = result[0]
    assert txid, "found txid"

    # should be no token orders
    result = rpc.tokenorders(tokenid)
    assert result == []

    # checking ask cancellation
    testorder = rpc.tokenask("100", tokenid, "7.77")
    testorderid = send_and_mine(testorder['hex'], rpc)
    # from other node (ensuring that second node have enough balance to cover txfee
    # to get the actual error - not "not enough balance" one
    rpc.sendtoaddress(rpc1.getnewaddress(), 1)
    time.sleep(10)  # to ensure transactions are in different blocks
    rpc.sendtoaddress(rpc1.getnewaddress(), 1)
    wait_some_blocks(rpc, 2)
    result = rpc1.getbalance()
    assert result > 0.1

    result = rpc1.tokencancelask(tokenid, testorderid)
    assert_error(result)

    # from valid node
    cancel = rpc.tokencancelask(tokenid, testorderid)
    send_and_mine(cancel["hex"], rpc)

    # TODO: should be no ask in orders - bad test
    if is_fresh_chain:
        result = rpc.tokenorders(tokenid)
        assert result == []

    # invalid numtokens bid
    result = rpc.tokenbid("-1", tokenid, "1")
    assert_error(result)

    # invalid numtokens bid
    result = rpc.tokenbid("0", tokenid, "1")
    assert_error(result)

    # invalid price bid
    result = rpc.tokenbid("1", tokenid, "-1")
    assert_error(result)

    # invalid price bid
    result = rpc.tokenbid("1", tokenid, "0")
    assert_error(result)

    # invalid tokenid bid
    result = rpc.tokenbid("100", "deadbeef", "1")
    assert_error(result)

    tokenbid = rpc.tokenbid("100", tokenid, "10")
    tokenbidhex = tokenbid['hex']
    tokenbidid = send_and_mine(tokenbid['hex'], rpc)
    result = rpc.tokenorders(tokenid)
    order = result[0]
    assert order, "found order"

    # invalid bid fillunits
    result = rpc.tokenfillbid(tokenid, tokenbidid, "0")
    assert_error(result)

    # invalid bid fillunits
    result = rpc.tokenfillbid(tokenid, tokenbidid, "-777")
    assert_error(result)

    # valid bid fillunits
    fillbid = rpc.tokenfillbid(tokenid, tokenbidid, "1000")
    result = send_and_mine(fillbid['hex'], rpc)
    txid = result[0]
    assert txid, "found txid"

    # should be no token orders
    # TODO: should be no bid in orders - bad test
    if is_fresh_chain:
        result = rpc.tokenorders(tokenid)
        assert result == []

    # checking bid cancellation
    testorder = rpc.tokenbid("100", tokenid, "7.77")
    testorderid = send_and_mine(testorder['hex'], rpc)

    # from other node
    result = rpc1.getbalance()
    assert result > 0.1

    result = rpc1.tokencancelbid(tokenid, testorderid)
    #TODO: not throwing error now on tx generation
    #assert_error(result)

    # from valid node
    cancel = rpc.tokencancelbid(tokenid, testorderid)
    send_and_mine(cancel["hex"], rpc)
    result = rpc.tokenorders(tokenid)
    assert result == []

    # invalid token transfer amount (have to add status to CC code!)
    randompubkey = "021a559101e355c907d9c553671044d619769a6e71d624f68bfec7d0afa6bd6a96"
    result = rpc.tokentransfer(tokenid, randompubkey, "0")
    assert_error(result)

    # invalid token transfer amount (have to add status to CC code!)
    result = rpc.tokentransfer(tokenid, randompubkey, "-1")
    assert_error(result)

    # valid token transfer
    sendtokens = rpc.tokentransfer(tokenid, randompubkey, "1")
    send_and_mine(sendtokens["hex"], rpc)
    result = rpc.tokenbalance(tokenid, randompubkey)
    assert result["balance"] == 1
