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
    initialize_basedir(opts.basedir)

    # Our convention is to build zc-flavored bitcoind with relative
    # .so paths, which is brittle and requires running it in the cwd that
    # contains it (ie as: './bitcoind'), so we change our whole process
    # working directory there:
    log.debug('chdir(%r)', opts.execdir)
    os.chdir(opts.execdir)

    # Explicitly specify relative directory in case '.' not in PATH:
    daemonexecutable = os.path.join('.', 'bitcoind')
    clientexecutable = os.path.join('.', 'bitcoin-cli')
    clientbaseopt = '-datadir=' + os.path.join(opts.basedir, '0')

    cliexec = partial(check_call, clientexecutable, clientbaseopt)
    cliout = partial(check_output, clientexecutable, clientbaseopt)

    with DaemonNodeProcesses(daemonexecutable, opts.basedir):
        sleep(2)
        cliexec('setgenerate', 'true', '200')

        addr = cliout('getnewaddress')

        for i in range(5):
            inputx = cliout('sendtoaddress', addr, '25')
            rawmint = cliout('zerocoinmint', '42', '2', inputx)
            signedtx = cliout('signrawtransaction', rawmint)
            tx = json.loads(signedtx)['hex']
            cliexec('sendrawtransaction', tx)

        [coin1, coin2] = parse_coins(opts.basedir, 2)

        cliexec('setgenerate', 'true', '1')
        cliexec('zerocoinpour', addr, 'true', coin1, coin2)


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

    opts = p.parse_args(args)

    logging.basicConfig(
        stream=sys.stdout,
        format='%(asctime)s %(levelname) 5s %(name)s | %(message)s',
        datefmt='%Y-%m-%dT%H:%M:%S%z',
        level=getattr(logging, opts.loglevel))

    log.debug('Parsed arguments: %r', opts)
    return opts


@curry_log
def initialize_basedir(log, basedir, nodecount=2, baseport=19000):
    create_new_dir_saving_existing_dir(basedir)

    # Algorithmically generate the config for each subdirectory:
    for n in range(nodecount):
        nodedir = os.path.join(basedir, str(n))
        ensure_dir_exists(nodedir)

        confpath = os.path.join(nodedir, 'bitcoin.conf')

        # Only overwrite config if non-existent to allow tinkering:
        if not os.path.exists(confpath):
            log.info('Writing: %r', confpath)
            with file(confpath, 'w') as f:
                k = (n+1) % nodecount # The next node's index.
                f.write(
                    ConfigTemplate.format(
                        PORT = baseport + (n*2),
                        RPCPORT = baseport + (n*2)+1,
                        PEERPORT = baseport + (k*2),
                        ))


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
def create_new_dir_saving_existing_dir(log, path):
    saved_path = path + ".saved"
    try:
        os.mkdir(path)
    except os.error as e:
        if e.errno == errno.EEXIST:
            # Directory exists, save it and create a new one.
            log.debug('Saving old directory at %r', path)
            # Only save one level of history.
            if os.path.exists(saved_path):
                shutil.rmtree(saved_path)
            shutil.move(path, saved_path)
            # Retry, now that we've moved the old one out of the way.
            create_new_dir_saving_existing_dir(path)
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
    def __init__(self, log, executable, basedir):
        self._log = log
        self._executable = executable
        self._basedir = basedir
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


def parse_coins(basedir, count):
    with file(os.path.join(basedir, '0', LOGNAME), 'r') as f:
        for line in f:
            m = CoinCommitmentLogRgx.match(line)
            if m:
                yield m.group(1)
                count -= 1
                if count == 0:
                    break


CoinCommitmentLogRgx = re.compile(r'^XXXX coin ([a-f0-9]{64}) :$')


ConfigTemplate = """
# testnet-box functionality
regtest=1
dnsseed=0
upnp=0
listen=1
connect=127.0.0.1:{PEERPORT}
keypool=10
# listen on different ports than default testnet
port={PORT}
rpcport={RPCPORT}

# always run a server, even with bitcoin-qt
server=1

# enable SSL for RPC server
#rpcssl=1

rpcuser=admin
rpcpassword=123
"""



if __name__ == '__main__':
    main()
