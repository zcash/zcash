Release Process
====================
Meta: There should always be a single release engineer to disambiguate responsibility.

## A. Define the release version as:

    $ ZCASH_RELEASE=${UPSTREAM_VERSION}.z${ZCASH_RELEASE_COUNTER}
    
Example:

    $ ZCASH_RELEASE=0.11.2.z2
    
Also, the following commands use the ZCASH_RELEASE_PREV bash variable for the previous release:

    $ ZCASH_RELEASE_PREV=0.11.2.z1
    
## B. create a new release branch / github PR
### B1. update (commit) version in sources

    doc/README.md
    src/clientversion.h
    configure.ac
    
In `configure.ac` and `clientversion.h` change CLIENT_VERSION_IS_RELEASE to
false while Zcash is in alpha-test phase.

### B2. write release notes

git shortlog helps a lot, for example:

    $ git shortlog --no-merges v${ZCASH_RELEASE_PREV}..HEAD \
        > ./doc/release-notes/release-notes-${ZCASH_RELEASE}.md

### B3. change the network magics

If this release breaks backwards compatibility, change the network magic
numbers. Set the four `pchMessageStart` in `CTestNetParams` in `chainparams.cpp`
to random values.
        
### B4. merge the previous changes

Do the normal pull-request, review, testing process for this release PR.

## C. Verify code artifact hosting

### C1. Ensure depends tree is working

http://ci.leastauthority.com:8010/builders/depends-sources

### C2. Ensure public parameters work

Run `./fetch-params.sh`.

## D. make tags / release-branch for the newly merged result

In this example, we ensure zc.v0.11.2.latest is up to date with the
previous merged PR, then:

    $ git tag v${ZCASH_RELEASE}
    $ git branch zc.v${ZCASH_RELEASE}
    $ git push origin v${ZCASH_RELEASE}
    $ git push origin zc.v${ZCASH_RELEASE}
    
## E. update github default branch to this new release branch
## F. write / publish a release announcement
## G. deploy testnet
## H. write and publish appropriate announcements (blog, zcash-dev, slack)
## I. celebrate
## missing steps

Zcash still needs:

* deterministic build

* signatured tags

* thorough pre-release testing (presumably more thorough than standard PR tests)

* release deployment steps (eg: updating build-depends mirror, deploying testnet, etc...)

* proper zcash-specific versions and names in software and documentation.
