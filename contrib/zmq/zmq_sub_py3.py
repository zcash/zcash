#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import array
import binascii
import zmq
import struct

port = 28332

zmqContext = zmq.Context()
zmqSubSocket = zmqContext.socket(zmq.SUB)
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, b"hashblock")
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, b"hashtx")
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, b"rawblock")
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, b"rawtx")
zmqSubSocket.connect("tcp://127.0.0.1:%i" % port)

try:
    while True:
        msg = zmqSubSocket.recv_multipart()
        topic = msg[0]
        body = msg[1]
        sequence = b"Unknown";
        if len(msg[-1]) == 4:
          msgSequence = struct.unpack('<I', msg[-1])[-1]
          sequence = msgSequence
        if topic == b"hashblock":
            print("- HASH BLOCK (%d) -" % sequence)
            print(binascii.hexlify(body))
        elif topic == b"hashtx":
            print("- HASH TX (%d) -" % sequence)
            print(binascii.hexlify(body))
        elif topic == b"rawblock":
            print("- RAW BLOCK HEADER (%d) -" % sequence)
            print(binascii.hexlify(body[:80]))
        elif topic == b"rawtx":
            print("- RAW TX (%d) -" % sequence)
            print(binascii.hexlify(body))

except KeyboardInterrupt:
    zmqContext.destroy()
