#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, start_nodes, wait_and_assert_operationid_status

from decimal import Decimal

# Test wallet z_listunspent behaviour across network upgrades

# Sequence of blockchain states:
#   200 (initial, sprout)
#   201 (overwinter): coinbase 10 ->      sproutzaddr 9.9999
#   212 (sapling):    coinbase 8 ->       saplingzaddr1 7.9999
#   213 (sapling):    saplingzaddr1 2 ->  saplingzaddr2 1.9999

MY_MEMO = 'c0ffee' + '0'*(1024-len('c0ffee'))
NO_MEMO = 'f6' + ('0'*1022) # see section 5.5 of the protocol spec

class WalletListNotes(BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, [[
            '-nuparams=5ba81b19:201', # Overwinter (sapling addrs)
            '-nuparams=76b809bb:212', # Sapling
        ]] * 4)

    def run_test(self):
        # Current height = 200 -> Sprout
        assert_equal(self.nodes[0].getblockcount(), 200)
        mining_addr = self.nodes[0].listunspent()[0]['address']
        sproutzaddr = self.nodes[0].z_getnewaddress('sprout')
        assert_equal(self.nodes[0].z_validateaddress(sproutzaddr)['type'], 'sprout')

        # test that we can create a sapling zaddr before sapling activates
        saplingzaddr1 = self.nodes[0].z_getnewaddress('sapling')
        assert_equal(self.nodes[0].z_validateaddress(saplingzaddr1)['type'], 'sapling')

        # we've got lots of coinbase (taddr) but no shielded funds yet
        assert_equal(Decimal(self.nodes[0].z_gettotalbalance()['private']), 0)

        # Set current height to 201 -> Sprout
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 201)

        # Shield coinbase funds (must be a multiple of 10, no change allowed pre-sapling)
        myopid = self.nodes[0].z_sendmany(mining_addr,
            [{'address':sproutzaddr, 'amount':Decimal('10'), 'memo': MY_MEMO}], 1, 0)
        txid_1 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()

        # No funds (with one or more confirmations) in any zaddr yet
        assert_equal(len(self.nodes[0].z_listunspent()), 0)
        assert_equal(len(self.nodes[0].z_listunspent(1)), 0)

        # list private unspent, this time allowing 0 confirmations
        unspent_cb = self.nodes[0].z_listunspent(0)
        assert_equal(len(unspent_cb), 1)
        assert_equal(unspent_cb[0]['memo'], MY_MEMO)
        assert_equal(unspent_cb[0]['amount'], Decimal('10'))
        assert_equal(unspent_cb[0]['confirmations'], 0)
        assert_equal(unspent_cb[0]['address'], sproutzaddr)
        assert_equal(unspent_cb[0]['spendable'], True)
        assert_equal(unspent_cb[0]['txid'], txid_1)
        assert_equal(unspent_cb[0]['change'], False)
        # check for presence, but the index value is unpredictable:
        assert('jsoutindex' in unspent_cb[0])   # sprout (and overwinter) only
        assert('outindex' not in unspent_cb[0]) # sapling only

        # filtering by address should produce same result
        assert_equal(unspent_cb,
            self.nodes[0].z_listunspent(0, 9999, False, [sproutzaddr]))

        # Set current height to 212 -> Sapling. Default address type remains Sprout
        self.nodes[0].generate(10)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 211)
        assert_equal(Decimal(self.nodes[0].z_gettotalbalance()['private']), 10)

        received = self.nodes[0].z_listreceivedbyaddress(sproutzaddr, 0)
        assert_equal(len(received), 1)
        assert_equal(received[0]['memo'], MY_MEMO)
        assert_equal(received[0]['amount'], Decimal('10'))
        assert_equal(received[0]['txid'], txid_1)
        assert_equal(received[0]['change'], False)
        assert('jsoutindex' in received[0])   # sprout (and overwinter) only
        assert('outindex' not in received[0]) # sapling only

        # beginning with Sapling, change is allowed from a coinbase transfer
        myopid = self.nodes[0].z_sendmany(mining_addr,
            [{'address':saplingzaddr1, 'amount':Decimal('8')}], 1, 0)
        txid_2 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        unspent_tx = self.nodes[0].z_listunspent(0)
        # sort low-to-high by amount (order of returned entries is unpredictable)
        unspent_tx = sorted(unspent_tx, key=lambda k: k['amount'])
        assert_equal(len(unspent_tx), 2)

        assert_equal(unspent_tx[0]['memo'], NO_MEMO)
        assert_equal(unspent_tx[0]['amount'], Decimal('8'))
        assert_equal(unspent_tx[0]['confirmations'], 0)
        assert_equal(unspent_tx[0]['address'], saplingzaddr1)
        assert_equal(unspent_tx[0]['spendable'], True)
        assert_equal(unspent_tx[0]['txid'], txid_2)
        assert_equal(unspent_tx[0]['change'], False)
        assert('jsoutindex' not in unspent_tx[0])   # sprout (and overwinter) only
        assert('outindex' in unspent_tx[0])         # sapling only

        # the earlier one will also be returned
        assert_equal(unspent_tx[1]['memo'], MY_MEMO)
        assert_equal(unspent_tx[1]['amount'], Decimal('10'))
        assert_equal(unspent_tx[1]['confirmations'], 10)
        assert_equal(unspent_tx[1]['address'], sproutzaddr)
        assert_equal(unspent_tx[1]['spendable'], True)
        assert_equal(unspent_tx[1]['txid'], txid_1)
        assert_equal(unspent_tx[1]['change'], False)
        assert('jsoutindex' in unspent_tx[1])   # sprout (and overwinter) only
        assert('outindex' not in unspent_tx[1]) # sapling only

        # filtering
        assert_equal(unspent_tx[0],
            self.nodes[0].z_listunspent(0, 9999, False, [saplingzaddr1])[0])
        assert_equal(unspent_tx[1],
            self.nodes[0].z_listunspent(0, 9999, False, [sproutzaddr])[0])
        assert_equal(unspent_tx,
            sorted(self.nodes[0].z_listunspent(0, 9999, False,
                    [saplingzaddr1, sproutzaddr]),
                key=lambda k: k['amount']))

        # minconf=1: require a confirmation
        assert_equal(len(self.nodes[0].z_listunspent()), 1)
        assert_equal(unspent_tx[1], self.nodes[0].z_listunspent()[0])

        # maxconf=0: only return the most recent transaction (0 = unconfirmed)
        assert_equal(len(self.nodes[0].z_listunspent(0, 0)), 1)
        assert_equal(unspent_tx[0], self.nodes[0].z_listunspent(0, 0)[0])

        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 212)
        assert_equal(Decimal(self.nodes[0].z_gettotalbalance()['private']), 18)

        # we now have one confirmation
        assert_equal(self.nodes[0].z_listreceivedbyaddress(sproutzaddr), received)
        assert_equal(self.nodes[0].z_listreceivedbyaddress(sproutzaddr, 0), received)

        received = self.nodes[0].z_listreceivedbyaddress(saplingzaddr1)
        assert_equal(len(received), 1)
        assert_equal(received[0]['memo'], NO_MEMO)
        assert_equal(received[0]['amount'], Decimal('8'))
        assert_equal(received[0]['txid'], txid_2)
        assert_equal(received[0]['change'], False)
        assert('jsoutindex' not in received[0])   # sprout (and overwinter) only
        assert('outindex' in received[0])         # sapling only

        saplingzaddr2 = self.nodes[0].z_getnewaddress('sapling')
        myopid = self.nodes[0].z_sendmany(saplingzaddr1,
            [{'address':saplingzaddr2, 'amount':Decimal('2'), 'memo':MY_MEMO}], 1, 0)
        txid_3 = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        unspent_tx = self.nodes[0].z_listunspent(0)
        unspent_tx = sorted(unspent_tx, key=lambda k: k['amount'])

        assert_equal(len(unspent_tx), 3)
        assert_equal(unspent_tx[0]['memo'], MY_MEMO)
        assert_equal(unspent_tx[0]['amount'], Decimal('2'))
        assert_equal(unspent_tx[0]['confirmations'], 0)
        assert_equal(unspent_tx[0]['address'], saplingzaddr2)
        assert_equal(unspent_tx[0]['spendable'], True)
        assert_equal(unspent_tx[0]['txid'], txid_3)
        assert_equal(unspent_tx[0]['change'], False)
        assert('jsoutindex' not in unspent_tx[0])   # sprout (and overwinter) only
        assert('outindex' in unspent_tx[0])         # sapling only

        assert_equal(unspent_tx[1]['memo'], NO_MEMO)
        assert_equal(unspent_tx[1]['amount'], Decimal('6'))
        assert_equal(unspent_tx[1]['confirmations'], 0)
        assert_equal(unspent_tx[1]['address'], saplingzaddr1)
        assert_equal(unspent_tx[1]['spendable'], True)
        assert_equal(unspent_tx[1]['txid'], txid_3)
        assert_equal(unspent_tx[1]['change'], True)
        assert('jsoutindex' not in unspent_tx[1])   # sprout (and overwinter) only
        assert('outindex' in unspent_tx[1])         # sapling only

        # the earlier one will also be returned
        assert_equal(unspent_tx[2]['memo'], MY_MEMO)
        assert_equal(unspent_tx[2]['amount'], Decimal('10'))
        assert_equal(unspent_tx[2]['confirmations'], 11)
        assert_equal(unspent_tx[2]['address'], sproutzaddr)
        assert_equal(unspent_tx[2]['spendable'], True)
        assert_equal(unspent_tx[2]['txid'], txid_1)
        assert_equal(unspent_tx[2]['change'], False)
        assert('jsoutindex' in unspent_tx[2])   # sprout (and overwinter) only
        assert('outindex' not in unspent_tx[2]) # sapling only

        # filtering on all three addresses, plus an unseen (unused) addr
        unseen = self.nodes[0].z_getnewaddress('sapling')
        assert_equal(unspent_tx,
            sorted(self.nodes[0].z_listunspent(0, 9999, False,
                    [saplingzaddr1, unseen, sproutzaddr, saplingzaddr2]),
                key=lambda k: k['amount']))

        # Including watchonly addresses gives the same result
        # (since we don't have any viewing keys)
        assert_equal(unspent_tx,
            sorted(self.nodes[0].z_listunspent(0, 9999, True),
                key=lambda k: k['amount']))

        # the latest transaction is not confirmed, so not included
        assert_equal(self.nodes[0].z_listreceivedbyaddress(saplingzaddr1), received)

        # check saplingzaddr1
        received = self.nodes[0].z_listreceivedbyaddress(saplingzaddr1, 0)
        received = sorted(received, key=lambda k: k['amount'])
        assert_equal(len(received), 2)

        assert_equal(received[0]['memo'], NO_MEMO)
        assert_equal(received[0]['amount'], Decimal('6'))
        assert_equal(received[0]['txid'], txid_3)
        assert_equal(received[0]['change'], True)
        assert('jsoutindex' not in received[0]) # sprout (and overwinter) only
        assert('outindex' in received[0])       # sapling only

        assert_equal(received[1]['memo'], NO_MEMO)
        assert_equal(received[1]['amount'], Decimal('8'))
        assert_equal(received[1]['txid'], txid_2)
        assert_equal(received[1]['change'], False)
        assert('jsoutindex' not in received[1]) # sprout (and overwinter) only
        assert('outindex' in received[1])       # sapling only

        # check saplingzaddr2
        received = self.nodes[0].z_listreceivedbyaddress(saplingzaddr2, 0)
        assert_equal(len(received), 1)
        assert_equal(received[0]['memo'], MY_MEMO)
        assert_equal(received[0]['amount'], Decimal('2'))
        assert_equal(received[0]['txid'], txid_3)
        assert_equal(received[0]['change'], False)
        assert('jsoutindex' not in received[0]) # sprout (and overwinter) only
        assert('outindex' in received[0])       # sapling only

        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 213)
        assert_equal(Decimal(self.nodes[0].z_gettotalbalance()['private']), 18)

        # copying a spending key to node 1 makes confirmed transactions visible there
        assert_equal(Decimal(self.nodes[1].z_gettotalbalance()['private']), 0)
        secretkey1 = self.nodes[0].z_exportkey(saplingzaddr1)
        self.nodes[1].z_importkey(secretkey1)
        assert_equal(Decimal(self.nodes[1].z_gettotalbalance()['private']), 6)
        unspent_tx_node1 = self.nodes[1].z_listunspent(0)

        assert_equal(len(unspent_tx_node1), 1)
        assert_equal(unspent_tx_node1[0]['memo'], NO_MEMO)
        assert_equal(unspent_tx_node1[0]['amount'], Decimal('6'))
        assert_equal(unspent_tx_node1[0]['confirmations'], 1)
        assert_equal(unspent_tx_node1[0]['address'], saplingzaddr1)
        assert_equal(unspent_tx_node1[0]['spendable'], True)
        assert_equal(unspent_tx_node1[0]['txid'], txid_3)
        assert_equal(unspent_tx_node1[0]['change'], True)
        assert('jsoutindex' not in unspent_tx_node1[0])   # sprout (and overwinter) only
        assert('outindex' in unspent_tx_node1[0])         # sapling only

        received = self.nodes[1].z_listreceivedbyaddress(saplingzaddr1)
        received = sorted(received, key=lambda k: k['amount'])
        assert_equal(len(received), 2)

        assert_equal(received[0]['memo'], NO_MEMO)
        assert_equal(received[0]['amount'], Decimal('6'))
        assert_equal(received[0]['txid'], txid_3)
        assert_equal(received[0]['change'], True)
        assert('jsoutindex' not in received[0]) # sprout (and overwinter) only
        assert('outindex' in received[0])       # sapling only

        assert_equal(received[1]['memo'], NO_MEMO)
        assert_equal(received[1]['amount'], Decimal('8'))
        assert_equal(received[1]['txid'], txid_2)
        assert_equal(received[1]['change'], False)
        assert('jsoutindex' not in received[1]) # sprout (and overwinter) only
        assert('outindex' in received[1])       # sapling only

        vk = self.nodes[0].z_exportviewingkey(sproutzaddr)
        self.nodes[1].z_importviewingkey(vk)

        # includeWatchonly = True (TODO: test Sapling when exporting and
        # importing viewing keys is supported (issue #3060))
        unspent_tx = self.nodes[1].z_listunspent(0, 9999, True, [sproutzaddr])
        assert_equal(len(unspent_tx), 1)
        assert_equal(unspent_tx[0]['memo'], MY_MEMO)
        assert_equal(unspent_tx[0]['amount'], Decimal('10'))
        assert_equal(unspent_tx[0]['confirmations'], 12)
        assert_equal(unspent_tx[0]['address'], sproutzaddr)
        assert_equal(unspent_tx[0]['spendable'], False)
        assert_equal(unspent_tx[0]['txid'], txid_1)
        assert('change' not in unspent_tx[0])   # since we have only viewing key
        assert('jsoutindex' in unspent_tx[0])   # sprout (and overwinter) only
        assert('outindex' not in unspent_tx[0]) # sapling only

        received = self.nodes[1].z_listreceivedbyaddress(sproutzaddr)
        assert_equal(len(received), 1)
        assert_equal(received[0]['memo'], MY_MEMO)
        assert_equal(received[0]['amount'], Decimal('10'))
        assert_equal(received[0]['txid'], txid_1)
        assert('change' not in unspent_tx[0]) # since we have only viewing key
        assert('jsoutindex' in received[0])   # sprout (and overwinter) only
        assert('outindex' not in received[0]) # sapling only

if __name__ == '__main__':
    WalletListNotes().main()
