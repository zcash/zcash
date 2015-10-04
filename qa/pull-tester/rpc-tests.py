#!/usr/bin/env python2
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Run Regression Test Suite
#

import os
import sys
import subprocess
import re
from tests_config import *
from sets import Set

#If imported values are not defined then set to zero (or disabled)
if not vars().has_key('ENABLE_WALLET'):
    ENABLE_WALLET=0
if not vars().has_key('ENABLE_BITCOIND'):
    ENABLE_BITCOIND=0
if not vars().has_key('ENABLE_UTILS'):
    ENABLE_UTILS=0
if not vars().has_key('ENABLE_ZMQ'):
    ENABLE_ZMQ=0

#Create a set to store arguments and create the passOn string
opts = Set()
passOn = ""
p = re.compile("^--")
for i in range(1,len(sys.argv)):
    if (p.match(sys.argv[i]) or sys.argv[i] == "-h"):
        passOn += " " + sys.argv[i]
    else:
        opts.add(sys.argv[i])

#Set env vars
buildDir = BUILDDIR
os.environ["BITCOIND"] = buildDir + '/src/zcashd' + EXEEXT
os.environ["BITCOINCLI"] = buildDir + '/src/zcash-cli' + EXEEXT

#Disable Windows tests by default
if EXEEXT == ".exe" and "-win" not in opts:
    print "Win tests currently disabled.  Use -win option to enable"
    sys.exit(0)

#Tests
testScripts = [
    'paymentdisclosure.py',
    'prioritisetransaction.py',
    'wallet_treestate.py',
    'wallet_anchorfork.py',
    'wallet_changeaddresses.py',
    'wallet_changeindicator.py',
    'wallet_import_export.py',
    'wallet_sendmany_any_taddr.py',
    'wallet_shieldingcoinbase.py',
    'wallet_shieldcoinbase_sprout.py',
    'wallet_shieldcoinbase_sapling.py',
    'wallet_listreceived.py',
    'wallet.py',
    'wallet_overwintertx.py',
    'wallet_persistence.py',
    'wallet_nullifiers.py',
    'wallet_1941.py',
    'wallet_addresses.py',
    'wallet_sapling.py',
    'wallet_listnotes.py',
    'mergetoaddress_sprout.py',
    'mergetoaddress_sapling.py',
    'mergetoaddress_mixednotes.py',
    'listtransactions.py',
    'mempool_resurrect_test.py',
    'txn_doublespend.py',
    'txn_doublespend.py --mineblock',
    'getchaintips.py',
    'rawtransactions.py',
    'getrawtransaction_insight.py',
    'rest.py',
    'mempool_limit.py',
    'mempool_spendcoinbase.py',
    'mempool_reorg.py',
    'mempool_nu_activation.py',
    'mempool_tx_expiry.py',
    'httpbasics.py',
    'multi_rpc.py',
    'zapwallettxes.py',
    'proxy_test.py',
    'merkle_blocks.py',
    'fundrawtransaction.py',
    'signrawtransactions.py',
    'signrawtransaction_offline.py',
    'walletbackup.py',
    'key_import_export.py',
    'nodehandling.py',
    'reindex.py',
    'addressindex.py',
    'spentindex.py',
    'timestampindex.py',
    'decodescript.py',
    'p2p-fullblocktest.py',
    'blockchain.py',
    'disablewallet.py',
    'zcjoinsplit.py',
    'zcjoinsplitdoublespend.py',
    'zkey_import_export.py',
    'reorg_limit.py',
    'getblocktemplate.py',
    'bip65-cltv-p2p.py',
    'bipdersig-p2p.py',
    'p2p_nu_peer_management.py',
    'rewind_index.py',
    'p2p_txexpiry_dos.py',
    'p2p_txexpiringsoon.py',
    'p2p_node_bloom.py',
    'regtest_signrawtransaction.py',
    'finalsaplingroot.py',
    'shorter_block_times.py',
    'sprout_sapling_migration.py',
    'turnstile.py',
    'mining_shielded_coinbase.py',
    'coinbase_funding_streams.py',
    'framework.py',
    'sapling_rewind_check.py',
    'feature_zip221.py',
    'upgrade_golden.py',
    'post_heartwood_rollback.py',
    'feature_logging.py',
    'remove_sprout_shielding.py',
    'feature_walletfile.py',
]
testScriptsExt = [
    'getblocktemplate_longpoll.py',
    'getblocktemplate_proposals.py',
    'pruning.py',
    'forknotify.py',
    'hardforkdetection.py',
    'invalidateblock.py',
    'keypool.py',
    'receivedby.py',
    'rpcbind_test.py',
#   'script_test.py',
    'smartfees.py',
    'maxblocksinflight.py',
    'invalidblockrequest.py',
#    'forknotify.py',
    'p2p-acceptblock.py',
    'wallet_db_flush.py',
]

#Enable ZMQ tests
if ENABLE_ZMQ == 1:
    testScripts.append('zmq_test.py')

if(ENABLE_WALLET == 1 and ENABLE_UTILS == 1 and ENABLE_BITCOIND == 1):
    rpcTestDir = buildDir + '/qa/rpc-tests/'
    #Run Tests
    for i in range(len(testScripts)):
       if (len(opts) == 0 or (len(opts) == 1 and "-win" in opts ) or '-extended' in opts 
           or testScripts[i] in opts or  re.sub(".py$", "", testScripts[i]) in opts ):
            print  "Running testscript " + testScripts[i] + "..." 
            subprocess.call(rpcTestDir + testScripts[i] + " --srcdir " + buildDir + '/src ' + passOn,shell=True) 
	    #exit if help is called so we print just one set of instructions
            p = re.compile(" -h| --help")
            if p.match(passOn):
                sys.exit(0)

    #Run Extended Tests
    for i in range(len(testScriptsExt)):
        if ('-extended' in opts or testScriptsExt[i] in opts 
           or re.sub(".py$", "", testScriptsExt[i]) in opts):
            print  "Running 2nd level testscript " + testScriptsExt[i] + "..." 
            subprocess.call(rpcTestDir + testScriptsExt[i] + " --srcdir " + buildDir + '/src ' + passOn,shell=True) 
else:
    print "No rpc tests to run. Wallet, utils, and bitcoind must all be enabled"
