Release Process
====================
Meta: There should always be a single release engineer to disambiguate responsibility.

## Pre-release

Check all of the following:

- All dependencies have been updated as appropriate:
  - BDB
  - Boost
  - ccache
  - libgmp
  - libsnark (upstream of our fork)
  - libsodium
  - miniupnpc
  - OpenSSL

## A. Define the release version as:

    $ ZCASH_RELEASE=MAJOR.MINOR.REVISION(-BUILD_STRING)

Example:

    $ ZCASH_RELEASE=1.0.0-beta2

Also, the following commands use the `ZCASH_RELEASE_PREV` bash variable for the
previous release:

    $ ZCASH_RELEASE_PREV=1.0.0-beta1

## B. Create a new release branch / github PR
### B1. Update (commit) version in sources

    README.md
    src/clientversion.h
    configure.ac
    contrib/gitian-descriptors/gitian-linux.yml

    Build and commit to update versions, and then perform the following commands:

    help2man -n "RPC client for the Zcash daemon" src/zcash-cli > contrib/DEBIAN/manpages/zcash-cli.1
    help2man -n "Network daemon for interacting with the Zcash blockchain" src/zcashd > contrib/DEBIAN/manpages/zcashd.1


In `configure.ac` and `clientversion.h`:

- Increment `CLIENT_VERSION_BUILD` according to the following schema:

  - 0-24: `1.0.0-beta1`-`1.0.0-beta25`
  - 25-49: `1.0.0-rc1`-`1.0.0-rc25`
  - 50: `1.0.0`
  - 51-99: `1.0.0-1`-`1.0.0-49`
  - (`CLIENT_VERSION_REVISION` rolls over)
  - 0-24: `1.0.1-beta1`-`1.0.1-beta25`

- Change `CLIENT_VERSION_IS_RELEASE` to false while Zcash is in beta-test phase.

### B2. Write release notes

Run the release-notes.py script to generate release notes and update authors.md file. For example:

    $ python zcutil/release-notes.py --version $ZCASH_RELEASE

Update the Debian package changelog:

    export DEBVERSION="${ZCASH_RELEASE}"
    export DEBEMAIL="${DEBEMAIL:-team@z.cash}"
    export DEBFULLNAME="${DEBFULLNAME:-Zcash Company}"

    dch -v $DEBVERSION -D jessie -c contrib/DEBIAN/changelog

(`dch` comes from the devscripts package.)

### B3. Change the network magics

If this release breaks backwards compatibility, change the network magic
numbers. Set the four `pchMessageStart` in `CTestNetParams` in `chainparams.cpp`
to random values.

### B4. Merge the previous changes

Do the normal pull-request, review, testing process for this release PR.

## C. Verify code artifact hosting

### C1. Ensure depends tree is working

https://ci.z.cash/builders/depends-sources

### C2. Ensure public parameters work

Run `./fetch-params.sh`.

## D. Make tag for the newly merged result

In this example, we ensure master is up to date with the
previous merged PR, then:

    $ git tag -s v${ZCASH_RELEASE}
    $ git push origin v${ZCASH_RELEASE}

## E. Deploy testnet

Notify the Zcash DevOps engineer/sysadmin that the release has been tagged. They update some variables in the company's automation code and then run an Ansible playbook, which:

* builds Zcash based on the specified branch
* deploys it as a public service (e.g. betatestnet.z.cash, mainnet.z.cash)
* often the same server can be re-used, and the role idempotently handles upgrades, but if not then they also need to update DNS records
* possible manual steps: blowing away the `testnet3` dir, deleting old parameters, restarting DNS seeder

Then, verify that nodes can connect to the testnet server, and update the guide on the wiki to ensure the correct hostname is listed in the recommended zcash.conf.

## F. Update the 1.0 User Guide

## G. Publish the release announcement (blog, zcash-dev, slack)

### G1. Check in with users who opened issues that were resolved in the release

Contact all users who opened `user support` issues that were resolved in the release, and ask them if the release fixes or improves their issue.

## H. Make and deploy deterministic builds

- Run the [Gitian deterministic build environment](https://github.com/zcash/zcash-gitian)
- Compare the uploaded [build manifests on gitian.sigs](https://github.com/zcash/gitian.sigs)
- If all is well, the DevOps engineer will build the Debian packages and update the
  [apt.z.cash package repository](https://apt.z.cash).

## I. Celebrate

## missing steps
Zcash still needs:

* thorough pre-release testing (presumably more thorough than standard PR tests)

* automated release deployment (e.g.: updating build-depends mirror, deploying testnet, etc...)
