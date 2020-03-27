#!/usr/bin/env python3
# Copyright (c) 2020 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

import pytest
import sys
sys.path.append('../')
from basic.pytest_util import validate_template, mine_and_waitconfirms, randomstring,\
     randomhex, wait_blocks, validate_raddr_pattern, check_synced
from decimal import *


@pytest.mark.usefixtures("proxy_connection")
class TestDiceCCBase:

    def test_diceaddress(self, test_params):
        diceaddress_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'DiceCCAddress': {'type': 'string'},
                'DiceCCBalance': {'type': 'number'},
                'DiceNormalAddress': {'type': 'string'},
                'DiceNormalBalance': {'type': 'number'},
                'DiceCCTokensAddress': {'type': 'string'},
                'myCCAddress(Dice)': {'type': 'string'},
                'myCCBalance(Dice)': {'type': 'number'},
                'myaddress': {'type': 'string'},
                'mybalance': {'type': 'number'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        pubkey1 = test_params.get('node1').get('pubkey')
        res = rpc1.diceaddress()
        validate_template(res, diceaddress_schema)
        res = rpc1.diceaddress('')
        validate_template(res, diceaddress_schema)
        res = rpc1.diceaddress(pubkey1)
        validate_template(res, diceaddress_schema)

    @staticmethod
    def new_casino(proxy, schema=None):
        rpc1 = proxy
        name = randomstring(4)
        funds = '777'
        minbet = '1'
        maxbet = '77'
        maxodds = '10'
        timeoutblocks = '5'
        res = rpc1.dicefund(name, funds, minbet, maxbet, maxodds, timeoutblocks)
        if schema:
            validate_template(res, schema)
        assert res.get('result') == 'success'
        txid = rpc1.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(txid, rpc1)
        casino = {
            'fundingtxid': txid,
            'name': name,
            'minbet': minbet,
            'maxbet': maxbet,
            'maxodds': maxodds
        }
        return casino

    def test_dicefund(self, test_params):
        # dicefund name funds minbet maxbet maxodds timeoutblocks
        dicefund_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        self.new_casino(rpc1, dicefund_schema)

    def test_dicelist(self, test_params):
        dicelist_schema = {
            'type': 'array',
            'items': {'type': 'string'}
        }

        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.dicelist()
        validate_template(res, dicelist_schema)

    @staticmethod
    def diceinfo_maincheck(proxy, fundtxid, schema):
        res = proxy.diceinfo(fundtxid)
        validate_template(res, schema)
        assert res.get('result') == 'success'

    def test_diceinfo(self, test_params):
        diceinfo_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'error': {'type': 'string'},
                'fundingtxid': {'type': 'string'},
                'name': {'type': 'string'},
                'sbits': {'type': 'integer'},
                'minbet': {'type': 'string'},
                'maxbet': {'type': 'string'},
                'maxodds': {'type': 'integer'},
                'timeoutblocks': {'type': 'integer'},
                'funding': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        try:
            fundtxid = rpc1.dicelist()[0]
        except IndexError:
            print('\nNo Dice CC available on chain')
            fundtxid = self.new_casino(rpc1).get('fundingtxid')
        self.diceinfo_maincheck(rpc1, fundtxid, diceinfo_schema)

    @staticmethod
    def diceaddfunds_maincheck(proxy, amount, fundtxid, schema):
        name = proxy.diceinfo(fundtxid).get('name')
        res = proxy.diceaddfunds(name, fundtxid, amount)
        validate_template(res, schema)
        assert res.get('result') == 'success'
        addtxid = proxy.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(addtxid, proxy)

    def test_diceaddfunds(self, test_params):
        diceaddfunds_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        amount = '15'
        try:
            fundtxid = rpc1.dicelist()[0]
        except IndexError:
            fundtxid = self.new_casino(rpc1).get('fundingtxid')
        self.diceaddfunds_maincheck(rpc1, amount, fundtxid, diceaddfunds_schema)

    @staticmethod
    def dicebet_maincheck(proxy, casino, schema):
        res = proxy.dicebet(casino.get('name'), casino.get('fundingtxid'), casino.get('minbet'), casino.get('maxodds'))
        validate_template(res, schema)
        assert res.get('result') == 'success'
        bettxid = proxy.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(bettxid, proxy)
        return bettxid

    @staticmethod
    def dicestatus_maincheck(proxy, casino, bettx, schema):
        res = proxy.dicestatus(casino.get('name'), casino.get('fundingtxid'), bettx)
        validate_template(res, schema)
        assert res.get('result') == 'success'

    @staticmethod
    def dicefinsish_maincheck(proxy, casino, bettx, schema):
        res = proxy.dicefinish(casino.get('name'), casino.get('fundingtxid'), bettx)
        validate_template(res, schema)
        assert res.get('result') == 'success'

    @staticmethod
    def create_entropy(proxy, casino):
        amount = '1'
        for i in range(100):
            res = proxy.diceaddfunds(casino.get('name'), casino.get('fundingtxid'), amount)
            fhex = res.get('hex')
            proxy.sendrawtransaction(fhex)
        checkhex = proxy.diceaddfunds(casino.get('name'), casino.get('fundingtxid'), amount).get('hex')
        tx = proxy.sendrawtransaction(checkhex)
        mine_and_waitconfirms(tx, proxy)

    def test_dicebet_dicestatus_dicefinish(self, test_params):
        dicebet_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }
        dicestatus_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'status': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }
        dicefinish_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            },
            'required': ['result']
        }

        rpc1 = test_params.get('node1').get('rpc')
        rpc2 = test_params.get('node2').get('rpc')
        casino = self.new_casino(rpc2)
        self.create_entropy(rpc2, casino)
        bettxid = self.dicebet_maincheck(rpc1, casino, dicebet_schema)
        self.dicestatus_maincheck(rpc1, casino, bettxid, dicestatus_schema)
        wait_blocks(rpc1, 5)  # 5 here is casino's block timeout
        self.dicefinsish_maincheck(rpc1, casino, bettxid, dicefinish_schema)


@pytest.mark.usefixtures("proxy_connection")
class TestDiceCC:

    def test_dice_addresses(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        pubkey = test_params.get('node1').get('pubkey')

        res = rpc1.diceaddress()
        for key in res.keys():
            if key.find('ddress') > 0:
                assert validate_raddr_pattern(res.get(key))

        res = rpc1.diceaddress(pubkey)
        for key in res.keys():
            if key.find('ddress') > 0:
                assert validate_raddr_pattern(res.get(key))

    def test_dice_errors(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        dicename = randomstring(4)

        # creating dice plan with too long name (>8 chars)
        res = rpc1.dicefund("THISISTOOLONG", "10000", "10", "10000", "10", "5")
        assert res.get('result') == 'error'

        # creating dice plan with < 100 funding
        res = rpc1.dicefund(dicename, "10", "1", "10000", "10", "5")
        assert res.get('result') == 'error'

        # adding negative and zero funds to plan
        try:
            fundtxid = rpc1.dicelist()[0]
            name = rpc1.diceinfo(fundtxid).get('name')
            res = rpc1.diceaddfunds(name, fundtxid, '0')
            assert res.get('result') == 'error'
            res = rpc1.diceaddfunds(name, fundtxid, '-123')
            assert res.get('result') == 'error'
        except IndexError:
            casino = TestDiceCCBase.new_casino(rpc1)
            fundtxid = casino.get('fundingtxid')
            name = rpc1.diceinfo(fundtxid).get('name')
            res = rpc1.diceaddfunds(name, fundtxid, '0')
            assert res.get('result') == 'error'
            res = rpc1.diceaddfunds(name, fundtxid, '-123')
            assert res.get('result') == 'error'

        # not valid dice info checking
        res = rpc1.diceinfo("invalid")
        assert res.get('result') == 'error'

    @staticmethod
    def badbets_check(proxy, fundtxid):
        casino = proxy.diceinfo(fundtxid)
        dname = str(casino.get('name'))
        minbet = str(casino.get('minbet'))
        maxbet = str(casino.get('maxbet'))
        maxodds = str(casino.get('maxodds'))
        funding = str(casino.get('funding'))

        res = proxy.dicebet(dname, fundtxid, '0', maxodds)  # 0 bet
        assert res.get('result') == 'error'

        res = proxy.dicebet(dname, fundtxid, minbet, '0')  # 0 odds
        assert res.get('result') == 'error'

        res = proxy.dicebet(dname, fundtxid, '-1', maxodds)  # negative bet
        assert res.get('result') == 'error'

        res = proxy.dicebet(dname, fundtxid, minbet, '-1')  # negative odds
        assert res.get('result') == 'error'

        # placing bet more than maxbet
        dmb = Decimal(maxbet)
        dmb += 1
        res = proxy.dicebet(dname, fundtxid, "{0:.8f}".format(dmb), maxodds)
        assert res.get('result') == 'error'

        # placing bet with odds more than allowed
        dmo = Decimal(maxodds)
        dmo += 1
        res = proxy.dicebet(dname, fundtxid, minbet, "{0:.8f}".format(dmo))
        assert res.get('result') == 'error'

        # placing bet with amount more than funding
        betamount = funding
        res = proxy.dicebet(dname, fundtxid, str(betamount), maxodds)
        assert res.get('result') == 'error'

        # placing bet with not correct dice name
        name = randomstring(5)
        res = proxy.dicebet(name, fundtxid, minbet, maxodds)
        assert res.get('result') == 'error'

        # placing bet with not correct dice id
        badtxid = randomhex()
        res = proxy.dicebet(dname, badtxid, minbet, maxodds)
        assert res.get('result') == 'error'

    def test_dice_badbets(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        rpc2 = test_params.get('node2').get('rpc')

        # before betting nodes should be synced
        check_synced(rpc1, rpc2)

        try:
            fundtxid = rpc2.dicelist()[0]
        except IndexError:
            casino = TestDiceCCBase.new_casino(rpc1)
            fundtxid = casino.get('fundingtxid')
        self.badbets_check(rpc2, fundtxid)
