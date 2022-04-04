This is a bugfix release that also bumps the End-of-Support height to May 16th.
It backports bugfixes from v4.7.0-rc1, including a `getblocktemplate` deadlock
fix, and several portability fixes.

Debian 9 "Stretch" is no longer supported from v4.6.0-2, due to its
[end-of-life](https://wiki.debian.org/LTS/Stretch) on June 30th, 2022. This will
allow us to direct more resources to supporting Debian 11 Bullseye, other Linux
distributions, and other platforms such as Windows and macOS.

Changelog
=========

Charlie O'Keefe (2):
      Update base image used by Dockerfile from debian 10 to debian 11
      Remove stretch (debian 9), add bullseye (debian 11) in gitian descriptor

Daira Hopwood (4):
      Avoid a warning by explicitly calling drop.
      Replace call to drop with zeroization.
      qa/zcash/updatecheck.py: print status code and response of failed http requests.
      Postpone native_clang and libcxx 14.0.0.

Jack Grigg (9):
      qa: Bump all postponed dependencies
      qa: Postpone recent CCache release
      depends: Update Rust to 1.59.0
      depends: Update Clang / libcxx to LLVM 13.0.1
      rust: Fix clippy lint
      depends: Revert to `libc++ 13.0.0-3` for Windows cross-compile
      qa: Exclude `native_libtinfo` from dependency update checks
      make-release.py: Versioning changes for 4.6.0-2.
      make-release.py: Updated manpages for 4.6.0-2.

Larry Ruane (1):
      document global variables

Pieter Wuille (2):
      Fix csBestBlock/cvBlockChange waiting in rpc/mining
      Modernize best block mutex/cv/hash variable naming

Taylor Hornby (1):
      Untested, not working yet, use libtinfo from the debian packages

sasha (2):
      on Arch only, use Debian's libtinfo5_6.0 to satisfy clang
      remove superfluous space at end of native_packages line

