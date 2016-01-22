#!/usr/bin/env python

"""fetch-params.py - fetch Zcash public alpha keys

Usage: python fetch-params.py
"""


from __future__ import print_function

import hashlib
import os
import subprocess
import sys


urls = [
    'https://z.cash/downloads/zc-testnet-public-alpha-proving.key',
    'https://z.cash/downloads/zc-testnet-public-alpha-verification.key',
]

sizes = [
    2007057094,
    3478,
]

hash_algo = hashlib.sha256

hashes = [
    '7844a96933979158886a5b69fb163f49de76120fa1dcfc33b16c83c134e61817',
    '6902fd687bface72e572a7cda57f6da5a0c606c7b9769f30becd255e57924f41',
]

params_readme = """
This directory stores common zcash zkSNARK parameters. Note that is is
distinct from the daemon's -datadir argument because the parameters are
large and may be shared across multiple distinct -datadir's such as when
setting up test networks.
"""

fetch_params_info = """
This script will fetch the Zcash zkSNARK parameters and verify their
integrity with sha256sum.

The parameters currently are about 2GiB in size, so plan accordingly
for your bandwidth constraints. If the files are already present and
have the correct sha256sum, no networking is used.
"""


def which(name):
    p = subprocess.Popen(
        ['which', name],
        stdout=subprocess.PIPE,
        universal_newlines=True,
    )
    p.wait()
    if not p.returncode == 0:
        return None
    out = p.stdout.read()
    return out.rstrip('\n')


# Note: This assumes cwd is set appropriately!
def fetch(url):
    cmd = which('curl') or which('wget') or None
    if cmd is None:
        raise OSError('curl or wget is required')
    options = {
        'curl': [
            '--remote-name',
            '--insecure',
            '--continue-at', '-',
            '--location',
        ],
        'wget': [
            '--progress=dot:giga',
            # Note: --no-check-certificate should be ok, since we rely
            # on sha256 for integrity, and there's no confidentiality
            # requirement.  Our website uses letsencrypt certificates
            # which are not supported by some wget installations,
            # so we expect some cert failures.
            '--no-check-certificate',
        ],
    }
    opt = options[os.path.basename(cmd)]
    ret = subprocess.call([cmd, url] + opt)
    if not ret == 0:
        raise OSError('%s failed with status %i' % (cmd, ret))


def verify(filename, digest):
    hash = hash_algo()
    with open(filename, 'rb') as f:
        while True:
            dat = f.read(hash.block_size * 1024)
            if len(dat) == 0:
                break
            hash.update(dat)
    return hash.hexdigest() == digest


if __name__ == '__main__':

    if len(sys.argv) == 1:
        pass
    elif len(sys.argv) == 2 and sys.argv[1] in ('-h', '--help'):
        print(__doc__)
        sys.exit(0)
    else:
        print(__doc__)
        sys.exit(1)

    home = os.environ['HOME']

    # This may be the first time the user's run this script, so give
    # them some info, especially about bandwidth usage:
    print(fetch_params_info)

    # Now create params_dir and insert a README if necessary:
    print('Creating params directory...')
    params_dir = os.path.join(home, '.zcash-params')
    try:
        os.mkdir(params_dir)
    except OSError:
        pass
    params_readme_path = os.path.join(params_dir, 'README')
    if not os.path.exists(params_readme_path):
        with open(params_readme_path, 'w+') as f:
            print(params_readme, file=f)
    print('For details about this directory, see:', params_readme_path)

    regtest_dir = os.path.join(params_dir, 'regtest')
    try:
        os.mkdir(regtest_dir)
    except OSError:
        pass
    # This should have the same params as regtest.
    # We use symlinks for now.
    testnet3_dir = os.path.join(params_dir, 'testnet3')
    try:
        os.mkdir(testnet3_dir)
    except OSError:
        pass
    os.chdir(regtest_dir)
    for (url, size, hash) in zip(urls, sizes, hashes):
        filename = url.split('/')[-1]
        print('Fetching', filename, '...')
        if not os.path.exists(filename) or \
                not os.stat(filename).st_size == size:
            fetch(url)
        # Now verify their hashes:
        print('Verifying', filename, '...')
        assert verify(filename, hash)
        try:
            os.remove(os.path.join(testnet3_dir, filename))
        except OSError:
            pass
        os.symlink(
            os.path.join(regtest_dir, filename),
            os.path.join(testnet3_dir, filename),
        )
