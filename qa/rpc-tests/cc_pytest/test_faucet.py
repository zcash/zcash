#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import json

from util import assert_success, assert_error, check_if_mined, send_and_mine, rpc_connect

@pytest.mark.first
def test_faucet():

    # test params inits
    with open('test_config.json', 'r') as f:
        params_dict = json.load(f)

    node1_params = params_dict["node1"]
    node2_params = params_dict["node2"]

    rpc = rpc_connect(node1_params["rpc_user"], node1_params["rpc_password"], node1_params["rpc_ip"], node1_params["rpc_port"])
    rpc1 = rpc_connect(node2_params["rpc_user"], node2_params["rpc_password"], node2_params["rpc_ip"], node2_params["rpc_port"])
    pubkey = node1_params["pubkey"]
    pubkey1 = node2_params["pubkey"]

    is_fresh_chain = params_dict["is_fresh_chain"]

    # faucet got only one entity per chain

    if is_fresh_chain:
        # basic sanity tests
        result = rpc.getinfo()
        assert result, "got response"

        result = rpc1.getinfo()
        assert result, "got response"

        result = rpc.getwalletinfo()
        assert result['balance'] > 0.0
        balance = result['balance']

        result = rpc.faucetaddress()
        assert result['result'] == 'success'

        # verify all keys look like valid AC addrs, could be better
        for x in result.keys():
            if x.find('ddress') > 0:
                assert result[x][0] == 'R'

        result = rpc.faucetaddress(pubkey)
        assert_success(result)
        for x in result.keys():
            print(x + ": " + str(result[x]))
            # test that additional CCaddress key is returned

        for x in result.keys():
            if x.find('ddress') > 0:
                assert result[x][0] == 'R'

        # no funds in the faucet yet
        result = rpc.faucetget()
        assert_error(result)

        result = rpc.faucetinfo()
        assert_success(result)

        result = rpc.faucetfund("0")
        assert_error(result)

        result = rpc.faucetfund("-1")
        assert_error(result)

        # we need at least 1 + txfee to get
        result = rpc.faucetfund("2")
        assert_success(result)
        assert result['hex'], "hex key found"

        # broadcast the xtn
        result = rpc.sendrawtransaction(result['hex'])
        txid = result
        assert txid, "found txid"
        # we need the tx above to be confirmed in the next block
        check_if_mined(rpc, txid)

        result = rpc.getwalletinfo()
        balance2 = result['balance']
        # make sure our balance is less now
        # TODO: this check not working at the moment because of the mining rewards
        # assert balance > balance2

        result = rpc.faucetinfo()
        assert_success(result)
        assert float(result['funding']) > 0

        # claiming faucet on second node
        # TODO: to pass it we should increase default timeout in rpclib
        # or sometimes we'll get such pycurl.error: (28, 'Operation timed out after 30000 milliseconds with 0 bytes received')
        #faucetgethex = rpc1.faucetget()
        #assert_success(faucetgethex)
        #assert faucetgethex['hex'], "hex key found"

        balance1 = rpc1.getwalletinfo()['balance']

        # TODO: this will not work now in tests suite because node2 mine too
        # try to broadcast the faucetget transaction
        #result = send_and_mine(faucetgethex['hex'], rpc1)
        #assert txid, "transaction broadcasted"

        #balance2 = rpc1.getwalletinfo()['balance']
        #assert balance2 > balance1