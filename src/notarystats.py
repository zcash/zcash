#!/usr/bin/env python3
"""
sudo apt-get install python3-dev
sudo apt-get install python3 libgnutls28-dev libssl-dev
sudo apt-get install python3-pip
pip3 install setuptools
pip3 install wheel
pip3 install base58 slick-bitcoinrpc
./notarystats.py
------------------------------------------------
"""
import platform
import os
import re
import sys
import time
import pprint
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
    

rpc = def_credentials('KMD')
pp = pprint.PrettyPrinter(indent=2)

notarynames = [ "0dev1_jl777", "0dev2_kolo", "0dev3_kolo", "0dev4_decker_AR", "a-team_SH", "artik_AR", "artik_EU", "artik_NA", "artik_SH", "badass_EU", "badass_NA", "batman_AR", "batman_SH", "ca333", "chainmakers_EU", "chainmakers_NA", "chainstrike_SH", "cipi_AR", "cipi_NA", "crackers_EU", "crackers_NA", "dwy_EU", "emmanux_SH", "etszombi_EU", "fullmoon_AR", "fullmoon_NA", "fullmoon_SH", "goldenman_EU", "indenodes_AR", "indenodes_EU", "indenodes_NA", "indenodes_SH", "jackson_AR", "jeezy_EU", "karasugoi_NA", "komodoninja_EU", "komodoninja_SH", "komodopioneers_SH", "libscott_SH", "lukechilds_AR", "madmax_AR", "meshbits_AR", "meshbits_SH", "metaphilibert_AR", "metaphilibert_SH", "patchkez_SH", "pbca26_NA", "peer2cloud_AR", "peer2cloud_SH", "polycryptoblog_NA", "hyper_AR", "hyper_EU", "hyper_SH", "hyper_NA", "popcornbag_AR", "popcornbag_NA", "alien_AR", "alien_EU", "thegaltmines_NA", "titomane_AR", "titomane_EU", "titomane_SH", "webworker01_NA", "xrobesx_NA" ]
notaries = 64 * [0]

startheight =  821657 #Second time filter for assetchains (block 821657) for KMD its 814000
stopheight = 1307200
for i in range(startheight,stopheight):
    ret = rpc.getNotarisationsForBlock(i)
    KMD = ret['KMD']
    if len(KMD) > 0:
        for obj in KMD:
            #for now skip KMD for this. As official stats are from BTC chain
            # this can be reversed to !== to count KMD numbers :)
            if obj['chain'] == 'KMD':
                continue;
            for notary in obj['notaries']:
                notaries[notary] = notaries[notary] + 1

i = 0
SH = [] 
AR = [] 
EU = [] 
NA = [] 
for notary in notaries:
    tmpnotary = {}
    tmpnotary['node'] = notarynames[i]
    tmpnotary['ac_count'] = notary
    if notarynames[i].endswith('SH'):
        SH.append(tmpnotary)
    elif notarynames[i].endswith('AR'):
        AR.append(tmpnotary)
    elif notarynames[i].endswith('EU'):
        EU.append(tmpnotary)
    elif notarynames[i].endswith('NA'):
        NA.append(tmpnotary)
    i = i + 1

regions = {}
regions['SH'] = sorted(SH, key=lambda k: k['ac_count'], reverse=True)
regions['AR'] = sorted(AR, key=lambda k: k['ac_count'], reverse=True)
regions['EU'] = sorted(EU, key=lambda k: k['ac_count'], reverse=True)
regions["NA"] = sorted(NA, key=lambda k: k['ac_count'], reverse=True)

pp.pprint(regions)
