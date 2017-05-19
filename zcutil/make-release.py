#! /usr/bin/env python2

import os
import re
import sys
import time
import logging
import argparse
import subprocess
import traceback
import unittest
import random


def main(args=sys.argv[1:]):
    """
    Perform the final Zcash release process up to the git tag.
    """
    opts = parse_args(args)
    chdir_to_repo(opts.REPO)
    initialize_logging()
    logging.debug('argv %r parsed %r', sys.argv, opts)

    try:
        main_logged(opts.RELEASE_VERSION, opts.RELEASE_PREV)
    except SystemExit as e:
        logging.error(str(e))
        raise SystemExit(1)
    except:
        logging.error(traceback.format_exc())
        raise


# Top-level flow:
def main_logged(release, releaseprev):
    verify_releaseprev_tag(releaseprev)
    verify_git_clean_master()
    raise NotImplementedError(main_logged)


def parse_args(args):
    p = argparse.ArgumentParser(description=main.__doc__)
    p.add_argument(
        '--repo',
        dest='REPO',
        type=str,
        help='Path to repository root.',
    )
    p.add_argument(
        'RELEASE_VERSION',
        type=Version.parse_arg,
        help='The release version: vX.Y.Z-N',
    )
    p.add_argument(
        'RELEASE_PREV',
        type=Version.parse_arg,
        help='The previously released version.',
    )
    return p.parse_args(args)


def verify_releaseprev_tag(releaseprev):
    candidates = []
    for tag in sh_out('git', 'tag', '--list').splitlines():
        if tag.startswith('v1'):  # Ignore v0.* bitcoin tags and other stuff.
            candidates.append(Version.parse_arg(tag))

    candidates.sort()
    try:
        latest = candidates[-1]
    except IndexError:
        raise SystemExit('No previous releases found by `git tag --list`.')

    if releaseprev != latest:
        raise SystemExit(
            'The latest candidate in `git tag --list` is {} not {}'
            .format(
                latest.vtext,
                releaseprev.vtext,
            ),
        )


def verify_git_clean_master():
    junk = sh_out('git', 'status', '--porcelain')
    if junk.strip():
        raise SystemExit('There are uncommitted changes:\n' + junk)

    branch = sh_out('git', 'rev-parse', '--abbrev-ref', 'HEAD').strip()
    if branch != 'master':
        raise SystemExit(
            "Expected branch 'master', found branch {!r}".format(
                branch,
            ),
        )


# Helper code:
def chdir_to_repo(repo):
    if repo is None:
        dn = os.path.dirname
        repo = dn(dn(os.path.abspath(sys.argv[0])))
    os.chdir(repo)


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


def sh_out(*args):
    logging.debug('Run: %r', args)
    return subprocess.check_output(args)


class Version (object):
    '''A release version.'''

    RGX = re.compile(
        r'^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(-(beta|rc)?([1-9]\d*))?$',
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
            [major, minor, patch, _, betarc, hotfix] = m.groups()
            return Version(
                int(major),
                int(minor),
                int(patch),
                betarc,
                int(hotfix) if hotfix is not None else None,
            )

    def __init__(self, major, minor, patch, betarc, hotfix):
        for i in [major, minor, patch]:
            assert type(i) is int, i
        assert betarc in {None, 'rc', 'beta'}, betarc
        assert hotfix is None or type(hotfix) is int, hotfix
        if betarc is not None:
            assert hotfix is not None, (betarc, hotfix)

        self.major = major
        self.minor = minor
        self.patch = patch
        self.betarc = betarc
        self.hotfix = hotfix

        self.vtext = 'v{}.{}.{}'.format(major, minor, patch)
        if hotfix is not None:
            self.vtext += '-{}{}'.format(
                '' if betarc is None else betarc,
                hotfix,
            )

    def __repr__(self):
        return '<Version {}>'.format(self.vtext)

    def _sort_tup(self):
        if self.hotfix is None:
            prio = 2
        else:
            prio = {'beta': 0, 'rc': 1, None: 3}[self.betarc]

        return (
            self.major,
            self.minor,
            self.patch,
            prio,
            self.hotfix,
        )

    def __cmp__(self, other):
        return cmp(self._sort_tup(), other._sort_tup())


# Unit Tests
class TestVersion (unittest.TestCase):
    ValidVersions = [
        # These are taken from: git tag --list | grep '^v1'
        'v1.0.0-beta1',
        'v1.0.0-beta2',
        'v1.0.0-rc1',
        'v1.0.0-rc2',
        'v1.0.0-rc3',
        'v1.0.0-rc4',
        'v1.0.0',
        'v1.0.1',
        'v1.0.2',
        'v1.0.3',
        'v1.0.4',
        'v1.0.5',
        'v1.0.6',
        'v1.0.7-1',
        'v1.0.8',
        'v1.0.8-1',
    ]

    def test_arg_parse_and_vtext_identity(self):
        for case in self.ValidVersions:
            v = Version.parse_arg(case)
            self.assertEqual(v.vtext, case)

    def test_arg_parse_negatives(self):
        cases = [
            'v07.0.0',
            'v1.0.03',
            'v1.2.3-0',  # Hotfix numbers must begin w/ 1
            'v1.2.3~0',
            'v1.2.3+0',
            '1.2.3',
        ]

        for case in cases:
            self.assertRaises(
                argparse.ArgumentTypeError,
                Version.parse_arg,
                case,
            )

    def test_version_sort(self):
        expected = [Version.parse_arg(v) for v in self.ValidVersions]

        rng = random.Random()
        rng.seed(0)

        for _ in range(1024):
            vec = list(expected)
            rng.shuffle(vec)
            vec.sort()
            self.assertEqual(vec, expected)


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == '--help':
        main()
    else:
        actualargs = sys.argv
        sys.argv = [sys.argv[0], '--verbose']

        print '=== Self Test ==='
        try:
            unittest.main()
        except SystemExit as e:
            if e.args[0] != 0:
                raise

        sys.argv = actualargs
        print '=== Running ==='
        main()
