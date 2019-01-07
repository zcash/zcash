#!/usr/bin/env python2
# Copyright (c) 2018 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


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

        # executing channel close
        result = rpc.channelsclose(channel_txid)
        assert_success(result)
        channel_close_txid = self.send_and_mine(result["hex"], rpc)
        assert channel_close_txid, "got txid"

        rpc.generate(2)
        self.sync_all()

        # now in channelinfo closed flag should appear
        result = rpc.channelsinfo(channel_txid)
        assert_equal(result["Transactions"][2]["Close"], channel_close_txid)

        # executing channel refund
        result = rpc.channelsrefund(channel_txid, channel_close_txid)
        assert_success(result)
        refund_txid = self.send_and_mine(result["hex"], rpc)
        assert refund_txid, "got txid"

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
