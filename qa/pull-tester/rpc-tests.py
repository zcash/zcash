#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2020-2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
"""
rpc-tests.py - run regression test suite

This module calls down into individual test cases via subprocess. It will
forward all unrecognized arguments onto the individual test scripts.

RPC tests are disabled on Windows by default. Use --force to run them anyway.

For a description of arguments recognized by test scripts, see
`qa/pull-tester/test_framework/test_framework.py:BitcoinTestFramework.main`.

"""

import argparse
import configparser
import os
import time
import shutil
import sys
import subprocess
import tempfile
import re

SERIAL_SCRIPTS = [
    # These tests involve enough shielded spends (consuming all CPU
    # cores) that we can't run them in parallel.
    'mergetoaddress_sapling.py',
    'mergetoaddress_ua_nu5.py',
    'mergetoaddress_ua_sapling.py',
    'wallet_shieldingcoinbase.py',
]

FLAKY_SCRIPTS = [
    # These tests have intermittent failures that we haven't diagnosed yet.
    'mempool_nu_activation.py', # this *may* be fixed
    'mempool_packages.py',
]

BASE_SCRIPTS= [
    # Longest test should go first, to favor running tests in parallel
    # vv Tests less than 5m vv
    'wallet.py',
    'sprout_sapling_migration.py',
    'remove_sprout_shielding.py',
    # vv Tests less than 2m vv
    'mergetoaddress_mixednotes.py',
    'wallet_shieldcoinbase_sapling.py',
    'wallet_shieldcoinbase_ua_sapling.py',
    'wallet_shieldcoinbase_ua_nu5.py',
    'turnstile.py',
    'walletbackup.py',
    'zkey_import_export.py',
    'prioritisetransaction.py',
    'wallet_changeaddresses.py',
    'wallet_listreceived.py',
    'mempool_tx_expiry.py',
    'finalsaplingroot.py',
    'finalorchardroot.py',
    'wallet_orchard.py',
    'wallet_overwintertx.py',
    'wallet_persistence.py',
    'wallet_listnotes.py',
    'wallet_listunspent.py',
    'wallet_golden_5_6_0.py',
    'wallet_tarnished_5_6_0.py',
    # vv Tests less than 60s vv
    'orchard_reorg.py',
    'fundrawtransaction.py',
    'reorg_limit.py',
    'mempool_limit.py',
    'p2p-fullblocktest.py',
    # vv Tests less than 30s vv
    'wallet_1941.py',
    'wallet_accounts.py',
    'wallet_addresses.py',
    'wallet_anchorfork.py',
    'wallet_changeindicator.py',
    'wallet_deprecation.py',
    'wallet_doublespend.py',
    'wallet_import_export.py',
    'wallet_isfromme.py',
    'wallet_orchard_change.py',
    'wallet_orchard_init.py',
    'wallet_orchard_persistence.py',
    'wallet_orchard_reindex.py',
    'wallet_nullifiers.py',
    'wallet_sapling.py',
    'wallet_sendmany_any_taddr.py',
    'wallet_treestate.py',
    'wallet_unified_change.py',
    'wallet_zip317_default.py',
    'listtransactions.py',
    'mempool_resurrect_test.py',
    'txn_doublespend.py',
    'txn_doublespend.py --mineblock',
    'getchaintips.py',
    'rawtransactions.py',
    'getrawtransaction_insight.py',
    'rest.py',
    'mempool_spendcoinbase.py',
    'mempool_reorg.py',
    'httpbasics.py',
    'multi_rpc.py',
    'zapwallettxes.py',
    'proxy_test.py',
    'merkle_blocks.py',
    'signrawtransactions.py',
    'signrawtransaction_offline.py',
    'key_import_export.py',
    'nodehandling.py',
    'reindex.py',
    'addressindex.py',
    'spentindex.py',
    'timestampindex.py',
    'decodescript.py',
    'blockchain.py',
    'disablewallet.py',
    'keypool.py',
    'getblocktemplate.py',
    'getmininginfo.py',
    'bip65-cltv-p2p.py',
    'bipdersig-p2p.py',
    'invalidblockrequest.py',
    'invalidtxrequest.py',
    'p2p_nu_peer_management.py',
    'rewind_index.py',
    'p2p_txexpiry_dos.py',
    'p2p_txexpiringsoon.py',
    'p2p_node_bloom.py',
    'regtest_signrawtransaction.py',
    'shorter_block_times.py',
    'mining_shielded_coinbase.py',
    'coinbase_funding_streams.py',
    'framework.py',
    'sapling_rewind_check.py',
    'feature_nsm.py',
    'feature_zip221.py',
    'feature_zip239.py',
    'feature_zip244_blockcommitments.py',
    'upgrade_golden.py',
    'nuparams.py',
    'post_heartwood_rollback.py',
    'feature_logging.py',
    'feature_walletfile.py',
    'wallet_parsing_amounts.py',
    'wallet_broadcast.py',
    'wallet_z_sendmany.py',
    'wallet_zero_value.py',
    'threeofthreerestore.py',
    'show_help.py',
    'errors.py',
    'converttex.py',
]

ZMQ_SCRIPTS = [
    # ZMQ test can only be run if bitcoin was built with zmq-enabled.
    # call rpc_tests.py with --nozmq to explicitly exclude these tests.
    "zmq_test.py"]

EXTENDED_SCRIPTS = [
    # These tests are not run by the travis build process.
    # Longest test should go first, to favor running tests in parallel
    'pruning.py',
    # vv Tests less than 5m vv
    # vv Tests less than 2m vv
    'getblocktemplate_longpoll.py',
    # vv Tests less than 60s vv
    'rpcbind_test.py',
    # vv Tests less than 30s vv
    'getblocktemplate_proposals.py',
    'forknotify.py',
    'hardforkdetection.py',
    'invalidateblock.py',
    'receivedby.py',
    'maxblocksinflight.py',
#    'forknotify.py',
    'p2p-acceptblock.py',
    'maxuploadtarget.py',
    'wallet_db_flush.py',
]

ALL_SCRIPTS = SERIAL_SCRIPTS + FLAKY_SCRIPTS + BASE_SCRIPTS + ZMQ_SCRIPTS + EXTENDED_SCRIPTS

def main():
    # Parse arguments and pass through unrecognised args
    parser = argparse.ArgumentParser(add_help=False,
                                     usage='%(prog)s [rpc-test.py options] [script options] [scripts]',
                                     description=__doc__,
                                     epilog='''
    Help text and arguments for individual test script:''',
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--coverage', action='store_true', help='generate a basic coverage report for the RPC interface')
    parser.add_argument('--deterministic', '-d', action='store_true', help='make the output a bit closer to deterministic in order to compare runs.')
    parser.add_argument('--exclude', '-x', help='specify a comma-separated-list of scripts to exclude. Do not include the .py extension in the name.')
    parser.add_argument('--extended', action='store_true', help='run the extended test suite in addition to the basic tests')
    parser.add_argument('--force', '-f', action='store_true', help='run tests even on platforms where they are disabled by default (e.g. windows).')
    parser.add_argument('--help', '-h', '-?', action='store_true', help='print help text and exit')
    parser.add_argument('--jobs', '-j', type=int, default=4, help='how many test scripts to run in parallel. Default=4.')
    parser.add_argument('--machines', '-m', type=int, default=-1, help='how many machines to shard the tests over. must also provide individual shard index. Default=-1 (no sharding).')
    parser.add_argument('--rpcgroup', '-r', type=int, default=-1, help='individual shard index. must also provide how many machines to shard the tests over. Default=-1 (no sharding).')
    parser.add_argument('--nozmq', action='store_true', help='do not run the zmq tests')
    args, unknown_args = parser.parse_known_args()

    # Create a set to store arguments and create the passon string
    tests = set(arg for arg in unknown_args if arg[:2] != "--")
    passon_args = [arg for arg in unknown_args if arg[:2] == "--"]

    # Read config generated by configure.
    config = configparser.ConfigParser()
    config.read_file(open(os.path.dirname(__file__) + "/tests_config.ini"))

    enable_wallet = config["components"].getboolean("ENABLE_WALLET")
    enable_utils = config["components"].getboolean("ENABLE_UTILS")
    enable_bitcoind = config["components"].getboolean("ENABLE_BITCOIND")
    enable_zmq = config["components"].getboolean("ENABLE_ZMQ") and not args.nozmq

    if config["environment"]["EXEEXT"] == ".exe" and not args.force:
        # https://github.com/bitcoin/bitcoin/commit/d52802551752140cf41f0d9a225a43e84404d3e9
        # https://github.com/bitcoin/bitcoin/pull/5677#issuecomment-136646964
        print("Tests currently disabled on Windows by default. Use --force option to enable")
        sys.exit(0)

    if not (enable_wallet and enable_utils and enable_bitcoind):
        print("No rpc tests to run. Wallet, utils, and bitcoind must all be enabled")
        print("Rerun `configure` with -enable-wallet, -with-utils and -with-daemon and rerun make")
        sys.exit(0)

    # python3-zmq may not be installed. Handle this gracefully and with some helpful info
    if enable_zmq:
        try:
            import zmq
            zmq # Silences pyflakes
        except ImportError:
            print("ERROR: \"import zmq\" failed. Use --nozmq to run without the ZMQ tests."
                  "To run zmq tests, see dependency info in /qa/README.md.")
            raise

    # Build list of tests
    if tests:
        # Individual tests have been specified. Run specified tests that exist
        # in the ALL_SCRIPTS list. Accept the name with or without .py extension.
        test_list = [t for t in ALL_SCRIPTS if
                (t in tests or re.sub(".py$", "", t) in tests)]

        print("Running individually selected tests: ")
        for t in test_list:
            print("\t" + t)
    else:
        # No individual tests have been specified. Run base tests, and
        # optionally ZMQ tests and extended tests.
        test_list = SERIAL_SCRIPTS + FLAKY_SCRIPTS + BASE_SCRIPTS
        if enable_zmq:
            test_list += ZMQ_SCRIPTS
        if args.extended:
            test_list += EXTENDED_SCRIPTS
            # TODO: BASE_SCRIPTS and EXTENDED_SCRIPTS are sorted by runtime
            # (for parallel running efficiency). This combined list will is no
            # longer sorted.

    # Remove the test cases that the user has explicitly asked to exclude.
    if args.exclude:
        for exclude_test in args.exclude.split(','):
            if exclude_test + ".py" in test_list:
                test_list.remove(exclude_test + ".py")

    if not test_list:
        print("No valid test scripts specified. Check that your test is in one "
              "of the test lists in rpc-tests.py, or run rpc-tests.py with no arguments to run all tests")
        sys.exit(0)

    if args.help:
        # Print help for rpc-tests.py, then print help of the first script and exit.
        parser.print_help()
        subprocess.check_call((config["environment"]["SRCDIR"] + '/qa/rpc-tests/' + test_list[0]).split() + ['-h'])
        sys.exit(0)


    if (args.rpcgroup == -1) != (args.machines == -1):
        print("ERROR: Please use both -m and -r options when using parallel rpc_groups.")
        sys.exit(0)
    if args.machines == 0:
        print("ERROR: -m/--machines must be greater than 0")
        sys.exit(0)
    if args.machines > 0 and (args.rpcgroup >= args.machines):
        print("ERROR: -r/--rpcgroup must be less than -m/--machines")
        sys.exit(0)
    if args.rpcgroup != -1 and args.machines != -1 and args.machines > args.rpcgroup:
        # Ceiling division using floor division, by inverting the world.
        # https://stackoverflow.com/a/17511341
        k = -(len(test_list) // -args.machines)
        split_list = list(test_list[i*k:(i+1)*k] for i in range(args.machines))
        tests_to_run = split_list[args.rpcgroup]
    else:
        tests_to_run = test_list
    all_passed = run_tests(
        RPCTestHandler,
        tests_to_run,
        config["environment"]["SRCDIR"],
        config["environment"]["BUILDDIR"],
        config["environment"]["EXEEXT"],
        args.jobs,
        args.coverage,
        args.deterministic,
        passon_args)
    sys.exit(not all_passed)

def run_tests(test_handler, test_list, src_dir, build_dir, exeext, jobs=1, enable_coverage=False, deterministic=False, args=[]):
    BOLD = ("","")
    if os.name == 'posix':
        # primitive formatting on supported
        # terminal via ANSI escape sequences:
        BOLD = ('\033[0m', '\033[1m')

    #Set env vars
    if "ZCASHD" not in os.environ:
        os.environ["ZCASHD"] = build_dir + '/src/zcashd' + exeext

    tests_dir = src_dir + '/qa/rpc-tests/'

    flags = ["--srcdir={}/src".format(build_dir)] + args
    flags.append("--cachedir=%s/qa/cache" % build_dir)

    if enable_coverage:
        coverage = RPCCoverage()
        flags.append(coverage.flag)
        print("Initializing coverage directory at %s\n" % coverage.dir)
    else:
        coverage = None

    if len(test_list) > 1 and jobs > 1:
        # Populate cache
        subprocess.check_output([tests_dir + 'create_cache.py'] + flags)

    #Run Tests
    time_sum = 0
    time0 = time.time()

    job_queue = test_handler(jobs, tests_dir, test_list, flags)

    max_len_name = len(max(test_list, key=len))
    total_count = 0
    passed_count = 0
    results = []
    try:
        for _ in range(len(test_list)):
            (name, stdout, stderr, passed, duration) = job_queue.get_next(deterministic)
            time_sum += duration

            print('\n' + BOLD[1] + name + BOLD[0] + ":")
            print('' if passed else stdout + '\n', end='')
            print('' if stderr == '' else 'stderr:\n' + stderr + '\n', end='')
            print("Pass: %s%s%s" % (BOLD[1], passed, BOLD[0]), end='')
            if deterministic:
                print("\n", end='')
            else:
                print(", Duration: %s s" % (duration,))
            total_count += 1
            if passed:
                passed_count += 1

            new_result = "%s | %s" % (name.ljust(max_len_name), str(passed).ljust(6))
            if not deterministic:
                new_result += (" | %s s" % (duration,))
            results.append(new_result)
    except (InterruptedError, KeyboardInterrupt):
        print('\nThe following tests were running when interrupted:')
        for j in job_queue.jobs:
            print("•", j[0])
        print('\n', end='')

    all_passed = passed_count == total_count

    if all_passed:
        success_rate = "True"
    else:
        success_rate = "%d/%d" % (passed_count, total_count)
    header = "%s | PASSED" % ("TEST".ljust(max_len_name),)
    footer = "%s | %s" % ("ALL".ljust(max_len_name), str(success_rate).ljust(6))
    if not deterministic:
        header += " | DURATION"
        footer += " | %s s (accumulated)\nRuntime: %s s" % (time_sum, int(time.time() - time0))
    print(
        BOLD[1] + header + BOLD[0] + "\n\n"
        + "\n".join(sorted(results)) + "\n"
        + BOLD[1] + footer + BOLD[0])

    if coverage:
        coverage.report_rpc_coverage()

        print("Cleaning up coverage data")
        coverage.cleanup()

    return all_passed

class RPCTestHandler:
    """
    Trigger the testscrips passed in via the list.
    """

    def __init__(self, num_tests_parallel, tests_dir, test_list=None, flags=None):
        assert(num_tests_parallel >= 1)
        self.num_jobs = num_tests_parallel
        self.tests_dir = tests_dir
        self.test_list = test_list
        self.flags = flags
        self.num_running = 0
        # In case there is a graveyard of zombie bitcoinds, we can apply a
        # pseudorandom offset to hopefully jump over them.
        # (625 is PORT_RANGE/MAX_NODES)
        self.portseed_offset = int(time.time() * 1000) % 625
        self.jobs = []

    def start_test(self, args, stdout, stderr):
        return subprocess.Popen(
            args,
            universal_newlines=True,
            stdout=stdout,
            stderr=stderr)

    def get_next(self, deterministic):
        while self.num_running < self.num_jobs and self.test_list:
            # Add tests
            self.num_running += 1
            t = self.test_list.pop(0)
            port_seed = ["--portseed={}".format(len(self.test_list) + self.portseed_offset)]
            log_stdout = tempfile.SpooledTemporaryFile(max_size=2**16)
            log_stderr = tempfile.SpooledTemporaryFile(max_size=2**16)
            self.jobs.append((t,
                              time.time(),
                              self.start_test((self.tests_dir + t).split() + self.flags + port_seed,
                                               log_stdout,
                                               log_stderr),
                              log_stdout,
                              log_stderr))
            # Run serial scripts on their own. We always run these first,
            # so we won't have added any other jobs yet.
            if t in SERIAL_SCRIPTS:
                break
        if not self.jobs:
            raise IndexError('pop from empty list')
        while True:
            # Return first proc that finishes
            time.sleep(.5)
            for j in self.jobs:
                (name, time0, proc, log_out, log_err) = j
                if proc.poll() is not None:
                    log_out.seek(0), log_err.seek(0)
                    [stdout, stderr] = [l.read().decode('utf-8') for l in (log_out, log_err)]
                    log_out.close(), log_err.close()
                    passed = stderr == "" and proc.returncode == 0
                    self.num_running -= 1
                    self.jobs.remove(j)
                    return name, stdout, stderr, passed, int(time.time() - time0)
            if not deterministic:
                print('.', end='', flush=True)


class RPCCoverage(object):
    """
    Coverage reporting utilities for pull-tester.

    Coverage calculation works by having each test script subprocess write
    coverage files into a particular directory. These files contain the RPC
    commands invoked during testing, as well as a complete listing of RPC
    commands per `bitcoin-cli help` (`rpc_interface.txt`).

    After all tests complete, the commands run are combined and diff'd against
    the complete list to calculate uncovered RPC commands.

    See also: qa/rpc-tests/test_framework/coverage.py

    """
    def __init__(self):
        self.dir = tempfile.mkdtemp(prefix="coverage")
        self.flag = '--coveragedir=%s' % self.dir

    def report_rpc_coverage(self):
        """
        Print out RPC commands that were unexercised by tests.

        """
        uncovered = self._get_uncovered_rpc_commands()

        if uncovered:
            print("Uncovered RPC commands:")
            print("".join(("  - %s\n" % i) for i in sorted(uncovered)))
        else:
            print("All RPC commands covered.")

    def cleanup(self):
        return shutil.rmtree(self.dir)

    def _get_uncovered_rpc_commands(self):
        """
        Return a set of currently untested RPC commands.

        """
        # This is shared from `qa/rpc-tests/test-framework/coverage.py`
        reference_filename = 'rpc_interface.txt'
        coverage_file_prefix = 'coverage.'

        coverage_ref_filename = os.path.join(self.dir, reference_filename)
        coverage_filenames = set()
        all_cmds = set()
        covered_cmds = set()

        if not os.path.isfile(coverage_ref_filename):
            raise RuntimeError("No coverage reference found")

        with open(coverage_ref_filename, 'r', encoding='utf8') as f:
            all_cmds.update([i.strip() for i in f.readlines()])

        for root, dirs, files in os.walk(self.dir):
            for filename in files:
                if filename.startswith(coverage_file_prefix):
                    coverage_filenames.add(os.path.join(root, filename))

        for filename in coverage_filenames:
            with open(filename, 'r', encoding='utf8') as f:
                covered_cmds.update([i.strip() for i in f.readlines()])

        return all_cmds - covered_cmds


if __name__ == '__main__':
    main()
