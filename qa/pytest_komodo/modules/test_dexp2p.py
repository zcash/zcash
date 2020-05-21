#!/usr/bin/env python3
# Copyright (c) 2020 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

import pytest
# from decimal import *
import sys
import os
import time

sys.path.append('../')
from basic.pytest_util import validate_template, randomhex, write_file, write_empty_file


@pytest.mark.usefixtures("proxy_connection")
class TestDexP2Prpc:

    def test_dexrpc_stats(self, test_params):
        schema_stats = {
            'type': 'object',
            'properties': {
                'publishable_pubkey': {'type': 'string'},
                'perfstats': {'type': 'string'},
                'result': {'type': 'string'},
                'secpkey': {'type': 'string'},
                'recvaddr': {'type': 'string'},
                'recvZaddr': {'type': 'string'},
                'handle': {'type': 'string'},
                'txpowbits': {'type': 'integer'},
                'vip': {'type': 'integer'},
                'cmdpriority': {'type': 'integer'}
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.DEX_stats()
        validate_template(res, schema_stats)

    def test_dexrpc_inbox_broadcast(self, test_params):
        # inbox message broadcast
        schema_broadcast = {
            'type': 'object',
            'properties': {
                'timestamp': {'type': 'integer'},
                'recvtime': {'type': 'integer'},
                'id': {'type': 'integer'},
                'tagA': {'type': 'string'},
                'tagB': {'type': 'string'},
                'senderpub': {'type': 'string'},
                'destpub': {'type': 'string'},
                'payload': {'type': 'string'},
                'hex': {'type': 'integer'},
                'amountA': {'type': ['string']},
                'amountB': {'type': ['string']},
                'priority': {'type': 'integer'},
                'error': {'type': 'string'},
                'cancelled': {'type': 'integer'}
            },
            'required': ['timestamp', 'id', 'tagA', 'tagB', 'payload']
        }

        rpc1 = test_params.get('node1').get('rpc')
        pubkey = rpc1.DEX_stats().get('publishable_pubkey')
        message = 'testmessage'
        res = rpc1.DEX_broadcast(message, '4', 'inbox', '', pubkey, '', '')
        validate_template(res, schema_broadcast)

    def test_dexrpc_list(self, test_params):
        # check dexlist
        schema_list = {
            'type': 'object',
            'properties': {
                'tagA': {'type': 'string'},
                'tagB': {'type': 'string'},
                'destpub': {'type': 'string'},
                'n': {'type': 'integer'},
                'matches': {
                    'type': 'array',
                    'items': {
                        'type': 'object',
                        'properties': {
                            'timestamp': {'type': 'integer'},
                            'id': {'type': 'integer'},
                            'hex': {'type': 'integer'},
                            'priority': {'type': 'integer'},
                            'amountA': {'type': ['string']},
                            'amountB': {'type': ['string']},
                            'tagA': {'type': 'string'},
                            'tagB': {'type': 'string'},
                            'destpub': {'type': 'string'},
                            'payload': {'type': 'string'},
                            'decrypted': {'type': 'string'},
                            'decryptedhex': {'type': 'integer'},
                            'senderpub': {'type': 'string'},
                            'recvtime': {'type': 'integer'},
                            'error': {'type': 'string'},
                            'cancelled': {'type': 'integer'}
                        }
                    }
                }
            },
            'required': ['tagA', 'tagB', 'n']
        }

        rpc1 = test_params.get('node1').get('rpc')
        pubkey = rpc1.DEX_stats().get('publishable_pubkey')
        message = randomhex()
        rpc1.DEX_broadcast(message, '4', 'inbox', '', pubkey, '', '')
        res = rpc1.DEX_list('', '4', '', '', pubkey)
        validate_template(res, schema_list)

    def test_dexrpc_orderbook_bc(self, test_params):
        # orderbook broadcast
        schema_broadcast = {
            'type': 'object',
            'properties': {
                'timestamp': {'type': 'integer'},
                'recvtime': {'type': 'integer'},
                'id': {'type': 'integer'},
                'tagA': {'type': 'string'},
                'tagB': {'type': 'string'},
                'senderpub': {'type': 'string'},
                'destpub': {'type': 'string'},
                'payload': {'type': 'string'},
                'hex': {'type': 'integer'},
                'amountA': {'type': ['string']},
                'amountB': {'type': ['string']},
                'priority': {'type': 'integer'},
                'error': {'type': 'string'},
                'cancelled': {'type': 'integer'}
            },
            'required': ['timestamp', 'id', 'tagA', 'tagB', 'payload']
        }

        rpc1 = test_params.get('node1').get('rpc')
        pubkey = rpc1.DEX_stats().get('publishable_pubkey')
        message = randomhex()
        res = rpc1.DEX_broadcast(message, '4', 'BASE', 'REL', pubkey, '100', '1')
        validate_template(res, schema_broadcast)

    def test_dexrpc_anonsend(self, test_params):
        # send anon message
        schema_anon = {
            'type': 'object',
            'properties': {
                'timestamp': {'type': 'integer'},
                'id': {'type': 'integer'},
                'hash': {'type': 'string'},
                'tagA': {'type': 'string'},
                'tagB': {'type': 'string'},
                'pubkey': {'type': 'string'},
                'payload': {'type': 'string'},
                'hex': {'type': 'integer'},
                'decrypted': {'type': 'string'},
                'decryptedhex': {'type': 'integer'},
                'anonmsg': {'type': 'string'},
                'anonsender': {'type': 'string'},
                'amountA': {'type': 'string'},
                'amountB': {'type': 'string'},
                'priority': {'type': 'integer'},
                'recvtime': {'type': 'integer'},
                'cancelled': {'type': 'integer'}
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        rpc2 = test_params.get('node2').get('rpc')
        destpub = rpc2.DEX_stats().get('publishable_pubkey')
        message = 'testanonmessage'
        res = rpc1.DEX_anonsend(message, '4', destpub)
        validate_template(res, schema_anon)

    def test_dexrpc_get(self, test_params):
        # check get id
        schema_broadcast = {
            'type': 'object',
            'properties': {
                'timestamp': {'type': 'integer'},
                'recvtime': {'type': 'integer'},
                'id': {'type': 'integer'},
                'tagA': {'type': 'string'},
                'tagB': {'type': 'string'},
                'senderpub': {'type': 'string'},
                'destpub': {'type': 'string'},
                'payload': {'type': 'string'},
                'hex': {'type': 'integer'},
                'amountA': {'type': ['string']},
                'amountB': {'type': ['string']},
                'priority': {'type': 'integer'},
                'error': {'type': 'string'},
                'cancelled': {'type': 'integer'}
            },
            'required': ['timestamp', 'id', 'tagA', 'tagB', 'payload']
        }

        rpc1 = test_params.get('node1').get('rpc')
        pubkey = rpc1.DEX_stats().get('publishable_pubkey')
        message = randomhex()
        res = rpc1.DEX_broadcast(message, '4', 'BASE', 'REL', pubkey, '10', '1')
        order_id = str(res.get('id'))
        res = rpc1.DEX_get(order_id)
        validate_template(res, schema_broadcast)  # same response to broadcast

    def test_dexrpc_orderbook(self, test_params):
        # check orderbook
        schema_orderbook = {
            'type': 'object',
            'properties': {
                'asks': {
                    'type': 'array',
                    'items': {
                        'type': 'object',
                        'properties': {
                            'price': {'type': 'string'},
                            'price15': {'type': 'string'},
                            'baseamount': {'type': 'string'},
                            'basesatoshis': {'type': 'integer'},
                            'relamount': {'type': 'string'},
                            'relsatoshis': {'type': 'integer'},
                            'priority': {'type': 'integer'},
                            'timestamp': {'type': 'integer'},
                            'id': {'type': 'integer'},
                            'pubkey': {'type': 'string'},
                            'hash': {'type': 'string'}
                        }
                    }
                },
                'bids': {
                    'type': 'array',
                    'items': {
                        'type': 'object',
                        'properties': {
                            'price': {'type': 'string'},
                            'price15': {'type': 'string'},
                            'baseamount': {'type': 'integer'},
                            'basesatoshis': {'type': 'integer'},
                            'relamount': {'type': 'integer'},
                            'relsatoshis': {'type': 'integer'},
                            'priority': {'type': 'integer'},
                            'timestamp': {'type': 'integer'},
                            'id': {'type': 'integer'},
                            'pubkey': {'type': 'string'},
                            'hash': {'type': 'string'}
                        }
                    }
                }
            },
            'required': ['asks', 'bids']
        }

        rpc1 = test_params.get('node1').get('rpc')
        pubkey = rpc1.DEX_stats().get('publishable_pubkey')
        message = randomhex()
        rpc1.DEX_broadcast(message, '4', 'BASE', 'REL', pubkey, '10', '1')
        res = rpc1.DEX_orderbook('', '0', 'BASE', 'REL')
        validate_template(res, schema_orderbook)

    def test_dexrpc_cancel(self, test_params):
        # cancel order
        schema_broadcast = {
            'type': 'object',
            'properties': {
                'timestamp': {'type': 'integer'},
                'recvtime': {'type': 'integer'},
                'id': {'type': 'integer'},
                'tagA': {'type': 'string'},
                'tagB': {'type': 'string'},
                'senderpub': {'type': 'string'},
                'destpub': {'type': 'string'},
                'payload': {'type': 'string'},
                'hex': {'type': 'integer'},
                'amountA': {'type': ['string']},
                'amountB': {'type': ['string']},
                'priority': {'type': 'integer'},
                'error': {'type': 'string'},
                'cancelled': {'type': 'integer'}
            },
            'required': ['timestamp', 'id', 'tagA', 'tagB', 'payload']
        }

        rpc1 = test_params.get('node1').get('rpc')
        pubkey = rpc1.DEX_stats().get('publishable_pubkey')
        message = randomhex()
        res = rpc1.DEX_broadcast(message, '4', 'BASE', 'REL', pubkey, '10', '1')
        order_id = str(res.get('id'))
        res = rpc1.DEX_cancel(order_id)
        validate_template(res, schema_broadcast)
        # currently cancel has response similar to broadcast => subject to change
        assert res.get('tagA') == 'cancel'

    def test_dexrpc_publish(self, test_params):
        # publish file
        schema_publish_subscribe = {
            'type': 'object',
            'properties': {
                'fname': {'type': 'string'},
                'result': {'type': 'string'},
                'id': {'type': 'integer'},
                'senderpub': {'type': 'string'},
                'filesize': {'type': 'integer'},
                'fragments': {'type': 'integer'},
                'numlocators': {'type': 'integer'},
                'filehash': {'type': 'string'}
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        file = 'to_publish.txt'
        write_file(file)
        res = rpc1.DEX_publish(file, '0')
        validate_template(res, schema_publish_subscribe)
        assert res.get('result') == 'success'

    def test_dexrpc_subscribe(self, test_params):
        # subscribe to published file
        schema_publish_subscribe = {
            'type': 'object',
            'properties': {
                'fname': {'type': 'string'},
                'result': {'type': 'string'},
                'id': {'type': 'integer'},
                'senderpub': {'type': 'string'},
                'filesize': {'type': 'integer'},
                'fragments': {'type': 'integer'},
                'numlocators': {'type': 'integer'},
                'filehash': {'type': 'string'}
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        pubkey = rpc1.DEX_stats().get('publishable_pubkey')
        file = 'to_publish.txt'
        write_file(file)
        res = rpc1.DEX_subscribe(file, '0', '0', pubkey)
        validate_template(res, schema_publish_subscribe)
        assert res.get('result') == 'success'

    def test_dexrpc_setpubkey(self, test_params):
        # check setpubkey with random pubkey-like string
        schema_setpub = {
            'type': 'object',
            'properties': {
                'publishable_pubkey': {'type': 'string'},
                'perfstats': {'type': 'string'},
                'result': {'type': 'string'},
                'secpkey': {'type': 'string'},
                'recvaddr': {'type': 'string'},
                'recvZaddr': {'type': 'string'},
                'handle': {'type': 'string'},
                'txpowbits': {'type': 'integer'},
                'vip': {'type': 'integer'},
                'cmdpriority': {'type': 'integer'}
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.DEX_setpubkey('01' + randomhex())
        validate_template(res, schema_setpub)
        assert res.get('result') == 'success'

    def test_dexrpc_stream(self, test_params):
        # stream file, single iteration
        schema_stream = {
            'type': 'object',
            'properties': {
                'fname': {'type': 'string'},
                'senderpub': {'type': 'string'},
                'filehash': {'type': 'string'},
                'checkhash': {'type': 'string'},
                'result': {'type': 'string'},
                'id': {'type': 'integer'},
                'filesize': {'type': 'integer'},
                'fragments': {'type': 'integer'},
                'numlocators': {'type': 'integer'}
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        filename = 'file_to_stream'
        write_empty_file(filename, 1)
        res = rpc1.DEX_stream(filename, '6')
        validate_template(res, schema_stream)
        assert res.get('result') == 'success'
        if os.path.isfile(filename):
            os.remove(filename)

    def test_dexrpc_streamsub(self, test_params):
        # subscribe to streamed file, single iteration
        schema_streamsub = {
            'type': 'object',
            'properties': {
                'fname': {'type': 'string'},
                'senderpub': {'type': 'string'},
                'filehash': {'type': 'string'},
                'checkhash': {'type': 'string'},
                'result': {'type': 'string'},
                'id': {'type': 'integer'},
                'filesize': {'type': 'integer'},
                'fragments': {'type': 'integer'},
                'numlocators': {'type': 'integer'}
            }
        }

        rpc1 = test_params.get('node1').get('rpc')
        rpc2 = test_params.get('node2').get('rpc')
        filename = 'file_to_stream'
        pubkey = rpc1.DEX_stats().get('publishable_pubkey')
        write_empty_file(filename, 1)
        res = rpc1.DEX_stream(filename, '6')
        assert res.get('result') == 'success'
        time.sleep(15)  # time to broadcast
        res = rpc2.DEX_streamsub(filename, '0', pubkey)
        assert res.get('result') == 'error'  # will always fail with current test setup
        print(res)
        validate_template(res, schema_streamsub)
        if os.path.isfile(filename):
            os.remove(filename)