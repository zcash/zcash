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
combinedpk = rpc.cclib("combine", "18", str(pubkeys))['combined_pk']

print('Your combined pubkey is: ' + combinedpk)
