#! /usr/bin/env python2

import os
import re
import sys
import logging
import argparse
import subprocess
import traceback
import unittest
import random
from cStringIO import StringIO
from functools import wraps


def main(args=sys.argv[1:]):
    """
    Perform the final Zcash release process up to the git tag.
    """
    opts = parse_args(args)
    chdir_to_repo(opts.REPO)
    initialize_logging()
    logging.debug('argv %r', sys.argv)

    try:
        main_logged(
            opts.RELEASE_VERSION,
            opts.RELEASE_PREV,
            opts.RELEASE_FROM,
            opts.RELEASE_HEIGHT,
            opts.HOTFIX,
        )
    except SystemExit as e:
        logging.error(str(e))
        raise SystemExit(1)
    except:
        logging.error(traceback.format_exc())
        raise SystemExit(2)


def parse_args(args):
    p = argparse.ArgumentParser(description=main.__doc__)
    p.add_argument(
        '--repo',
        dest='REPO',
        type=str,
        help='Path to repository root.',
    )
    p.add_argument(
        '--hotfix',
        action='store_true',
        dest='HOTFIX',
        help='Use if this is a hotfix release from a non-master branch.',
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
    p.add_argument(
        'RELEASE_FROM',
        type=Version.parse_arg,
        help='The previously released non-beta non-RC version. May be the same as RELEASE_PREV.',
    )
    p.add_argument(
        'RELEASE_HEIGHT',
        type=int,
        help='A block height approximately occuring on release day.',
    )
    return p.parse_args(args)


# Top-level flow:
def main_logged(release, releaseprev, releasefrom, releaseheight, hotfix):
    verify_tags(releaseprev, releasefrom)
    verify_version(release, releaseprev, hotfix)
    initialize_git(release, hotfix)
    patch_version_in_files(release, releaseprev)
    patch_release_height(releaseheight)
    commit('Versioning changes for {}.'.format(release.novtext))

    build()
    gen_manpages()
    commit('Updated manpages for {}.'.format(release.novtext))

    gen_release_notes(release, releasefrom)
    update_debian_changelog(release)
    commit(
        'Updated release notes and changelog for {}.'.format(
            release.novtext,
        ),
    )


def phase(message):
    def deco(f):
        @wraps(f)
        def g(*a, **kw):
            logging.info('%s', message)
            return f(*a, **kw)
        return g
    return deco


@phase('Checking tags.')
def verify_tags(releaseprev, releasefrom):
    candidates = []

    # Any tag beginning with a 'v' followed by [1-9] must be a version
    # matching our Version parser. Tags beginning with v0 may exist from
    # upstream and those do not follow our schema and are silently
    # ignored. Any other tag is silently ignored.
    candidatergx = re.compile('^v[1-9].*$')

    for tag in sh_out('git', 'tag', '--list').splitlines():
        if candidatergx.match(tag):
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

    candidates.reverse()
    prev_tags = []
    for candidate in candidates:
        if releasefrom == candidate:
            break
        else:
            prev_tags.append(candidate)
    else:
        raise SystemExit(
            '{} does not appear in `git tag --list`'
            .format(
                releasefrom.vtext,
            ),
        )

    for tag in prev_tags:
        if not tag.betarc:
            raise SystemExit(
                '{} appears to be a more recent non-beta non-RC release than {}'
                .format(
                    tag.vtext,
                    releasefrom.vtext,
                ),
            )


@phase('Checking version.')
def verify_version(release, releaseprev, hotfix):
    if not hotfix:
        return

    expected = Version(
        releaseprev.major,
        releaseprev.minor,
        releaseprev.patch,
        releaseprev.betarc,
        releaseprev.hotfix + 1 if releaseprev.hotfix else 1,
    )
    if release != expected:
        raise SystemExit(
            "Expected {!r}, given {!r}".format(
                expected, release,
            ),
        )


@phase('Initializing git.')
def initialize_git(release, hotfix):
    junk = sh_out('git', 'status', '--porcelain')
    if junk.strip():
        raise SystemExit('There are uncommitted changes:\n' + junk)

    branch = sh_out('git', 'rev-parse', '--abbrev-ref', 'HEAD').strip()
    if hotfix:
        expected = 'hotfix-' + release.vtext
    else:
        expected = 'master'
    if branch != expected:
        raise SystemExit(
            "Expected branch {!r}, found branch {!r}".format(
                expected, branch,
            ),
        )

    logging.info('Pulling to latest master.')
    sh_log('git', 'pull', '--ff-only')

    branch = 'release-' + release.vtext
    logging.info('Creating release branch: %r', branch)
    sh_log('git', 'checkout', '-b', branch)
    return branch


@phase('Patching versioning in files.')
def patch_version_in_files(release, releaseprev):
    patch_README(release, releaseprev)
    patch_clientversion_h(release)
    patch_configure_ac(release)
    patch_gitian_linux_yml(release, releaseprev)


@phase('Patching release height for end-of-support halt.')
def patch_release_height(releaseheight):
    rgx = re.compile(
        r'^(static const int APPROX_RELEASE_HEIGHT = )\d+(;)$',
    )
    with PathPatcher('src/deprecation.h') as (inf, outf):
        for line in inf:
            m = rgx.match(line)
            if m is None:
                outf.write(line)
            else:
                [prefix, suffix] = m.groups()
                outf.write(
                    '{}{}{}\n'.format(
                        prefix,
                        releaseheight,
                        suffix,
                    ),
                )


@phase('Building...')
def build():
    base_dir = os.getcwd()
    depends_dir = os.path.join(base_dir, 'depends')
    src_dir = os.path.join(base_dir, 'src')
    nproc = sh_out('nproc').strip()
    sh_progress([
        'Staging boost...',
        'Staging libevent...',
        'Staging zeromq...',
        'Staging libgmp...',
        'Staging libsodium...',
        "Leaving directory '%s'" % depends_dir,
        'config.status: creating libzcashconsensus.pc',
        "Entering directory '%s'" % src_dir,
        'httpserver.cpp',
        'torcontrol.cpp',
        'gtest/test_tautology.cpp',
        'gtest/test_metrics.cpp',
        'test/equihash_tests.cpp',
        'test/util_tests.cpp',
        "Leaving directory '%s'" % src_dir,
        ], './zcutil/build.sh', '-j', nproc)


@phase('Generating manpages.')
def gen_manpages():
    sh_log('./contrib/devtools/gen-manpages.sh')


@phase('Generating release notes.')
def gen_release_notes(release, releasefrom):
    release_notes = [
        'python',
        './zcutil/release-notes.py',
        '--version',
        release.novtext,
        '--prev',
        releasefrom.vtext,
    ]
    if not release.betarc:
        release_notes.append('--clear')
    sh_log(*release_notes)
    sh_log(
        'git',
        'add',
        './doc/authors.md',
        './doc/release-notes/release-notes-{}.md'.format(release.novtext),
    )


@phase('Updating debian changelog.')
def update_debian_changelog(release):
    os.environ['DEBEMAIL'] = 'team@z.cash'
    os.environ['DEBFULLNAME'] = 'Electric Coin Company'
    sh_log(
        'debchange',
        '--newversion', release.debversion,
        '--distribution', 'stable',
        '--changelog', './contrib/debian/changelog',
        '{} release.'.format(release.novtext),
    )


# Helper code:
def commit(message):
    logging.info('Committing: %r', message)
    fullmsg = 'make-release.py: {}'.format(message)
    sh_log('git', 'commit', '--all', '-m', fullmsg)


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


def patch_clientversion_h(release):
    _patch_build_defs(
        release,
        'src/clientversion.h',
        (r'^(#define CLIENT_VERSION_(MAJOR|MINOR|REVISION|BUILD|IS_RELEASE))'
         r' \d+()$'),
    )


def patch_configure_ac(release):
    _patch_build_defs(
        release,
        'configure.ac',
        (r'^(define\(_CLIENT_VERSION_(MAJOR|MINOR|REVISION|BUILD|IS_RELEASE),)'
         r' \d+(\))$'),
    )


def patch_gitian_linux_yml(release, releaseprev):
    path = 'contrib/gitian-descriptors/gitian-linux.yml'
    with PathPatcher(path) as (inf, outf):
        outf.write(inf.readline())

        secondline = inf.readline()
        assert secondline == 'name: "zcash-{}"\n'.format(
            releaseprev.novtext
        ), repr(secondline)

        outf.write('name: "zcash-{}"\n'.format(release.novtext))
        outf.write(inf.read())


def _patch_build_defs(release, path, pattern):
    rgx = re.compile(pattern)
    with PathPatcher(path) as (inf, outf):
        for line in inf:
            m = rgx.match(line)
            if m:
                prefix, label, suffix = m.groups()
                repl = {
                    'MAJOR': release.major,
                    'MINOR': release.minor,
                    'REVISION': release.patch,
                    'BUILD': release.build,
                    'IS_RELEASE': (
                        'false' if release.build < 50 else 'true'
                    ),
                }[label]
                outf.write('{} {}{}\n'.format(prefix, repl, suffix))
            else:
                outf.write(line)


def initialize_logging():
    logname = './zcash-make-release.log'
    fmtr = logging.Formatter(
        '%(asctime)s L%(lineno)-4d %(levelname)-5s | %(message)s',
        '%Y-%m-%d %H:%M:%S'
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
    logging.info('zcash make-release.py debug log: %r', logname)


def sh_out(*args):
    logging.debug('Run (out): %r', args)
    return subprocess.check_output(args)


def sh_log(*args):
    PIPE = subprocess.PIPE
    STDOUT = subprocess.STDOUT
    try:
        p = subprocess.Popen(args, stdout=PIPE, stderr=STDOUT, stdin=None)
    except OSError:
        logging.error('Error launching %r...', args)
        raise

    logging.debug('Run (log PID %r): %r', p.pid, args)
    for line in p.stdout:
        logging.debug('> %s', line.rstrip())
    status = p.wait()
    if status != 0:
        raise SystemExit('Nonzero exit status: {!r}'.format(status))


def sh_progress(markers, *args):
    try:
        import progressbar
    except:
        sh_log(*args)
        return

    PIPE = subprocess.PIPE
    STDOUT = subprocess.STDOUT
    try:
        p = subprocess.Popen(args, stdout=PIPE, stderr=STDOUT, stdin=None)
    except OSError:
        logging.error('Error launching %r...', args)
        raise

    pbar = progressbar.ProgressBar(max_value=len(markers))
    marker = 0
    pbar.update(marker)
    logging.debug('Run (log PID %r): %r', p.pid, args)
    for line in p.stdout:
        logging.debug('> %s', line.rstrip())
        for idx, val in enumerate(markers[marker:]):
            if val in line:
                marker += idx + 1
                pbar.update(marker)
                break
    pbar.finish()
    status = p.wait()
    if status != 0:
        raise SystemExit('Nonzero exit status: {!r}'.format(status))


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

        if hotfix is None:
            self.build = 50
        else:
            assert hotfix > 0, hotfix
            if betarc is None:
                assert hotfix < 50, hotfix
                self.build = 50 + hotfix
            else:
                assert hotfix < 26, hotfix
                self.build = {'beta': 0, 'rc': 25}[betarc] + hotfix - 1

    @property
    def novtext(self):
        return self._novtext(debian=False)

    @property
    def vtext(self):
        return 'v' + self.novtext

    @property
    def debversion(self):
        return self._novtext(debian=True)

    def _novtext(self, debian):
        novtext = '{}.{}.{}'.format(self.major, self.minor, self.patch)

        if self.hotfix is None:
            return novtext
        else:
            assert self.hotfix > 0, self.hotfix
            if self.betarc is None:
                assert self.hotfix < 50, self.hotfix
                sep = '+' if debian else '-'
                return '{}{}{}'.format(novtext, sep, self.hotfix)
            else:
                assert self.hotfix < 26, self.hotfix
                sep = '~' if debian else '-'
                return '{}{}{}{}'.format(
                    novtext,
                    sep,
                    self.betarc,
                    self.hotfix,
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


class PathPatcher (object):
    def __init__(self, path):
        self._path = path

    def __enter__(self):
        logging.debug('Patching %r', self._path)
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
        ('v1.0.9', 50),
        ('v1.0.10', 50),
        ('v7.42.1000', 50),
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
