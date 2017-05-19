#! /usr/bin/env python2

import os
import re
import sys
import time
import logging
import argparse
import unittest


def main(args=sys.argv[1:]):
    """
    Perform the final Zcash release process up to the git tag.
    """
    chdir_to_repo()
    opts = parse_args(args)
    initialize_logging()
    logging.debug('argv %r parsed %r', sys.argv, opts)
    raise NotImplementedError((main, opts))


def chdir_to_repo():
    dn = os.path.dirname
    repodir = dn(dn(os.path.abspath(sys.argv[0])))
    os.chdir(repodir)


def initialize_logging():
    TIME_FMT = '%Y-%m-%dT%H:%M:%S'
    logname = './zcash-make-release.{}.log'.format(time.strftime(TIME_FMT))
    fmtr = logging.Formatter(
        '%(asctime)s L%(lineno)-4d %(levelname)-5s | %(message)s',
        TIME_FMT,
    )

    hout = logging.StreamHandler(sys.stdout)
    hout.setLevel(logging.INFO)
    hout.setFormatter(fmtr)

    hpath = logging.FileHandler(logname, mode='a')
    hpath.setLevel(logging.DEBUG)
    hpath.setFormatter(fmtr)

    root = logging.getLogger()
    root.setLevel(logging.DEBUG)
    root.addHandler(hout)
    root.addHandler(hpath)
    logging.info('zcash make-release.py logging to: %r', logname)


def parse_args(args):
    p = argparse.ArgumentParser(description=main.__doc__)
    p.add_argument(
        'RELEASE_VERSION',
        type=Version.parse_arg,
        help='The release version: vX.Y.Z-N',
    )
    return p.parse_args(args)


class Version (object):
    '''A release version.'''

    RGX = re.compile(
        r'^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(-([1-9]\d*))?$',
    )

    @staticmethod
    def parse_arg(text):
        m = Version.RGX.match(text)
        if m is None:
            raise argparse.ArgumentTypeError(
                'Could not parse version {!r} against regex {}'.format(
                    text,
                    Version.RGX.pattern,
                ),
            )
        else:
            [major, minor, patch, _, hotfix] = m.groups()
            return Version(
                int(major),
                int(minor),
                int(patch),
                int(hotfix) if hotfix is not None else None,
            )

    def __init__(self, major, minor, patch, hotfix):
        self.major = major
        self.minor = minor
        self.patch = patch
        self.hotfix = hotfix

        self.vtext = 'v{}.{}.{}'.format(major, minor, patch)
        if hotfix is not None:
            self.vtext += '-{}'.format(hotfix)

    def __repr__(self):
        return '<Version {}>'.format(self.vtext)


# Unit Tests
class TestVersion (unittest.TestCase):
    def test_arg_parse_and_vtext_identity(self):
        cases = [
            'v0.0.0',
            'v1.0.0',
            'v1.0.0-7',
            'v1.2.3-1',
        ]

        for case in cases:
            v = Version.parse_arg(case)
            self.assertEqual(v.vtext, case)

    def test_arg_parse_negatives(self):
        cases = [
            'v07.0.0',
            'v1.0.03',
            'v1.0.0-rc2',
            'v1.2.3-0',  # Hotfix numbers must begin w/ 1
            'v1.2.3~0',
            '1.2.3',
        ]

        for case in cases:
            self.assertRaises(
                argparse.ArgumentTypeError,
                Version.parse_arg,
                case,
            )


if __name__ == '__main__':
    if len(sys.argv) >= 2 and sys.argv[1] == 'test':
        sys.argv.pop(1)
        if len(sys.argv) == 1:
            sys.argv.append('--verbose')
        unittest.main()
    else:
        main()
