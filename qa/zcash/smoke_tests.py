#!/usr/bin/env python3
#
# Execute the standard smoke tests for Zcash releases.
#

import argparse
import sys

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

def run_stage(stage):
    print('Running stage %s' % stage)
    print('=' * (len(stage) + 14))
    print()

    cmd = STAGE_COMMANDS[stage]
    if cmd is not None:
        ret = cmd()
    else:
        print('WARNING: stage not yet implemented, skipping')
        ret = {}

    print()
    print('-' * (len(stage) + 15))
    print('Finished stage %s' % stage)
    print()

    return ret


#
# Test driver
#

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--list-stages', dest='list', action='store_true')
    parser.add_argument('--mainnet', action='store_true', help='Use mainnet instead of testnet')
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

    # Run the stages
    results = {}
    for s in args.stage:
        results.update(run_stage(s))

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
