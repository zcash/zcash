#! /usr/bin/env python2

import os
import re
import sys
import errno
import argparse
import logging
import subprocess
import shutil
from functools import wraps, partial
from time import sleep
import json

DESCRIPTION = """
Run various system tests, which involve running multiple zerocash nodes and interacting with them.
"""

LOGNAME = 'stdout.log'


def curry_log(f):
    """A decorator to curry in a logger named after the function."""
    log = logging.getLogger(f.__name__)

    @wraps(f)
    def g(*args, **kw):
        return f(log, *args, **kw)
    return g


@curry_log
def main(log, args = sys.argv[1:]):

    if sys.version_info >= (3, 0):
        print("Sorry, you must run this script with Python 2.x, not Python 3.x.")
        print("To run this script with Python 2.x, do: python2 " + sys.argv[0])
        sys.exit(1)

    opts = parse_args(args)
    initialize_basedir(opts.basedir, opts.NODECONFIG, opts.pkpath, opts.vkpath)

    # The paths below are relative to the current directory, which is expected
    # to be the 'src/' directory containing zerocashd and zerocash-cli.
    log.debug('chdir(%r)', opts.execdir)
    os.chdir(opts.execdir)

    # Explicitly specify relative directory in case '.' not in PATH:
    daemonexecutable = os.path.join('.', 'zerocashd')
    clientexecutable = os.path.join('.', 'zerocash-cli')
    clientbaseopt = '-datadir=' + os.path.join(opts.basedir, '0')

    cliexec = partial(check_call, clientexecutable, clientbaseopt)
    cliout = partial(check_output, clientexecutable, clientbaseopt)

    with DaemonNodeProcesses(daemonexecutable, opts.basedir, opts.debugpause):
        for i in xrange(0, 100):
            sleep(1)
            try:
                cliexec('getwalletinfo')
                break
            except subprocess.CalledProcessError:
                # Wait some more then try again.
                pass

        # Ensure the wallet has been loaded by now.
        cliexec('getwalletinfo')

        cliexec('setgenerate', 'true', '200')

        change_addr_1 = cliout('getnewaddress')
        change_addr_2 = cliout('getnewaddress')

        unspent_txout = cliout('listunspent')
        unspent_txout_1 = json.loads(unspent_txout)[0]
        unspent_txout_2 = json.loads(unspent_txout)[1]

        zc_addr_1 = json.loads(cliout('zc-raw-keygen')) # {"zcaddress", "zcsecretkey"}
        zc_addr_2 = json.loads(cliout('zc-raw-keygen')) # {"zcaddress", "zcsecretkey"}

        change_txout_tmp_1 = {}
        change_txout_tmp_1[change_addr_1] = 25

        change_txout_tmp_2 = {}
        change_txout_tmp_2[change_addr_2] = 25

        raw_protect_tx_1 = cliout('createrawtransaction', json.dumps([
            {
                'txid': unspent_txout_1["txid"],
                'vout': unspent_txout_1["vout"]
            }
        ]), json.dumps(change_txout_tmp_1))

        raw_protect_tx_2 = cliout('createrawtransaction', json.dumps([
            {
                'txid': unspent_txout_2["txid"],
                'vout': unspent_txout_2["vout"]
            }
        ]), json.dumps(change_txout_tmp_2))

        unsigned_protect_1 = json.loads(cliout('zc-raw-protect', raw_protect_tx_1, zc_addr_1["zcaddress"], "20.0"))
        unsigned_protect_2 = json.loads(cliout('zc-raw-protect', raw_protect_tx_2, zc_addr_2["zcaddress"], "20.0"))
        
        signed_protect_1 = json.loads(cliout('signrawtransaction', unsigned_protect_1["rawtxn"]))['hex']
        signed_protect_2 = json.loads(cliout('signrawtransaction', unsigned_protect_2["rawtxn"]))['hex']

        cliout('sendrawtransaction', signed_protect_1)
        cliout('sendrawtransaction', signed_protect_2)

        cliexec('setgenerate', 'true', '5')

        zc_addr_pour_1 = json.loads(cliout('zc-raw-keygen'))
        zc_addr_pour_2 = json.loads(cliout('zc-raw-keygen'))

        vpub_addr = cliout('getnewaddress')
        vpub_secret = cliout('dumpprivkey', vpub_addr)

        pour = json.loads(cliout('zc-raw-pour',
                      zc_addr_1["zcsecretkey"],
                      unsigned_protect_1["bucketsecret"],
                      zc_addr_2["zcsecretkey"],
                      unsigned_protect_2["bucketsecret"],
                      zc_addr_pour_1["zcaddress"],
                      "10.0",
                      zc_addr_pour_2["zcaddress"],
                      "10.0",
                      vpub_secret,
                      "20.0"
                     ))

        cliout('sendrawtransaction', pour["rawtxn"])

        cliexec('setgenerate', 'true', '5')

        receive_1 = json.loads(cliout('zc-raw-receive',
                                      zc_addr_pour_1["zcsecretkey"],
                                      pour['encryptedbucket1']
                                   ))

        receive_2 = json.loads(cliout('zc-raw-receive',
                                      zc_addr_pour_2["zcsecretkey"],
                                      pour['encryptedbucket2']
                                   ))

        if (receive_1["amount"] != 1000000000) or (receive_2["amount"] != 1000000000):
            print("ERROR: Receive amount invalid!")
            sys.exit(1)

@curry_log
def parse_args(log, args):
    p = argparse.ArgumentParser(
        description=DESCRIPTION,
        formatter_class=argparse.RawTextHelpFormatter)

    p.add_argument('--log-level',
                   dest='loglevel',
                   default='INFO',
                   choices=['DEBUG', 'INFO', 'WARN', 'ERROR', 'CRITICAL'],
                   help='Set logging level.')

    basedirdefault = os.path.realpath('./.zc-system-test')
    p.add_argument('--basedir',
                   dest='basedir',
                   default=basedirdefault,
                   help=('Base directory for system test state and configuration. '
                         + 'Default: {0!r}'.format(basedirdefault)))

    execdirdefault = os.path.abspath(
        os.path.join(sys.argv[0], '..', '..', '..', 'src'))

    p.add_argument('--exec-dir',
                   dest='execdir',
                   default=execdirdefault,
                   help=('The bin dir for zerocash-flavored bitcoin executables. '
                         + 'Default: {0!r}'.format(execdirdefault)))

    p.add_argument('--dummy-pours',
                   dest='dummypours',
                   action='store_true',
                   default=False,
                   help='Skip Pour proving. Insert a dummy pad into Pour TxIns.')

    p.add_argument('--debug-pause',
                   dest='debugpause',
                   action='store_true',
                   default=False,
                   help='Pause after launching nodes to facilitate attaching gdb.')

    p.add_argument('--pk-path',
                   dest='pkpath',
                   required=True,
                   help='The path to the Pour proving key.')

    p.add_argument('--vk-path',
                   dest='vkpath',
                   required=True,
                   help='The path to the Pour verification key.')

    def node_config_arg(argstr):
        try:
            [key, value] = argstr.split('=',1)
        except ValueError as e:
            e.args += (argstr,)
            raise
        else:
            return (key, value)

    p.add_argument('NODECONFIG',
                   type=node_config_arg,
                   nargs='*',
                   help='Node config settings of the form KEY=VALUE.')

    opts = p.parse_args(args)

    logging.basicConfig(
        stream=sys.stdout,
        format='%(asctime)s %(levelname) 5s %(name)s | %(message)s',
        datefmt='%Y-%m-%dT%H:%M:%S%z',
        level=getattr(logging, opts.loglevel))

    log.debug('Parsed arguments: %r', opts)
    return opts


@curry_log
def initialize_basedir(log, basedir, clconfigs, pkpath, vkpath, nodecount=2, baseport=19000):
    create_new_dir_saving_existing_dir(basedir)

    # Algorithmically generate the config for each subdirectory:
    for n in range(nodecount):
        nodedir = os.path.join(basedir, str(n))
        ensure_dir_exists(nodedir)

        confpath = os.path.join(nodedir, 'bitcoin.conf')

        # Only overwrite config if non-existent to allow tinkering:
        if not os.path.exists(confpath):
            log.info('Writing: %r', confpath)

            k = (n+1) % nodecount # The next node's index.

            configvals = ConfigValues.copy()
            configvals.update({
                'connect': '127.0.0.1:' + str(baseport + (k*2)),
                'port': baseport + (n*2),
                'rpcport': baseport + (n*2)+1,
            })
            configvals.update(clconfigs)

            with file(confpath, 'w') as f:
                for (key, value) in sorted(configvals.items()):
                    f.write('{}={}\n'.format(key, value))

        # Add symlinks to the proving and verification keys.
        os.mkdir(os.path.join(nodedir, "regtest"))
        pk_link_path = os.path.join(nodedir, "regtest", "zc-testnet-alpha-proving.key")
        vk_link_path = os.path.join(nodedir, "regtest", "zc-testnet-alpha-verification.key")

        os.symlink(os.path.abspath(pkpath), pk_link_path)
        os.symlink(os.path.abspath(vkpath), vk_link_path)

@curry_log
def ensure_dir_exists(log, path):
    try:
        os.mkdir(path)
    except os.error as e:
        if e.errno == errno.EEXIST:
            pass # It already exists, postcondition fulfilled.
        else:
            raise # Some other error.
    else:
        log.debug('Created: %r', path)

@curry_log
def create_new_dir_saving_existing_dir(log, path, first_try = True):
    saved_path = path + ".saved"
    try:
        os.mkdir(path)
    except os.error as e:
        if first_try and e.errno == errno.EEXIST:
            # Directory exists, save it and create a new one.
            log.debug('Saving old directory at %r', path)
            # Only save one level of history.
            if os.path.exists(saved_path):
                shutil.rmtree(saved_path)
            shutil.move(path, saved_path)
            # Retry, now that we've moved the old one out of the way.
            create_new_dir_saving_existing_dir(path, False)
        else:
            # Something else went wrong.
            raise
    else:
        log.debug('Created %r', path)


@curry_log
def check_call(log, *args):
    log.info('Running %r', args)
    subprocess.check_call(args)


@curry_log
def check_output(log, *args):
    log.info('Gathering output from %r', args)
    output = subprocess.check_output(args)
    log.debug('Raw Output:\n%s<-- End of output', output)
    return output.rstrip()


@curry_log
class DaemonNodeProcesses (object):
    def __init__(self, log, executable, basedir, debugpause):
        self._log = log
        self._executable = executable
        self._basedir = basedir
        self._debugpause = debugpause
        self._procs = []

    def __enter__(self):
        for n in os.listdir(self._basedir):
            if not re.match(r'^[0-9]+$', n):
                continue
            dirpath = os.path.join(self._basedir, n)
            if not os.path.isdir(dirpath):
                continue

            args = [self._executable,
                    '-debug=zerocoin',
                    '-printtoconsole',
                    '-datadir=' + dirpath,
                    ]

            outf = file(os.path.join(dirpath, LOGNAME), 'w')
            proc = subprocess.Popen(args, stdout=outf, stderr=subprocess.STDOUT)
            self._log.info('Launched %r PID %r Log %r', args, proc.pid, outf.name)
            self._procs.append( (proc, outf) )

        if self._debugpause:
            print('*** blocking so you can attach gdb in another terminal ***')
            print('You probably want this command:\n')
            print('  gdb -p {}\n'.format(self._procs[0][0].pid))
            raw_input('press enter here after you have executed "continue" in gdb...')

    def __exit__(self, type, value, traceback):
        self._log.debug('Terminating subprocesses...')
        for (proc, _) in self._procs:
            proc.terminate()

        for (proc, logf) in self._procs:
            self._log.debug('Waiting for subprocess PID %r', proc.pid)
            s = proc.wait()
            self._log.info('PID %r %s', proc.pid, describe_process_status(s))
            logf.close()


def describe_process_status(status):
    if status < 0:
        return 'terminated by signal {!r}'.format(-status)
    else:
        return 'exited with status {!r}'.format(status)


def parse_coins(basedir, count, kind):
    assert kind in ('protect', 'pour'), repr(kind)

    with file(os.path.join(basedir, '0', LOGNAME), 'r') as f:
        for line in f:
            m = CoinCommitmentLogRgx.match(line)
            if m and m.group(1) == kind:
                yield m.group(2)
                count -= 1
                if count == 0:
                    break


CoinCommitmentLogRgx = re.compile(
    r'^XXXX (pour|protect) commitment ([a-f0-9]{64})$'
)


ConfigValues = {
    # disable fee requirements, since fees do not yet work for Pours:
    'relaypriority': 0,

    # testnet-box functionality:
    'regtest': '1',
    'dnsseed': '0',
    'upnp': '0',
    'listen': '1',
    'connect': '127.0.0.1:{PEERPORT}',
    'keypool': '10',

    # listen on different ports than default testnet
    'port': '{PORT}',
    'rpcport': '{RPCPORT}',

    # always run a server, even with bitcoin-qt
    'server': '1',

    'rpcuser': 'admin',
    'rpcpassword': '123',
}



if __name__ == '__main__':
    main()
