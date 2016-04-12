Release Process
====================

### Define the release version as:

	$ ZCASH_RELEASE=${UPSTREAM_VERSION}.z${ZCASH_RELEASE_COUNTER}

Example:

	$ ZCASH_RELEASE=0.11.2.z2

Also, the following commands use the ZCASH_RELEASE_PREV bash variable
for the previous release:

	$ ZCASH_RELEASE_PREV=0.11.2.z1

### update (commit) version in sources

	doc/README.md
	src/clientversion.h

In clientverion.h change CLIENT_VERSION_IS_RELEASE to false while Zcash
is in alpha-test phase.

### write release notes

git shortlog helps a lot, for example:

    $ git shortlog --no-merges v${ZCASH_RELEASE_PREV}..HEAD \
        > ./doc/release-notes/release-notes-${ZCASH_RELEASE}.md

### merge the previous changes

Do the normal pull-request, review, testing process.

### make tags / release-branch for the newly merged result

In this example, we ensure zc.v0.11.2.latest is up to date with the
previous merged PR, then:

	$ git tag v${ZCASH_RELEASE}
    $ git branch zc.v${ZCASH_RELEASE}
    $ git push origin v${ZCASH_RELEASE}
    $ git push origin zc.v${ZCASH_RELEASE}

### update github default branch to this new release branch

### write / publish a release announcement

### celebrate

### missing steps

Zcash still needs:

a. deterministic build
b. signatured tags
c. thorough pre-release testing (presumably more thorough than standard PR tests)
d. release deployment steps (eg: updating build-depends mirror, deploying testnet, etc...)
e. proper zcash-specific versions and names in software and documentation.
