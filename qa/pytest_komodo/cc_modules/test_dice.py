#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import json

from util import assert_success, assert_error, check_if_mined, send_and_mine,\
    rpc_connect, wait_some_blocks, generate_random_string, komodo_teardown


@pytest.mark.usefixtures("proxy_connection")
def test_dice(test_params):

    # test params inits
    rpc = test_params.get('node1').get('rpc')
    rpc1 = test_params.get('node2').get('rpc')

    pubkey = test_params.get('node1').get('pubkey')
    pubkey1 = test_params.get('node2').get('pubkey')

    is_fresh_chain = test_params.get("is_fresh_chain")


    # second node should have some balance to place bets
    result = rpc1.getbalance()
    assert result > 99

    result = rpc.diceaddress()
    assert result['result'] == 'success'

    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    result = rpc.diceaddress(pubkey)
    for x in result.keys():
        print(x + ": " + str(result[x]))
    assert result['result'] == 'success'

    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    # no dice created yet
    if is_fresh_chain:
        result = rpc.dicelist()
        assert result == []

    # set dice name for futher usage
    dicename = generate_random_string(5)

    # creating dice plan with too long name (>8 chars)
    result = rpc.dicefund("THISISTOOLONG", "10000", "10", "10000", "10", "5")
    assert_error(result)

    # creating dice plan with < 100 funding
    result = rpc.dicefund(dicename, "10", "1", "10000", "10", "5")
    assert_error(result)

    # creating dice plan with 0 blocks timeout
    result = rpc.dicefund(dicename, "10", "1", "10000", "10", "0")
    assert_error(result)

    # creating dice plan
    dicefundtx = rpc.dicefund(dicename, "1000", "1", "800", "10", "5")
    diceid = send_and_mine(dicefundtx['hex'], rpc)

    # checking if it in plans list now
    result = rpc.dicelist()
    assert diceid in result

    # adding zero funds to plan
    result = rpc.diceaddfunds(dicename, diceid, "0")
    assert_error(result)

    # adding negative funds to plan
    result = rpc.diceaddfunds(dicename, diceid, "-1")
    assert_error(result)

    # adding funds to plan
    addfundstx = rpc.diceaddfunds(dicename, diceid, "1100")
    result = send_and_mine(addfundstx['hex'], rpc)

    # checking if funds added to plan
    result = rpc.diceinfo(diceid)
    assert result["funding"] == "2100.00000000"

    # not valid dice info checking
    result = rpc.diceinfo("invalid")
    assert_error(result)

    # placing 0 amount bet
    result = rpc1.dicebet(dicename, diceid, "0", "2")
    assert_error(result)

    # placing negative amount bet
    result = rpc1.dicebet(dicename, diceid, "-1", "2")
    assert_error(result)

    # placing bet more than maxbet
    result = rpc1.dicebet(dicename, diceid, "900", "2")
    assert_error(result)

    # placing bet with amount more than funding
    result = rpc1.dicebet(dicename, diceid, "3000", "2")
    assert_error(result)

    # placing bet with potential won more than funding
    result = rpc1.dicebet(dicename, diceid, "750", "9")
    assert_error(result)

    # placing 0 odds bet
    result = rpc1.dicebet(dicename, diceid, "1", "0")
    assert_error(result)

    # placing negative odds bet
    result = rpc1.dicebet(dicename, diceid, "1", "-1")
    assert_error(result)

    # placing bet with odds more than allowed
    result = rpc1.dicebet(dicename, diceid, "1", "11")
    assert_error(result)

    # placing bet with not correct dice name
    result = rpc1.dicebet("nope", diceid, "100", "2")
    assert_error(result)

    # placing bet with not correct dice id
    result = rpc1.dicebet(dicename, pubkey, "100", "2")
    assert_error(result)

    # TODO: fixme
    # # have to make some entropy for the next test
    # entropytx = 0
    # fundingsum = 1
    # while entropytx < 110:
    #     fundingsuminput = str(fundingsum)
    #     fundinghex = rpc.diceaddfunds(dicename, diceid, fundingsuminput)
    #     entropytx = entropytx + 1
    #     fundingsum = fundingsum + 1
    #     if entropytx < 109:
    #         result = rpc.sendrawtransaction(fundinghex['hex'])
    #     else:
    #         result = send_and_mine(fundinghex['hex'], rpc)
    #
    # wait_some_blocks(rpc, 2)
    #
    # # valid bet placing
    # placebet = rpc1.dicebet(dicename, diceid, "101", "3")
    # print(placebet)
    # betid = send_and_mine(placebet["hex"], rpc1)
    # assert result, "bet placed"
    #
    # # check bet status
    # result = rpc1.dicestatus(dicename, diceid, betid)
    # assert_success(result)
    #
    # # note initial dice funding state at this point.
    # # TODO: track player balance somehow (hard to do because of mining and fees)
    # diceinfo = rpc.diceinfo(diceid)
    # funding = float(diceinfo['funding'])
    #
    #
    # # placing  same amount bets with amount 1 and odds  1:3, checking if balance changed correct
    # losscounter = 0
    # wincounter = 0
    # betcounter = 0
    #
    # while (betcounter < 10):
    #     placebet = rpc1.dicebet(dicename,diceid,"1","2")
    #     betid = self.send_and_mine(placebet["hex"], rpc1)
    #     time.sleep(3)
    #     self.sync_all()
    #     finish = rpc.dicefinish(dicename,diceid,betid)
    #     self.send_and_mine(finish["hex"], rpc1)
    #     self.sync_all()
    #     time.sleep(3)
    #     betresult = rpc1.dicestatus(dicename,diceid,betid)
    #     betcounter = betcounter + 1
    #     if betresult["status"] == "loss":
    #         losscounter = losscounter + 1
    #     elif betresult["status"] == "win":
    #         wincounter = wincounter + 1
    #     else:
    #         pass
    #
    # # funding balance should increase if player loss, decrease if player won
    # fundbalanceguess = funding + losscounter - wincounter * 2
    # fundinfoactual = rpc.diceinfo(diceid)
    # assert_equal(round(fundbalanceguess),round(float(fundinfoactual['funding'])))
