#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import time
from decimal import *
from pytest_util import validate_template


@pytest.mark.usefixtures("proxy_connection")
class TestNetworkMining:

    def test_generate(self, test_params):  # generate, getgenerate, setgenerate calls
        rpc = test_params.get('node1').get('rpc')
        res = rpc.setgenerate(False, -1)
        assert not res
        res = rpc.getgenerate()
        assert not res.get('generate')
        assert res.get('numthreads') == -1
        res = rpc.setgenerate(True, 1)
        assert not res
        res = rpc.getgenerate()
        assert res.get('generate')
        assert res.get('numthreads') == 1
        # rpc.generate(2)  -- requires regtest mode

    def test_getmininginfo(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getinfo()
        blocks = res.get('blocks')
        rpc.setgenerate(True, 1)
        res = rpc.getmininginfo()
        assert res.get('blocks') == blocks
        assert res.get('generate')
        assert res.get('numthreads') == 1

    def test_getblocksubsidy(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'miner': {'type': 'number'}
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getblocksubsidy()
        validate_template(res, schema)
        res = rpc.getinfo()
        block = res.get('blocks')
        res = rpc.getblocksubsidy(block)
        validate_template(res, schema)

    def test_getblocktemplate(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getblocktemplate()
        validate_template(res)

    def test_getlocalsolps(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getlocalsolps()
        # python-bitcoinrpc Proxy can return value as decimal
        assert isinstance(res, float) or isinstance(res, int) or isinstance(res, Decimal)

    #  getnetworkhashps call is deprecated

    def test_getnetworksolps(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getnetworksolps()
        assert isinstance(res, float) or isinstance(res, int)

    def test_prioritisetransaction(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.prioritisetransaction("68ee9d23ba51e40112be3957dd15bc5c8fa9a751a411db63ad0c8205bec5e8a1", 0.0, 10000)
        assert res  # returns True on success

    def test_submitblock(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getinfo()
        height = res.get('blocks')
        block = rpc.getblock(str(height), False)
        res = rpc.submitblock(block)
        assert res == 'duplicate'

    def test_getnetworkinfo(self, test_params):
        schema = {
            'type': 'object',
            'properties': {
                'version': {'type': 'integer'},
                'subversion': {'type': 'string'},
                'protocolversion': {'type': 'integer'},
                'localservices': {'type': 'string'},
                'timeoffset': {'type': 'integer'},
                'connections': {'type': 'integer'},
                'networks': {
                    'type': 'array',
                    'items': {
                        'type': 'object',
                        'properties': {
                            'name': {'type': 'string'},
                            'limited': {'type': 'boolean'},
                            'reachable': {'type': 'boolean'},
                            'proxy': {'type': 'string'},
                            'proxy_randomize_credentials': {'type': 'boolean'},
                        }
                    }
                },
                'relayfee': {'type': 'number'},
                'localadresses': {
                    'type': 'array',
                    'items': {
                        'type': 'object',
                        'properties': {
                            'address': {'type': 'string'},
                            'port': {'type': 'integer'},
                            'score': {'type': 'integer'},
                        },
                        'required': ['address', 'port', 'score']
                    }
                },
                'warnings': {'type': 'string'}
            },
            'required': ['version', 'subversion', 'protocolversion', 'localservices', 'timeoffset', 'connections',
                         'networks', 'relayfee', 'localaddresses', 'warnings']
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getnetworkinfo()
        validate_template(res, schema)

    def test_getconnectioncount(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getconnectioncount()
        assert isinstance(res, int)

    def test_getpeerinfo(self, test_params):
        schema = {
            'type': 'array',
            'items': {
                'type': 'object',
                'properties': {
                    'id': {'type': 'integer'},
                    'addr': {'type': 'string'},
                    'addrlocal': {'type': 'string'},
                    'services': {'type': 'string'},
                    'lastsend': {'type': 'integer'},
                    'lastrecv': {'type': 'integer'},
                    'bytessent': {'type': 'integer'},
                    'bytesrecv': {'type': 'integer'},
                    'conntime': {'type': 'integer'},
                    'timeoffset': {'type': 'integer'},
                    'pingtime': {'type': ['number', 'integer']},
                    'pingwait': {'type': ['number', 'integer']},
                    'version': {'type': 'integer'},
                    'subver': {'type': 'string'},
                    'inbound': {'type': 'boolean'},
                    'startingheight': {'type': 'integer'},
                    'banscore': {'type': ['number', 'integer']},
                    'synced_headers': {'type': 'integer'},
                    'synced_blocks': {'type': 'integer'},
                    'inflight': {'type': 'array'},
                    'whitelisted': {'type': 'boolean'},
                    'number': {'type': 'integer'}
                }
            }
        }
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getpeerinfo()
        validate_template(res, schema)

    def test_getnettotals(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getnettotals()
        assert isinstance(res.get('timemillis'), int)

    def test_ping(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.ping()
        assert not res  # ping call has empty response

    def test_getdeprecationinfo(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.getdeprecationinfo()
        assert isinstance(res.get('version'), int)

    def test_addnode(self, test_params):
        rpc = test_params.get('node2').get('rpc')
        cnode_ip = test_params.get('node1').get('rpc_ip')
        connect_node = (cnode_ip + ':' + str(test_params.get('node1').get('net_port')))
        res = rpc.addnode(connect_node, 'remove')
        assert not res
        res = rpc.addnode(connect_node, 'onetry')
        assert not res
        res = rpc.addnode(connect_node, 'add')
        assert not res
        rpc.ping()

    def test_disconnectnode(self, test_params):
        rpc = test_params.get('node2').get('rpc')
        cnode_ip = test_params.get('node1').get('rpc_ip')
        disconnect_node = (cnode_ip + ':' + str(test_params.get('node1').get('net_port')))
        rpc.addnode(disconnect_node, 'remove')  # remove node from addnode list to prevent reconnection
        res = rpc.disconnectnode(disconnect_node)  # has empty response
        assert not res
        time.sleep(15)  # time to stop node connection
        res = rpc.getpeerinfo()
        for peer in res:
            assert peer.get('addr') != disconnect_node
        rpc.addnode(disconnect_node, 'add')
        time.sleep(10)  # wait for 2nd to reconnect after test

    def test_ban(self, test_params):  # setban, listbanned, clearbanned calls
        rpc = test_params.get('node1').get('rpc')
        ban_list = ['144.144.140.0/255.255.255.0', '144.144.140.12/255.255.255.255', '192.168.0.0/255.255.0.0']
        res = rpc.clearbanned()
        assert not res
        res = rpc.setban(ban_list[0], 'add', 64800)
        assert not res
        res = rpc.setban(ban_list[1], 'add', 64800)
        assert not res
        res = rpc.setban(ban_list[2], 'add', 64800)
        assert not res
        res = rpc.listbanned()
        for peer in res:
            node = peer.get('address')
            assert node in ban_list
        rpc.clearbanned()
        res = rpc.listbanned()
        assert not res
