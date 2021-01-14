#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, start_nodes, start_node, \
    connect_nodes_bi, sync_blocks, sync_mempools

from decimal import Decimal

class WalletTest (BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 4

    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        print("Mining blocks...")

        self.nodes[0].generate(4)
        self.sync_all()

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 40)
        assert_equal(walletinfo['balance'], 0)

        blockchaininfo = self.nodes[0].getblockchaininfo()
        assert_equal(blockchaininfo['estimatedheight'], 4)

        self.sync_all()
        self.nodes[1].generate(101)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 40)
        assert_equal(self.nodes[1].getbalance(), 10)
        assert_equal(self.nodes[2].getbalance(), 0)
        assert_equal(self.nodes[0].getbalance("*"), 40)
        assert_equal(self.nodes[1].getbalance("*"), 10)
        assert_equal(self.nodes[2].getbalance("*"), 0)

        # Send 21 ZEC from 0 to 2 using sendtoaddress call.
        # Second transaction will be child of first, and will require a fee
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 11)
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 10)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 0)

        blockchaininfo = self.nodes[0].getblockchaininfo()
        assert_equal(blockchaininfo['estimatedheight'], 105)

        # Have node0 mine a block, thus it will collect its own fee.
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Have node1 generate 100 blocks (so node0 can recover the fee)
        self.nodes[1].generate(100)
        self.sync_all()

        # node0 should end up with 50 ZEC in block rewards plus fees, but
        # minus the 21 ZEC plus fees sent to node2
        assert_equal(self.nodes[0].getbalance(), 50-21)
        assert_equal(self.nodes[2].getbalance(), 21)
        assert_equal(self.nodes[0].getbalance("*"), 50-21)
        assert_equal(self.nodes[2].getbalance("*"), 21)

        # Node0 should have three unspent outputs.
        # Create a couple of transactions to send them to node2, submit them through
        # node1, and make sure both node0 and node2 pick them up properly:
        node0utxos = self.nodes[0].listunspent(1)
        assert_equal(len(node0utxos), 3)

        # Check 'generated' field of listunspent
        # Node 0: has one coinbase utxo and two regular utxos
        assert_equal(sum(int(uxto["generated"] is True) for uxto in node0utxos), 1)
        # Node 1: has 101 coinbase utxos and no regular utxos
        node1utxos = self.nodes[1].listunspent(1)
        assert_equal(len(node1utxos), 101)
        assert_equal(sum(int(uxto["generated"] is True) for uxto in node1utxos), 101)
        # Node 2: has no coinbase utxos and two regular utxos
        node2utxos = self.nodes[2].listunspent(1)
        assert_equal(len(node2utxos), 2)
        assert_equal(sum(int(uxto["generated"] is True) for uxto in node2utxos), 0)

        # Catch an attempt to send a transaction with an absurdly high fee.
        # Send 1.0 ZEC from an utxo of value 10.0 ZEC but don't specify a change output, so then
        # the change of 9.0 ZEC becomes the fee, which is greater than estimated fee of 0.0021 ZEC.
        inputs = []
        outputs = {}
        for utxo in node2utxos:
            if utxo["amount"] == Decimal("10.0"):
                break
        assert_equal(utxo["amount"], Decimal("10.0"))
        inputs.append({ "txid" : utxo["txid"], "vout" : utxo["vout"]})
        outputs[self.nodes[2].getnewaddress("")] = Decimal("1.0")
        raw_tx = self.nodes[2].createrawtransaction(inputs, outputs)
        signed_tx = self.nodes[2].signrawtransaction(raw_tx)
        try:
            self.nodes[2].sendrawtransaction(signed_tx["hex"])
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("absurdly high fees" in errorString)
        assert("900000000 > 10000000" in errorString)

        # create both transactions
        txns_to_send = []
        for utxo in node0utxos:
            inputs = []
            outputs = {}
            inputs.append({ "txid" : utxo["txid"], "vout" : utxo["vout"]})
            outputs[self.nodes[2].getnewaddress("")] = utxo["amount"]
            raw_tx = self.nodes[0].createrawtransaction(inputs, outputs)
            txns_to_send.append(self.nodes[0].signrawtransaction(raw_tx))

        # Have node 1 (miner) send the transactions
        self.nodes[1].sendrawtransaction(txns_to_send[0]["hex"], True)
        self.nodes[1].sendrawtransaction(txns_to_send[1]["hex"], True)
        self.nodes[1].sendrawtransaction(txns_to_send[2]["hex"], True)

        # Have node1 mine a block to confirm transactions:
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 0)
        assert_equal(self.nodes[2].getbalance(), 50)
        assert_equal(self.nodes[0].getbalance("*"), 0)
        assert_equal(self.nodes[2].getbalance("*"), 50)

        # Send 10 ZEC normally
        address = self.nodes[0].getnewaddress("")
        self.nodes[2].settxfee(Decimal('0.001'))  # not the default
        self.nodes[2].sendtoaddress(address, 10, "", "", False)
        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()
        assert_equal(self.nodes[2].getbalance(), Decimal('39.99900000'))
        assert_equal(self.nodes[0].getbalance(), Decimal('10.00000000'))
        assert_equal(self.nodes[2].getbalance("*"), Decimal('39.99900000'))
        assert_equal(self.nodes[0].getbalance("*"), Decimal('10.00000000'))

        # Send 10 ZEC with subtract fee from amount
        self.nodes[2].sendtoaddress(address, 10, "", "", True)
        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()
        assert_equal(self.nodes[2].getbalance(), Decimal('29.99900000'))
        assert_equal(self.nodes[0].getbalance(), Decimal('19.99900000'))
        assert_equal(self.nodes[2].getbalance("*"), Decimal('29.99900000'))
        assert_equal(self.nodes[0].getbalance("*"), Decimal('19.99900000'))

        # Sendmany 10 ZEC
        self.nodes[2].sendmany("", {address: 10}, 0, "", [])
        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()
        assert_equal(self.nodes[2].getbalance(), Decimal('19.99800000'))
        assert_equal(self.nodes[0].getbalance(), Decimal('29.99900000'))
        assert_equal(self.nodes[2].getbalance("*"), Decimal('19.99800000'))
        assert_equal(self.nodes[0].getbalance("*"), Decimal('29.99900000'))

        # Sendmany 10 ZEC with subtract fee from amount
        self.nodes[2].sendmany("", {address: 10}, 0, "", [address])
        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()
        assert_equal(self.nodes[2].getbalance(), Decimal('9.99800000'))
        assert_equal(self.nodes[0].getbalance(), Decimal('39.99800000'))
        assert_equal(self.nodes[2].getbalance("*"), Decimal('9.99800000'))
        assert_equal(self.nodes[0].getbalance("*"), Decimal('39.99800000'))

        # Test ResendWalletTransactions:
        # Create a couple of transactions, then start up a fourth
        # node (nodes[3]) and ask nodes[0] to rebroadcast.
        # EXPECT: nodes[3] should have those transactions in its mempool.
        txid1 = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 1)
        txid2 = self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 1)
        sync_mempools(self.nodes)

        self.nodes.append(start_node(3, self.options.tmpdir))
        connect_nodes_bi(self.nodes, 0, 3)
        sync_blocks(self.nodes)

        relayed = self.nodes[0].resendwallettransactions()
        assert_equal(set(relayed), set([txid1, txid2]))
        sync_mempools(self.nodes)

        assert(txid1 in self.nodes[3].getrawmempool())

        # check integer balances from getbalance
        assert_equal(self.nodes[2].getbalance("*", 1, False, True), 999800000)

        # send from node 0 to node 2 taddr
        mytaddr = self.nodes[2].getnewaddress()
        mytxid = self.nodes[0].sendtoaddress(mytaddr, 10.0)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        mybalance = self.nodes[2].z_getbalance(mytaddr)
        assert_equal(mybalance, Decimal('10.0'))

        # check integer balances from z_getbalance
        assert_equal(self.nodes[2].z_getbalance(mytaddr, 1, True), 1000000000)

        mytxdetails = self.nodes[2].gettransaction(mytxid)
        myvjoinsplits = mytxdetails["vjoinsplit"]
        assert_equal(0, len(myvjoinsplits))
        assert("joinSplitPubKey" not in mytxdetails)
        assert("joinSplitSig" not in mytxdetails)

if __name__ == '__main__':
    WalletTest ().main ()
