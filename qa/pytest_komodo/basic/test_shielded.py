#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import time
from decimal import *
from pytest_util import validate_template, check_synced, mine_and_waitconfirms


@pytest.mark.usefixtures("proxy_connection")
class TestZcalls:

    def test_z_getnewaddress(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.z_getnewaddress()
        assert isinstance(res, str)
    # test sendmany, operationstatus, operationresult and listreceivedbyaddress calls

    def test_z_send(self, test_params):
        schema = {
            'type': 'array',
            'items': {
                'type': 'object',
                'properties': {
                    'id': {'type': 'string'},
                    'status': {'type': 'string'},
                    'creation_time': {'type': 'integer'},
                    'execution_secs': {'type': ['number', 'integer']},
                    'method': {'type': 'string'},
                    'error': {
                        'type': 'object',
                        'properties': {
                            'code': {'type': 'integer'},
                            'message': {'type': 'string'}
                        }
                    },
                    'result': {
                        'type': 'object',
                        'properties': {'txid': {'type': 'string'}}
                    },
                    'params': {
                        'type': 'object',
                        'properties': {
                            'fromaddress': {'type': 'string'},
                            'minconf': {'type': 'integer'},
                            'fee': {'type': ['number', 'integer']},
                            'amounts': {
                                'type': 'array',
                                'items': {
                                    'type': 'object',
                                    'properties': {
                                        'address': {'type': 'string'},
                                        'amount': {'type': ['integer', 'number']}
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        schema_list = {
            'type': 'array',
            'items': {
                'type': 'object',
                'properties': {
                    'txid': {'type': 'string'},
                    'memo': {'type': 'string'},
                    'amount': {'type': ['number', 'integer']},
                    'change': {'type': 'boolean'},
                    'outindex': {'type': 'integer'},
                    'confirmations': {'type': 'integer'},
                    'rawconfirmations': {'type': 'integer'},
                    'jsoutindex': {'type': 'integer'},
                    'jsindex': {'type': 'integer'}
                }
            }
        }
        rpc1 = test_params.get('node1').get('rpc')
        rpc2 = test_params.get('node2').get('rpc')
        transparent1 = rpc1.getnewaddress()
        shielded1 = rpc1.z_getnewaddress()
        transparent2 = rpc2.getnewaddress()
        shielded2 = rpc2.z_getnewaddress()
        amount1 = float("{0:.8f}".format(rpc1.listunspent()[-1].get('amount') / 10))
        amount2 = float("{0:.8f}".format(amount1 / 10))
        try:
            import slickrpc
            authproxy = 0
        except ImportError:
            authproxy = 1
        if authproxy:  # type correction when using python-bitcoinrpc Proxy
            amount1 = float(amount1)
            amount2 = float(amount2)
        # python float() is double precision floating point number,
        # where z_sendmany expects regural float (8 digits) value
        # "{0:.8f}".format(value)) returns number string with 8 digit precision and float() corrects the type
        t_send1 = [{'address': transparent1, 'amount': float("{0:.8f}".format(amount2))}]
        t_send2 = [{'address': transparent2, 'amount': float("{0:.8f}".format(amount2 * 0.4))}]
        z_send1 = [{'address': shielded1, 'amount': float("{0:.8f}".format(amount2 * 0.95))}]
        z_send2 = [{'address': shielded2, 'amount': float("{0:.8f}".format(amount2 * 0.4))}]
        cases = [(transparent1, t_send1), (transparent1, z_send1), (shielded1, t_send2), (shielded1, z_send2)]
        # sendmany cannot use coinbase tx vouts
        txid = rpc1.sendtoaddress(transparent1, amount1)
        mine_and_waitconfirms(txid, rpc1)
        for case in cases:
            assert check_synced(rpc1)  # to perform z_sendmany nodes should be synced
            opid = rpc1.z_sendmany(case[0], case[1])
            assert isinstance(opid, str)
            attempts = 0
            while True:
                res = rpc1.z_getoperationstatus([opid])
                validate_template(res, schema)
                status = res[0].get('status')
                if status == 'success':
                    print('Operation successfull\nWaiting confirmations\n')
                    res = rpc1.z_getoperationresult([opid])  # also clears op from memory
                    validate_template(res, schema)
                    txid = res[0].get('result').get('txid')
                    time.sleep(30)
                    tries = 0
                    while True:
                        try:
                            res = rpc1.getrawtransaction(txid, 1)
                            confirms = res['confirmations']
                            print('TX confirmed \nConfirmations: ', confirms)
                            break
                        except Exception as e:
                            print("\ntx is in mempool still probably, let's wait a little bit more\nError: ", e)
                            time.sleep(5)
                            tries += 1
                            if tries < 100:
                                pass
                            else:
                                print("\nwaited too long - probably tx stuck by some reason")
                                return False
                    break
                else:
                    attempts += 1
                    print('Waiting operation result\n')
                    time.sleep(10)
                if attempts >= 100:
                    print('operation takes too long, aborting\n')
                    return False
        res = rpc1.z_listreceivedbyaddress(shielded1)
        validate_template(res, schema_list)

    def test_z_getbalance(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        zaddr = rpc.z_getnewaddress()
        res = rpc.z_getbalance(zaddr, 1)
        assert isinstance(res, float) or isinstance(res, int) or isinstance(res, Decimal)

    def test_z_gettotalbalance(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'transparent': {'type': ['string']},
                'interest': {'type': ['string']},
                'private': {'type': ['string']},
                'total': {'type': ['string']},
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.z_gettotalbalance(1)
        validate_template(res, schema)

    def test_z_export_viewing_key(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        zaddr = rpc.z_getnewaddress()
        res = rpc.z_exportkey(zaddr)
        assert isinstance(res, str)
        res = rpc.z_exportviewingkey(zaddr)
        assert isinstance(res, str)

    def test_z_import_viewing_key(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        rpc2 = test_params.get('node2').get('rpc')
        zaddr = rpc2.z_getnewaddress()
        zkey = rpc2.z_exportkey(zaddr)
        zvkey = rpc2.z_exportviewingkey(zaddr)
        # res = rpc1.z_importviewingkey(zvkey)  # https://github.com/zcash/zcash/issues/3060
        # assert not res
        res = rpc1.z_importkey(zkey)
        assert not res

    def test_z_listaddresses(self, test_params):
        schema = {
            'type': 'array',
            'items': {
                'type': 'string'
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.z_listaddresses()
        validate_template(res, schema)

    def test_z_listopertaionsids(self, test_params):
        schema = {
            'type': 'array',
            'items': {
                'type': 'string'
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.z_listoperationids()
        validate_template(res, schema)
        res = rpc.z_listoperationids('success')
        validate_template(res, schema)

    def test_z_validateaddress(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'isvalid': {'type': 'boolean'},
                'address': {'type': 'string'},
                'payingkey': {'type': 'string'},
                'transmissionkey': {'type': 'string'},
                'ismine': {'type': 'boolean'}
            }
        }
        rpc = test_params.get('node1').get('rpc')
        zaddr = rpc.z_getnewaddress()
        res = rpc.z_validateaddress(zaddr)
        validate_template(res, schema)
