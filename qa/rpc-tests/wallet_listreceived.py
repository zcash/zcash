#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_true,
    assert_false,
    get_coinbase_address,
    DEFAULT_FEE,
    DEFAULT_FEE_ZATS
)
from test_framework.util import wait_and_assert_operationid_status
from decimal import Decimal

my_memo_str = 'c0ffee' # stay awake
my_memo = '633066666565'
my_memo = my_memo + '0'*(1024-len(my_memo))

no_memo = 'f6' + ('0'*1022) # see section 5.5 of the protocol spec

class ListReceivedTest (BitcoinTestFramework):
    def generate_and_sync(self, new_height):
        current_height = self.nodes[0].getblockcount()
        assert(new_height > current_height)
        self.nodes[0].generate(new_height - current_height)
        self.sync_all()
        assert_equal(new_height, self.nodes[0].getblockcount())

    def test_received_sprout(self, height):
        self.generate_and_sync(height+2)

        taddr0 = get_coinbase_address(self.nodes[0])
        zaddr1 = self.nodes[1].z_getnewaddress('sprout')

        # Send 10 ZEC each zaddr1 and zaddrExt via z_shieldcoinbase
        result = self.nodes[0].z_shieldcoinbase(taddr0, zaddr1, 0, 1)
        txid_shielding1 = wait_and_assert_operationid_status(self.nodes[0], result['opid'])

        # TODO: the following z_shieldcoinbase call can't find any funds to shield,
        # although there should be a unused transparent coinbase output. Is this a
        # problem in z_shieldcoinbase output selection?
        #
        # zaddrExt = self.nodes[3].z_getnewaddress('sprout')
        # result = self.nodes[0].z_shieldcoinbase(taddr0, zaddrExt, 0, 1)
        # txid_shieldingExt = wait_and_assert_operationid_status(self.nodes[0], result['opid'])

        self.sync_all()

        # Decrypted transaction details should be correct
        # for txid in [txid_shielding1, txid_shieldingExt]:
        for txid in [txid_shielding1]:
            pt = self.nodes[1].z_viewtransaction(txid)
            assert_equal(pt['txid'], txid)
            assert_equal(len(pt['spends']), 0)
            assert_equal(len(pt['outputs']), 1)
            assert_equal(pt['outputs'][0]['type'], 'sprout')
            assert_equal(pt['outputs'][0]['js'], 0)
            jsOutputPrev = pt['outputs'][0]['jsOutput']

            outputs = []
            outputs.append({
                'address': pt['outputs'][0]['address'],
                'value': pt['outputs'][0]['value'],
                'valueZat': pt['outputs'][0]['valueZat'],
                'memo': pt['outputs'][0]['memo'],
            })
            assert({
                'address': zaddr1,
                'value': Decimal('10'),
                'valueZat': 1000000000,
                'memo': no_memo,
            } in outputs)

        r = self.nodes[1].z_listreceivedbyaddress(zaddr1)
        assert_equal(0, len(r), "Should have received no confirmed note")
        c = self.nodes[1].z_getnotescount()
        assert_equal(0, c['sprout'], "Count of confirmed notes should be 0")

        # No confirmation required, one note should be present
        r = self.nodes[1].z_listreceivedbyaddress(zaddr1, 0)
        assert_equal(1, len(r), "Should have received one (unconfirmed) note")
        assert_equal(txid_shielding1, r[0]['txid'])
        assert_equal(10, r[0]['amount'])
        assert_equal(1000000000, r[0]['amountZat'])
        assert_false(r[0]['change'], "Note should not be change")
        assert_equal(no_memo, r[0]['memo'])
        assert_equal(0, r[0]['confirmations'])
        assert_equal(-1, r[0]['blockindex'])
        assert_equal(0, r[0]['blockheight'])

        c = self.nodes[1].z_getnotescount(0)
        assert_equal(1, c['sprout'], "Count of unconfirmed notes should be 1")

        # Confirm transaction (10 ZEC shielded)
        self.generate_and_sync(height+3)

        # adjust confirmations
        r[0]['confirmations'] = 1
        # adjust blockindex
        r[0]['blockindex'] = 1
        # adjust height
        r[0]['blockheight'] = height + 3

        # Require one confirmation, note should be present
        assert_equal(r, self.nodes[1].z_listreceivedbyaddress(zaddr1))

        taddr = self.nodes[1].getnewaddress()
        # Generate some change by sending part of zaddr1 back to taddr
        opid = self.nodes[1].z_sendmany(zaddr1, [{'address': taddr, 'amount': 0.6}])
        txid = wait_and_assert_operationid_status(self.nodes[1], opid)

        self.generate_and_sync(height+4)

        # Decrypted transaction details should be correct
        pt = self.nodes[1].z_viewtransaction(txid)
        assert_equal(pt['txid'], txid)
        assert_equal(len(pt['spends']), 1)
        # TODO: enable once z_viewtransaction displays transparent elements
        # assert_equal(len(pt['outputs']), 2)
        assert_equal(len(pt['outputs']), 1)

        assert_equal(pt['spends'][0]['type'], 'sprout')
        assert_equal(pt['spends'][0]['txidPrev'], txid_shielding1)
        assert_equal(pt['spends'][0]['js'], 0)
        assert_equal(pt['spends'][0]['jsPrev'], 0)
        assert_equal(pt['spends'][0]['jsOutputPrev'], jsOutputPrev)
        assert_equal(pt['spends'][0]['address'], zaddr1)
        assert_equal(pt['spends'][0]['value'], Decimal('10.0'))
        assert_equal(pt['spends'][0]['valueZat'], 1000000000)

        # We expect a transparent output and a Sprout output. Output order can
        # be randomized.
        outputs.append({
            'type': pt['outputs'][0]['type'],
            'address': pt['outputs'][0]['address'],
            'value': pt['outputs'][0]['value'],
            'valueZat': pt['outputs'][0]['valueZat'],
            'memo': pt['outputs'][0]['memo'],
        })

        # TODO: enable once z_viewtransaction displays transparent elements
        #
        # outputs.append({
        #     'type': ['outputs'][1]['type'],
        #     'address': pt['outputs'][1]['address'],
        #     'value': pt['outputs'][1]['value'],
        #     'valueZat': pt['outputs'][1]['valueZat'],
        #     'memo': pt['outputs'][1]['memo'],
        # })
        #
        # assert({
        #     'type': 'transparent',
        #     'address': taddr,
        #     'value': Decimal('0.6'),
        #     'valueZat': 60000000,
        # } in outputs)
        assert({
            'type': 'sprout',
            'address': zaddr1,
            'value': Decimal('9.4') - DEFAULT_FEE,
            'valueZat': 940000000 - DEFAULT_FEE_ZATS,
            'memo': no_memo,
        } in outputs)

        # zaddr1 should have a note with change
        r = self.nodes[1].z_listreceivedbyaddress(zaddr1, 0)
        r = sorted(r, key = lambda received: received['amount'])
        assert_equal(2, len(r), "zaddr1 Should have received 2 notes")

        assert_equal(txid, r[0]['txid'])
        assert_equal(Decimal('9.4')-DEFAULT_FEE, r[0]['amount'])
        assert_equal(940000000-DEFAULT_FEE_ZATS, r[0]['amountZat'])
        assert_true(r[0]['change'], "Note valued at (9.4-"+str(DEFAULT_FEE)+") should be change")
        assert_equal(no_memo, r[0]['memo'])

        # The old note still exists (it's immutable), even though it is spent
        assert_equal(Decimal('10.0'), r[1]['amount'])
        assert_equal(1000000000, r[1]['amountZat'])
        assert_false(r[1]['change'], "Note valued at 1.0 should not be change")
        assert_equal(no_memo, r[1]['memo'])

    def test_received_sapling(self, height):
        self.generate_and_sync(height+1)
        taddr = self.nodes[1].getnewaddress()
        zaddr1 = self.nodes[1].z_getnewaddress('sapling')
        zaddrExt = self.nodes[3].z_getnewaddress('sapling')

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
        assert_equal(len(pt['outputs']), 2)

        # Expect one internal output and one external.
        assert_equal(len([output for output in pt['outputs'] if output['outgoing']]), 1)

        # Output orders can be randomized, so we check the output
        # positions and contents separately
        outputs = []

        assert_equal(pt['outputs'][0]['type'], 'sapling')
        if pt['outputs'][0]['address'] == zaddr1:
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

        assert_equal(pt['outputs'][1]['type'], 'sapling')
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

        assert({
            'address': zaddrExt,
            'value': Decimal('2'),
            'valueZat': 200000000,
            'memo': no_memo,
        } in outputs)

        r = self.nodes[1].z_listreceivedbyaddress(zaddr1)
        assert_equal(0, len(r), "Should have received no confirmed note")
        c = self.nodes[1].z_getnotescount()
        assert_equal(0, c['sapling'], "Count of confirmed notes should be 0")

        # No confirmation required, one note should be present
        r = self.nodes[1].z_listreceivedbyaddress(zaddr1, 0)
        assert_equal(1, len(r), "Should have received one (unconfirmed) note")
        assert_equal(txid, r[0]['txid'])
        assert_equal(1, r[0]['amount'])
        assert_equal(100000000, r[0]['amountZat'])
        assert_false(r[0]['change'], "Note should not be change")
        assert_equal(my_memo, r[0]['memo'])
        assert_equal(0, r[0]['confirmations'])
        assert_equal(-1, r[0]['blockindex'])
        assert_equal(0, r[0]['blockheight'])

        c = self.nodes[1].z_getnotescount(0)
        assert_equal(1, c['sapling'], "Count of unconfirmed notes should be 1")

        # Confirm transaction (1 ZEC from taddr to zaddr1)
        self.generate_and_sync(height+3)

        # adjust confirmations
        r[0]['confirmations'] = 1
        # adjust blockindex
        r[0]['blockindex'] = 1
        # adjust height
        r[0]['blockheight'] = height + 3

        # Require one confirmation, note should be present
        assert_equal(r, self.nodes[1].z_listreceivedbyaddress(zaddr1))

        # Generate some change by sending part of zaddr1 to zaddr2
        txidPrev = txid
        zaddr2 = self.nodes[1].z_getnewaddress('sapling')
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

        assert_equal(pt['spends'][0]['type'], 'sapling')
        assert_equal(pt['spends'][0]['txidPrev'], txidPrev)
        assert_equal(pt['spends'][0]['spend'], 0)
        assert_equal(pt['spends'][0]['outputPrev'], 0)
        assert_equal(pt['spends'][0]['address'], zaddr1)
        assert_equal(pt['spends'][0]['value'], Decimal('1.0'))
        assert_equal(pt['spends'][0]['valueZat'], 100000000)

        # Output orders can be randomized, so we check the output
        # positions and contents separately
        outputs = []

        assert_equal(pt['outputs'][0]['type'], 'sapling')
        assert_equal(pt['outputs'][0]['output'], 0)
        assert_equal(pt['outputs'][0]['outgoing'], False)
        outputs.append({
            'address': pt['outputs'][0]['address'],
            'value': pt['outputs'][0]['value'],
            'valueZat': pt['outputs'][0]['valueZat'],
            'memo': pt['outputs'][0]['memo'],
        })

        assert_equal(pt['outputs'][1]['type'], 'sapling')
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
            'value': Decimal('0.4') - DEFAULT_FEE,
            'valueZat': 40000000 - DEFAULT_FEE_ZATS,
            'memo': no_memo,
        } in outputs)

        # zaddr1 should have a note with change
        r = self.nodes[1].z_listreceivedbyaddress(zaddr1, 0)
        r = sorted(r, key = lambda received: received['amount'])
        assert_equal(2, len(r), "zaddr1 Should have received 2 notes")

        assert_equal(txid, r[0]['txid'])
        assert_equal(Decimal('0.4')-DEFAULT_FEE, r[0]['amount'])
        assert_equal(40000000-DEFAULT_FEE_ZATS, r[0]['amountZat'])
        assert_true(r[0]['change'], "Note valued at (0.4-"+str(DEFAULT_FEE)+") should be change")
        assert_equal(no_memo, r[0]['memo'])

        # The old note still exists (it's immutable), even though it is spent
        assert_equal(Decimal('1.0'), r[1]['amount'])
        assert_equal(100000000, r[1]['amountZat'])
        assert_false(r[1]['change'], "Note valued at 1.0 should not be change")
        assert_equal(my_memo, r[1]['memo'])

        # zaddr2 should not have change
        r = self.nodes[1].z_listreceivedbyaddress(zaddr2, 0)
        r = sorted(r, key = lambda received: received['amount'])
        assert_equal(1, len(r), "zaddr2 Should have received 1 notes")
        assert_equal(txid, r[0]['txid'])
        assert_equal(Decimal('0.6'), r[0]['amount'])
        assert_equal(60000000, r[0]['amountZat'])
        assert_false(r[0]['change'], "Note valued at 0.6 should not be change")
        assert_equal(no_memo, r[0]['memo'])

        c = self.nodes[1].z_getnotescount(0)
        assert_equal(3, c['sapling'], "Count of unconfirmed notes should be 3(2 in zaddr1 + 1 in zaddr2)")

    def run_test(self):
        self.test_received_sprout(200)
        self.test_received_sapling(214)

if __name__ == '__main__':
    ListReceivedTest().main()
