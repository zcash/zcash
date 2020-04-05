#!/usr/bin/env python3
# Copyright (c) 2020 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

import pytest
import sys
import re
from slickrpc import exc
import warnings
sys.path.append('../')
from basic.pytest_util import validate_template, mine_and_waitconfirms


@pytest.mark.usefixtures("proxy_connection")
class TestFaucetCCBase:

    def test_faucetinfo(self, test_params):
        faucetinfo_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'name': {'type': 'string'},
                'funding': {'type': 'string'}
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.faucetinfo()
        validate_template(res, faucetinfo_schema)

    def test_faucetfund(self, test_params):
        faucetfund_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.faucetfund('10')
        validate_template(res, faucetfund_schema)
        txid = rpc1.sendrawtransaction(res.get('hex'))
        mine_and_waitconfirms(txid, rpc1, 1)

    def test_faucetaddress(self, test_params):
        faucetaddress_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'FaucetCCAddress': {'type': 'string'},
                'FaucetCCBalance': {'type': 'number'},
                'FaucetNormalAddress': {'type': 'string'},
                'FaucetNormalBalance': {'type': 'number'},
                'FaucetCCTokenAddress': {'type': 'string'},
                'PubkeyCCaddress(Faucet)': {'type': 'string'},
                'PubkeyCCbalance(Faucet)': {'type': 'number'},
                'myCCaddress(Faucet)': {'type': 'string'},
                'myCCbalance(Faucet)': {'type': 'number'},
                'myaddress': {'type': 'string'},
                'mybalance': {'type': 'number'}
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.faucetaddress()
        validate_template(res, faucetaddress_schema)

    def test_faucetget(self, test_params):
        faucetget_schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'hex': {'type': 'string'},
                'error': {'type': 'string'}
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        node_addr = test_params.get('node1').get('address')
        res = rpc1.faucetget()
        # should return error if faucetget have already been used by pubkey
        validate_template(res, faucetget_schema)
        try:
            fhex = res.get('hex')
            isinstance(fhex, str)
            res = rpc1.decoderawtransaction(fhex)
            vout_fauc = res.get('vout')[1]
            assert node_addr in vout_fauc.get('scriptPubKey').get('addresses')
            assert vout_fauc.get('valueZat') == pow(10, 8) * vout_fauc.get('value')
        except (KeyError, TypeError, exc.RpcTypeError):
            assert res.get('result') == 'error'


@pytest.mark.usefixtures("proxy_connection")
class TestFaucetCCe2e:

    # verify all addresses look like valid AC address
    def test_faucet_addresses(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        pubkey = test_params.get('node1').get('pubkey')
        address_pattern = re.compile(r"R[a-zA-Z0-9]{33}\Z")  # normal R-addr

        res = rpc1.faucetaddress()
        for key in res.keys():
            if key.find('ddress') > 0:
                assert address_pattern.match(str(res.get(key)))

        res = rpc1.faucetaddress(pubkey)
        for key in res.keys():
            if key.find('ddress') > 0:
                assert address_pattern.match(str(res.get(key)))

    def test_faucet_badvalues(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.faucetfund('')
        assert res.get('result') == 'error'
        res = rpc1.faucetfund('asdfqwe')
        assert res.get('result') == 'error'
        res = rpc1.faucetfund('0')
        assert res.get('result') == 'error'
        res = rpc1.faucetfund('-1.99')
        assert res.get('result') == 'error'

    def test_faucetget_mine(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.faucetget()
        try:
            fhex = res.get('hex')
            txid = rpc1.sendrawtransaction(fhex)
            mine_and_waitconfirms(txid, rpc1)
        except exc.RpcVerifyRejected:  # excepts scenario when pubkey already received faucet funds
            warnings.warn(RuntimeWarning('Faucet funds were already claimed by pubkey'))
