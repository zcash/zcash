#!/usr/bin/env python3
# Copyright (c) 2020 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

import pytest
import time
import sys
import re
sys.path.append('../')
from basic.pytest_util import validate_template, mine_and_waitconfirms, randomstring, check_synced, wait_blocks


@pytest.mark.usefixtures("proxy_connection")
class TestHeirCCcalls:

    def test_heirfund(self, test_params):
        heirfund_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        pubkey1 = test_params.get('node1').get('pubkey')
        amount = '100'
        name = 'heir' + randomstring(5)
        inactivitytime = '20'
        res = rpc1.heirfund(amount, name, pubkey1, inactivitytime, 'testMemo')
        validate_template(res, heirfund_schema)
        txid = rpc1.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(txid, rpc1)

    def test_heiraddress(self, test_params):
        heiraddress_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'HeirCCAddress': {'type': 'string'},
                'CCbalance': {'type': 'number'},
                'HeirNormalAddress': {'type': 'string'},
                'HeirCC`1of2`Address': {'type': 'string'},
                'HeirCC`1of2`TokensAddress': {'type': 'string'},
                'myAddress': {'type': 'string'},
                'myCCAddress(Heir)': {'type': 'string'},
                'PubkeyCCAddress(Heir)': {'type': 'string'},
                'myCCaddress': {'type': 'string'},
                'myCCbalance': {'type': 'number'},
                'myaddress': {'type': 'string'},
                'mybakance': {'type': 'number'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        pubkey1 = test_params.get('node1').get('pubkey')
        res = rpc1.heiraddress(pubkey1)
        validate_template(res, heiraddress_schema)
        assert res.get('result') == 'success'

    def test_heirlist(self, test_params):
        heirlist_schema = {
            'type': 'array',
            'items': {
                'type': 'string'
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.heirlist()
        validate_template(res, heirlist_schema)

    def test_heiradd(self, test_params):
        hieradd_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        amount = '100'
        try:
            fundid = rpc1.heirlist()[0]
            res = rpc1.heiradd(amount, fundid)
            validate_template(res, hieradd_schema)
            assert res.get('result') == 'success'
            txid = rpc1.sendrawtransaction(res.get('hex'))
            mine_and_waitconfirms(txid, rpc1)
        except IndexError:
            print('\nNo heirplan on chain, creating one\n')
            pubkey1 = test_params.get('node1').get('pubkey')
            amount = '100'
            name = 'heir' + randomstring(5)
            inactivitytime = '20'
            res = rpc1.heirfund(amount, name, pubkey1, inactivitytime, 'testMemoHeirInfo')
            txid = rpc1.sendrawtransaction(res.get('hex'))
            mine_and_waitconfirms(txid, rpc1)
            fundid = rpc1.heirlist()[0]
            res = rpc1.heiradd(amount, fundid)
            validate_template(res, hieradd_schema)
            assert res.get('result') == 'success'
            txid = rpc1.sendrawtransaction(res.get('hex'))
            mine_and_waitconfirms(txid, rpc1)

    def test_heirinfo(self, test_params):
        heirinfo_schema = {
            'type': 'object',
            'properties': {
                'name': {'type': 'string'},
                'fundingtxid': {'type': 'string'},
                'owner': {'type': 'string'},
                'tokenid': {'type': 'string'},
                'heir': {'type': 'string'},
                'type': {'type': 'string'},
                'lifetime': {'type': 'string'},
                'available': {'type': 'string'},
                'OwnerRemainderTokens': {'type': 'string'},
                'InactivityTimeSetting': {'type': 'string'},
                'IsHeirSpendingAllowed': {'type': 'string'},
                'memo': {'type': 'string'},
                'result': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        try:
            fundid = rpc1.heirlist()[0]
            res = rpc1.heirinfo(fundid)
            validate_template(res, heirinfo_schema)
            assert res.get('result') == 'success'
        except IndexError:
            print('\nNo heirplan on chain, creating one\n')
            pubkey1 = test_params.get('node1').get('pubkey')
            amount = '100'
            name = 'heir' + randomstring(5)
            inactivitytime = '20'
            res = rpc1.heirfund(amount, name, pubkey1, inactivitytime, 'testMemoHeirInfo')
            txid = rpc1.sendrawtransaction(res.get('hex'))
            mine_and_waitconfirms(txid, rpc1)
            fundid = rpc1.heirlist()[0]
            res = rpc1.heirinfo(fundid)
            validate_template(res, heirinfo_schema)
            assert res.get('result') == 'success'

    def test_heirclaim(self, test_params):
        heirclaim_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        # create heir plan to claim
        pubkey1 = test_params.get('node2').get('pubkey')
        amount = '100'
        name = 'heir' + randomstring(5)
        inactivitytime = '120'
        res = rpc1.heirfund(amount, name, pubkey1, inactivitytime, 'testMemo')
        fundtxid = rpc1.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(fundtxid, rpc1)

        # Wait inactivitytime and claim funds
        time.sleep(int(inactivitytime))
        print("\n Sleeping for inactivity time")
        res = rpc1.heirclaim(amount, fundtxid)
        validate_template(res, heirclaim_schema)
        claimtxid = rpc1.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(claimtxid, rpc1)


@pytest.mark.usefixtures("proxy_connection")
class TestHeirFunc:

    def test_heir_addresses(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        pubkey = test_params.get('node2').get('pubkey')
        address_pattern = re.compile(r"R[a-zA-Z0-9]{33}\Z")  # normal R-addr

        # verify all keys look like valid AC addrs
        res = rpc1.faucetaddress('')
        for key in res.keys():
            if key.find('ddress') > 0:
                assert address_pattern.match(str(res.get(key)))

        # test that additional CCaddress key is returned
        res = rpc1.faucetaddress(pubkey)
        for key in res.keys():
            if key.find('ddress') > 0:
                assert address_pattern.match(str(res.get(key)))

    def test_heir_flow(self, test_params):
        # Check basic heirCC flow from fund to claim
        # Create valid heir plan
        rpc1 = test_params.get('node1').get('rpc')
        pubkey1 = test_params.get('node1').get('pubkey')
        rpc2 = test_params.get('node2').get('rpc')
        pubkey2 = test_params.get('node2').get('pubkey')
        inactivitytime = 70  # ideally, should be a bit more than blocktime
        amount = 777
        plan_name = 'heir' + randomstring(5)
        comment = 'HeirFlowTest' + randomstring(5)
        res = rpc1.heirfund(str(amount), plan_name, pubkey2,
                            str(inactivitytime), comment)
        fundtx = rpc1.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(fundtx, rpc1)
        time.sleep(5)

        # Check plan availability in heirlist
        res = rpc1.heirlist()
        assert fundtx in res

        # Check plan info
        res = rpc1.heirinfo(fundtx)
        assert res.get('result') == 'success'
        # check here stuff
        assert res['fundingtxid'] == fundtx
        assert res['name'] == plan_name
        assert res['owner'] == pubkey1
        assert res['heir'] == pubkey2
        assert res['memo'] == comment
        assert res['lifetime'] == str(amount) + '.00000000'
        assert res['type'] == "coins"
        assert res['InactivityTimeSetting'] == str(inactivitytime)

        # Check Heir spending allowed after inactivity time
        print("\n Sleeping for inactivity time")
        time.sleep(inactivitytime + 1)
        wait_blocks(rpc1, 2)
        check_synced(rpc1, rpc2)  # prevents issues when inactivity time =< block time
        res = rpc1.heirinfo(fundtx)
        assert res['lifetime'] == str(amount) + '.00000000'
        assert res['available'] == str(amount) + '.00000000'
        assert res['IsHeirSpendingAllowed'] == 'true'

        # Claim all available funds from hier node
        res = rpc2.heirclaim(str(amount), fundtx)
        assert res.get('result') == 'success'
        claimtx = rpc2.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(claimtx, rpc2)
        time.sleep(5)
        # Check claim success
        # Wait sync
        wait_blocks(rpc1, 2)
        check_synced(rpc1, rpc2)
        res = rpc1.heirinfo(fundtx)
        assert res['lifetime'] == str(amount) + '.00000000'
        assert res['available'] == '0.00000000'
        res = rpc2.heirinfo(fundtx)
        assert res['lifetime'] == str(amount) + '.00000000'
        assert res['available'] == '0.00000000'

    def test_heir_tokens_flow(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        pubkey1 = test_params.get('node1').get('pubkey')
        rpc2 = test_params.get('node2').get('rpc')
        pubkey2 = test_params.get('node2').get('pubkey')
        inactivitytime = 70  # ideally, should be a bit more than blocktime
        amount = 100000000
        plan_name = 'heir' + randomstring(5)
        comment = 'HeirFlowTest' + randomstring(5)
        token_name = 'token' + randomstring(5)

        # Create on-chain tokens
        res = rpc1.tokencreate(token_name, '1', 'heirCCTest')
        tokentx = rpc1.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(tokentx, rpc1)
        res = rpc1.tokenbalance(tokentx, pubkey1)['balance']
        assert res == amount  # validate init tokenbalance

        res = rpc1.heirfund(str(amount), plan_name, pubkey2,
                            str(inactivitytime), comment, tokentx)
        fundtx = rpc1.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(fundtx, rpc1)
        time.sleep(5)

        # Check plan availability in heirlist
        res = rpc1.heirlist()
        assert fundtx in res

        # Check plan info
        res = rpc1.heirinfo(fundtx)
        assert res.get('result') == 'success'
        # check here stuff
        assert res['fundingtxid'] == fundtx
        assert res['name'] == plan_name
        assert res['owner'] == pubkey1
        assert res['heir'] == pubkey2
        assert res['memo'] == comment
        assert res['lifetime'] == str(amount)
        assert res['tokenid'] == tokentx
        assert res['type'] == 'tokens'
        assert res['InactivityTimeSetting'] == str(inactivitytime)

        # Check Heir spending allowed after inactivity time
        print("\n Sleeping for inactivity time")
        time.sleep(inactivitytime + 1)
        wait_blocks(rpc1, 2)
        check_synced(rpc1, rpc2)
        res = rpc1.heirinfo(fundtx)
        assert res['lifetime'] == str(amount)
        assert res['available'] == str(amount)
        assert res['IsHeirSpendingAllowed'] == 'true'

        # Claim all available funds from hier node
        res = rpc2.heirclaim(str(amount), fundtx)
        assert res.get('result') == 'success'
        claimtx = rpc2.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(claimtx, rpc2)
        time.sleep(5)
        # Check claim success
        # Wait sync
        wait_blocks(rpc1, 1)
        check_synced(rpc1, rpc2)
        res = rpc1.heirinfo(fundtx)
        assert res['lifetime'] == str(amount)
        assert res['available'] == '0'
        res = rpc2.heirinfo(fundtx)
        assert res['lifetime'] == str(amount)
        assert res['available'] == '0'

        # Check heir balance after claim
        res = rpc2.tokenbalance(tokentx, pubkey2)['balance']
        assert res == amount
