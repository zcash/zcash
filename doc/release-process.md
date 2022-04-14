Release Process
====================
Meta: There should always be a single release engineer to disambiguate responsibility.

If this is a hotfix release, please see the [Hotfix Release
Process](https://github.com/zcash/zcash/blob/master/doc/hotfix-process.md) documentation
before proceeding.

## Pre-release

### Github Milestone

Ensure all goals for the github milestone are met. If not, remove tickets
or PRs with a comment as to why it is not included. (Running out of time
is a common reason.)

### Pre-release checklist:

Check that dependencies are properly hosted.

Check that there are no surprising performance regressions.

Update `src/chainparams.cpp` nMinimumChainWork with information from the getblockchaininfo rpc.

Check that dependencies are up-to-date or have been postponed:

```
$ ./qa/zcash/updatecheck.py
```

If you are missing the `.updatecheck-token` file required to run this script,
please ask Taylor or another Zcash developer for a copy, or create an
unprivileged personal access token for a github account and save it to the
file in the format `username:hex-token`.

If there are updates that have not been postponed, review their changelogs
for urgent security fixes, and if there aren't any, postpone the update by
adding a line to `qa/zcash/postponed-updates.txt`.

### Protocol Safety Checks:

If this release changes the behavior of the protocol or fixes a serious
bug, verify that a pre-release PR merge updated `PROTOCOL_VERSION` in
`version.h` correctly.

If this release breaks backwards compatibility or needs to prevent
interaction with software forked projects, change the network magic
numbers. Set the four `pchMessageStart` in `CTestNetParams` in
`chainparams.cpp` to random values.

Both of these should be done in standard PRs ahead of the release
process. If these were not anticipated correctly, this could block the
release, so if you suspect this is necessary, double check with the
whole engineering team.

## Release dependencies

The release script has the following dependencies:

- `help2man`
- `debchange` (part of the devscripts Debian package)

You can optionally install the `progressbar2` Python module with pip to have a
progress bar displayed during the build process.

## Release process

Identify the commit from which the release will be made. This is frequently the current
`HEAD` of `master`, but it's also often useful to instead use a release stabilization
branch based upon a previous release candidate when producing a release, so that
development can proceed unblocked on the `master` branch during the release candidate
testing and bug-fixing process.

In the commands below, <RELEASE> and <RELEASE_PREV> must be prefixed with a v, i.e. v1.0.9
(not 1.0.9). <COMMIT_ID> is the `git` hash identifying the commit on which the release
branch will be based. It is recommended to use the entire hash value to identify the
commit, although a prefix of at least 10 characters is also permitted.

### Create the release branch

Run the release script, which will create a branch based upon the specified commit ID,
then commit standard automated changes to that branch locally:

    $ ./zcutil/make-release.py <COMMIT_ID> <RELEASE> <RELEASE_PREV> <RELEASE_FROM> <APPROX_RELEASE_HEIGHT>

Examples:

    $ ./zcutil/make-release.py 600c4acee1 v1.1.0-rc1 v1.0.0 v1.0.0 280300
    $ ./zcutil/make-release.py b89b48cda1 v1.1.0 v1.1.0-rc1 v1.0.0 300600

### Create, Review, and Merge the release branch pull request

Review the automated changes in git:

    $ git log master..HEAD

Push the resulting branch to github:

    $ git push 'git@github.com:$YOUR_GITHUB_NAME/zcash' $(git rev-parse --abbrev-ref HEAD)

Then create the PR on github. Complete the standard review process and wait
for CI to complete.

## Make tag for the the tip of the release branch

NOTE: This has changed from the previously recommended process. The tag should be created
at the tip of the release branch; this ensures that any changes made to the `master`
branch since the initiation of the release process are not accidentally tagged as being
part of the release as a consequence of having been included in a merge commit.

Check the last commit on the local and remote versions of the release branch to make sure
they are the same:

    $ git log -1

If you haven't previously done so, set the gpg key id you intend to use for signing:

    git config --global user.signingkey <keyid>

Then create the git tag. The `-s` means the release tag will be signed.
Enter "Release <version>." and save when prompted for a commit message.
**CAUTION:** Remember the `v` at the beginning here:

    $ git tag -s v1.1.0
    $ git push origin v1.1.0

## Make and deploy deterministic builds

- Run the [Gitian deterministic build environment](https://github.com/zcash/zcash-gitian)
- Compare the uploaded [build manifests on gitian.sigs](https://github.com/zcash/gitian.sigs)
- If all is well, the DevOps engineer will build the Debian packages and update the
  [apt.z.cash package repository](https://apt.z.cash).

## Add release notes to GitHub

- Go to the [GitHub tags page](https://github.com/zcash/zcash/tags).
- Click "Add release notes" beside the tag for this release.
- Copy the release blog post into the release description, and edit to suit
  publication on GitHub. See previous release notes for examples.
- Click "Publish release" if publishing the release blog post now, or
  "Save draft" to store the notes internally (and then return later to publish
  once the blog post is up).

Note that some GitHub releases are marked as "Verified", and others as
"Unverified". This is related to the GPG signature on the release tag - in
particular, GitHub needs the corresponding public key to be uploaded to a
corresponding GitHub account. If this release is marked as "Unverified", click
the marking to see what GitHub wants to be done.

## Post Release Task List

### Merge the release branch

Merge the release branch back to `master` to ensure that any changes made during
release stabilization are reflected in the master branch's history. 

### Deploy testnet

Notify the Zcash DevOps engineer/sysadmin that the release has been tagged. They update
some variables in the company's automation code and then run an Ansible playbook, which:

* builds Zcash based on the specified branch
* deploys it as a public service (e.g. betatestnet.z.cash, mainnet.z.cash)
* often the same server can be re-used, and the role idempotently handles upgrades, but if
  not then they also need to update DNS records
* possible manual steps: blowing away the `testnet3` dir, deleting old parameters,
  restarting DNS seeder

Then, verify that nodes can connect to the testnet server, and update the guide on the
wiki to ensure the correct hostname is listed in the recommended zcash.conf.

### Update the 1.0 User Guide

This also means updating [the translations](https://github.com/zcash/zcash-docs).
Coordinate with the translation team for now. Suggestions for improving this
part of the process should be added to #2596.

### Publish the release announcement (blog, github, zcash-dev, slack)

## Celebrate
