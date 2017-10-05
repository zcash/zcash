#!/usr/bin/env python2
#
# Execute all of the automated tests related to Zcash.
#

import argparse
import os
import subprocess
import sys

REPOROOT = os.path.dirname(
    os.path.dirname(
        os.path.dirname(
            os.path.abspath(__file__)
        )
    )
)

def repofile(filename):
    return os.path.join(REPOROOT, filename)


#
# Tests
#

STAGES = [
    'btest',
    'gtest',
    'sec-hard',
    'no-dot-so',
    'secp256k1',
    'univalue',
    'rpc',
]

STAGE_COMMANDS = {
    'btest': [repofile('src/test/test_bitcoin'), '-p'],
    'gtest': [repofile('src/zcash-gtest')],
    'sec-hard': [repofile('qa/zcash/check-security-hardening.sh')],
    'no-dot-so': [repofile('qa/zcash/ensure-no-dot-so-in-depends.py')],
    'secp256k1': ['make', '-C', repofile('src/secp256k1'), 'check'],
    'univalue': ['make', '-C', repofile('src/univalue'), 'check'],
    'rpc': [repofile('qa/pull-tester/rpc-tests.sh')],
}


#
# Test driver
#

def run_stage(stage):
    print('Running stage %s' % stage)
    print('=' * (len(stage) + 14))
    print

    ret = subprocess.call(STAGE_COMMANDS[stage])

    print
    print('-' * (len(stage) + 15))
    print('Finished stage %s' % stage)
    print

    return ret == 0

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('stage', nargs='*', default=STAGES,
                        help='One of %s'%STAGES)
    args = parser.parse_args()

    # Check validity of stages
    for s in args.stage:
        if s not in STAGES:
            print("Invalid stage '%s' (choose from %s)" % (s, STAGES))
            sys.exit(1)

    # Run the stages
    passed = True
    for s in args.stage:
        passed &= run_stage(s)

    if not passed:
        sys.exit(1)

if __name__ == '__main__':
    main()
