#!/usr/bin/env python3
# Copyright (c) 2020 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

import pytest
import time
from pytest_util import env_get, get_chainstate, check_notarized, get_notary_stats


@pytest.mark.usefixtures("proxy_connection")
class TestChainSync:

    def test_base_sync(self, test_params):
        rpc = test_params.get('node1').get('rpc')
        # Get ENV test params
        coin = env_get('CHAIN', 'KMD')
        sync_timeout = env_get('TIMEOUT', 86400)
        blocktime = int(env_get('BLOCKTIME_AVR', 60))
        check_notarizations = env_get('NOTARIZATIONS', False)

        # Main loop
        start_time = time.time()
        current_time = start_time
        timeout = start_time + sync_timeout
        warnings = 0
        while timeout > current_time:
            values = get_chainstate(rpc)
            if values.get('synced'):
                print('Chain synced')
                assert values.get('blocks') == values.get('longestchain')
                if check_notarizations:
                    notarystats = get_notary_stats()
                    assert check_notarized(rpc, notarystats, coin, blocktime)
                    print("Notarization check passed")
                break
            print('Waiting synchronization...')
            time.sleep(blocktime)
            current_blocks = get_chainstate(rpc).get('blocks')
            try:
                assert current_blocks > values.get('blocks')
                warnings = 0
            except AssertionError as e:
                warnings += 1
                print("Synchronization might be stuck ", e)
            if warnings >= 5:
                raise TimeoutError("Synchronization stuck on block: ", current_blocks)
