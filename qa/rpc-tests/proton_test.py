#!/usr/bin/env python2
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test Proton interface (provides AMQP 1.0 messaging support).
#
# Requirements:
#   Python library for Qpid Proton:
#     https://pypi.python.org/pypi/python-qpid-proton
#   To install:
#     pip install python-qpid-proton
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, bytes_to_hex_str, \
    start_nodes

from proton.handlers import MessagingHandler
from proton.reactor import Container

import threading


class Server(MessagingHandler):

    def __init__(self, url, limit):
        super(Server, self).__init__()
        self.url = url
        self.counter = limit
        self.blockhashes = []
        self.txids = []
        self.blockseq = -1
        self.txidseq = -1

    def on_start(self, event):
        print "Proton listening on:", self.url
        self.container = event.container
        self.acceptor = event.container.listen(self.url)

    def on_message(self, event):
        m = event.message
        hash = bytes_to_hex_str(m.body)
        sequence = m.properties['x-opt-sequence-number']
        if m.subject == "hashtx":
            self.txids.append(hash)

            # Test that sequence id is incrementing
            assert(sequence == 1 + self.txidseq)
            self.txidseq = sequence
        elif m.subject == "hashblock":
            self.blockhashes.append(hash)

            # Test that sequence id is incrementing
            assert(sequence == 1 + self.blockseq)
            self.blockseq = sequence

        self.counter = self.counter - 1
        if self.counter == 0:
            self.container.stop()


class ProtonTest (BitcoinTestFramework):

    port = 25672
    numblocks = 10  # must be even, as two nodes generate equal number
    assert(numblocks % 2 == 0)

    def setup_nodes(self):

        # Launch proton server in background thread
        # It terminates after receiving numblocks * 2 messages (one for coinbase, one for block)
        self.server = Server("127.0.0.1:%i" % self.port, self.numblocks * 2)
        self.container = Container(self.server)
        self.t1 = threading.Thread(target=self.container.run)
        self.t1.start()

        return start_nodes(4, self.options.tmpdir, extra_args=[
            ['-experimentalfeatures', '-debug=amqp', '-amqppubhashtx=amqp://127.0.0.1:'+str(self.port),
             '-amqppubhashblock=amqp://127.0.0.1:'+str(self.port)],
            [],
            [],
            []
            ])

    def run_test(self):
        self.sync_all()
        baseheight = self.nodes[0].getblockcount()    # 200 blocks already mined

        # generate some blocks
        self.nodes[0].generate(self.numblocks/2)
        self.sync_all()
        self.nodes[1].generate(self.numblocks/2)
        self.sync_all()

        # wait for server to finish
        self.t1.join()

        # sequence numbers have already been checked in the server's message handler

        # sanity check that we have the right number of block hashes and coinbase txids
        assert_equal(len(self.server.blockhashes), self.numblocks)
        assert_equal(len(self.server.txids), self.numblocks)

        # verify that each block has the correct coinbase txid
        for i in xrange(0, self.numblocks):
            height = baseheight + i + 1
            blockhash = self.nodes[0].getblockhash(height)
            assert_equal(blockhash, self.server.blockhashes[i])
            resp = self.nodes[0].getblock(blockhash)
            coinbase = resp["tx"][0]
            assert_equal(coinbase, self.server.txids[i])


if __name__ == '__main__':
    ProtonTest().main()
