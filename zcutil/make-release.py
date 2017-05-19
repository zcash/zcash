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
from cStringIO import StringIO


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
        raise SystemExit(2)


# Top-level flow:
def main_logged(release, releaseprev):
    verify_releaseprev_tag(releaseprev)
    initialize_git(release)
    patch_version_in_files(release, releaseprev)

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


def initialize_git(release):
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

    logging.info('Pulling to latest master.')
    sh_out_logged('git', 'pull', '--ff-only')

    branch = 'release-' + release.vtext
    logging.info('Creating release branch: %r', branch)
    sh_out_logged('git', 'checkout', '-b', branch)
    return branch


def patch_version_in_files(release, releaseprev):
    patch_README(release, releaseprev)
    patch_clientversion_h(release, releaseprev)


# Helper code:
def chdir_to_repo(repo):
    if repo is None:
        dn = os.path.dirname
        repo = dn(dn(os.path.abspath(sys.argv[0])))
    os.chdir(repo)


def patch_README(release, releaseprev):
    with PathPatcher('README.md') as (inf, outf):
        firstline = inf.readline()
        assert firstline == 'Zcash {}\n'.format(releaseprev.novtext), \
            repr(firstline)

        outf.write('Zcash {}\n'.format(release.novtext))
        outf.write(inf.read())


def patch_clientversion_h(release, releaseprev):
    rgx = re.compile(
        r'^(#define CLIENT_VERSION_(MAJOR|MINOR|REVISION|BUILD)) \d+$'
    )
    with PathPatcher('src/clientversion.h') as (inf, outf):
        for line in inf:
            m = rgx.match(line)
            if m:
                prefix, label = m.groups()
                repl = {
                    'MAJOR': release.major,
                    'MINOR': release.minor,
                    'REVISION': release.patch,
                    'BUILD': release.build,
                }[label]
                outf.write('{} {}\n'.format(prefix, repl))
            else:
                outf.write(line)


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


def sh_out_logged(*args):
    out = sh_out(*args)
    logging.debug('Output:\n%s', out)
    return out


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

        self.novtext = '{}.{}.{}'.format(major, minor, patch)

        if hotfix is None:
            self.build = 50
        else:
            assert hotfix > 0, hotfix
            if betarc is None:
                assert hotfix < 50, hotfix
                self.build = 50 + hotfix
                self.novtext += '-{}'.format(hotfix)
            else:
                assert hotfix < 26, hotfix
                self.novtext += '-{}{}'.format(betarc, hotfix)
                self.build = {'beta': 0, 'rc': 25}[betarc] + hotfix - 1

        self.vtext = 'v' + self.novtext

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


class PathPatcher (object):
    def __init__(self, path):
        self._path = path

    def __enter__(self):
        logging.info('Patching %r', self._path)
        self._inf = file(self._path, 'r')
        self._outf = StringIO()
        return (self._inf, self._outf)

    def __exit__(self, et, ev, tb):
        if (et, ev, tb) == (None, None, None):
            self._inf.close()
            with file(self._path, 'w') as f:
                f.write(self._outf.getvalue())


# Unit Tests
class TestVersion (unittest.TestCase):
    ValidVersionsAndBuilds = [
        # These are taken from: git tag --list | grep '^v1'
        ('v1.0.0-beta1', 0),
        ('v1.0.0-beta2', 1),
        ('v1.0.0-rc1', 25),
        ('v1.0.0-rc2', 26),
        ('v1.0.0-rc3', 27),
        ('v1.0.0-rc4', 28),
        ('v1.0.0', 50),
        ('v1.0.1', 50),
        ('v1.0.2', 50),
        ('v1.0.3', 50),
        ('v1.0.4', 50),
        ('v1.0.5', 50),
        ('v1.0.6', 50),
        ('v1.0.7-1', 51),
        ('v1.0.8', 50),
        ('v1.0.8-1', 51),
    ]

    ValidVersions = [
        v
        for (v, _)
        in ValidVersionsAndBuilds
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

    def test_build_nums(self):
        for (text, expected) in self.ValidVersionsAndBuilds:
            version = Version.parse_arg(text)
            self.assertEqual(version.build, expected)


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
