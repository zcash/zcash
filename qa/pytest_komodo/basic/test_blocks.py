#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
from pytest_util import validate_transaction
from pytest_util import validate_template
from decimal import *


@pytest.mark.usefixtures("proxy_connection")
class TestBlockchainMethods:

    def test_coinsupply(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getinfo()
        height = res.get('blocks')
        # fixed height
        res = rpc.coinsupply(str(height))
        assert res.get('result') == 'success'
        assert isinstance(res.get('height'), int)
        # coinsupply without value should return max height
        res = rpc.coinsupply()
        assert res.get('result') == 'success'
        assert isinstance(res.get('height'), int)
        # invalid height
        res = rpc.coinsupply("-1")
        assert res.get('error') == "invalid height"
        # invalid value
        res = rpc.coinsupply("aaa")
        assert res.get('error') == "couldnt calculate supply"

    def test_getbestblockhash(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getbestblockhash()
        assert isinstance(res, str)

    def test_getblock(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'last_notarized_height': {'type': 'integer'},
                'hash': {'type': 'string'},
                'confirmations': {'type': 'integer'},
                'rawconfirmations': {'type': 'integer'},
                'size': {'type': 'integer'},
                'height': {'type': 'integer'},
                'version': {'type': 'integer'},
                'merkleroot': {'type': 'string'},
                'segid': {'type': 'integer'},
                'finalsaplingroot': {'type': 'string'},
                'tx': {'type': 'array'},
                'time': {'type': 'integer'},
                'nonce': {'type': 'string'},
                'solution': {'type': 'string'},
                'bits': {'type': 'string'},
                'difficulty': {'type': ['number', 'integer']},
                'chainwork': {'type': 'string'},
                'blocktype': {'type': 'string'},
                'valuePools': {
                    'type': 'array',
                    'items': {
                        'type': 'object',
                        'properties': {
                            'id': {'type': 'string'},
                            'monitored': {'type': 'boolean'},
                            'chainValue': {'type': ['number', 'integer']},
                            'chainValueZat': {'type': ['number', 'integer']},
                            'valueDelta': {'type': ['number', 'integer']},
                            'valueDeltaZat': {'type': ['number', 'integer']}
                        }
                    }
                },
                'previousblockhash': {'type': 'string'},
                'nextblockhash': {'type': 'string'}
            },
            'required': ['last_notarized_height', 'hash', 'confirmations', 'rawconfirmations', 'size', 'height',
                         'version', 'merkleroot', 'segid', 'finalsaplingroot', 'tx', 'time', 'nonce', 'solution',
                         'bits', 'difficulty', 'chainwork', 'anchor', 'blocktype', 'valuePools',
                         'previousblockhash']
        }
        rpc = test_params.get('node1').get('rpc')
        blocknum = str(rpc.getblockcount())
        res = rpc.getblock(blocknum)
        validate_template(res, schema)
        res = rpc.getblock(blocknum, False)
        assert isinstance(res, str)

    def test_getblockchaininfo(self, test_params):
        schema = {
            'type': 'object',
            'required': ['chain', 'blocks', 'synced', 'headers', 'bestblockhash', 'upgrades', 'consensus',
                         'difficulty', 'verificationprogress', 'chainwork', 'pruned', 'commitments'],
            'properties': {
                'chain': {'type': 'string'},
                'blocks': {'type': 'integer'},
                'synced': {'type': 'boolean'},
                'headers': {'type': 'integer'},
                'bestblockhash': {'type': 'string'},
                'difficulty': {'type': ['integer', 'number']},
                'verificationprogress': {'type': ['integer', 'number']},
                'chainwork': {'type': 'string'},
                'pruned': {'type': 'boolean'},
                'commitments': {'type': ['integer', 'number']},
                'valuePools': {'type': 'array',
                               'items': {'type': 'object'}},
                'upgrades': {'type': 'object'},
                'consensus': {'type': 'object'}
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getblockchaininfo()
        validate_template(res, schema)

    def test_getblockcount(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getinfo()
        height = res.get('blocks')
        res = rpc.getblockcount()
        assert res == height

    def test_getblockhash(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getinfo()
        height = res.get('blocks')
        res = rpc.getblockhash(height)
        assert isinstance(res, str)

    #
    #  timestampindex -- required param
    #
    # def test_getblockhashes(self, test_params):
    #     test_values = {
    #         'high': 101,
    #         'low': 99,
    #         'options': '{"noOrphans":false, "logicalTimes":true}'
    #     }
    #     rpc = test_params.get('node1').get('rpc')
    #     res = rpc.getblockhashes(test_values['high'], test_values['low'], test_values['options'])

    def test_getblockheader(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'last_notarized_height': {'type': 'integer'},
                'hash': {'type': 'string'},
                'confirmations': {'type': 'integer'},
                'rawconfirmations': {'type': 'integer'},
                'height': {'type': 'integer'},
                'version': {'type': 'integer'},
                'merkleroot': {'type': 'string'},
                'segid': {'type': 'integer'},
                'finalsaplingroot': {'type': 'string'},
                'time': {'type': 'integer'},
                'nonce': {'type': 'string'},
                'solution': {'type': 'string'},
                'bits': {'type': 'string'},
                'difficulty': {'type': ['number', 'integer']},
                'chainwork': {'type': 'string'},
                'previousblockhash': {'type': 'string'},
                'nextblockhash': {'type': 'string'}
            },
            'required': ['last_notarized_height', 'hash', 'confirmations', 'rawconfirmations',
                         'height', 'version', 'merkleroot', 'segid', 'finalsaplingroot', 'time',
                         'nonce', 'solution', 'bits', 'difficulty', 'chainwork', 'previousblockhash']
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getinfo()
        block = res.get('blocks')
        blockhash = rpc.getblockhash(block)
        res1 = rpc.getblockheader(blockhash)
        res2 = rpc.getblockheader(blockhash, True)
        assert res1 == res2
        validate_template(res1, schema)
        res = rpc.getblockheader(blockhash, False)
        assert isinstance(res, str)

    def test_getchaintips(self, test_params):
        schema = {
            'type': 'array',
            'items': {
                'type': 'object',
                'required': ['height', 'hash', 'branchlen', 'status'],
                'properties': {
                    'height': {'type': 'integer'},
                    'hash': {'type': 'string'},
                    'branchlen': {'type': 'integer'},
                    'status': {'type': 'string'}
                }
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getchaintips()
        validate_template(res, schema)

    def test_getchaintxstats(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'time': {'type': 'integer'},
                'txcount': {'type': 'integer'},
                'window_final_block_hash': {'type': 'string'},
                'window_final_block_count': {'type': 'integer'},
                'window_block_count': {'type': 'integer'},
                'window_tx_count': {'type': 'integer'},
                'window_interval': {'type': 'integer'},
                'txrate': {'type': ['number', 'integer']}
            },
            'required': ['time', 'txcount', 'txrate', 'window_final_block_hash', 'window_interval',
                         'window_block_count', 'window_tx_count']
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getchaintxstats()
        validate_template(res, schema)
        res = rpc.getinfo()
        nblocks = (int(res.get('blocks') / 2))
        blockhash = rpc.getblockhash(res.get('blocks'))
        res = rpc.getchaintxstats(nblocks)
        validate_template(res, schema)
        res = rpc.getchaintxstats(nblocks, blockhash)
        validate_template(res, schema)

    def test_getdifficulty(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getdifficulty()
        # python-bitcoinrpc Proxy can return value as decimal
        assert isinstance(res, float) or isinstance(res, int) or isinstance(res, Decimal)

    #
    # Only applies to -ac_staked Smart Chains
    #
    # def test_(self, test_params):
    #     test_values = {}

    def test_getmempoolinfo(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getmempoolinfo()
        assert isinstance(res.get('size'), int)
        assert isinstance(res.get('bytes'), int)
        assert isinstance(res.get('usage'), int)

    #
    # The method requires spentindex to be enabled.
    # txid 68ee9d23ba51e40112be3957dd15bc5c8fa9a751a411db63ad0c8205bec5e8a1
    #
    # def test_getspentinfo(self, test_params):
    #     pass

    def test_gettxout(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'bestblock': {'type': 'string'},
                'confirmations': {'type': 'integer'},
                'rawconfirmations': {'type': 'integer'},
                'value': {'type': 'number'},
                'scriptPubKey': {
                    'type': 'object',
                    'properties': {
                                 'asm': {'type': 'string'},
                                 'hex': {'type': 'string'},
                                 'reqSigs': {'type': 'integer'},
                                 'type': {'type': 'string'},
                                 'addresses': {
                                               'type': 'array',
                                               'items': {'type': 'string'}
                                               }
                                  }
                                 },
                'version': {'type': 'integer'},
                'coinbase': {'type': 'boolean'}
            },
            'required': ['bestblock', 'confirmations', 'rawconfirmations',
                         'value', 'scriptPubKey', 'version', 'coinbase']
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listunspent()
        txid = res[0].get('txid')
        vout = res[0].get('vout')
        res = rpc.gettxout(txid, vout)
        validate_template(res, schema)
        res = rpc.gettxout(txid, -1)
        assert not res  # gettxout retuns None when vout does not exist

    def test_gettxoutproof(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listunspent()
        txid = res[0].get('txid')
        res = rpc.gettxoutproof([txid])
        assert isinstance(res, str)

    def test_gettxoutsetinfo(self, test_params):
        schema = {
            'type': 'object',
            'required': ['height', 'bestblock', 'transactions', 'txouts', 'bytes_serialized',
                         'hash_serialized', 'total_amount'],
            'properties': {
                'height': {'type': 'integer'},
                'bestblock': {'type': 'string'},
                'transactions': {'type': 'integer'},
                'txouts': {'type': 'integer'},
                'bytes_serialized': {'type': 'integer'},
                'hash_serialized': {'type': 'string'},
                'total_amount': {'type': ['integer', 'number']}
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.gettxoutsetinfo()
        validate_template(res, schema)

    def test_kvupdate(self, test_params):
        test_values = {
            'v_key': 'valid_key',
            'value': 'test+value',
            'days': '2',
            'pass': 'secret',
            'n_key': 'invalid_key',
            'keylen': 9
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.kvupdate(test_values['v_key'], test_values['value'], test_values['days'], test_values['pass'])
        assert res.get('key') == test_values['v_key']
        assert res.get('keylen') == test_values['keylen']
        assert res.get('value') == test_values['value']

    def test_getrawmempool(self, test_params):
        test_values = {
            'key': 'mempool_key',
            'value': 'key_value',
            'days': '1',
            'pass': 'secret'
        }  # to get info into mempool, we need to create tx, kvupdate call creates one for us
        rpc = test_params.get('node1').get('rpc')
        res = rpc.kvupdate(test_values['key'], test_values['value'], test_values['days'], test_values['pass'])
        txid = res.get('txid')
        kvheight = res.get('height')
        res = rpc.getrawmempool()
        assert txid in res
        res = rpc.getrawmempool(False)  # False is default value, res should be same as in call above
        assert txid in res
        res = rpc.getrawmempool(True)
        assert res.get(txid).get('height') == kvheight

    def test_kvsearch(self, test_params):
        test_values = {
            'key': 'search_key',
            'value': 'search_value',
            'days': '1',
            'pass': 'secret'
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.kvupdate(test_values['key'], test_values['value'], test_values['days'], test_values['pass'])
        txid = res.get('txid')
        keylen = res.get('keylen')
        validate_transaction(rpc, txid, 1)  # wait for block
        res = rpc.kvsearch(test_values['key'])
        assert res.get('key') == test_values['key']
        assert res.get('keylen') == keylen
        assert res.get('value') == test_values['value']

    def test_notaries(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.notaries('1')
        assert res.get('notaries')  # call should return list of notary nodes disregarding blocknum

    def test_minerids(self, test_params):
        test_values = {
            'error': "couldnt extract minerids"
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.minerids('1')
        assert res.get('error') == test_values['error'] or isinstance(res.get('mined'), list)
        # likely to fail on bootstrap test chains

    def test_verifychain(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.verifychain()
        assert res  # rpc returns True if chain was verified

    def test_verifytxoutproof(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listunspent()
        txid = res[0].get('txid')
        txproof = rpc.gettxoutproof([txid])
        res = rpc.verifytxoutproof(txproof)
        assert res[0] == txid
