#!/usr/bin/env python3
#
# Execute the standard smoke tests for Zcash releases.
#

import argparse
import os
import subprocess
import sys
import time

from decimal import Decimal
from slickrpc import Proxy
from slickrpc.exc import RpcException

DEFAULT_FEE = Decimal('0.0001')

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
# Test helpers
#

def run_cmd(results, case, zcash, name, args=[]):
    print('-----')
    print('%s $ zcash-cli %s %s' % (
        case.ljust(3),
        name,
        ' '.join([str(arg) for arg in args],
    )))
    try:
        res = zcash.__getattr__(name)(*args)
        print(res)
        print()
        if results is not None and len(case) > 0:
            results[case] = True
        return res
    except RpcException as e:
        print('ERROR: %s' % str(e))
        if results is not None and len(case) > 0:
            results[case] = False
        return None

def wait_for_balance(zcash, zaddr, expected=None, timeout=900):
    print('Waiting for funds to %s...' % zaddr)
    unconfirmed_balance = Decimal(zcash.z_getbalance(zaddr, 0)).quantize(Decimal('1.00000000'))
    print('Expecting: %s; Unconfirmed Balance: %s' % (expected, unconfirmed_balance))
    if expected is not None and unconfirmed_balance != expected:
        print('WARNING: Unconfirmed balance does not match expected balance')

    ttl = timeout
    while True:
        balance = Decimal(zcash.z_getbalance(zaddr)).quantize(Decimal('1.00000000'))
        if (expected is not None and balance == unconfirmed_balance) or (expected is None and balance > 0):
            print('Received %s' % balance)
            return balance

        time.sleep(1)
        ttl -= 1
        if ttl == 0:
            # Ask user if they want to keep waiting
            print()
            ret = input('Do you wish to continue waiting? (Y/n) ')
            if ret.to_lower() == 'n':
                print('Address contained %s at timeout' % balance)
                return balance
            else:
                ttl = 300

def wait_and_check_balance(results, case, zcash, addr, expected):
    balance = wait_for_balance(zcash, addr, expected)
    if balance != expected and results is not None and len(case) > 0:
        results[case] = False
    return balance

def wait_for_txid_operation(zcash, opid, timeout=300):
    print('Waiting for async operation %s' % opid)
    result = None
    for _ in iter(range(timeout)):
        results = zcash.z_getoperationresult([opid])
        if len(results) > 0:
            result = results[0]
            break
        time.sleep(1)

    status = result['status']
    if status == 'failed':
        print('Operation failed')
        print(result['error']['message'])
        return None
    elif status == 'success':
        txid = result['result']['txid']
        print(txid)
        return txid

def async_txid_cmd(results, case, zcash, name, args=[]):
    opid = run_cmd(results, case, zcash, name, args)
    # Some async commands return a dictionary containing the opid
    if type(opid) == type({}):
        opid = opid['opid']
    if opid is None:
        if results is not None and len(case) > 0:
            results[case] = False
        return None

    txid = wait_for_txid_operation(zcash, opid)
    if txid is None:
        if results is not None and len(case) > 0:
            results[case] = False
    return txid

def z_sendmany(results, case, zcash, from_addr, recipients):
    return async_txid_cmd(results, case, zcash, 'z_sendmany', [
        from_addr,
        [{
            'address': to_addr,
            'amount': amount,
        } for (to_addr, amount) in recipients]
    ])

def check_z_sendmany(results, case, zcash, from_addr, recipients):
    txid = z_sendmany(results, case, zcash, from_addr, recipients)
    if txid is None:
        return [Decimal('0')]
    return [wait_and_check_balance(results, case, zcash, to_addr, amount) for (to_addr, amount) in recipients]

def check_z_sendmany_parallel(results, zcash, runs):
    # First attempt to create all the transactions
    txids = [(run, z_sendmany(results, run[0], zcash, run[1], run[2])) for run in runs]
    # Then wait for balance updates caused by successful transactions
    return [
        wait_and_check_balance(results, run[0], zcash, to_addr, amount) if txid is not None else Decimal('0')
        for (run, txid) in txids
        for (to_addr, amount) in run[2]]

def z_mergetoaddress(results, case, zcash, from_addrs, to_addr):
    return async_txid_cmd(results, case, zcash, 'z_mergetoaddress', [from_addrs, to_addr])

def check_z_mergetoaddress(results, case, zcash, from_addrs, to_addr, amount):
    txid = z_mergetoaddress(results, case, zcash, from_addrs, to_addr)
    if txid is None:
        return Decimal('0')
    return wait_and_check_balance(results, case, zcash, to_addr, amount)

def check_z_mergetoaddress_parallel(results, zcash, runs):
    # First attempt to create all the transactions
    txids = [(run, z_mergetoaddress(results, run[0], zcash, run[1], run[2])) for run in runs]
    # Then wait for balance updates caused by successful transactions
    return [
        wait_and_check_balance(results, run[0], zcash, run[2], run[3]) if txid is not None else Decimal('0')
        for (run, txid) in txids]


#
# Test runners
#

def simple_commands(zcash):
    results = {}
    run_cmd(results, '2a', zcash, 'getinfo'),
    run_cmd(results, '2b', zcash, 'help'),
    run_cmd(results, '5o', zcash, 'getwalletinfo'),
    run_cmd(results, '6a', zcash, 'getpeerinfo'),
    run_cmd(results, '6b', zcash, 'getnetworkinfo'),
    run_cmd(results, '6c', zcash, 'getdeprecationinfo'),
    run_cmd(results, '6d', zcash, 'getconnectioncount'),
    run_cmd(results, '6e', zcash, 'getaddednodeinfo', [False]),
    run_cmd(results, '7a', zcash, 'getblocksubsidy'),
    run_cmd(results, '7c', zcash, 'getmininginfo'),
    run_cmd(results, '7d', zcash, 'getnetworkhashps'),
    run_cmd(results, '7e', zcash, 'getnetworksolps'),
    return results

def transaction_chain(zcash):
    results = {}

    # Generate the various addresses we will use
    sprout_zaddr_1 = run_cmd(results, '3a', zcash, 'z_getnewaddress', ['sprout'])
    sprout_zaddr_2 = run_cmd(results, '3b', zcash, 'z_getnewaddress', ['sprout'])
    sprout_zaddr_3 = run_cmd(results, '3b', zcash, 'z_getnewaddress', ['sprout'])
    taddr_1 = run_cmd(results, '3c', zcash, 'getnewaddress')
    taddr_2 = run_cmd(results, '3d', zcash, 'getnewaddress')
    taddr_3 = run_cmd(results, '3d', zcash, 'getnewaddress')
    taddr_4 = run_cmd(results, '3d', zcash, 'getnewaddress')
    taddr_5 = run_cmd(results, '3d', zcash, 'getnewaddress')
    sapling_zaddr_1 = run_cmd(results, '3e', zcash, 'z_getnewaddress', ['sapling'])
    sapling_zaddr_2 = run_cmd(results, '3f', zcash, 'z_getnewaddress', ['sapling'])
    sapling_zaddr_3 = run_cmd(results, '3f', zcash, 'z_getnewaddress', ['sapling'])

    # Check that the zaddrs are all listed
    zaddrs = run_cmd(results, '5a', zcash, 'z_listaddresses')
    if (sprout_zaddr_1 not in zaddrs or
        sprout_zaddr_2 not in zaddrs or
        sapling_zaddr_1 not in zaddrs or
        sapling_zaddr_2 not in zaddrs):
        results['5a'] = False

    # Validate the addresses
    ret = run_cmd(results, '5b', zcash, 'z_validateaddress', [sprout_zaddr_1])
    if not ret['isvalid'] or ret['type'] != 'sprout':
        results['5b'] = False

    ret = run_cmd(results, '5c', zcash, 'z_validateaddress', [sapling_zaddr_1])
    if not ret['isvalid'] or ret['type'] != 'sapling':
        results['5c'] = False

    # Set up beginning and end of the chain
    print('#')
    print('# Initialising transaction chain')
    print('#')
    print()
    chain_end = input('Type or paste transparent address where leftover funds should be sent: ')
    if not zcash.validateaddress(chain_end)['isvalid']:
        print('Invalid transparent address')
        return results
    print()
    print('Please send at least 0.01 ZEC/TAZ to the following address:')
    print(sprout_zaddr_1)
    print()
    input('Press any key once the funds have been sent.')
    print()

    # Wait to receive starting balance
    sprout_balance = wait_for_balance(zcash, sprout_zaddr_1)
    starting_balance = sprout_balance

    #
    # Start the transaction chain!
    #
    print()
    print('#')
    print('# Starting transaction chain')
    print('#')
    print()
    try:
        #
        # First, split the funds across all three pools
        #

        # Sprout -> taddr
        taddr_balance = check_z_sendmany(
            results, '4c', zcash, sprout_zaddr_1, [(taddr_1, (starting_balance / Decimal('10')) * Decimal('6'))])[0]
        sprout_balance -= taddr_balance + DEFAULT_FEE

        balance = Decimal(run_cmd(results, '5f', zcash, 'z_getbalance', [sprout_zaddr_1])).quantize(Decimal('1.00000000'))
        if balance != sprout_balance:
            results['5f'] = False

        # taddr -> Sapling
        # Send it all here because z_sendmany pick a new t-addr for change
        sapling_balance = check_z_sendmany(
            results, '4f', zcash, taddr_1, [(sapling_zaddr_1, taddr_balance - DEFAULT_FEE)])[0]
        taddr_balance = Decimal('0')

        # Sapling -> taddr
        taddr_balance = check_z_sendmany(
            results, '4i', zcash, sapling_zaddr_1, [(taddr_1, (starting_balance / Decimal('10')) * Decimal('3'))])[0]
        sapling_balance -= taddr_balance + DEFAULT_FEE

        #
        # Intra-pool tests
        #

        # Sprout -> same Sprout
        # Sapling -> same Sapling
        (sprout_balance, sapling_balance) = check_z_sendmany_parallel(results, zcash, [
            ('4a', sprout_zaddr_1, [(sprout_zaddr_1, sprout_balance - DEFAULT_FEE)]),
            ('4g', sapling_zaddr_1, [(sapling_zaddr_1, sapling_balance - DEFAULT_FEE)]),
        ])

        # Sprout -> different Sprout
        # taddr -> different taddr
        # Sapling -> different Sapling
        (sprout_balance, taddr_balance, sapling_balance) = check_z_sendmany_parallel(results, zcash, [
            ('4b', sprout_zaddr_1, [(sprout_zaddr_2, sprout_balance - DEFAULT_FEE)]),
            ('4e', taddr_1, [(taddr_2, taddr_balance - DEFAULT_FEE)]),
            ('4h', sapling_zaddr_1, [(sapling_zaddr_2, sapling_balance - DEFAULT_FEE)]),
        ])

        # Sprout -> multiple Sprout
        # taddr -> multiple taddr
        # Sapling -> multiple Sapling
        check_z_sendmany_parallel(results, zcash, [
            ('4k', sprout_zaddr_2, [
                (sprout_zaddr_1, starting_balance / Decimal('10')),
                (sprout_zaddr_3, starting_balance / Decimal('10')),
            ]),
            ('4p', taddr_2, [
                (taddr_1, starting_balance / Decimal('10')),
                (taddr_3, taddr_balance - (starting_balance / Decimal('10')) - DEFAULT_FEE),
            ]),
            ('4t', sapling_zaddr_2, [
                (sapling_zaddr_1, starting_balance / Decimal('10')),
                (sapling_zaddr_3, starting_balance / Decimal('10')),
            ]),
        ])
        sprout_balance -= DEFAULT_FEE
        taddr_balance -= DEFAULT_FEE
        sapling_balance -= DEFAULT_FEE

        # multiple Sprout -> Sprout
        # multiple Sapling -> Sapling
        # multiple taddr -> taddr
        check_z_mergetoaddress_parallel(results, zcash, [
            ('4ff', [sprout_zaddr_1, sprout_zaddr_3], sprout_zaddr_2, sprout_balance - DEFAULT_FEE),
            ('4gg', [sapling_zaddr_1, sapling_zaddr_3], sapling_zaddr_2, sapling_balance - DEFAULT_FEE),
            ('', [taddr_1, taddr_3], taddr_2, taddr_balance - DEFAULT_FEE),
        ])
        sprout_balance -= DEFAULT_FEE
        sapling_balance -= DEFAULT_FEE
        taddr_balance -= DEFAULT_FEE

        #
        # Now test a bunch of failing cases
        #

        # Sprout -> Sapling
        txid = z_sendmany(results, '4j', zcash, sprout_zaddr_2, [(sapling_zaddr_1, sprout_balance - DEFAULT_FEE)])
        if txid is not None:
            print('Should have failed')
            return results

        # Sprout -> taddr and Sapling
        txid = z_sendmany(results, '4n', zcash, sprout_zaddr_2, [
            (taddr_2, starting_balance / Decimal('10')),
            (sapling_zaddr_1, starting_balance / Decimal('10')),
        ])
        if txid is not None:
            print('Should have failed')
            return results

        # Sprout -> multiple Sapling
        txid = z_sendmany(results, '4o', zcash, sprout_zaddr_2, [
            (sapling_zaddr_1, starting_balance / Decimal('10')),
            (sapling_zaddr_2, starting_balance / Decimal('10')),
        ])
        if txid is not None:
            print('Should have failed')
            return results

        # taddr -> Sprout and Sapling
        txid = z_sendmany(results, '4s', zcash, taddr_2, [
            (sprout_zaddr_1, starting_balance / Decimal('10')),
            (sapling_zaddr_1, starting_balance / Decimal('10')),
        ])
        if txid is not None:
            print('Should have failed')
            return results

        # Sapling -> multiple Sprout
        txid = z_sendmany(results, '4u', zcash, sapling_zaddr_2, [
            (sprout_zaddr_1, starting_balance / Decimal('10')),
            (sprout_zaddr_2, starting_balance / Decimal('10')),
        ])
        if txid is not None:
            print('Should have failed')
            return results

        # Sapling -> Sapling and Sprout
        txid = z_sendmany(results, '4x', zcash, sapling_zaddr_2, [
            (sapling_zaddr_1, starting_balance / Decimal('10')),
            (sprout_zaddr_1, starting_balance / Decimal('10')),
        ])
        if txid is not None:
            print('Should have failed')
            return results

        # multiple same taddr -> Sprout
        txid = z_mergetoaddress(results, '4aa', zcash, [taddr_2, taddr_2], sprout_zaddr_2)
        if txid is not None:
            print('Should have failed')
            return results

        #
        # Inter-pool tests
        #

        # Sprout -> taddr and Sprout
        # Sapling -> taddr and Sapling
        check_z_sendmany_parallel(results, zcash, [
            ('4m', sprout_zaddr_2, [
                (taddr_1, starting_balance / Decimal('10')),
                (sprout_zaddr_1, starting_balance / Decimal('10')),
            ]),
            ('4w', sapling_zaddr_2, [
                (taddr_3, starting_balance / Decimal('10')),
                (sapling_zaddr_1, starting_balance / Decimal('10')),
            ]),
        ])
        sprout_balance -= (starting_balance / Decimal('10')) + DEFAULT_FEE
        sapling_balance -= (starting_balance / Decimal('10')) + DEFAULT_FEE
        taddr_balance += (starting_balance / Decimal('10')) * Decimal('2')

        # taddr and Sprout -> Sprout
        # taddr and Sapling -> Sapling
        check_z_mergetoaddress_parallel(results, zcash, [
            ('4dd', [taddr_1, sprout_zaddr_1], sprout_zaddr_2, sprout_balance + (starting_balance / Decimal('10')) - DEFAULT_FEE),
            ('4ee', [taddr_3, sapling_zaddr_1], sapling_zaddr_2, sapling_balance + (starting_balance / Decimal('10')) - DEFAULT_FEE),
        ])
        sprout_balance += (starting_balance / Decimal('10')) - DEFAULT_FEE
        sapling_balance += (starting_balance / Decimal('10')) - DEFAULT_FEE
        taddr_balance -= (starting_balance / Decimal('10')) * Decimal('2')

        # Sprout -> multiple taddr
        # Sapling -> multiple taddr
        check_z_sendmany_parallel(results, zcash, [
            ('4l', sprout_zaddr_2, [
                (taddr_1, (starting_balance / Decimal('10'))),
                (taddr_3, (starting_balance / Decimal('10'))),
            ]),
            ('4v', sapling_zaddr_2, [
                (taddr_4, (starting_balance / Decimal('10'))),
                (taddr_5, (starting_balance / Decimal('10'))),
            ]),
        ])
        sprout_balance -= ((starting_balance / Decimal('10')) * Decimal('2')) + DEFAULT_FEE
        sapling_balance -= ((starting_balance / Decimal('10')) * Decimal('2')) + DEFAULT_FEE
        taddr_balance += (starting_balance / Decimal('10')) * Decimal('4')

        # multiple taddr -> Sprout
        # multiple taddr -> Sapling
        check_z_mergetoaddress_parallel(results, zcash, [
            ('4z', [taddr_1, taddr_3], sprout_zaddr_2, sprout_balance + ((starting_balance / Decimal('10')) * Decimal('2')) - DEFAULT_FEE),
            ('4bb', [taddr_4, taddr_5], sapling_zaddr_2, sapling_balance + ((starting_balance / Decimal('10')) * Decimal('2')) - DEFAULT_FEE),
        ])
        sprout_balance += ((starting_balance / Decimal('10')) * Decimal('2')) - DEFAULT_FEE
        sapling_balance += ((starting_balance / Decimal('10')) * Decimal('2')) - DEFAULT_FEE
        taddr_balance -= (starting_balance / Decimal('10')) * Decimal('4')

        # taddr -> Sprout
        check_z_sendmany_parallel(results, zcash, [
            ('4d', taddr_2, [(sprout_zaddr_3, taddr_balance - DEFAULT_FEE)]),
        ])
        sprout_balance += taddr_balance - DEFAULT_FEE
        taddr_balance = Decimal('0')

        # multiple Sprout -> taddr
        # multiple Sapling -> taddr
        check_z_mergetoaddress_parallel(None, zcash, [
            ('', [sprout_zaddr_1, sprout_zaddr_2, sprout_zaddr_3], taddr_1, sprout_balance - DEFAULT_FEE),
            ('', [sapling_zaddr_1, sapling_zaddr_2, sapling_zaddr_3], taddr_2, sapling_balance - DEFAULT_FEE),
        ])
        taddr_balance = sprout_balance + sapling_balance - (2 * DEFAULT_FEE)
        sprout_balance = Decimal('0')
        sapling_balance = Decimal('0')

        # taddr -> multiple Sprout
        # taddr -> multiple Sapling
        taddr_1_balance = Decimal(zcash.z_getbalance(taddr_1)).quantize(Decimal('1.00000000'))
        taddr_2_balance = Decimal(zcash.z_getbalance(taddr_2)).quantize(Decimal('1.00000000'))
        check_z_sendmany_parallel(results, zcash, [
            ('4q', taddr_1, [
                (sprout_zaddr_1, (starting_balance / Decimal('10'))),
                (sprout_zaddr_2, taddr_1_balance - (starting_balance / Decimal('10')) - DEFAULT_FEE),
            ]),
            ('4r', taddr_2, [
                (sapling_zaddr_1, (starting_balance / Decimal('10'))),
                (sapling_zaddr_2, taddr_2_balance - (starting_balance / Decimal('10')) - DEFAULT_FEE),
            ]),
        ])
        sprout_balance = taddr_1_balance - DEFAULT_FEE
        sapling_balance = taddr_2_balance - DEFAULT_FEE
        taddr_balance = Decimal('0')

        # multiple Sprout -> taddr
        # multiple Sapling -> taddr
        check_z_mergetoaddress_parallel(None, zcash, [
            ('', [sprout_zaddr_1, sprout_zaddr_2], taddr_1, sprout_balance - DEFAULT_FEE),
            ('', [sapling_zaddr_1, sapling_zaddr_2], taddr_2, sapling_balance - DEFAULT_FEE),
        ])
        taddr_balance = sprout_balance + sapling_balance - (Decimal('2') * DEFAULT_FEE)
        sprout_balance = Decimal('0')
        sapling_balance = Decimal('0')

        # z_mergetoaddress taddr -> Sprout
        # z_mergetoaddress taddr -> Sapling
        taddr_1_balance = Decimal(zcash.z_getbalance(taddr_1)).quantize(Decimal('1.00000000'))
        taddr_2_balance = Decimal(zcash.z_getbalance(taddr_2)).quantize(Decimal('1.00000000'))
        check_z_mergetoaddress_parallel(results, zcash, [
            ('4y', [taddr_1], sprout_zaddr_1, taddr_1_balance - DEFAULT_FEE),
            ('4cc', [taddr_2], sapling_zaddr_1, taddr_2_balance - DEFAULT_FEE),
        ])
        sprout_balance = taddr_1_balance - DEFAULT_FEE
        sapling_balance = taddr_2_balance - DEFAULT_FEE
        taddr_balance = Decimal('0')
    finally:
        #
        # End the chain by returning the remaining funds
        #
        print()
        print('#')
        print('# Finishing transaction chain')
        print('#')
        all_addrs = [
            sprout_zaddr_1, sprout_zaddr_2, sprout_zaddr_3,
            taddr_1, taddr_2, taddr_3, taddr_4, taddr_5,
            sapling_zaddr_1, sapling_zaddr_2, sapling_zaddr_3,
        ]

        print()
        print('Waiting for all transactions to be mined')
        for addr in all_addrs:
            balance = Decimal(zcash.z_getbalance(addr, 0)).quantize(Decimal('1.00000000'))
            if balance > 0:
                wait_for_balance(zcash, addr, balance)

        print()
        print('Returning remaining balance minus fees')
        for addr in all_addrs:
            balance = Decimal(zcash.z_getbalance(addr)).quantize(Decimal('1.00000000'))
            if balance > 0:
                z_sendmany(None, '', zcash, addr, [(chain_end, balance - DEFAULT_FEE)])

    return results


#
# Test stages
#

STAGES = [
    'simple-commands',
    'transaction-chain'
]

STAGE_COMMANDS = {
    'simple-commands': simple_commands,
    'transaction-chain': transaction_chain,
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
            self.__proxy = Proxy(url, timeout=timewait)
        else:
            self.__proxy = Proxy(url)

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

    # Don't allow using the default wallet.dat in mainnet mode
    if args.mainnet and args.wallet == 'wallet.dat':
        print('Cannot use wallet.dat as wallet file when running mainnet tests. Keep your funds safe!')
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
