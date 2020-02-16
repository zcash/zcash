#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import os
import time
from decimal import *
from pytest_util import validate_template, mine_and_waitconfirms


@pytest.mark.usefixtures("proxy_connection")
class TestWalletRPC:

    def test_addmultisigadress(self, test_params):
        pass

    def test_getbalance(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getbalance()
        # python-bitcoinrpc Proxy can return value as decimal
        assert isinstance(res, float) or isinstance(res, int) or isinstance(res, Decimal)

    def test_getnewaddress(self,  test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getnewaddress()
        assert isinstance(res, str)

    def test_dumpprivkey(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        addr = rpc.getnewaddress()
        res = rpc.dumpprivkey(addr)
        assert isinstance(res, str)

    def test_getrawchangeaddress(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getrawchangeaddress()
        assert isinstance(res, str)

    def test_getreceivedbyaddress(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        addr = rpc.getnewaddress()
        res = rpc.getreceivedbyaddress(addr)
        # python-bitcoinrpc Proxy can return value as decimal
        assert isinstance(res, float) or isinstance(res, int) or isinstance(res, Decimal)

    def test_gettransaction(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'amount': {'type': ['integer', 'number']},
                'rawconfirmations': {'type': 'integer'},
                'confirmations': {'type': 'integer'},
                'blockindex': {'type': 'integer'},
                'blocktime': {'type': 'integer'},
                'walletconflicts': {'type': 'array'},
                'time': {'type': 'integer'},
                'timereceived': {'type': 'integer'},
                'txid': {'type': 'string'},
                'blockhash': {'type': 'string'},
                'hex': {'type': 'string'},
                'vjoinsplit': {
                    'type': 'array',
                    'items': {'type': 'object'}
                },
                'details': {
                    'type': 'array',
                    'properties': {
                        'account': {'type': 'string'},
                        'address': {'type': 'string'},
                        'category': {'type': 'string'},
                        'amount': {'type': ['number', 'integer']},
                        'vout': {'type': 'integer'},
                        'size': {'type': 'integer'},
                    }
                }
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listunspent()
        txid = res[0].get('txid')
        res = rpc.gettransaction(txid)
        validate_template(res, schema)

    def test_getunconfirmedbalance(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getunconfirmedbalance()
        # python-bitcoinrpc Proxy can return value as decimal
        assert isinstance(res, float) or isinstance(res, int) or isinstance(res, Decimal)

    def test_getwalletinfo(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'walletversion': {'type': 'integer'},
                'balance': {'type': ['number', 'integer']},
                'unconfirmed_balance': {'type': ['number', 'integer']},
                'immature_balance': {'type': ['number', 'integer']},
                'txount': {'type': 'integer'},
                'keypoololdest': {'type': 'integer'},
                'keypoolsize': {'type': 'integer'},
                'paytxfee': {'type': ['number', 'integer']},
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getwalletinfo()
        validate_template(res, schema)

    def test_importaddress(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        rpc2 = test_params.get('node2').get('rpc')
        addr = rpc2.getnewaddress()
        res = rpc1.importaddress(addr)
        assert not res  # empty response on success

    def test_importprivkey(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        addr = rpc.getnewaddress()
        key = rpc.dumpprivkey(addr)
        res = rpc.importprivkey(key)
        assert isinstance(res, str)

    def test_keypoolrefill(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.keypoolrefill(100)
        assert not res  # empty response on success

    def test_listaddressgroupings(self, test_params):
        schema = {
            'type': 'array',
            'items': {
                'type': 'array',
                'items': {
                    'type': 'array',
                    'items': {'type': ['string', 'integer', 'number']}
                }
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listaddressgroupings()
        validate_template(res, schema)

    def test_list_lockunspent(self, test_params):
        schema = {
            'type': 'array',
            'items': {
                'type': 'object',
                'properties': {
                    'txid': {'type': 'string'},
                    'vout': {'type': 'integer'}
                }
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listunspent()
        txid = res[0].get('txid')
        lock = [{"txid": txid, "vout": 0}]
        res = rpc.lockunspent(False, lock)
        assert res  # returns True on success
        res = rpc.listlockunspent()
        validate_template(res, schema)
        res = rpc.lockunspent(True, lock)
        assert res  # returns True on success

    def test_listreceivedbyaddress(self, test_params):
        schema = {
            'type': 'array',
            'items': {
                'type': 'object',
                'properties': {
                    'involvesWatchonly': {'type': 'boolean'},
                    'address': {'type': 'string'},
                    'account': {'type': 'string'},
                    'amount': {'type': ['integer', 'number']},
                    'rawconfirmations': {'type': 'integer'},
                    'confirmations': {'type': 'integer'},
                    'txids': {
                        'type': 'array',
                        'items': {'type': 'string'}
                    }
                }
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listreceivedbyaddress()
        validate_template(res, schema)

    def test_listsinceblock(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'lastblock': {'type': 'string'},
                'transactions': {
                    'type': 'array',
                    'items': {
                        'type': 'object',
                        'properties': {
                            'account': {'type': 'string'},
                            'address': {'type': 'string'},
                            'category': {'type': 'string'},
                            'blockhash': {'type': 'string'},
                            'txid': {'type': 'string'},
                            'vjoinsplit': {'type': 'array'},
                            'walletconflicts': {'type': 'array'},
                            'amount': {'type': ['integer', 'number']},
                            'vout': {'type': 'integer'},
                            'rawconfirmations': {'type': 'integer'},
                            'confirmations': {'type': 'integer'},
                            'blockindex': {'type': 'integer'},
                            'blocktime': {'type': 'integer'},
                            'expiryheight': {'type': 'integer'},
                            'time': {'type': 'integer'},
                            'timereceived': {'type': 'integer'},
                            'size': {'type': 'integer'},
                            'to': {'type': 'string'},
                            'comment': {'type': 'string'}
                        }
                    }
                }
            }
        }
        rpc = test_params.get('node1').get('rpc')
        blockhash = rpc.getbestblockhash()
        res = rpc.listsinceblock(blockhash, 1)
        validate_template(res, schema)

    def test_listtransactions(self, test_params):
        schema = {
            'type': 'array',
            'items': {
                'type': 'object',
                'properties': {
                    'account': {'type': 'string'},
                    'address': {'type': 'string'},
                    'category': {'type': 'string'},
                    'blockhash': {'type': 'string'},
                    'txid': {'type': 'string'},
                    'vjoinsplit': {'type': 'array'},
                    'walletconflicts': {'type': 'array'},
                    'amount': {'type': ['integer', 'number']},
                    'vout': {'type': 'integer'},
                    'rawconfirmations': {'type': 'integer'},
                    'confirmations': {'type': 'integer'},
                    'blockindex': {'type': 'integer'},
                    'fee': {'type': ['integer', 'number']},
                    'time': {'type': 'integer'},
                    'timereceived': {'type': 'integer'},
                    'size': {'type': 'integer'},
                    'comment': {'type': 'string'},
                    'otheraccount': {'type': 'string'}
                }
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listtransactions()
        validate_template(res, schema)

    def test_listunspent(self, test_params):
        schema = {
            'type': 'array',
            'items': {
                'type': 'object',
                'properties': {
                    'txid': {'type': 'string'},
                    'address': {'type': 'string'},
                    'scriptPubKey': {'type': 'string'},
                    'generated': {'type': 'boolean'},
                    'spendable': {'type': 'boolean'},
                    'vout': {'type': 'integer'},
                    'confirmations': {'type': 'integer'},
                    'rawconfirmations': {'type': 'integer'},
                    'amount': {'type': ['integer', 'number']},
                    'interest': {'type': ['integer', 'number']}
                }
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listunspent()
        validate_template(res, schema)

    def test_resendwallettransactions(self, test_params):
        schema = {
            'type': 'array',
            'itmes': {'type': 'string'}
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.resendwallettransactions()
        validate_template(res, schema)

    def test_settxfee(self, test_params):
        txfee = 0.00001
        rpc = test_params.get('node1').get('rpc')
        res = rpc.settxfee(txfee)
        assert res  # returns True on success

    def test_signmessage(self, test_params):
        message = "my test message"
        rpc = test_params.get('node1').get('rpc')
        addr = rpc.getnewaddress()
        sign = rpc.signmessage(addr, message)
        assert isinstance(sign, str)
        res = rpc.verifymessage(addr, sign, message)
        assert res  # returns True on success

    def test_sendtoaddress(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        addr = rpc.getnewaddress()
        # python float() is double precision floating point number,
        # where sendmany expects regural float (8 digits) value
        # "{0:.8f}".format(value)) returns number string with 8 digit precision and float() corrects the type
        amount = float("{0:.8f}".format(rpc.listunspent()[-1].get('amount') / 10))
        txid = rpc.sendtoaddress(addr, amount)
        assert isinstance(txid, str)
        # wait tx to be confirmed
        mine_and_waitconfirms(txid, rpc)

    def test_sendmany(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        rpc2 = test_params.get('node2').get('rpc')
        address1 = rpc1.getnewaddress()
        address2 = rpc2.getnewaddress()
        # clarification in test_sendtoaddress above
        amount = float("{0:.8f}".format(rpc1.listunspent()[-1].get('amount') / 10))  # float("{0:.8f}".format(amount2))
        send = {address1: amount, address2: amount}
        txid = rpc1.sendmany("", send)
        assert isinstance(txid, str)
        # wait tx to be confirmed
        mine_and_waitconfirms(txid, rpc1)

    def test_setupkey(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'address': {'type': 'string'},
                'pubkey': {'type': 'string'},
                'ismine': {'type': 'boolean'}
            }
        }
        rpc = test_params.get('node1').get('rpc')
        pubkey = test_params.get('node1').get('pubkey')
        res = rpc.setpubkey(pubkey)
        validate_template(res, schema)
