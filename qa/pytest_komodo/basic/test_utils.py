#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
from pytest_util import validate_template


@pytest.mark.usefixtures("proxy_connection")
class TestUtil:

    def test_createmultisig(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'address': {'type': 'string'},
                'redeemScript': {'type': 'string'},
            }
        }
        rpc = test_params.get('node1').get('rpc')
        keys = [test_params.get('node1').get('pubkey'), test_params.get('node2').get('pubkey')]
        res = rpc.createmultisig(2, keys)
        validate_template(res, schema)

    def test_decodeccopret(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        schema = {
            'type': 'object',
            'properties': {
                'result': {'type': 'string'},
                'OpRets': {
                    'type': 'array',
                    'items': {
                        'type': 'object',
                        'properties': {
                            'eval_code': {'type': 'string'},
                            'function': {'type': 'string'}
                        }
                    }
                }
            }
        }
        rawhex = rpc.oraclescreate('TEST', 'Orale creation tx for test purpose', 'L')
        res = rpc.decoderawtransaction(rawhex.get('hex'))
        vouts = res.get('vout')
        ccopret = ''
        for n in vouts:
            if n.get('scriptPubKey').get('type') == 'nulldata':
                ccopret = n.get('scriptPubKey').get('hex')
                break
        res = rpc.decodeccopret(ccopret)
        assert res.get('result') == 'success'
        validate_template(res, schema)

    def test_estimatefee(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.estimatefee(6)
        assert isinstance(res, int) or isinstance(res, float)

    def test_estimatepriority(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.estimatepriority(6)
        assert isinstance(res, int) or isinstance(res, float)

    def test_invalidate_reconsider_block(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getinfo()
        block = res.get('blocks')
        blockhash = rpc.getblockhash(block)
        res = rpc.invalidateblock(blockhash)
        assert not res  # none response on success
        res = rpc.reconsiderblock(blockhash)
        assert not res  # none response on success

    def test_txnotarizedconfirmed(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.listunspent()
        txid = res[0].get('txid')
        res = rpc.txnotarizedconfirmed(txid)
        assert isinstance(res.get('result'), bool)

    def test_validateaddress(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'isvalid': {'type': 'boolean'},
                'ismine': {'type': 'boolean'},
                'isscript': {'type': 'boolean'},
                'iscompressed': {'type': 'boolean'},
                'account': {'type': 'string'},
                'pubkey': {'type': 'string'},
                'address': {'type': 'string'},
                'scriptPubKey': {'type': 'string'},
                'segid': {'type': 'integer'}
            }
        }
        rpc = test_params.get('node1').get('rpc')
        addr = test_params.get('node1').get('address')
        res = rpc.validateaddress(addr)
        validate_template(res, schema)
        assert addr == res.get('address')

    def test_verifymessage(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        addr = test_params.get('node1').get('address')
        sign = rpc.signmessage(addr, 'test test')
        assert isinstance(sign, str)
        res = rpc.verifymessage(addr, sign, "test test")
        assert isinstance(res, bool)
