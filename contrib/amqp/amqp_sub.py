#!/usr/bin/env python2
# Copyright (c) 2017 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Requirements:
#  pip install python-qpid-proton

import binascii
from proton.handlers import MessagingHandler
from proton.reactor import Container

port = 5672

class Server(MessagingHandler):
    def __init__(self, url):
        super(Server, self).__init__()
        self.url = url
        self.senders = {}

    def on_start(self, event):
        print "Listening on:", self.url
        self.container = event.container
        self.acceptor = event.container.listen(self.url)

    def on_message(self, event):
        m = event.message
        topic = m.subject
        body = m.body
        sequence = str( m.properties['x-opt-sequence-number'] )
        if topic == "hashablock":
            print '- HASH BLOCK ('+sequence+') -'
            print binascii.hexlify(body)
        elif topic == "hashtx":
            print '- HASH TX ('+sequence+') -'
            print binascii.hexlify(body)
        elif topic == "rawblock":
            print '- RAW BLOCK HEADER ('+sequence+') -'
            print binascii.hexlify(body[:80])
        elif topic == "rawtx":
            print '- RAW TX ('+sequence+') -'
            print binascii.hexlify(body)

try:
    Container(Server("127.0.0.1:%i" % port)).run()
except KeyboardInterrupt:
    pass

