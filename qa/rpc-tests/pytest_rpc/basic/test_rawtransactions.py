#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
from decimal import *
from pytest_util import validate_template, mine_and_waitconfirms


@pytest.mark.usefixtures("proxy_connection")
class TestRawTransactions:

    def test_rawtransactions(self, test_params):  # create, fund, sign, send calls
        fund_schema = {
            'type': 'object',
            'properties': {
                'hex': {'type': 'string'},
                'fee': {'type': ['integer', 'number']},
                'changepos': {'type': ['integer', 'number']}
            }
        }
        sign_schema = {
            'type': 'object',
            'properties': {
                'hex': {'type': 'string'},
                'complete': {'type': 'boolean'}
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listunspent()
        txid = res[0].get('txid')
        vout = res[0].get('vout')
        base_amount = res[0].get('amount')
        if isinstance(base_amount, Decimal):
            amount = float(base_amount) * 0.9
            print(amount)
        else:
            amount = base_amount * 0.9
        address = rpc.getnewaddress()
        ins = [{'txid': txid, 'vout': vout}]
        outs = {address: amount}

        rawtx = rpc.createrawtransaction(ins, outs)
        assert isinstance(rawtx, str)

        fundtx = rpc.fundrawtransaction(rawtx)
        validate_template(fundtx, fund_schema)

        signtx = rpc.signrawtransaction(fundtx.get('hex'))
        validate_template(signtx, sign_schema)
        assert signtx['complete']

        sendtx = rpc.sendrawtransaction(signtx.get('hex'))
        assert isinstance(sendtx, str)
        assert mine_and_waitconfirms(sendtx, rpc)

    def test_getrawtransaction(self, test_params):  # decode, get methods
        txschema = {
            'type': 'object',
            'properties': {
                'hex': {'type': 'string'},
                'txid': {'type': 'string'},
                'overwintered': {'type': 'boolean'},
                'version': {'type': 'integer'},
                'versiongroupid': {'type': 'string'},
                'locktime': {'type': 'integer'},
                'expiryheight': {'type': 'integer'},
                'vin': {
                    'type': 'array',
                    'items': {
                        'type': 'object',
                        'properties': {
                            'coinbase': {'type': 'string'},
                            'txid': {'type': 'string'},
                            'vout': {'type': 'integer'},
                            'address': {'type': 'string'},
                            'scriptSig': {
                                'type': 'object',
                                'properties': {
                                    'asm': {'type': 'string'},
                                    'hex': {'type': 'string'}
                                }
                            },
                            'value': {'type': ['integer', 'number']},
                            'valueSat': {'type': 'integer'},
                            'sequence': {'type': 'integer'}
                        }
                    }
                },
                'vout': {
                    'type': 'array',
                    'items': {
                        'type': 'object',
                        'properties': {
                            'value': {'type': ['integer', 'number']},
                            'valueSat': {'type': 'integer'},
                            'interest': {'type': ['integer', 'number']},
                            'n': {'type': 'integer'},
                            'scriptPubKey': {
                                'type': 'object',
                                'properties': {
                                    'asm': {'type': 'string'},
                                    'hex': {'type': 'string'},
                                    'reqSigs': {'type': 'integer'},
                                    'type': {'type': 'string'},
                                    'addresses': {'type': 'array', 'items': {'type': 'string'}}
                                }
                            }
                        }
                    }
                },
                'vjoinsplit': {'type': 'array'},
                'valueBalance': {'type': 'number'},
                'vShieldedSpend': {'type': 'array'},
                'vShieldedOutput': {'type': 'array'},
                'blockhash': {'type': 'string'},
                'height': {'type': 'integer'},
                'confirmations': {'type': 'integer'},
                'rawconfirmations': {'type': 'integer'},
                'time': {'type': 'integer'},
                'blocktime': {'type': 'integer'}
            }
        }
        scriptschema = {
            'type': 'object',
            'properties': {
                'asm': {'type': 'string'},
                'hex': {'type': 'string'},
                'type': {'type': 'string'},
                'reqSigs': {'type': 'integer'},
                'address': {'type': 'string'},
                'p2sh': {'type': 'string'},
                'addresses': {'type': 'array',
                              'items': {'type': 'string'}}
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listunspent()
        txid = res[0].get('txid')
        rawhex = rpc.getrawtransaction(txid)
        assert isinstance(rawhex, str)
        res = rpc.getrawtransaction(txid, 1)
        validate_template(res, txschema)

        scripthex = res.get('vout')[0].get('scriptPubKey').get('hex')
        res = rpc.decodescript(scripthex)
        validate_template(res, scriptschema)

        res = rpc.decoderawtransaction(rawhex)
        validate_template(res, txschema)
