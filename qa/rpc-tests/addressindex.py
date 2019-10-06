#!/usr/bin/env python
# Copyright (c) 2019 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
#
# Test addressindex generation and fetching for insightexplorer
# 
# RPCs tested here:
#
#   getaddresstxids
#   getaddressbalance
#   getaddressdeltas
#   getaddressutxos
#   getaddressmempool

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework

from test_framework.util import (
    assert_equal,
    initialize_chain_clean,
    start_nodes,
    stop_nodes,
    connect_nodes,
    wait_bitcoinds,
)

from test_framework.script import (
    CScript,
    OP_HASH160,
    OP_EQUAL,
    OP_DUP,
    OP_DROP,
)

from test_framework.mininode import (
    COIN,
    CTransaction,
    CTxIn, CTxOut, COutPoint,
)

from binascii import hexlify


class AddressIndexTest(BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self):
        # -insightexplorer causes addressindex to be enabled (fAddressIndex = true)
        args = ('-debug', '-txindex', '-experimentalfeatures', '-insightexplorer')
        self.nodes = start_nodes(3, self.options.tmpdir, [args] * 3)
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[0], 2)

        self.is_network_split = False
        self.sync_all()

    def run_test(self):

        # helper functions
        def getaddresstxids(node_index, addresses, start, end):
            return self.nodes[node_index].getaddresstxids({
                'addresses': addresses,
                'start': start,
                'end': end
            })

        def getaddressdeltas(node_index, addresses, start, end, chainInfo=None):
            params = {
                'addresses': addresses,
                'start': start,
                'end': end,
            }
            if chainInfo is not None:
                params.update({'chainInfo': chainInfo})
            return self.nodes[node_index].getaddressdeltas(params)

        # default received value is the balance value
        def check_balance(node_index, address, expected_balance, expected_received=None):
            if isinstance(address, list):
                bal = self.nodes[node_index].getaddressbalance({'addresses': address})
            else:
                bal = self.nodes[node_index].getaddressbalance(address)
            assert_equal(bal['balance'], expected_balance)
            if expected_received is None:
                expected_received = expected_balance
            assert_equal(bal['received'], expected_received)

        # begin test

        self.nodes[0].generate(105)
        self.sync_all()
        assert_equal(self.nodes[0].getbalance(), 5 * 10)
        assert_equal(self.nodes[1].getblockcount(), 105)
        assert_equal(self.nodes[1].getbalance(), 0)

        # only the oldest 5; subsequent are not yet mature
        unspent_txids = [u['txid'] for u in self.nodes[0].listunspent()]

        # Currently our only unspents are coinbase transactions, choose any one
        tx = self.nodes[0].getrawtransaction(unspent_txids[0], 1)

        # It just so happens that the first output is the mining reward,
        # which has type pay-to-public-key-hash, and the second output
        # is the founders' reward, which has type pay-to-script-hash.
        addr_p2pkh = tx['vout'][0]['scriptPubKey']['addresses'][0]
        addr_p2sh = tx['vout'][1]['scriptPubKey']['addresses'][0]

        # Check that balances from mining are correct (105 blocks mined); in
        # regtest, all mining rewards from a single call to generate() are sent
        # to the same pair of addresses.
        check_balance(1, addr_p2pkh, 105 * 10 * COIN)
        check_balance(1, addr_p2sh, 105 * 2.5 * COIN)

        # Multiple address arguments, results are the sum
        check_balance(1, [addr_p2sh, addr_p2pkh], 105 * 12.5 * COIN)

        assert_equal(len(self.nodes[1].getaddresstxids(addr_p2pkh)), 105)
        assert_equal(len(self.nodes[1].getaddresstxids(addr_p2sh)), 105)

        # only the oldest 5 transactions are in the unspent list,
        # dup addresses are ignored
        height_txids = getaddresstxids(1, [addr_p2pkh, addr_p2pkh], 1, 5)
        assert_equal(sorted(height_txids), sorted(unspent_txids))

        height_txids = getaddresstxids(1, [addr_p2sh], 1, 5)
        assert_equal(sorted(height_txids), sorted(unspent_txids))

        # each txid should appear only once
        height_txids = getaddresstxids(1, [addr_p2pkh, addr_p2sh], 1, 5)
        assert_equal(sorted(height_txids), sorted(unspent_txids))

        # do some transfers, make sure balances are good
        txids_a1 = []
        addr1 = self.nodes[1].getnewaddress()
        expected = 0
        expected_deltas = []  # for checking getaddressdeltas (below)
        for i in range(5):
            # first transaction happens at height 105, mined in block 106
            txid = self.nodes[0].sendtoaddress(addr1, i + 1)
            txids_a1.append(txid)
            self.nodes[0].generate(1)
            self.sync_all()
            expected += i + 1
            expected_deltas.append({
                'height': 106 + i,
                'satoshis': (i + 1) * COIN,
                'txid': txid,
            })
        check_balance(1, addr1, expected * COIN)
        assert_equal(sorted(self.nodes[0].getaddresstxids(addr1)), sorted(txids_a1))
        assert_equal(sorted(self.nodes[1].getaddresstxids(addr1)), sorted(txids_a1))

        # Restart all nodes to ensure indices are saved to disk and recovered
        stop_nodes(self.nodes)
        wait_bitcoinds()
        self.setup_network()

        bal = self.nodes[1].getaddressbalance(addr1)
        assert_equal(bal['balance'], expected * COIN)
        assert_equal(bal['received'], expected * COIN)
        assert_equal(sorted(self.nodes[0].getaddresstxids(addr1)), sorted(txids_a1))
        assert_equal(sorted(self.nodes[1].getaddresstxids(addr1)), sorted(txids_a1))

        # Send 3 from addr1, but -- subtlety alert! -- addr1 at this
        # time has 4 UTXOs, with values 1, 2, 3, 4. Sending value 3 requires
        # using up the value 4 UTXO, because of the tx fee
        # (the 3 UTXO isn't quite large enough).
        #
        # The txid from sending *from* addr1 is also added to the list of
        # txids associated with that address (test will verify below).

        addr2 = self.nodes[2].getnewaddress()
        txid = self.nodes[1].sendtoaddress(addr2, 3)
        self.sync_all()

        # the one tx in the mempool refers to addresses addr1 and addr2,
        # check that duplicate addresses are processed correctly
        mempool = self.nodes[0].getaddressmempool({'addresses': [addr2, addr1, addr2]})
        assert_equal(len(mempool), 3)

        # addr2 (first arg)
        assert_equal(mempool[0]['address'], addr2)
        assert_equal(mempool[0]['satoshis'], 3 * COIN)
        assert_equal(mempool[0]['txid'], txid)

        # addr1 (second arg)
        assert_equal(mempool[1]['address'], addr1)
        assert_equal(mempool[1]['satoshis'], (-4) * COIN)
        assert_equal(mempool[1]['txid'], txid)

        # addr2 (third arg)
        assert_equal(mempool[2]['address'], addr2)
        assert_equal(mempool[2]['satoshis'], 3 * COIN)
        assert_equal(mempool[2]['txid'], txid)

        # a single address can be specified as a string (not json object)
        assert_equal([mempool[1]], self.nodes[0].getaddressmempool(addr1))

        tx = self.nodes[0].getrawtransaction(txid, 1)
        assert_equal(tx['vin'][0]['address'], addr1)
        assert_equal(tx['vin'][0]['value'], 4)
        assert_equal(tx['vin'][0]['valueSat'], 4 * COIN)

        txids_a1.append(txid)
        expected_deltas.append({
            'height': 111,
            'satoshis': (-4) * COIN,
            'txid': txid,
        })
        self.sync_all()  # ensure transaction is included in the next block
        self.nodes[0].generate(1)
        self.sync_all()

        # the send to addr2 tx is now in a mined block, no longer in the mempool
        mempool = self.nodes[0].getaddressmempool({'addresses': [addr2, addr1]})
        assert_equal(len(mempool), 0)

        # Test DisconnectBlock() by invalidating the most recent mined block
        tip = self.nodes[1].getchaintips()[0]
        for i in range(3):
            node = self.nodes[i]
            # the value 4 UTXO is no longer in our balance
            check_balance(i, addr1, (expected - 4) * COIN, expected * COIN)
            check_balance(i, addr2, 3 * COIN)

            assert_equal(node.getblockcount(), 111)
            node.invalidateblock(tip['hash'])
            assert_equal(node.getblockcount(), 110)

            mempool = node.getaddressmempool({'addresses': [addr2, addr1]})
            assert_equal(len(mempool), 2)

            check_balance(i, addr1, expected * COIN)
            check_balance(i, addr2, 0)

        # now re-mine the addr1 to addr2 send
        self.nodes[0].generate(1)
        self.sync_all()
        for node in self.nodes:
            assert_equal(node.getblockcount(), 111)

        mempool = self.nodes[0].getaddressmempool({'addresses': [addr2, addr1]})
        assert_equal(len(mempool), 0)

        # the value 4 UTXO is no longer in our balance
        check_balance(2, addr1, (expected - 4) * COIN, expected * COIN)

        # Ensure the change from that transaction appears
        tx = self.nodes[0].getrawtransaction(txid, 1)
        change_vout = filter(lambda v: v['valueZat'] != 3 * COIN, tx['vout'])
        change = change_vout[0]['scriptPubKey']['addresses'][0]
        bal = self.nodes[2].getaddressbalance(change)
        assert(bal['received'] > 0)
        # the inequality is due to randomness in the tx fee
        assert(bal['received'] < (4 - 3) * COIN)
        assert_equal(bal['received'], bal['balance'])
        assert_equal(self.nodes[2].getaddresstxids(change), [txid])

        # Further checks that limiting by height works

        # various ranges
        for i in range(5):
            height_txids = getaddresstxids(1, [addr1], 106, 106 + i)
            assert_equal(height_txids, txids_a1[0:i+1])

        height_txids = getaddresstxids(1, [addr1], 1, 108)
        assert_equal(height_txids, txids_a1[0:3])

        # Further check specifying multiple addresses
        txids_all = list(txids_a1)
        txids_all += self.nodes[1].getaddresstxids(addr_p2pkh)
        txids_all += self.nodes[1].getaddresstxids(addr_p2sh)
        multitxids = self.nodes[1].getaddresstxids({
            'addresses': [addr1, addr_p2sh, addr_p2pkh]
        })
        # No dups in return list from getaddresstxids
        assert_equal(len(multitxids), len(set(multitxids)))

        # set(txids_all) removes its (expected) duplicates
        assert_equal(set(multitxids), set(txids_all))

        deltas = self.nodes[1].getaddressdeltas({'addresses': [addr1]})
        assert_equal(len(deltas), len(expected_deltas))
        for i in range(len(deltas)):
            assert_equal(deltas[i]['address'],  addr1)
            assert_equal(deltas[i]['height'],   expected_deltas[i]['height'])
            assert_equal(deltas[i]['satoshis'], expected_deltas[i]['satoshis'])
            assert_equal(deltas[i]['txid'],     expected_deltas[i]['txid'])

        # 106-111 is the full range (also the default)
        deltas_limited = getaddressdeltas(1, [addr1], 106, 111)
        assert_equal(deltas_limited, deltas)

        # only the first element missing
        deltas_limited = getaddressdeltas(1, [addr1], 107, 111)
        assert_equal(deltas_limited, deltas[1:])

        deltas_limited = getaddressdeltas(1, [addr1], 109, 109)
        assert_equal(deltas_limited, deltas[3:4])

        # the full range (also the default)
        deltas_info = getaddressdeltas(1, [addr1], 106, 111, chainInfo=True)
        assert_equal(deltas_info['deltas'], deltas)

        # check the additional items returned by chainInfo
        assert_equal(deltas_info['start']['height'], 106)
        block_hash = self.nodes[1].getblockhash(106)
        assert_equal(deltas_info['start']['hash'], block_hash)

        assert_equal(deltas_info['end']['height'], 111)
        block_hash = self.nodes[1].getblockhash(111)
        assert_equal(deltas_info['end']['hash'], block_hash)

        # Test getaddressutxos by comparing results with deltas
        utxos = self.nodes[1].getaddressutxos(addr1)

        # The value 4 note was spent, so won't show up in the utxo list,
        # so for comparison, remove the 4 (and -4 for output) from the
        # deltas list
        deltas = self.nodes[1].getaddressdeltas({'addresses': [addr1]})
        deltas = filter(lambda d: abs(d['satoshis']) != 4 * COIN, deltas)
        assert_equal(len(utxos), len(deltas))
        for i in range(len(utxos)):
            assert_equal(utxos[i]['address'],   addr1)
            assert_equal(utxos[i]['height'],    deltas[i]['height'])
            assert_equal(utxos[i]['satoshis'],  deltas[i]['satoshis'])
            assert_equal(utxos[i]['txid'],      deltas[i]['txid'])

        # Check that outputs with the same address in the same tx return one txid
        # (can't use createrawtransaction() as it combines duplicate addresses)
        addr = "t2LMJ6Arw9UWBMWvfUr2QLHM4Xd9w53FftS"
        addressHash = "97643ce74b188f4fb6bbbb285e067a969041caf2".decode('hex')
        scriptPubKey = CScript([OP_HASH160, addressHash, OP_EQUAL])
        # Add an unrecognized script type to vout[], a legal script that pays,
        # but won't modify the addressindex (since the address can't be extracted).
        # (This extra output has no effect on the rest of the test.)
        scriptUnknown = CScript([OP_HASH160, OP_DUP, OP_DROP, addressHash, OP_EQUAL])
        unspent = filter(lambda u: u['amount'] >= 4, self.nodes[0].listunspent())
        tx = CTransaction()
        tx.vin = [CTxIn(COutPoint(int(unspent[0]['txid'], 16), unspent[0]['vout']))]
        tx.vout = [
            CTxOut(1 * COIN, scriptPubKey),
            CTxOut(2 * COIN, scriptPubKey),
            CTxOut(7 * COIN, scriptUnknown),
        ]
        tx = self.nodes[0].signrawtransaction(hexlify(tx.serialize()).decode('utf-8'))
        txid = self.nodes[0].sendrawtransaction(tx['hex'], True)
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(self.nodes[1].getaddresstxids(addr), [txid])
        check_balance(2, addr, 3 * COIN)


if __name__ == '__main__':
    AddressIndexTest().main()
