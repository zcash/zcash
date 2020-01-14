#!/usr/bin/env python3
# Copyright (c) 2019 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import pytest
import json


@pytest.mark.usefixtures("proxy_connection")
class TestControlAddress:

    def test_getinfo(self, test_params):
        rpc1 = test_params.get('node1').get('rpc')
        res = rpc1.getinfo()
        assert res.get('blocks')
        assert not res.get('errors')
        rpc2 = test_params.get('node2').get('rpc')
        res = rpc2.getinfo()
        assert res.get('blocks')
        assert not res.get('errors')

    def test_help(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        res = rpc.help()
        assert "cclib method [evalcode] [JSON params]" in res
