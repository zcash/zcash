#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

"""This isn't a standard RPC integration test, rather it's an experiment aimed at isolating sources
of intermittent failures in the test suite.

The wait_and_assert_oprationid_status function catches exceptions, and error states, only returning
a txid if a successful transaction was reported for the operation <-- Hmmm...


"""

import sys
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import get_coinbase_address


from decimal import Decimal
import simplejson as json
import time

# Test wallet z_listunspent behaviour across network upgrades
class ZGetOperationResultsLatentSuccess(BitcoinTestFramework):

    def _send_amt(self, from_addr, to_addr, amnt):
        recipients = [{"address": to_addr, "amount": amnt}]
        myopid = self.nodes[0].z_sendmany(from_addr, recipients)
        for _ in range(1, 3000):
            results = self.nodes[0].z_getoperationresult([myopid])
            if len(results) > 0:
                result = results[0]
                break
            time.sleep(.01)
        if result['status'] != 'success':
            sys.exit(56)
        return result # NOTE:  This test doesn't actually use this.

    def run_test(self):
        coinbase_addr = get_coinbase_address(self.nodes[0])
        self.nodes[0].generate(10)
        faucet = self.nodes[0].z_getnewaddress('sapling')
        breward_minus_tx_fee = Decimal('9.9999')
        self._send_amt(coinbase_addr, faucet, breward_minus_tx_fee)
        self.nodes[0].generate(10)
        millizec = Decimal('0.001')
        lag_times = []
        sync_times = []
        for iteration in range(100):
            print("Iteration: %s" % iteration)
            toaddr = self.nodes[0].z_getnewaddress('sapling')
            self._send_amt(faucet, toaddr, millizec)
            start = time.time()
            while self.nodes[0].z_listunspent(0, 9999, False, [toaddr]) == []:
                time.sleep(0.001)
            stop = time.time()
            lagtime = Decimal(stop) - Decimal(start)
            lag_times.append(lagtime)
            self.nodes[0].generate(10)
            sync_start = time.time()
            self.sync_all()
            sync_stop = time.time()
            sync_times.append(sync_stop - sync_start)
        print("Lag Loop Completed,")
        print("lagtimes are:")
        print(json.loads(str([str(x) for x in lag_times]).replace("'", '"')))

if __name__ == '__main__':
    ZGetOperationResultsLatentSuccess().main()
