#!/usr/bin/env python3
import csv
import os
import subprocess

#
# Constants
#

MAX_BLOCK_SIZE = 2000000

#
# Verification times
#

def collect_times():
    # Find bench_bitcoin binary
    script_dir = os.path.dirname(os.path.realpath(__file__))
    base_dir = os.path.dirname(os.path.dirname(script_dir))
    bench_bitcoin = os.path.join(base_dir, 'src', 'bench', 'bench_bitcoin')

    # Run bench_bitcoin binary
    try:
        result = subprocess.run([bench_bitcoin], stdout=subprocess.PIPE, universal_newlines=True)
        result.check_returncode()
        result = result.stdout
    except AttributeError:
        # Use the older API
        result = subprocess.check_output([bench_bitcoin], universal_newlines=True)

    # Collect benchmarks
    benchmarks = {}
    for row in result.strip().split('\n')[1:]: # Skip the headings
        parts = row.split(',')
        benchmarks[parts[0]] = int(parts[2])

    return {
        'proof': benchmarks['SaplingOutput'],
        'ecdsa': benchmarks['ECDSA'],
        'redjubjub': benchmarks['SaplingSpend'] - benchmarks['SaplingOutput'],
        'ed25519': benchmarks['JoinSplitSig'],
    }

#
# Size calculations
#

def compact_size_size(nSize):
    if nSize < 253:
        return 1
    elif nSize <= 0xFFFF:
        return 3
    elif nSize <= 0xFFFFFFFF:
        return 5
    else:
        return 9

def script_size(script):
    return (
        compact_size_size(len(script)) +
        len(script)
    )

def tx_in_size(scriptSig):
    return (
        32 + # prevout.hash
        4  + # prevout.n
        script_size(scriptSig) +
        4    # nSequence
    )

def tx_out_size(scriptPubKey):
    return (
        8 + # nValue
        script_size(scriptPubKey)
    )

def v4_tx_size(vin, vout, nShieldedSpend, nShieldedOutput, nJoinSplit):
    return (
        4 + # header
        4 + # nVersionGroupId
        compact_size_size(len(vin)) +
        sum([tx_in_size(scriptSig) for scriptSig in vin]) +
        compact_size_size(len(vout)) +
        sum([tx_out_size(scriptPubKey) for scriptPubKey in vout]) +
        4 + # lock_time
        4 + # nExpiryHeight
        4 + # valueBalance
        compact_size_size(nShieldedSpend) +
        (384 * nShieldedSpend) +
        compact_size_size(nShieldedOutput) +
        (948 * nShieldedOutput) +
        compact_size_size(nJoinSplit) +
        (1698 * nJoinSplit) +
        ((32 + 64) if nJoinSplit > 0 else 0) + # joinSplitPubKey + joinSplitSig
        (64 if (nShieldedSpend + nShieldedOutput) > 0 else 0) # bindingSig
    )

def block_size(vtx):
    return (
        4  + # nVersion
        32 + # hashPrevBlock
        32 + # hashMerkleRoot
        32 + # hashFinalSaplingRoot
        4  + # nTime
        4  + # nBits
        32 + # nNonce
        compact_size_size(1344) + # solutionSize
        1344 + # solution
        compact_size_size(len(vtx)) +
        sum([v4_tx_size(**tx) for tx in vtx])
    )

#
# Runners
#

def worst_case_many_identical_txs(tx):
    vtx = []
    while True:
        vtx.append(tx)
        if block_size(vtx) > MAX_BLOCK_SIZE:
            # Keep under the size limit
            vtx.pop()
            break
    return vtx

def worst_case_one_tx_containing(item):
    vtx = [{
        'vin': [],
        'vout': [],
        'nShieldedSpend': 0,
        'nShieldedOutput': 0,
        'nJoinSplit': 0,
    }]
    while True:
        vtx[0][item] += 1
        if block_size(vtx) > MAX_BLOCK_SIZE:
            # Keep under the size limit
            vtx[0][item] -= 1
            break
    return vtx

def print_makeup(vtx, times):
    # One proof per Sapling spend, Sapling output, and JoinSplit
    proofs = sum([tx['nShieldedSpend'] + tx['nShieldedOutput'] + tx['nJoinSplit'] for tx in vtx])

    # One ECDSA signature per transparent input
    ecdsa_sigs = sum([len(tx['vin']) for tx in vtx])

    # One RedJubjub signature per Sapling spend (spendAuthSig) and per transaction (bindingSig)
    redjubjub_sigs = sum([tx['nShieldedSpend'] + (
        1 if tx['nShieldedSpend'] + tx['nShieldedOutput'] > 0 else 0) for tx in vtx])

    # One Ed25519 signature per transaction that contains JoinSplits
    ed25519_sigs = sum([1 if tx['nJoinSplit'] > 0 else 0 for tx in vtx])

    print('- Block size:          ', block_size(vtx), 'bytes')
    print('- Transactions:        ', len(vtx))
    print('- Proofs:              ', proofs)
    print('- ECDSA signatures:    ', ecdsa_sigs)
    print('- RedJubjub signatures:', redjubjub_sigs)
    print('- Ed25519 signatures:  ', ed25519_sigs)
    print('- Verification time:    %0.2f seconds' % (float(
        (times['proof'] * proofs) +
        (times['ecdsa'] * ecdsa_sigs) +
        (times['redjubjub'] * redjubjub_sigs) +
        (times['ed25519'] * ed25519_sigs)
    ) / 10**9))

#
# Worst-case scenarios
#

def worst_case_one_sapling_spend_per_tx(times):
    vtx = worst_case_many_identical_txs({
        'vin': [],
        'vout': [],
        'nShieldedSpend': 1,
        'nShieldedOutput': 0,
        'nJoinSplit': 0,
    })
    print('One Sapling spend per transaction:')
    print_makeup(vtx, times)
    print()

def worst_case_one_tx_containing_sapling_spends(times):
    vtx = worst_case_one_tx_containing('nShieldedSpend')
    print('One transaction containing Sapling spends:')
    print_makeup(vtx, times)
    print()

def worst_case_one_sapling_output_per_tx(times):
    vtx = worst_case_many_identical_txs({
        'vin': [],
        'vout': [],
        'nShieldedSpend': 0,
        'nShieldedOutput': 1,
        'nJoinSplit': 0,
    })
    print('One Sapling output per transaction:')
    print_makeup(vtx, times)
    print()

def worst_case_one_tx_containing_sapling_outputs(times):
    vtx = worst_case_one_tx_containing('nShieldedOutput')
    print('One transaction containing Sapling outputs:')
    print_makeup(vtx, times)
    print()

def worst_case_one_joinsplit_per_tx(times):
    vtx = worst_case_many_identical_txs({
        'vin': [],
        'vout': [],
        'nShieldedSpend': 0,
        'nShieldedOutput': 0,
        'nJoinSplit': 1,
    })
    print('One JoinSplit per transaction:')
    print_makeup(vtx, times)
    print()

def worst_case_one_tx_containing_joinsplits(times):
    vtx = worst_case_one_tx_containing('nJoinSplit')
    print('One transaction containing JoinSplits:')
    print_makeup(vtx, times)
    print()


def run():
    print('Collecting benchmarks...')
    times = collect_times()
    # times = hard_coded_times()
    print('Times (ns):', times)
    print()

    print('Running worst-case simulations...')
    worst_case_one_sapling_spend_per_tx(times)
    worst_case_one_tx_containing_sapling_spends(times)
    worst_case_one_sapling_output_per_tx(times)
    worst_case_one_tx_containing_sapling_outputs(times)
    worst_case_one_joinsplit_per_tx(times)
    worst_case_one_tx_containing_joinsplits(times)

if __name__ == '__main__':
    run()
