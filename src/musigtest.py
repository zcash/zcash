#!/usr/bin/env python3
import platform
import os
import re
import json
import random
import base58
import binascii
import hashlib
import sys
import time
from slickrpc import Proxy

# fucntion to define rpc_connection
def def_credentials(chain):
    rpcport = '';
    operating_system = platform.system()
    if operating_system == 'Darwin':
        ac_dir = os.environ['HOME'] + '/Library/Application Support/Komodo'
    elif operating_system == 'Linux':
        ac_dir = os.environ['HOME'] + '/.komodo'
    elif operating_system == 'Windows':
        ac_dir = '%s/komodo/' % os.environ['APPDATA']
    if chain == 'KMD':
        coin_config_file = str(ac_dir + '/komodo.conf')
    else:
        coin_config_file = str(ac_dir + '/' + chain + '/' + chain + '.conf')
    with open(coin_config_file, 'r') as f:
        for line in f:
            l = line.rstrip()
            if re.search('rpcuser', l):
                rpcuser = l.replace('rpcuser=', '')
            elif re.search('rpcpassword', l):
                rpcpassword = l.replace('rpcpassword=', '')
            elif re.search('rpcport', l):
                rpcport = l.replace('rpcport=', '')
    if len(rpcport) == 0:
        if chain == 'KMD':
            rpcport = 7771
        else:
            print("rpcport not in conf file, exiting")
            print("check " + coin_config_file)
            exit(1)
    return (Proxy("http://%s:%s@127.0.0.1:%d" % (rpcuser, rpcpassword, int(rpcport))))
    

# generate address, validate address, dump private key
def genvaldump(rpc_connection):
    # get new address
    address = rpc_connection.getnewaddress()
    # validate address
    validateaddress_result = rpc_connection.validateaddress(address)
    pubkey = validateaddress_result['pubkey']
    address = validateaddress_result['address']
    # dump private key for the address
    privkey = rpc_connection.dumpprivkey(address)
    # function output
    output = [pubkey, privkey, address]
    return(output)

CHAIN = 'MUSIG' #sys.argv[1]

rpc = def_credentials(CHAIN)

pubkeys = []
address_info = []
ret = input('Do you want to generate new pubkeys? ').lower()

if ret.startswith('y'):
    numpks = int(input('Enter number of pubkeys to combine: '))
    if os.path.isfile("list.json"):
        print('Already have list.json, move it if you would like to generate a new set.')
        sys.exit(0)
    while len(address_info) < numpks:
        addressinfo = genvaldump(rpc)
        address_info.append(addressinfo)
    f = open("list.json", "w+")
    f.write(json.dumps(address_info))
else:
    if os.path.isfile("list.json"):
        with open('list.json') as list:
            address_info = json.load(list)
    else:
        sys.exit('No list.json you need to create new pubkeys!')

for addressinfo in address_info:
    pubkeys.append(addressinfo[0])

ret = rpc.setpubkey(pubkeys[0])
ret = rpc.cclib("combine", "18", str(pubkeys))
pkhash = str(ret['pkhash'])
combinedpk = str(ret['combined_pk'])
print('Your combined pubkey is: ' + combinedpk)
print('Your pkhash is: ' + pkhash)
amount = float(input('Enter amount to send: '))
if amount == 0:
    sys.exit('Cannot send 0 coins. Exiting.')
tmp = str([combinedpk, amount])
hex = rpc.cclib("send", "18", tmp)['hex']
senttxid = rpc.sendrawtransaction(hex)
print('Your senttxid is: ' + senttxid)

print("Waiting for tx to be confirmed")
while True:
    confirmed = int(rpc.gettransaction(senttxid)["confirmations"])
    if not confirmed:
        time.sleep(10)
    else:
        print('SentTX confirmed')
        break

scriptPubKey = rpc.getrawtransaction(senttxid,1)['vout'][1]['scriptPubKey']['hex']
print('Your scriptPubKey is: ' + scriptPubKey)
tmp = str([senttxid, scriptPubKey])
msg = rpc.cclib("calcmsg", "18", tmp)['msg']
print('Your msg is: ' + msg)

i = 0;
commitments = []
for pubkey in pubkeys:
    ret = rpc.setpubkey(pubkey)
    tmp = str([i, len(pubkeys), combinedpk, pkhash, msg, i])
    commitments.append(rpc.cclib("session", "18", tmp)['commitment'])
    i = i + 1

print("Created commitments sucessfully... Sending to all signers.")

i = 0
nonces = []
for pubkey in pubkeys:
    ret = rpc.setpubkey(pubkey)
    n = 0
    for commitment in commitments:
        tmp = str([pkhash, n, commitment, i])
        ret = rpc.cclib("commit", "18", tmp)
        try:
            nonces.append(ret['nonce'])
        except:
            x = 1
        n = n + 1
    i = i + 1

print("Created nounce's sucessfully... Sending to all signers.")

i = 0
partialsigs = []
for pubkey in pubkeys:
    ret = rpc.setpubkey(pubkey)
    n = 0
    for nonce in nonces:
        tmp = str([pkhash, n, nonce, i])
        ret = rpc.cclib("nonce", "18", tmp)
        try:
            partialsigs.append(ret['partialsig'])
        except:
            x = 1
        n = n + 1
    i = i + 1

print("Created partial sigs sucessfully... Sending to all signers.")

i = 0
combinedsigs = []
for pubkey in pubkeys:
    ret = rpc.setpubkey(pubkey)
    n = 0
    for partialsig in partialsigs:
        tmp = str([pkhash, n, partialsig, i])
        ret = rpc.cclib("partialsig", "18", tmp)
        try:
            combinedsigs.append(ret['combinedsig'])
        except:
            x = 1
        n = n + 1
    i = i + 1

print("Created combined sigs sucessfully... Verifying.")

tmp = str([msg, combinedpk, combinedsigs[0]])
ret = rpc.cclib("verify", "18", tmp)

if ret['result'] != "success":
    print(ret)
    sys.exit('Could not verify signature.')

print('Verified... Attempting to send.')

tmp = str([senttxid, scriptPubKey, combinedsigs[0]])
ret = rpc.cclib("spend", "18", tmp)

if ret['result'] != "success":
    print(ret)
    sys.exit('Could not create spend transaction.')

try:
    ret = rpc.sendrawtransaction(ret['hex'])
except:
    sys.exit('Could not send transaction.')

print('Spent txid: ' + ret)
