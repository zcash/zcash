#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_true, assert_false
from test_framework.util import wait_and_assert_operationid_status
from decimal import Decimal

my_memo_str = 'c0ffee' # stay awake
my_memo = '633066666565'
my_memo = my_memo + '0'*(1024-len(my_memo))

no_memo = 'f6' + ('0'*1022) # see section 5.5 of the protocol spec

fee = Decimal('0.0001')

class ListReceivedTest (BitcoinTestFramework):

    def generate_and_sync(self, new_height):
        current_height = self.nodes[0].getblockcount()
        assert(new_height > current_height)
        self.nodes[0].generate(new_height - current_height)
        self.sync_all()
        assert_equal(new_height, self.nodes[0].getblockcount())

    def run_test_release(self, release, height):
        self.generate_and_sync(height+1)
        taddr = self.nodes[1].getnewaddress()
        zaddr1 = self.nodes[1].z_getnewaddress(release)
        zaddrExt = self.nodes[3].z_getnewaddress(release)

        self.nodes[0].sendtoaddress(taddr, 4.0)
        self.generate_and_sync(height+2)

        # Send 1 ZEC to zaddr1
        opid = self.nodes[1].z_sendmany(taddr, [
            {'address': zaddr1, 'amount': 1, 'memo': my_memo},
            {'address': zaddrExt, 'amount': 2},
        ])
        txid = wait_and_assert_operationid_status(self.nodes[1], opid)
        self.sync_all()

        # Decrypted transaction details should be correct
        pt = self.nodes[1].z_viewtransaction(txid)
        assert_equal(pt['txid'], txid)
        assert_equal(len(pt['spends']), 0)
        assert_equal(len(pt['outputs']), 1 if release == 'sprout' else 2)

        # Output orders can be randomized, so we check the output
        # positions and contents separately
        outputs = []

        assert_equal(pt['outputs'][0]['type'], release)
        if release == 'sprout':
            assert_equal(pt['outputs'][0]['js'], 0)
            jsOutputPrev = pt['outputs'][0]['jsOutput']
        elif pt['outputs'][0]['address'] == zaddr1:
            assert_equal(pt['outputs'][0]['outgoing'], False)
            assert_equal(pt['outputs'][0]['memoStr'], my_memo_str)
        else:
            assert_equal(pt['outputs'][0]['outgoing'], True)
        outputs.append({
            'address': pt['outputs'][0]['address'],
            'value': pt['outputs'][0]['value'],
            'valueZat': pt['outputs'][0]['valueZat'],
            'memo': pt['outputs'][0]['memo'],
        })

        if release != 'sprout':
            assert_equal(pt['outputs'][1]['type'], release)
            if pt['outputs'][1]['address'] == zaddr1:
                assert_equal(pt['outputs'][1]['outgoing'], False)
                assert_equal(pt['outputs'][1]['memoStr'], my_memo_str)
            else:
                assert_equal(pt['outputs'][1]['outgoing'], True)
            outputs.append({
                'address': pt['outputs'][1]['address'],
                'value': pt['outputs'][1]['value'],
                'valueZat': pt['outputs'][1]['valueZat'],
                'memo': pt['outputs'][1]['memo'],
            })

        assert({
            'address': zaddr1,
            'value': Decimal('1'),
            'valueZat': 100000000,
            'memo': my_memo,
        } in outputs)
        if release != 'sprout':
            assert({
                'address': zaddrExt,
                'value': Decimal('2'),
                'valueZat': 200000000,
                'memo': no_memo,
            } in outputs)

        r = self.nodes[1].z_listreceivedbyaddress(zaddr1)
        assert_equal(0, len(r), "Should have received no confirmed note")

        # No confirmation required, one note should be present
        r = self.nodes[1].z_listreceivedbyaddress(zaddr1, 0)
        assert_equal(1, len(r), "Should have received one (unconfirmed) note")
        assert_equal(txid, r[0]['txid'])
        assert_equal(1, r[0]['amount'])
        assert_false(r[0]['change'], "Note should not be change")
        assert_equal(my_memo, r[0]['memo'])

        # Confirm transaction (1 ZEC from taddr to zaddr1)
        self.generate_and_sync(height+3)

        # Require one confirmation, note should be present
        assert_equal(r, self.nodes[1].z_listreceivedbyaddress(zaddr1))

        # Generate some change by sending part of zaddr1 to zaddr2
        txidPrev = txid
        zaddr2 = self.nodes[1].z_getnewaddress(release)
        opid = self.nodes[1].z_sendmany(zaddr1,
            [{'address': zaddr2, 'amount': 0.6}])
        txid = wait_and_assert_operationid_status(self.nodes[1], opid)
        self.sync_all()
        self.generate_and_sync(height+4)

        # Decrypted transaction details should be correct
        pt = self.nodes[1].z_viewtransaction(txid)
        assert_equal(pt['txid'], txid)
        assert_equal(len(pt['spends']), 1)
        assert_equal(len(pt['outputs']), 2)

        assert_equal(pt['spends'][0]['type'], release)
        assert_equal(pt['spends'][0]['txidPrev'], txidPrev)
        if release == 'sprout':
            assert_equal(pt['spends'][0]['js'], 0)
            # jsSpend is randomised during transaction creation
            assert_equal(pt['spends'][0]['jsPrev'], 0)
            assert_equal(pt['spends'][0]['jsOutputPrev'], jsOutputPrev)
        else:
            assert_equal(pt['spends'][0]['spend'], 0)
            assert_equal(pt['spends'][0]['outputPrev'], 0)
        assert_equal(pt['spends'][0]['address'], zaddr1)
        assert_equal(pt['spends'][0]['value'], Decimal('1.0'))
        assert_equal(pt['spends'][0]['valueZat'], 100000000)

        # Output orders can be randomized, so we check the output
        # positions and contents separately
        outputs = []

        assert_equal(pt['outputs'][0]['type'], release)
        if release == 'sapling':
            assert_equal(pt['outputs'][0]['output'], 0)
            assert_equal(pt['outputs'][0]['outgoing'], False)
        outputs.append({
            'address': pt['outputs'][0]['address'],
            'value': pt['outputs'][0]['value'],
            'valueZat': pt['outputs'][0]['valueZat'],
            'memo': pt['outputs'][0]['memo'],
        })

        assert_equal(pt['outputs'][1]['type'], release)
        if release == 'sapling':
            assert_equal(pt['outputs'][1]['output'], 1)
            assert_equal(pt['outputs'][1]['outgoing'], False)
        outputs.append({
            'address': pt['outputs'][1]['address'],
            'value': pt['outputs'][1]['value'],
            'valueZat': pt['outputs'][1]['valueZat'],
            'memo': pt['outputs'][1]['memo'],
        })

        assert({
            'address': zaddr2,
            'value': Decimal('0.6'),
            'valueZat': 60000000,
            'memo': no_memo,
        } in outputs)
        assert({
            'address': zaddr1,
            'value': Decimal('0.3999'),
            'valueZat': 39990000,
            'memo': no_memo,
        } in outputs)

        # zaddr1 should have a note with change
        r = self.nodes[1].z_listreceivedbyaddress(zaddr1, 0)
        r = sorted(r, key = lambda received: received['amount'])
        assert_equal(2, len(r), "zaddr1 Should have received 2 notes")

        assert_equal(txid, r[0]['txid'])
        assert_equal(Decimal('0.4')-fee, r[0]['amount'])
        assert_true(r[0]['change'], "Note valued at (0.4-fee) should be change")
        assert_equal(no_memo, r[0]['memo'])

        # The old note still exists (it's immutable), even though it is spent
        assert_equal(Decimal('1.0'), r[1]['amount'])
        assert_false(r[1]['change'], "Note valued at 1.0 should not be change")
        assert_equal(my_memo, r[1]['memo'])

        # zaddr2 should not have change
        r = self.nodes[1].z_listreceivedbyaddress(zaddr2, 0)
        r = sorted(r, key = lambda received: received['amount'])
        assert_equal(1, len(r), "zaddr2 Should have received 1 notes")
        assert_equal(txid, r[0]['txid'])
        assert_equal(Decimal('0.6'), r[0]['amount'])
        assert_false(r[0]['change'], "Note valued at 0.6 should not be change")
        assert_equal(no_memo, r[0]['memo'])

    def run_test(self):
        self.run_test_release('sprout', 200)
        self.run_test_release('sapling', 214)

if __name__ == '__main__':
    ListReceivedTest().main()
