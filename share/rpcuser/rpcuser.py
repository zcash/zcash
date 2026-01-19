#!/usr/bin/env python2 
# Copyright (c) 2015 The Bitcoin Core developers 
# Distributed under the MIT software license, see the accompanying 
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

import hashlib
import sys
import os
import base64
import binascii
import hmac

if len(sys.argv) < 2:
    sys.stderr.write('Please include username as an argument.\n')
    sys.exit(0)

username = sys.argv[1]

#Create 16 byte hex salt
salt = binascii.hexlify(os.urandom(16))

#Create 32 byte b64 password
password = base64.urlsafe_b64encode(os.urandom(32))

digestmod = hashlib.sha256

if sys.version_info.major >= 3:
    salt = salt.decode('utf-8')
    password = password.decode('utf-8')
    digestmod = 'SHA256'
 
m = hmac.new(bytearray(salt, 'utf-8'), bytearray(password, 'utf-8'), digestmod)
result = m.hexdigest()

print("String to be appended to zcash.conf:")
print("rpcauth="+username+":"+salt+"$"+result)
print("Your password:\n"+password)
