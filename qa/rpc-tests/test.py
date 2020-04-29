# /************************************************************************
#  File: test.py
#  Author: mdr0id
#  Date: 4/25/2020
#  Description:  Used to run RPC test per platform
#
#  Usage: python3 test.py
#
#  Notes:
#
#  Known bugs/missing features:
#  1. Win / Lin / Mac signal handler (need to go back and check/implement) 
#
# ************************************************************************/

import os
import psutil
import platform
import time
import logging
import argparse
from subprocess import Popen, PIPE

logging.basicConfig(format='%(asctime)s - PID:%(process)d - %(levelname)s: %(message)s', level=logging.INFO)

#get system information to setup according work directory for RPCs
HOST_OS = platform.system()

#get host platform path for current user
WORK_DIR = os.path.expanduser('~')

#@TODO Change these to defaults per platform of cwd
if HOST_OS == 'Windows':
    os.environ["BITCOIND"] = os.path.join(WORK_DIR, "Documents", "zcash", "zcashd")
    os.environ["BITCOINCLI"] = os.path.join(WORK_DIR, "Documents", "zcash", "zcash-cli")
    WORK_DIR = os.path.join(WORK_DIR,"zcash")
elif HOST_OS == 'Linux':
    os.environ["BITCOIND"] = os.path.join(WORK_DIR,"zcash", "zcashd")
    os.environ["BITCOINCLI"] = os.path.join(WORK_DIR, "zcash", "zcash-cli")
    WORK_DIR = os.path.join(WORK_DIR,"zcash")
elif HOST_OS == 'Darwin':
    os.environ["BITCOIND"] = os.path.join(WORK_DIR,"zcash", "zcashd")
    os.environ["BITCOINCLI"] = os.path.join(WORK_DIR, "zcash", "zcash-cli")
    WORK_DIR = os.path.join(WORK_DIR,"zcash")
else:
    logging.error(" %s is not currently supported.", HOST_OS)


BASE_SCRIPTS=(
    'paymentdisclosure.py',
    'prioritisetransaction.py',
    'wallet_treestate.py',
    'wallet_anchorfork.py',
    'wallet_changeaddresses.py',
    'wallet_changeindicator.py',
    'wallet_import_export.py',
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
    'framework.py',
    'feature_zip221.py'
)

EXTENDED_SCRIPTS=(
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
    'p2p-acceptblock.py'
)

NON_SCRIPTS =(
    'rpc-tester.py'
)

def cleanup_failed_tests():
    pass

def run_test_scripts(test_list, args):
    fail_list = []
    pass_list = []
    stats = []

    total_tests = len(test_list)
    num_tests_ran = 0

    logging.info("=== Starting RPC Tester  ===")
    total_stime = time.time()
    for file in test_list:
        logging.info("--- Running test : %s ---", file)
        stats.append(file)
        try:
            start_time = time.time()
            p1 = Popen(['python3', file])
            buffer = p1.communicate()[0]
        except:
            fail_list.append(file)
        
        elapsed_time = time.time() - start_time
        logging.info("Elapsed time: %s", time.strftime("%H:%M:%S", time.gmtime(elapsed_time)))
        stats.append(time.strftime("%H:%M:%S", time.gmtime(elapsed_time)))
       
        if p1.returncode != 0:
            fail_list.append(stats)
        else:
            pass_list.append(stats)
        
        num_tests_ran += 1

        if args.failfast and len(fail_list) > 0 :
            break
    total_elapsed_time = time.time() - total_stime

    logging.info("=====")
    logging.info("Total tests: %i ", total_tests)
    logging.info("Tests ran: %i ", num_tests_ran)
    logging.info("Duration: ~%s ", time.strftime("%H:%M:%S", time.gmtime(total_elapsed_time)))
    logging.info("Num Fail: %i", len(fail_list) )
    logging.info("Num Pass: %i", len(pass_list) )
    logging.info("=====")
    #add sort routine
    #maybe cache meta stuff
    #sanity check failed tests (e.g. clean cache, kill zombie zcashd pids, dump mem info, retry 3 times)
    logging.info("+ TEST(S) PASSED :")
    logging.info("------------------")
    print_test_results(pass_list)
    logging.info("- TEST(S) FAILED :")
    logging.info("------------------")
    print_test_results(fail_list)

def print_test_results(test_list):
    i =0
    for test in test_list:
        logging.info("%s : %s", test[i], test[i+1])
        i+=2

def opt_list():
    opt_listbase()
    opt_listextended()

def opt_listbase():
    for test_file in BASE_SCRIPTS:
            print(test_file[:-3])

def opt_listextended():
    for ext_test_file in EXTENDED_SCRIPTS:
            print(ext_test_file[:-3])

def opt_individual(args):
    print( 'files: {}'.format(args.args))
    run_test_scripts(args.args, args)

def main():
    parser = argparse.ArgumentParser(description="Zcashd RPC Tester")
    parser.add_argument('--ENABLE_PROTON', '-ep', action='store_true', help='Enable Proton RPC tests.')
    parser.add_argument('--ENABLE_ZMQ', '-ez', action='store_true', help='Enable ZMQ RPC tests.')
    parser.add_argument('--list', '-l', action='store_true', help='Prints a list of the base and extended RPC tests.')
    parser.add_argument('--listbase', '-lb', action='store_true', help='Prints a list of the base RPC tests.')
    parser.add_argument('--listextended', '-le', action='store_true', help='Prints a list of the extended RPC tests.')
    parser.add_argument('--extended', '-e', action='store_true', help='Run the extended RPC tests.')
    parser.add_argument('--individual', '-i', action='store_true', help='Run individual RPC tests.')
    parser.add_argument('--failfast', '-f', action='store_true', help='Stop execution of tests after first failure.')
    parser.add_argument('--version', '-v', action='version', version='1.0.0' )
    parser.add_argument('args', nargs=argparse.REMAINDER)
    args = parser.parse_args()

    if args.list:
        opt_list()
    if args.listbase:
        opt_listbase()
    if args.listextended:
        opt_listextended()
    if args.individual:
        opt_individual(args)
        return
    
    run_test_scripts(BASE_SCRIPTS, args)
    if args.extended:
        run_test_scripts(EXTENDED_SCRIPTS, args)

if __name__ == '__main__':
    main()
