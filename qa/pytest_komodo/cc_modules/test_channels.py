#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import time

from util import assert_success, assert_error, check_if_mined, send_and_mine,\
    rpc_connect, wait_some_blocks, generate_random_string, komodo_teardown


@pytest.mark.usefixtures("proxy_connection")
def test_channels(test_params):

    # test params inits
    rpc = test_params.get('node1').get('rpc')
    rpc1 = test_params.get('node2').get('rpc')

    pubkey = test_params.get('node1').get('pubkey')
    pubkey1 = test_params.get('node2').get('pubkey')

    is_fresh_chain = test_params.get("is_fresh_chain")

    """!!! for testing needed test daemon which built with custom flag
    export CONFIGURE_FLAGS='CPPFLAGS=-DTESTMODE'
    since in usual mode 101 confirmations are needed for payment/refund
    """

    # checking channelsaddress call

    result = rpc.channelsaddress(pubkey)
    assert_success(result)
    # test that additional CCaddress key is returned

    for x in result.keys():
        if x.find('ddress') > 0:
            assert result[x][0] == 'R'

    # getting empty channels list

    result = rpc.channelslist()
    assert result["result"] == "success"
    assert result["name"] == "Channels List"
    if is_fresh_chain:
        assert len(result) == 2

    # 10 payments, 100000 sat denomination channel opening with second node pubkey
    new_channel_hex = rpc.channelsopen(pubkey1, "10", "100000")
    assert_success(new_channel_hex)
    channel_txid = send_and_mine(new_channel_hex["hex"], rpc)
    assert channel_txid, "got channel txid"

    # checking if our new channel in common channels list
    if is_fresh_chain:
        result = rpc.channelslist()
        assert len(result) == 3

    # checking info about channel directly
    result = rpc.channelsinfo(channel_txid)
    assert_success(result)
    assert result["Transactions"][0]["Open"] == channel_txid

    # open transaction should be confirmed at least twice
    wait_some_blocks(rpc, 3)

    # trying to make wrong denomination channel payment
    result = rpc.channelspayment(channel_txid, "199000")
    assert_error(result)

    # trying to make 0 channel payment
    result = rpc.channelspayment(channel_txid, "0")
    assert_error(result)

    # trying to make negative channel payment
    result = rpc.channelspayment(channel_txid, "-1")
    assert_error(result)

    # valid channel payment
    result = rpc.channelspayment(channel_txid, "100000")
    assert_success(result)
    payment_tx_id = send_and_mine(result["hex"], rpc)
    assert payment_tx_id, "got txid"

    # now in channelinfo payment information should appear
    result = rpc.channelsinfo(channel_txid)
    assert result["Transactions"][1]["Payment"] == payment_tx_id

    # number of payments should be equal 1 (one denomination used)
    result = rpc.channelsinfo(channel_txid)["Transactions"][1]["Number of payments"]
    assert result == 1
    # payments left param should reduce 1 and be equal 9 now ( 10 - 1 = 9 )
    result = rpc.channelsinfo(channel_txid)["Transactions"][1]["Payments left"]
    assert result == 9

    # lets try payment with x2 amount to ensure that counters works correct
    result = rpc.channelspayment(channel_txid, "200000")
    assert_success(result)
    payment_tx_id = send_and_mine(result["hex"], rpc)
    assert payment_tx_id, "got txid"

    result = rpc.channelsinfo(channel_txid)
    assert result["Transactions"][2]["Payment"] == payment_tx_id

    result = rpc.channelsinfo(channel_txid)["Transactions"][2]["Number of payments"]
    assert result == 2

    result = rpc.channelsinfo(channel_txid)["Transactions"][2]["Payments left"]
    assert result == 7

    # check if payment value really transferred
    raw_transaction = rpc.getrawtransaction(payment_tx_id, 1)

    result = raw_transaction["vout"][3]["valueSat"]
    assert result == 200000

    result = rpc1.validateaddress(raw_transaction["vout"][3]["scriptPubKey"]["addresses"][0])["ismine"]
    assert result

    # have to check that second node have coins to cover txfee at least
    rpc.sendtoaddress(rpc1.getnewaddress(), 1)
    time.sleep(10)  # to ensure transactions are in different blocks
    rpc.sendtoaddress(rpc1.getnewaddress(), 1)
    wait_some_blocks(rpc, 2)
    result = rpc1.getbalance()
    assert result > 0.1

    # trying to initiate channels payment from node B without any secret
    # TODO: have to add RPC validation
    payment_hex = rpc1.channelspayment(channel_txid, "100000")
    try:
        result = rpc1.sendrawtransaction(payment_hex["hex"])
    except Exception as e:
        pass

    # trying to initiate channels payment from node B with secret from previous payment
    result = rpc1.channelspayment(channel_txid, "100000", rpc1.channelsinfo(channel_txid)["Transactions"][1]["Secret"])
    # result = rpc1.sendrawtransaction(payment_hex["hex"])
    assert_error(result)

    # executing channel close
    result = rpc.channelsclose(channel_txid)
    # TODO: by some reason channels close just returning hex instead of result and hex json
    channel_close_txid = send_and_mine(result, rpc)
    assert channel_close_txid, "got txid"

    wait_some_blocks(rpc, 2)

    # now in channelinfo closed flag should appear
    result = rpc.channelsinfo(channel_txid)
    assert result["Transactions"][3]["Close"] == channel_close_txid

    # executing channel refund
    result = rpc.channelsrefund(channel_txid, channel_close_txid)
    # TODO: by some reason channels refund just returning hex instead of result and hex json
    refund_txid = send_and_mine(result, rpc)
    assert refund_txid, "got txid"

    # checking if it refunded to opener address
    raw_transaction = rpc.getrawtransaction(refund_txid, 1)

    result = raw_transaction["vout"][2]["valueSat"]
    assert result == 700000

    result = rpc.validateaddress(raw_transaction["vout"][2]["scriptPubKey"]["addresses"][0])["ismine"]
    assert result

    # creating and draining channel (10 payment by 100000 satoshies in total to fit full capacity)
    new_channel_hex1 = rpc.channelsopen(pubkey1, "10", "100000")
    assert_success(new_channel_hex1)
    channel1_txid = send_and_mine(new_channel_hex1["hex"], rpc)
    assert channel1_txid, "got channel txid"

    # need to have 2+ confirmations in the test mode
    wait_some_blocks(rpc, 2)

    # TODO: maybe it's possible to send in single block to not wait 10 blocks?
    for i in range(10):
        result = rpc.channelspayment(channel1_txid, "100000")
        assert_success(result)
        payment_tx_id = send_and_mine(result["hex"], rpc)
        assert payment_tx_id, "got txid"

    # last payment should indicate that 0 payments left
    result = rpc.channelsinfo(channel1_txid)["Transactions"][10]["Payments left"]
    assert result == 0

    # no more payments possible
    result = rpc.channelspayment(channel1_txid, "100000")
    assert_error(result)

# TODO: fixme
#
#    # creating new channel to test the case when node B initiate payment when node A revealed secret in offline
#    # 10 payments, 100000 sat denomination channel opening with second node pubkey
#    new_channel_hex2 = rpc.channelsopen(pubkey1, "10", "100000")
#    assert_success(new_channel_hex)
#    channel2_txid = send_and_mine(new_channel_hex2["hex"], rpc)
#    assert channel2_txid, "got channel txid"
#
#    wait_some_blocks(rpc, 2)
#
#    # disconnecting first node from network
#    rpc.setban("127.0.0.0/24", "add")
#    assert rpc.getinfo()["connections"] == 0
#    assert rpc1.getinfo()["connections"] == 0
#
#    # sending one payment to mempool to reveal the secret but not mine it
#    payment_hex = rpc.channelspayment(channel2_txid, "100000")
#    result = rpc.sendrawtransaction(payment_hex["hex"])
#    assert result, "got payment txid"
#
#    secret = rpc.channelsinfo(channel2_txid)["Transactions"][1]["Secret"]
#    assert secret, "Secret revealed"
#
#    # secret shouldn't be available for node B
#    secret_not_revealed = None
#    try:
#        rpc1.channelsinfo(channel2_txid)["Transactions"][1]["Secret"]
#    except Exception:
#        secret_not_revealed
#    assert secret_not_revealed
#
#    # trying to initiate payment from second node with revealed secret
#    assert rpc1.getinfo()["connections"] == 0
#    dc_payment_hex = rpc1.channelspayment(channel2_txid, "100000", secret)
#    assert_success(dc_payment_hex)
#    result = rpc1.sendrawtransaction(dc_payment_hex["hex"])
#    assert result, "got channelspayment transaction id"
