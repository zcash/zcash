#!/usr/bin/env python3
#
# Execute all of the automated tests related to Zcash.
#

import argparse
from glob import glob
import os
import re
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

def get_arch_dir():
    depends_dir = os.path.join(REPOROOT, 'depends')

    arch_dir = os.path.join(depends_dir, 'x86_64-pc-linux-gnu')
    if os.path.isdir(arch_dir):
        return arch_dir

    # Not Linux, try MacOS
    arch_dirs = glob(os.path.join(depends_dir, 'x86_64-apple-darwin*'))
    if arch_dirs:
        # Just try the first one; there will only be one in CI
        return arch_dirs[0]

    print("!!! cannot find architecture dir under depends/ !!!")
    return None


#
# Custom test runners
#

RE_RPATH_RUNPATH = re.compile('No RPATH.*No RUNPATH')
RE_FORTIFY_AVAILABLE = re.compile('FORTIFY_SOURCE support available.*Yes')
RE_FORTIFY_USED = re.compile('Binary compiled with FORTIFY_SOURCE support.*Yes')

def test_rpath_runpath(filename):
    output = subprocess.check_output(
        [repofile('qa/zcash/checksec.sh'), '--file=' + repofile(filename)]
    )
    if RE_RPATH_RUNPATH.search(output.decode('utf-8')):
        print('PASS: %s has no RPATH or RUNPATH.' % filename)
        return True
    else:
        print('FAIL: %s has an RPATH or a RUNPATH.' % filename)
        print(output)
        return False

def test_fortify_source(filename):
    proc = subprocess.Popen(
        [repofile('qa/zcash/checksec.sh'), '--fortify-file=' + repofile(filename)],
        stdout=subprocess.PIPE,
    )
    line1 = proc.stdout.readline()
    line2 = proc.stdout.readline()
    proc.terminate()
    if RE_FORTIFY_AVAILABLE.search(line1.decode('utf-8')) and RE_FORTIFY_USED.search(line2.decode('utf-8')):
        print('PASS: %s has FORTIFY_SOURCE.' % filename)
        return True
    else:
        print('FAIL: %s is missing FORTIFY_SOURCE.' % filename)
        return False

def check_security_hardening():
    ret = True

    # PIE, RELRO, Canary, and NX are tested by make check-security.
    ret &= subprocess.call(['make', '-C', repofile('src'), 'check-security']) == 0

    # The remaining checks are only for ELF binaries
    # Assume that if zcashd is an ELF binary, they all are
    with open(repofile('src/zcashd'), 'rb') as f:
        magic = f.read(4)
        if not magic.startswith(b'\x7fELF'):
            return ret

    ret &= test_rpath_runpath('src/zcashd')
    ret &= test_rpath_runpath('src/zcash-cli')
    ret &= test_rpath_runpath('src/zcash-gtest')
    ret &= test_rpath_runpath('src/zcash-tx')
    ret &= test_rpath_runpath('src/test/test_bitcoin')

    # NOTE: checksec.sh does not reliably determine whether FORTIFY_SOURCE
    # is enabled for the entire binary. See issue #915.
    # FORTIFY_SOURCE does mostly nothing for Clang before 10, which we don't
    # pin yet, so we disable these tests.
    # ret &= test_fortify_source('src/zcashd')
    # ret &= test_fortify_source('src/zcash-cli')
    # ret &= test_fortify_source('src/zcash-gtest')
    # ret &= test_fortify_source('src/zcash-tx')
    # ret &= test_fortify_source('src/test/test_bitcoin')

    return ret

def ensure_no_dot_so_in_depends():
    arch_dir = get_arch_dir()
    if arch_dir is None:
        return False

    exit_code = 0

    if os.path.isdir(arch_dir):
        lib_dir = os.path.join(arch_dir, 'lib')
        libraries = os.listdir(lib_dir)

        for lib in libraries:
            if lib.find(".so") != -1:
                print(lib)
                exit_code = 1
    else:
        exit_code = 2
        print("arch-specific build dir not present")
        print("Did you build the ./depends tree?")
        print("Are you on a currently unsupported architecture?")

    if exit_code == 0:
        print("PASS.")
    else:
        print("FAIL.")

    return exit_code == 0

def util_test():
    python = []
    if os.path.isfile('/usr/local/bin/python3'):
        python = ['/usr/local/bin/python3']

    return subprocess.call(
        python + [repofile('src/test/bitcoin-util-test.py')],
        cwd=repofile('src'),
        env={'PYTHONPATH': repofile('src/test'), 'srcdir': repofile('src')}
    ) == 0

def rust_test():
    arch_dir = get_arch_dir()
    if arch_dir is None:
        return False

    rust_env = os.environ.copy()
    rust_env['RUSTC'] = os.path.join(arch_dir, 'native', 'bin', 'rustc')
    return subprocess.call([
        os.path.join(arch_dir, 'native', 'bin', 'cargo'),
        'test',
        '--manifest-path',
        os.path.join(REPOROOT, 'Cargo.toml'),
    ], env=rust_env) == 0

#
# Tests
#

STAGES = [
    'rust-test',
    'btest',
    'gtest',
    'sec-hard',
    'no-dot-so',
    'util-test',
    'secp256k1',
    'univalue',
    'rpc',
]

STAGE_COMMANDS = {
    'rust-test': rust_test,
    'btest': [repofile('src/test/test_bitcoin'), '-p'],
    'gtest': [repofile('src/zcash-gtest')],
    'sec-hard': check_security_hardening,
    'no-dot-so': ensure_no_dot_so_in_depends,
    'util-test': util_test,
    'secp256k1': ['make', '-C', repofile('src/secp256k1'), 'check'],
    'univalue': ['make', '-C', repofile('src/univalue'), 'check'],
    'rpc': [repofile('qa/pull-tester/rpc-tests.py'), '--nozmq', '-j16', '-m0', '-t1'],
    'rpc0': [repofile('qa/pull-tester/rpc-tests.py'), '--nozmq', '-j16', '-m0', '-t4'],
    'rpc1': [repofile('qa/pull-tester/rpc-tests.py'), '--nozmq', '-j16', '-m1', '-t4'],
    'rpc2': [repofile('qa/pull-tester/rpc-tests.py'), '--nozmq', '-j16', '-m2', '-t4'],
    'rpc3': [repofile('qa/pull-tester/rpc-tests.py'), '--nozmq', '-j16', '-m3', '-t4'],
}


#
# Test driver
#

def run_stage(stage):
    print('Running stage %s' % stage)
    print('=' * (len(stage) + 14))
    print()

    cmd = STAGE_COMMANDS[stage]
    if type(cmd) == type([]):
        ret = subprocess.call(cmd) == 0
    else:
        ret = cmd()

    print()
    print('-' * (len(stage) + 15))
    print('Finished stage %s' % stage)
    print()

    return ret

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--list-stages', dest='list', action='store_true')
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
    all_passed = True
    for s in args.stage:
        passed = run_stage(s)
        if not passed:
            print("!!! Stage %s failed !!!" % (s,))
        all_passed &= passed

    if not all_passed:
        print("!!! One or more test stages failed !!!")
        sys.exit(1)

if __name__ == '__main__':
    main()
