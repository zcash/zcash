#!/usr/bin/env python3
#
# This script checks for updates to zcashd's dependencies.
#
# The SOURCE_ROOT constant specifies the location of the zcashd codebase to
# check, and the GITHUB_API_* constants specify a personal access token for the
# GitHub API, which need not have any special privileges.
#
# All dependencies must be specified inside the get_dependency_list() function
# below. A dependency is specified by:
#
#   (a) A way to fetch a list of current releases.
#
#       This is usually regular-expression-based parsing of GitHub tags, but
#       might otherwise parse version numbers out of the project's webpage.
#
#	GitHub tag regexps can be tested by specifying test cases in the third
#	argument to GithubTagReleaseLister's constructor.
#
#   (b) A way to fetch the currently-used version out of the source tree.
#
#	This is typically parsed out of the depends/packages/*.mk files.
#
# If any dependency is found to be out-of-date, or there are un-accounted-for
# .mk files in depends/packages, this script will exit with
# a nonzero status. The latter case would suggest someone added a new dependency
# without adding a corresponding entry to get_dependency_list() below.

import requests
import os
import re
import sys

SOURCE_ROOT = os.path.join(os.path.dirname(os.path.realpath(__file__)), "..", "..")
# The email for this account is taylor@electriccoin.co and the token does not
# have any privileges.
GITHUB_API_BASIC_AUTH_USER = "taylor-ecc"
GITHUB_API_BASIC_AUTH_PASSWORD = "df2cb6d13a29837e9dc97c7db1eff058e8fa6618"

def get_dependency_list():
    dependencies = [
        Dependency("bdb",
            BerkeleyDbReleaseLister(),
            DependsVersionGetter("bdb")),
        Dependency("boost",
            GithubTagReleaseLister("boostorg", "boost", "^boost-(\d+)\.(\d+)\.(\d+)$",
                { "boost-1.69.0": (1, 69, 0), "boost-1.69.0-beta1": None }),
            DependsVersionGetter("boost")),
        Dependency("googletest",
            GithubTagReleaseLister("google", "googletest", "^release-(\d+)\.(\d+)\.(\d+)$",
                { "release-1.8.1": (1, 8, 1) }),
            DependsVersionGetter("googletest")),
        Dependency("libevent",
            GithubTagReleaseLister("libevent", "libevent", "^release-(\d+)\.(\d+)\.(\d+)-stable$",
                { "release-2.0.22-stable": (2, 0, 22), "release-2.1.9-beta": None }),
            DependsVersionGetter("libevent")),
        Dependency("libsodium",
            GithubTagReleaseLister("jedisct1", "libsodium", "^(\d+)\.(\d+)\.(\d+)$",
                { "1.0.17": (1, 0, 17) }),
            DependsVersionGetter("libsodium")),
        Dependency("native_ccache",
            GithubTagReleaseLister("ccache", "ccache", "^v?(\d+)\.(\d+)(?:\.(\d+))?$",
                { "v3.5.1": (3, 5, 1), "v3.6": (3, 6)}),
            DependsVersionGetter("native_ccache")),
        Dependency("openssl",
            GithubTagReleaseLister("openssl", "openssl", "^OpenSSL_(\d+)_(\d+)_(\d+)([a-z]+)?$",
                { "OpenSSL_1_1_1b": (1, 1, 1, 'b'), "OpenSSL_1_1_1-pre9": None }),
            DependsVersionGetter("openssl")),
        Dependency("proton",
            GithubTagReleaseLister("apache", "qpid-proton", "^(\d+)\.(\d+)(?:\.(\d+))?$",
                { "0.27.0": (0, 27, 0), "0.10": (0, 10), "0.12.0-rc": None }),
            DependsVersionGetter("proton")),
        Dependency("rust",
            GithubTagReleaseLister("rust-lang", "rust", "^(\d+)\.(\d+)(?:\.(\d+))?$",
                { "1.33.0": (1, 33, 0), "0.9": (0, 9) }),
            DependsVersionGetter("rust")),
        Dependency("zeromq",
            GithubTagReleaseLister("zeromq", "libzmq", "^v(\d+)\.(\d+)(?:\.(\d+))?$",
                { "v4.3.1": (4, 3, 1), "v4.2.0-rc1": None }),
            DependsVersionGetter("zeromq")),
        Dependency("leveldb",
            GithubTagReleaseLister("google", "leveldb", "^v(\d+)\.(\d+)$",
                { "v1.13": (1, 13) }),
            LevelDbVersionGetter()),
        Dependency("utfcpp",
            GithubTagReleaseLister("nemtrif", "utfcpp", "^v(\d+)\.(\d+)(?:\.(\d+))?$",
                { "v3.1": (3, 1), "v3.0.3": (3, 0, 3) }),
            DependsVersionGetter("utfcpp"))
    ]

    # Rust crates.
    crates = [
        "aes", "aesni", "aes_soft", "arrayvec", "bellman",
        "arrayref", "autocfg", "bech32", "blake2b_simd", "blake2s_simd",
        "bit_vec", "block_cipher_trait", "byteorder",
        "block_buffer", "block_padding", "c2_chacha", "cfg_if",
        "byte_tools", "constant_time_eq", "crossbeam", "digest", "fpe",
        "crossbeam_channel", "crossbeam_deque", "crossbeam_epoch",
        "crossbeam_utils", "crossbeam_queue", "crypto_api", "crypto_api_chachapoly",
        "directories", "fake_simd", "getrandom", "hex", "log",
        "futures_cpupool", "futures",
        "generic_array", "lazy_static", "libc", "nodrop", "num_bigint",
        "memoffset", "ppv_lite86", "proc_macro2", "quote",
        "num_cpus", "num_integer", "num_traits", "opaque_debug", "pairing",
        "rand", "typenum",
        "rand_chacha", "rand_core", "rand_hc", "rand_os", "rand_xorshift",
        "rustc_version", "scopeguard", "semver", "semver_parser", "sha2", "syn",
        "unicode_xid", "wasi",
        "winapi_i686_pc_windows_gnu", "winapi", "winapi_x86_64_pc_windows_gnu",
    ]

    for crate in crates:
        dependencies.append(
            Dependency("crate_" + crate,
                RustCrateReleaseLister(crate),
                DependsVersionGetter("crate_" + crate)
            )
        )

    return dependencies

class Version(list):
    def __init__(self, version_tuple):
        for part in version_tuple:
            if part: # skip None's which can come from optional regexp groups
                if str(part).isdigit():
                    self.append(int(part))
                else:
                    self.append(part)

    def __str__(self):
        return '.'.join(map(str, self))

    def __hash__(self):
        return hash(tuple(self))

class Dependency:
    def __init__(self, name, release_lister, current_getter):
        self.name = name
        self.release_lister = release_lister
        self.current_getter = current_getter
        self.cached_known_releases = None

    def current_version(self):
        return self.current_getter.current_version()

    def known_releases(self):
        if self.cached_known_releases is None:
            self.cached_known_releases = sorted(self.release_lister.known_releases())
        return self.cached_known_releases

    def released_versions_after_current_version(self):
        current_version = self.current_version()
        releases_after_current = []

        for release in self.known_releases():
            if release > current_version:
                releases_after_current.append(release)

        return releases_after_current

    def is_up_to_date(self):
        return len(self.released_versions_after_current_version()) == 0

class GithubTagReleaseLister:
    def __init__(self, org, repo, regex, testcases={}):
        self.org = org
        self.repo = repo
        self.regex = regex
        self.testcases = testcases

        for tag, expected in testcases.items():
            match = re.match(self.regex, tag)
            if (expected and not match) or (match and not expected) or (match and Version(match.groups()) != list(expected)):
                groups = str(match.groups())
                raise RuntimeError("GitHub tag regex test case [" + tag + "] failed, got [" + groups + "].")

    def known_releases(self):
        release_versions = []
        all_tags = self.all_tag_names()

        # sanity check against the test cases
        for tag, expected in self.testcases.items():
            if tag not in all_tags:
                raise RuntimeError("Didn't find expected tag [" + tag + "].")

        for tag_name in all_tags:
            match = re.match(self.regex, tag_name)
            if match:
                release_versions.append(Version(match.groups()))

        return release_versions

    def all_tag_names(self):
        url = "https://api.github.com/repos/" + safe(self.org) + "/" + safe(self.repo) + "/git/refs/tags"
        r = requests.get(url, auth=requests.auth.HTTPBasicAuth(GITHUB_API_BASIC_AUTH_USER, GITHUB_API_BASIC_AUTH_PASSWORD))
        if r.status_code != 200:
            raise RuntimeError("Request to GitHub tag API failed.")
        json = r.json()
        return list(map(lambda t: t["ref"].split("/")[-1], json))

class RustCrateReleaseLister:
    def __init__(self, crate):
        self.crate = crate

    def known_releases(self):
        url = "https://crates.io/api/v1/crates/" + safe(self.crate) + "/versions"
        r = requests.get(url)
        if r.status_code != 200:
            raise RuntimeError("Request to crates.io versions API failed.")
        json = r.json()
        version_numbers = list(map(lambda t: t["num"], json["versions"]))

        release_versions = []
        for num in version_numbers:
            match = re.match("^(\d+)\.(\d+)\.(\d+)$", num)
            if match:
                release_versions.append(Version(match.groups()))

        if len(release_versions) == 0:
            raise RuntimeError("Failed to list release versions from crates.io.")

        return release_versions

class LibGmpReleaseLister:
    def known_releases(self):
        url = "https://gmplib.org/download/gmp/"
        r = requests.get(url)
        if r.status_code != 200:
            raise RuntimeError("Request to libgmp download directory failed.")
        page = r.text

        # We use a set because the search will result in duplicates.
        release_versions = set()
        for match in re.findall("gmp-(\d+)\.(\d+)\.(\d+)\.tar.bz2", page):
            release_versions.add(Version(match))

        if Version((6, 1, 2)) not in release_versions:
            raise RuntimeError("Missing expected version from libgmp download directory.")

        return list(release_versions)

class BerkeleyDbReleaseLister:
    def known_releases(self):
        url = "https://www.oracle.com/technetwork/products/berkeleydb/downloads/index-082944.html"
        r = requests.get(url)
        if r.status_code != 200:
            raise RuntimeError("Request to Berkeley DB download directory failed.")
        page = r.text

        # We use a set because the search will result in duplicates.
        release_versions = set()
        for match in re.findall("Berkeley DB (\d+)\.(\d+)\.(\d+)\.tar.gz", page):
            release_versions.add(Version(match))

        if Version((6, 2, 38)) not in release_versions:
            raise RuntimeError("Missing expected version from Oracle web page.")

        return list(release_versions)

class DependsVersionGetter:
    def __init__(self, name):
        self.name = name

    def current_version(self):
        mk_file_path = os.path.join(SOURCE_ROOT, "depends", "packages", safe(self.name) + ".mk")
        mk_file = open(mk_file_path, 'r').read()

        regexp_whitelist = [
            "package\)_version=(\d+)\.(\d+)\.(\d+)$",
            "package\)_version=(\d+)\.(\d+)$",
            "package\)_version=(\d+)_(\d+)_(\d+)$",
            "package\)_version=(\d+)\.(\d+)\.(\d+)([a-z])$"
        ]

        current_version = None

        for regexp in regexp_whitelist:
            match = re.search(regexp, mk_file, re.MULTILINE)
            if match:
                current_version = Version(match.groups())

        if not current_version:
            raise RuntimeError("Couldn't parse version number from depends .mk file.")

        return current_version

class LevelDbVersionGetter:
    def current_version(self):
        header_path = os.path.join(SOURCE_ROOT, "src", "leveldb", "include", "leveldb", "db.h")
        header_contents = open(header_path, 'r').read()

        match = re.search("kMajorVersion\s*=\s*(\d+);\s*.*kMinorVersion\s*=\s*(\d+);\s*$", header_contents, re.MULTILINE)
        if match:
            return Version(match.groups())
        else:
            raise RuntimeError("Couldn't parse LevelDB's version from db.h")

def safe(string):
    if re.match('^[a-zA-Z0-9_-]*$', string):
        return string
    else:
        raise RuntimeError("Potentially-dangerous string encountered.")

def print_row(name, status, current_version, known_versions):
    COL_FMT_LARGE = "{:<35}"
    COL_FMT_SMALL = "{:<18}"
    print(COL_FMT_LARGE.format(name) +
            COL_FMT_SMALL.format(status) +
            COL_FMT_SMALL.format(current_version) +
            COL_FMT_SMALL.format(known_versions))

def main():
    # Get a list of all depends-system dependencies so we can verify that we're
    # checking them all for updates.
    unchecked_dependencies = [f[:-3] for f in os.listdir(os.path.join(SOURCE_ROOT, "depends", "packages")) if f.endswith(".mk")]

    untracked = [
        # packages.mk is not a dependency, it just specifies the list of them all.
        "packages",
        # just a template
        "vendorcrate",
        # These are pinned to specific git revisions.
        "librustzcash",
        "crate_sapling_crypto",
        "crate_zip32",
        # This package doesn't have conventional version numbers
        "native_cctools"
    ]

    print_row("NAME", "STATUS", "CURRENT VERSION", "NEWER VERSIONS")

    for dep in untracked:
        print_row(dep, "skipped", "", "")
        unchecked_dependencies.remove(dep)

    deps = get_dependency_list()
    status = 0
    for dependency in deps:
        if dependency.name in unchecked_dependencies:
            unchecked_dependencies.remove(dependency.name)
        if len(sys.argv) == 2 and sys.argv[1] == "skipcheck":
            print("Skipping the actual dependency update checks.")
        else:
            if dependency.is_up_to_date():
                print_row(
                    dependency.name,
                    "up to date",
                    str(dependency.current_version()),
                    "")
            else:
                print_row(
                    dependency.name,
                    "OUT OF DATE",
                    str(dependency.current_version()),
                    str(list(map(str, dependency.released_versions_after_current_version()))))
                status = 1

    if len(unchecked_dependencies) > 0:
        unchecked_dependencies.sort()
        print("WARNING: The following dependences are not being checked for updates by this script: " + ', '.join(unchecked_dependencies))
        status = 2

    sys.exit(status)

main()
