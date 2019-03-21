#!/usr/bin/env python3
#
# Execute the standard smoke tests for Zcash releases.
#

# Add RPC test_framework to module search path:
import os
import sys
sys.path.insert(
    0,
    os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        'rpc-tests',
        'test_framework',
    )
)

import argparse
import subprocess

from authproxy import AuthServiceProxy, JSONRPCException

#
# Smoke test definitions
#

# (case, expected_mainnet, expected_testnet)
SMOKE_TESTS = [
    # zcashd start/stop/restart flows
    ('1a', True, True), # zcashd start
    ('1b', True, True), # Graceful zcashd stop
    ('1c', True, True), # Ungraceful zcashd stop
    ('1d', True, True), # zcashd start; graceful zcashd stop; zcashd start
    ('1e', True, True), # zcashd start; ungraceful zcashd stop; zcashd start
    # Control
    ('2a', True, True), # Run getinfo
    ('2b', True, True), # Run help
    # Address generation
    ('3a', True, True), # Generate a Sprout z-addr
    ('3b', True, True), # Generate multiple Sprout z-addrs
    ('3c', True, True), # Generate a t-addr
    ('3d', True, True), # Generate multiple t-addrs
    ('3e', True, True), # Generate a Sapling z-addr
    ('3f', True, True), # Generate multiple Sapling z-addrs
    # Transactions
    ('4a', True,  True ), # Send funds from Sprout z-addr to same Sprout z-addr
    ('4b', True,  True ), # Send funds from Sprout z-addr to a different Sprout z-addr
    ('4c', True,  True ), # Send funds from Sprout z-addr to a t-addr
    ('4d', True,  True ), # Send funds from t-addr to Sprout z-addr
    ('4e', True,  True ), # Send funds from t-addr to t-addr
    ('4f', True,  True ), # Send funds from t-addr to Sapling z-addr
    ('4g', True,  True ), # Send funds from Sapling z-addr to same Sapling z-addr
    ('4h', True,  True ), # Send funds from Sapling z-addr to a different Sapling z-addr
    ('4i', True,  True ), # Send funds from Sapling z-addr to a t-addr
    ('4j', False, False), # Send funds from Sprout z-addr to Sapling z-addr
    ('4k', True,  True ), # Send funds from Sprout z-addr to multiple Sprout z-addrs
    ('4l', True,  True ), # Send funds from Sprout z-addr to multiple t-addrs
    ('4m', True,  True ), # Send funds from Sprout z-addr to t-addr and Sprout z-addrs
    ('4n', False, False), # Send funds from Sprout z-addr to t-addr and Sapling z-addr
    ('4o', False, False), # Send funds from Sprout z-addr to multiple Sapling z-addrs
    ('4p', True,  True ), # Send funds from t-addr to multiple t-addrs
    ('4q', True,  True ), # Send funds from t-addr to multiple Sprout z-addrs
    ('4r', True,  True ), # Send funds from t-addr to multiple Sapling z-addrs
    ('4s', False, False), # Send funds from t-addr to Sprout z-addr and Sapling z-addr
    ('4t', True,  True ), # Send funds from Sapling z-addr to multiple Sapling z-addrs
    ('4u', False, False), # Send funds from Sapling z-addr to multiple Sprout z-addrs
    ('4v', True,  True ), # Send funds from Sapling z-addr to multiple t-addrs
    ('4w', True,  True ), # Send funds from Sapling z-addr to t-addr and Sapling z-addr
    ('4x', False, False), # Send funds from Sapling z-addr to Sapling z-addr and Sprout z-addr
    ('4y', True,  True ), # Send funds from t-addr to Sprout z-addr using z_mergetoaddress
    ('4z', True,  True ), # Send funds from 2 different t-addrs to Sprout z-addr using z_mergetoaddress
    ('4aa', False, False), # Send funds from the same 2 t-addrs to Sprout z-addr using z_mergetoaddress
    ('4bb', True,  True ), # Send funds from 2 different t-addrs to Sapling z-addr using z_mergetoaddress
    ('4cc', True,  True ), # Send funds from t-addr to Sapling z-addr using z_mergetoaddress
    ('4dd', True,  True ), # Send funds from t-addr and Sprout z-addr to Sprout z-addr using z_mergetoaddress
    ('4ee', True,  True ), # Send funds from t-addr and Sapling z-addr to Sapling z-addr using z_mergetoaddress
    ('4ff', True,  True ), # Send funds from Sprout z-addr and Sprout z-addr to Sprout z-addr using z_mergetoaddress
    ('4gg', True,  True ), # Send funds from  Sapling z-addr and Sapling z-addr to Sapling z-addr using z_mergetoaddress
    # Wallet
    ('5a', True, True), # After generating multiple z-addrs, run z_listaddresses
    ('5b', True, True), # Run z_validateaddress with a Sprout z-addr
    ('5c', True, True), # Run z_validateaddress with a Sapling z-addr
    ('5d', True, True), # After a transaction, run z_listunspent
    ('5e', True, True), # After a transaction, run z_listreceivedbyaddress
    ('5f', True, True), # After a transaction, run z_getbalance
    ('5g', True, True), # After a transaction, run z_gettotalbalance
    ('5h', True, True), # Run z_exportkey using a Sprout z-addr
    ('5i', True, True), # Run z_importkey using the zkey from a Sprout z-addr
    ('5j', True, True), # Run z_exportkey using a Sapling z-addr
    ('5k', True, True), # Run z_importkey using the zkey from a Sapling z-addr
    ('5l', True, True), # Run z_exportwallet
    ('5m', True, True), # Run z_importwallet
    ('5n', True, True), # Run z_shieldcoinbase
    ('5o', True, True), # Run getwalletinfo
    # Network
    ('6a', True, True), # Run getpeerinfo
    ('6b', True, True), # Run getnetworkinfo
    ('6c', True, False), # Run getdeprecationinfo
    ('6d', True, True), # Run getconnectioncount
    ('6e', True, True), # Run getaddednodeinfo
    # Mining
    ('7a', True, True), # Run getblocksubsidy
    ('7b', True, True), # Run getblocktemplate
    ('7c', True, True), # Run getmininginfo
    ('7d', True, True), # Run getnetworkhashps
    ('7e', True, True), # Run getnetworksolps
]


#
# Test stages
#

STAGES = [
]

STAGE_COMMANDS = {
}

def run_stage(stage, zcash):
    print('Running stage %s' % stage)
    print('=' * (len(stage) + 14))
    print()

    cmd = STAGE_COMMANDS[stage]
    if cmd is not None:
        ret = cmd(zcash)
    else:
        print('WARNING: stage not yet implemented, skipping')
        ret = {}

    print()
    print('-' * (len(stage) + 15))
    print('Finished stage %s' % stage)
    print()

    return ret


#
# Zcash wrapper
#

class ZcashNode(object):
    def __init__(self, datadir, wallet, zcashd=None, zcash_cli=None):
        if zcashd is None:
            zcashd = os.getenv('ZCASHD', 'zcashd')
        if zcash_cli is None:
            zcash_cli = os.getenv('ZCASHCLI', 'zcash-cli')

        self.__datadir = datadir
        self.__wallet = wallet
        self.__zcashd = zcashd
        self.__zcash_cli = zcash_cli
        self.__process = None
        self.__proxy = None

    def start(self, testnet=True, extra_args=None, timewait=None):
        if self.__proxy is not None:
            raise RuntimeError('Already started')

        rpcuser = 'st'
        rpcpassword = 'st'

        args = [
            self.__zcashd,
            '-datadir=%s' % self.__datadir,
            '-wallet=%s' % self.__wallet,
            '-rpcuser=%s' % rpcuser,
            '-rpcpassword=%s' % rpcpassword,
            '-showmetrics=0',
            '-experimentalfeatures',
            '-zmergetoaddress',
        ]
        if testnet:
            args.append('-testnet=1')
        if extra_args is not None:
            args.extend(extra_args)

        self.__process = subprocess.Popen(args)

        cli_args = [
            self.__zcash_cli,
            '-datadir=%s' % self.__datadir,
            '-rpcuser=%s' % rpcuser,
            '-rpcpassword=%s' % rpcpassword,
            '-rpcwait',
        ]
        if testnet:
            cli_args.append('-testnet=1')
        cli_args.append('getblockcount')

        devnull = open('/dev/null', 'w+')
        if os.getenv('PYTHON_DEBUG', ''):
            print('start_node: zcashd started, calling zcash-cli -rpcwait getblockcount')
        subprocess.check_call(cli_args, stdout=devnull)
        if os.getenv('PYTHON_DEBUG', ''):
            print('start_node: calling zcash-cli -rpcwait getblockcount returned')
        devnull.close()

        rpcuserpass = '%s:%s' % (rpcuser, rpcpassword)
        rpchost = '127.0.0.1'
        rpcport = 18232 if testnet else 8232

        url = 'http://%s@%s:%d' % (rpcuserpass, rpchost, rpcport)
        if timewait is not None:
            self.__proxy = AuthServiceProxy(url, timeout=timewait)
        else:
            self.__proxy = AuthServiceProxy(url)

    def stop(self):
        if self.__proxy is None:
            raise RuntimeError('Not running')

        self.__proxy.stop()
        self.__process.wait()
        self.__proxy = None
        self.__process = None

    def __getattr__(self, name):
        if self.__proxy is None:
            raise RuntimeError('Not running')

        return self.__proxy.__getattr__(name)


#
# Test driver
#

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--list-stages', dest='list', action='store_true')
    parser.add_argument('--mainnet', action='store_true', help='Use mainnet instead of testnet')
    parser.add_argument('--wallet', default='wallet.dat', help='Wallet file to use (within data directory)')
    parser.add_argument('datadir', help='Data directory to use for smoke testing', default=None)
    parser.add_argument('stage', nargs='*', default=STAGES,
                        help='One of %s'%STAGES)
    args = parser.parse_args()

    # Check for list
    if args.list:
        for s in STAGES:
            print(s)
        sys.exit(0)

    # Check validity of stages
    for s in args.stage:
        if s not in STAGES:
            print("Invalid stage '%s' (choose from %s)" % (s, STAGES))
            sys.exit(1)

    # Start zcashd
    zcash = ZcashNode(args.datadir, args.wallet)
    print('Starting zcashd...')
    zcash.start(not args.mainnet)
    print()

    # Run the stages
    results = {}
    for s in args.stage:
        results.update(run_stage(s, zcash))

    # Stop zcashd
    print('Stopping zcashd...')
    zcash.stop()

    passed = True
    print()
    print('========================')
    print('       Results')
    print('========================')
    print('Case | Expected | Actual')
    print('========================')
    for test_case in SMOKE_TESTS:
        case = test_case[0]
        expected = test_case[1 if args.mainnet else 2]
        if case in results:
            actual = results[case]
            actual_str = '%s%s' % (
                'Passed' if actual else 'Failed',
                '' if expected == actual else '!!!'
            )
            passed &= (expected == actual)
        else:
            actual_str = ' N/A'
        print('%s | %s | %s' % (
            case.ljust(4),
            ' Passed ' if expected else ' Failed ',
            actual_str
        ))

    if not passed:
        print()
        print("!!! One or more smoke test stages failed !!!")
        sys.exit(1)

if __name__ == '__main__':
    main()
