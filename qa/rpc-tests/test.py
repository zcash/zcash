# /************************************************************************
#  File: test.py
#  Author: mdr0id
#  Date: 1/7/2018
#  Description:  Used to run RPC test per platform
#
#  Usage: python test.py
#
#  Notes:
#
#  Known bugs/missing features:
#  1. Win / Lin / Mac signal handler (need to go back and check/implement) 
#
# ************************************************************************/

import os
import sys
import psutil
import platform
import time
import logging
import argparse
from subprocess import Popen, PIPE

logging.basicConfig(format='%(asctime)s - PID:%(process)d - %(levelname)s: %(message)s', level=logging.INFO)

#get system information to setup according work directory for RPCs
HOST_OS = platform.system()

def expanduser(path):
    # os.path.expanduser is hopelessly broken for Unicode paths on Windows (ticket #1674).
    if sys.platform == "win32":
        return windows_expanduser(path)
    else:
        return os.path.expanduser(path)

# <https://msdn.microsoft.com/en-us/library/windows/desktop/ms681382%28v=vs.85%29.aspx>
ERROR_ENVVAR_NOT_FOUND = 203

def windows_getenv(name):
    # Based on <http://stackoverflow.com/questions/2608200/problems-with-umlauts-in-python-appdata-environvent-variable/2608368#2608368>,
    # with improved error handling. Returns None if there is no enivronment variable of the given name.
    if not isinstance(name, unicode):
        raise AssertionError("name must be Unicode")

    n = GetEnvironmentVariableW(name, None, 0)
    # GetEnvironmentVariableW returns DWORD, so n cannot be negative.
    if n == 0:
        err = get_last_error()
        if err == ERROR_ENVVAR_NOT_FOUND:
            return None
        raise OSError("WinError: %s\n attempting to read size of environment variable %r"
                      % (WinError(err), name))
    if n == 1:
        # Avoid an ambiguity between a zero-length string and an error in the return value of the
        # call to GetEnvironmentVariableW below.
        return u""

    buf = create_unicode_buffer(u'\0'*n)
    retval = GetEnvironmentVariableW(name, buf, n)
    if retval == 0:
        err = get_last_error()
        if err == ERROR_ENVVAR_NOT_FOUND:
            return None
        raise OSError("WinError: %s\n attempting to read environment variable %r"
                      % (WinError(err), name))
    if retval >= n:
        raise OSError("Unexpected result %d (expected less than %d) from GetEnvironmentVariableW attempting to read environment variable %r"
                      % (retval, n, name))

    return buf.value

def windows_expanduser(path):
    if not path.startswith('~'):
        return path

    home_dir = windows_getenv(u'USERPROFILE')
    if home_dir is None:
        home_drive = windows_getenv(u'HOMEDRIVE')
        home_path = windows_getenv(u'HOMEPATH')
        if home_drive is None or home_path is None:
            raise OSError("Could not find home directory: neither %USERPROFILE% nor (%HOMEDRIVE% and %HOMEPATH%) are set.")
        home_dir = os.path.join(home_drive, home_path)

    if path == '~':
        return home_dir
    elif path.startswith('~/') or path.startswith('~\\'):
        return os.path.join(home_dir, path[2 :])
    else:
        return path

#get host platform path for current user
WORK_DIR = expanduser('~')

if HOST_OS == 'Windows':
    os.environ["BITCOIND"] = os.path.join(WORK_DIR, "Documents", "zcash", "zcashd")
    os.environ["BITCOINCLI"] = os.path.join(WORK_DIR, "Documents", "zcash", "zcash-cli")
    WORK_DIR = os.path.join(WORK_DIR,"zcash")
elif HOST_OS != 'Windows':
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
    'wallet_changeindicator.py',
    'wallet_import_export.py',
    'wallet_protectcoinbase.py',
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
    'listtransactions.py',
    'mempool_resurrect_test.py',
    'txn_doublespend.py',
    'txn_doublespend.py --mineblock',
    'getchaintips.py',
    'rawtransactions.py',
    'rest.py',
    'mempool_spendcoinbase.py',
    'mempool_reorg.py',
    'mempool_tx_input_limit.py',
    'mempool_nu_activation.py',
    'mempool_tx_expiry.py',
    'httpbasics.py',
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
    'finalsaplingroot.py'
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
#    'forknotify.py',
    'p2p-acceptblock.py'
)

NON_SCRIPTS =(
    'test.py'
)

#@TODO Add Support for conditionally running ZMQ, Proton RPC, enable wallet, enable unittest tests

def cleanup_failed_tests():
    pass

def run_test_scripts(test_list, args):
    fail_list = []
    pass_list = []

    total_tests = len(test_list)
    num_tests_ran = 0

    logging.info("=== Starting RPC Tester  ===")
    total_stime = time.time()
    for file in test_list:
        logging.info("--- Running test : %s ---", file)
        #might want to add routine to properly clean SIGTERM tests so they don't conga line the rest
        try:
            start_time = time.time()
            p1 = Popen(['python', file])
            buffer = p1.communicate()[0]
        except:
            fail_list.append(file)
        
        elapsed_time = time.time() - start_time
        logging.info("Elapsed time: %s", time.strftime("%H:%M:%S", time.gmtime(elapsed_time)))
        t_time = time.strftime("%H:%M:%S", time.gmtime(elapsed_time))
        stats = (file, t_time)
       
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
    logging.info("Num Fail: %i", len(fail_list))
    logging.info("Num Pass: %i", len(pass_list))
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
    for stats in test_list:
        logging.info("%s %s", stats[0], stats[1])

def opt_list():
    opt_listbase()
    opt_listextended()

def opt_listbase():
    for test_file in BASE_SCRIPTS:
            logging.info("%s", test_file[:-3])

def opt_listextended():
    for ext_test_file in EXTENDED_SCRIPTS:
            logging.info("%s", ext_test_file[:-3])

def opt_individual(args):
    logging.info("%s", 'files: {}'.format(args.args))
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


    if args.ENABLE_PROTON:
        BASE_SCRIPTS.apped('proton_test.py')
    if args.ENABLE_ZMQ:
        BASE_SCRIPTS.append('zmq_test.py' )
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
