#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import time
from basic.pytest_util import validate_template, mine_and_waitconfirms, validate_raddr_pattern, \
                              randomstring, randomhex


@pytest.mark.usefixtures("proxy_connection")
class TestRewardsCC:

    @staticmethod
    def new_rewardsplan(proxy, schema=None):
        name = randomstring(4)
        amount = '250'
        apr = '25'
        mindays = '0'
        maxdays = '10'
        mindeposit = '10'
        res = proxy.rewardscreatefunding(name, amount, apr, mindays, maxdays, mindeposit)
        if schema:
            validate_template(res, schema)
        assert res.get('result') == 'success'
        txid = proxy.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(txid, proxy)
        rewardsplan = {
            'fundingtxid': txid,
            'name': name
        }
        return rewardsplan

    def test_rewardscreatefunding(self, test_params):
        createfunding_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc = test_params.get('node1').get('rpc')
        self.new_rewardsplan(rpc, schema=createfunding_schema)

    def test_rewardsaddress(self, test_params):
        rewardsaddress_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'error': {'type': 'string'},
                'RewardsCCAddress': {'type': 'string'},
                'RewardsCCBalance': {'type': 'number'},
                'RewardsNormalAddress': {'type': 'string'},
                'RewardsNormalBalance': {'type': 'number'},
                'RewardsCCTokensAddress': {'type': 'string'},
                'PubkeyCCaddress(Rewards)': {'type': 'string'},
                'PubkeyCCbalance(Rewards)': {'type': 'number'},
                'myCCAddress(Rewards)': {'type': 'string'},
                'myCCbalance(Rewards)': {'type': 'number'},
                'myaddress': {'type': 'string'},
                'mybalance': {'type': 'number'}
            },
            'required': ['result']
        }

        rpc = test_params.get('node1').get('rpc')
        res = rpc.rewardsaddress()
        validate_template(res, rewardsaddress_schema)
        assert res.get('result') == 'success'

    def test_rewarsdlist(self, test_params):
        rewadslist_schema = {
            'type': 'array',
            'items': {'type': 'string'}
        }

        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.rewardslist()
        validate_template(res, rewadslist_schema)

    @staticmethod
    def rewardsinfo_maincheck(proxy, fundtxid, schema):
        res = proxy.rewardsinfo(fundtxid)
        validate_template(res, schema)
        assert res.get('result') == 'success'

    def test_rewardsinfo(self, test_params):
        rewardsinfo_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'error': {'type': 'string'},
                'fundingtxid': {'type': 'string'},
                'name': {'type': 'string'},
                'sbits': {'type': 'integer'},
                'APR': {'type': 'string'},
                'minseconds': {'type': 'integer'},
                'maxseconds': {'type': 'integer'},
                'mindeposit': {'type': 'string'},
                'funding': {'type': 'string'},
                'locked': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        try:
            fundtxid = rpc1.rewardslist()[0]
        except IndexError:
            print('\nNo Rewards CC available on chain')
            fundtxid = self.new_rewardsplan(rpc1).get('fundingtxid')
        self.rewardsinfo_maincheck(rpc1, fundtxid, rewardsinfo_schema)

    @staticmethod
    def rewardsaddfunding_maincheck(proxy, fundtxid, schema):
        name = proxy.rewardsinfo(fundtxid).get('name')
        amount = proxy.rewardsinfo(fundtxid).get('mindeposit')  # not related to mindeposit here, just to get amount
        res = proxy.rewardsaddfunding(name, fundtxid, amount)
        validate_template(res, schema)
        assert res.get('result') == 'success'
        txid = proxy.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(txid, proxy)

    def test_rewardsaddfunding(self, test_params):
        addfunding_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        try:
            fundtxid = rpc1.rewardslist()[0]
        except IndexError:
            print('\nNo Rewards CC available on chain')
            fundtxid = self.new_rewardsplan(rpc1).get('fundingtxid')
        self.rewardsaddfunding_maincheck(rpc1, fundtxid, addfunding_schema)

    @staticmethod
    def un_lock_maincheck(proxy, fundtxid, schema):
        name = proxy.rewardsinfo(fundtxid).get('name')
        amount = proxy.rewardsinfo(fundtxid).get('mindeposit')
        res = proxy.rewardslock(name, fundtxid, amount)
        validate_template(res, schema)
        assert res.get('result') == 'success'
        locktxid = proxy.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(locktxid, proxy)
        print('\nWaiting some time to gain reward for locked funds')
        time.sleep(10)
        res = proxy.rewardsunlock(name, fundtxid, locktxid)
        print(res)
        validate_template(res, schema)
        assert res.get('result') == 'error'  # reward is less than txfee atm

    def test_lock_unlock(self, test_params):
        lock_unlock_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        try:
            fundtxid = rpc1.rewardslist()[0]
        except IndexError:
            print('\nNo Rewards CC available on chain')
            fundtxid = self.new_rewardsplan(rpc1).get('fundingtxid')
        self.un_lock_maincheck(rpc1, fundtxid, lock_unlock_schema)


@pytest.mark.usefixtures("proxy_connection")
class TestRewardsCCExtras:

    def test_rewardsaddress(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        pubkey = test_params.get('node1').get('pubkey')

        res = rpc1.rewardsaddress()
        for key in res.keys():
            if key.find('ddress') > 0:
                assert validate_raddr_pattern(res.get(key))

        res = rpc1.rewardsaddress(pubkey)
        for key in res.keys():
            if key.find('ddress') > 0:
                assert validate_raddr_pattern(res.get(key))

    @staticmethod
    def bad_calls(proxy, fundtxid):
        name = proxy.rewardsinfo(fundtxid).get('name')
        invalidfunding = randomhex()
        # looking up non-existent reward should return error
        res = proxy.rewardsinfo(invalidfunding)
        assert res.get('result') == 'error'

        # creating rewards plan with name > 8 chars, should return error
        res = proxy.rewardscreatefunding("STUFFSTUFF", "7777", "25", "0", "10", "10")
        assert res.get('result') == 'error'

        # creating rewards plan with 0 funding
        res = proxy.rewardscreatefunding("STUFF", "0", "25", "0", "10", "10")
        assert res.get('result') == 'error'

        # creating rewards plan with 0 maxdays
        res = proxy.rewardscreatefunding("STUFF", "7777", "25", "0", "10", "0")
        assert res.get('result') == 'error'

        # creating rewards plan with > 25% APR
        res = proxy.rewardscreatefunding("STUFF", "7777", "30", "0", "10", "10")
        assert res.get('result') == 'error'

        # creating reward plan with already existing name, should return error
        res = proxy.rewardscreatefunding(name, "777", "25", "0", "10", "10")
        assert res.get('result') == 'error'

        # add funding amount must be positive
        res = proxy.rewardsaddfunding(name, fundtxid, "-1")
        assert res.get('result') == 'error'

        # add funding amount must be positive
        res = proxy.rewardsaddfunding(name, fundtxid, "0")
        assert res.get('result') == 'error'

        # trying to lock funds, locking funds amount must be positive
        res = proxy.rewardslock(name, fundtxid, "-5")
        assert res.get('result') == 'error'

        # trying to lock funds, locking funds amount must be positive
        res = proxy.rewardslock(name, fundtxid, "0")
        assert res.get('result') == 'error'

        # trying to lock less than the min amount is an error
        res = proxy.rewardslock(name, fundtxid, "7")
        assert res.get('result') == 'error'

    def test_bad_calls(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        try:
            fundtxid = rpc.rewardslist()[0]
        except IndexError:
            print('\nNo Rewards CC available on chain')
            fundtxid = TestRewardsCC.new_rewardsplan(rpc).get('fundingtxid')
        self.bad_calls(rpc, fundtxid)
