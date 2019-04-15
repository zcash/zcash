#!/usr/bin/env python2
# Copyright (c) 2018 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


import time
from test_framework.test_framework import CryptoconditionsTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, initialize_chain, start_nodes, start_node, connect_nodes_bi, \
    stop_nodes, sync_blocks, sync_mempools, wait_bitcoinds, rpc_port, assert_raises
from cryptoconditions import assert_success, assert_error, generate_random_string


class CryptoconditionsChannelsTest(CryptoconditionsTestFramework):

    def run_channels_tests(self):


        """!!! for testing needed test daemon which built with custom flag
        export CONFIGURE_FLAGS='CPPFLAGS=-DTESTMODE'
        since in usual mode 101 confirmations are needed for payment/refund
        """

        rpc = self.nodes[0]
        rpc1 = self.nodes[1]

        # checking channelsaddress call

        result = rpc.channelsaddress(self.pubkey)
        assert_success(result)
        # test that additional CCaddress key is returned

        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        # getting empty channels list
        result = rpc.channelslist()
        assert_equal(len(result), 2)
        assert_equal(result["result"], "success")
        assert_equal(result["name"], "Channels List")

        # 10 payments, 100000 sat denomination channel opening with second node pubkey
        new_channel_hex = rpc.channelsopen(self.pubkey1, "10", "100000")
        assert_success(new_channel_hex)
        channel_txid = self.send_and_mine(new_channel_hex["hex"], rpc)
        assert channel_txid, "got channel txid"

        # checking if our new channel in common channels list
        result = rpc.channelslist()
        assert_equal(len(result), 3)

        # checking info about channel directly
        result = rpc.channelsinfo(channel_txid)
        assert_success(result)
        assert_equal(result["Transactions"][0]["Open"], channel_txid)

        # open transaction should be confirmed
        rpc.generate(1)

        # trying to make wrong denomination channel payment
        result = rpc.channelspayment(channel_txid, "199000")
        assert_error(result)

        # trying to make 0 channel payment
        result = rpc.channelspayment(channel_txid, "0")
        assert_error(result)

        # trying to make negative channel payment
        result = rpc.channelspayment(channel_txid, "-1")
        assert_error(result)

        # valid channel payment
        result = rpc.channelspayment(channel_txid, "100000")
        assert_success(result)
        payment_tx_id = self.send_and_mine(result["hex"], rpc)
        assert payment_tx_id, "got txid"

        # now in channelinfo payment information should appear
        result = rpc.channelsinfo(channel_txid)
        assert_equal(result["Transactions"][1]["Payment"], payment_tx_id)
        
        # number of payments should be equal 1 (one denomination used)
        result = rpc.channelsinfo(channel_txid)["Transactions"][1]["Number of payments"]
        assert_equal(result, 1)
        # payments left param should reduce 1 and be equal 9 now ( 10 - 1 = 9 )
        result = rpc.channelsinfo(channel_txid)["Transactions"][1]["Payments left"]
        assert_equal(result, 9)

        # lets try payment with x2 amount to ensure that counters works correct
        result = rpc.channelspayment(channel_txid, "200000")
        assert_success(result)
        payment_tx_id = self.send_and_mine(result["hex"], rpc)
        assert payment_tx_id, "got txid"

        result = rpc.channelsinfo(channel_txid)
        assert_equal(result["Transactions"][2]["Payment"], payment_tx_id)

        result = rpc.channelsinfo(channel_txid)["Transactions"][2]["Number of payments"]
        assert_equal(result, 2)

        result = rpc.channelsinfo(channel_txid)["Transactions"][2]["Payments left"]
        assert_equal(result, 7)

        # check if payment value really transferred
        raw_transaction = rpc.getrawtransaction(payment_tx_id, 1)

        result = raw_transaction["vout"][3]["valueSat"]
        assert_equal(result, 200000)

        result = rpc1.validateaddress(raw_transaction["vout"][3]["scriptPubKey"]["addresses"][0])["ismine"]
        assert_equal(result, True)

        # have to check that second node have coins to cover txfee at least
        rpc.sendtoaddress(rpc1.getnewaddress(), 1)
        rpc.sendtoaddress(rpc1.getnewaddress(), 1)
        rpc.generate(2)
        self.sync_all()
        result = rpc1.getbalance()
        assert_greater_than(result, 0.1)

        # trying to initiate channels payment from node B without any secret
        # TODO: have to add RPC validation
        payment_hex = rpc1.channelspayment(channel_txid, "100000")
        try:
            result = rpc1.sendrawtransaction(payment_hex["hex"])
        except Exception as e:
            pass

        # trying to initiate channels payment from node B with secret from previous payment
        result = rpc1.channelspayment(channel_txid, "100000", rpc1.channelsinfo(channel_txid)["Transactions"][1]["Secret"])
        #result = rpc1.sendrawtransaction(payment_hex["hex"])
        assert_error(result)

        # executing channel close
        result = rpc.channelsclose(channel_txid)
        assert_success(result)
        channel_close_txid = self.send_and_mine(result["hex"], rpc)
        assert channel_close_txid, "got txid"

        rpc.generate(2)
        self.sync_all()

        # now in channelinfo closed flag should appear
        result = rpc.channelsinfo(channel_txid)
        assert_equal(result["Transactions"][3]["Close"], channel_close_txid)

        # executing channel refund
        result = rpc.channelsrefund(channel_txid, channel_close_txid)
        assert_success(result)
        refund_txid = self.send_and_mine(result["hex"], rpc)
        assert refund_txid, "got txid"

        # checking if it refunded to opener address
        raw_transaction = rpc.getrawtransaction(refund_txid, 1)

        result = raw_transaction["vout"][2]["valueSat"]
        assert_equal(result, 700000)

        result = rpc.validateaddress(raw_transaction["vout"][2]["scriptPubKey"]["addresses"][0])["ismine"]
        assert_equal(result, True)


        # creating and draining channel (10 payment by 100000 satoshies in total to fit full capacity)
        new_channel_hex1 = rpc.channelsopen(self.pubkey1, "10", "100000")
        assert_success(new_channel_hex1)
        channel1_txid = self.send_and_mine(new_channel_hex1["hex"], rpc)
        assert channel1_txid, "got channel txid"

        # need to have 2+ confirmations in the test mode
        rpc.generate(2)
        self.sync_all()

        for i in range(10):
            result = rpc.channelspayment(channel1_txid, "100000")
            assert_success(result)
            payment_tx_id = self.send_and_mine(result["hex"], rpc)
            assert payment_tx_id, "got txid"

        # last payment should indicate that 0 payments left
        result = rpc.channelsinfo(channel1_txid)["Transactions"][10]["Payments left"]
        assert_equal(result, 0)

        # no more payments possible
        result = rpc.channelspayment(channel1_txid, "100000")
        assert_error(result)

        # creating new channel to test the case when node B initiate payment when node A revealed secret in offline
        # 10 payments, 100000 sat denomination channel opening with second node pubkey
        new_channel_hex2 = rpc.channelsopen(self.pubkey1, "10", "100000")
        assert_success(new_channel_hex)
        channel2_txid = self.send_and_mine(new_channel_hex2["hex"], rpc)
        assert channel2_txid, "got channel txid"

        rpc.generate(2)
        self.sync_all()

        # disconnecting first node from network
        rpc.setban("127.0.0.0/24","add")
        assert_equal(rpc.getinfo()["connections"], 0)
        assert_equal(rpc1.getinfo()["connections"], 0)

        rpc1.generate(1)

        # sending one payment to mempool to reveal the secret but not mine it
        payment_hex = rpc.channelspayment(channel2_txid, "100000")
        result = rpc.sendrawtransaction(payment_hex["hex"])
        assert result, "got payment txid"

        secret = rpc.channelsinfo(channel2_txid)["Transactions"][1]["Secret"]
        assert secret, "Secret revealed"

        # secret shouldn't be available for node B
        secret_not_revealed = None
        try:
            rpc1.channelsinfo(channel2_txid)["Transactions"][1]["Secret"]
        except Exception:
            secret_not_revealed = True
        assert_equal(secret_not_revealed, True)

        # trying to initiate payment from second node with revealed secret
        assert_equal(rpc1.getinfo()["connections"], 0)
        dc_payment_hex = rpc1.channelspayment(channel2_txid, "100000", secret)
        assert_success(dc_payment_hex)
        result = rpc1.sendrawtransaction(dc_payment_hex["hex"])
        assert result, "got channelspayment transaction id"

        # TODO: it crash first node after block generating on mempools merging
        # # restoring connection between nodes
        # rpc.setban("127.0.0.0/24","remove")
        # #rpc.generate(1)
        # #rpc1.generate(1)
        # sync_blocks(self.nodes)
        # rpc.generate(1)
        # sync_blocks(self.nodes)
        # sync_mempools(self.nodes)
        # assert_equal(rpc.getinfo()["connections"], 1)
        # assert_equal(rpc1.getinfo()["connections"], 1)

    def run_test(self):
        print("Mining blocks...")
        rpc = self.nodes[0]
        rpc1 = self.nodes[1]
        # utxos from block 1 become mature in block 101
        if not self.options.noshutdown:
            rpc.generate(101)
        self.sync_all()
        rpc.getinfo()
        rpc1.getinfo()
        # this corresponds to -pubkey above
        print("Importing privkeys")
        rpc.importprivkey(self.privkey)
        rpc1.importprivkey(self.privkey1)
        self.run_channels_tests()


if __name__ == '__main__':
    CryptoconditionsChannelsTest().main()
