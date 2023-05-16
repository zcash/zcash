Release Process
====================
Meta: There should always be a single release engineer to disambiguate responsibility.

If this is a hotfix release, please see the [Hotfix Release
Process](./hotfix-process.md) documentation before proceeding.

## Pre-pre-release

These are steps that can be considered done even weeks ahead of the release, so if you know you are going to be running an upcoming release, you can get ahead of the game and start these steps.

### (optional) have a GitHub token for updatecheck.py

This isn’t necessary, but can help avoid rate limiting failures from the GitHub API.

If you had one in the past, it might have expired. If it did, it’s easy to regenerate it.

1. visit [GitHub’s personal access tokens page](https://github.com/settings/tokens?type=beta).
2. if you have an existing “ECC updatecheck” token, check if it says “This token has expired.” If not, you’re probably good to go, but if it is expired, click on the token name, then click “Regenerate taken” and jump to step 5.
3. if you don’t have one, click “Generate new token”
4. Fill in “ECC updatecheck” for the token name and click “Generate token” (you can change the expiration or description if you like, but don’t change the resource owner (you), repository access (public repositories), or permissions)
5. copy the new token and write it to `${XDG_DATA_HOME:=~/.local/share}/zcash/updatecheck/token`, replacing any existing contents – if you close the page before writing it to the file, you won’t be able to access the token again, so jump back to step 1.

### set up PGP for signing the tag

You will need git to be able to sign with your PGP key to complete the release, so make sure it all works now.

**TODO**: Add steps here that cover creating and setting up a PGP key to use with git.

## Pre-release

### Github Milestone

Ensure all goals for the github milestone are met. If not, remove tickets
or PRs with a comment as to why it is not included. (Running out of time
is a common reason.)

### Pre-release checklist:

#### Check that dependencies are properly hosted.

**What does this mean?**

#### Check that there are no surprising performance regressions.

**How do we check for performance regressions?**

#### Update [`src/chainparams.cpp`](../src/chainparams.cpp)

Call `getblockchaininfo` against an up-to-date mainnet note and copy the value of the “chainwork” field to `consensus.nMinimumChainWork` in the definition of `CMainParams::CMainParams()` (be careful to not update `CTestNetParams` or `CRegTestParams`).

#### Check that dependencies are up-to-date or have been postponed.

See the [pre-pre-relase section](#pre-pre-release) for optional setup.

```bash
$ ./qa/zcash/updatecheck.py
```

If there are updates that have not been postponed, review their changelogs
for urgent security fixes, and if there aren't any, postpone the update by
adding a line to `qa/zcash/postponed-updates.txt`.

**TODO**: For hotfixes, should we maybe backport changes to this from master? As it may have changed since the release branched.

**TODO**: If you follow this process, then this PR is made against master (because we don’t yet have a release candidate or release stabilization branch), but for hotfixes it will be made against the hotfix branch. Should we change this process to match what’s required on the hotfix side?

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
- the python modules `progressbar2` (optional - displays a progress bar),
  `requests` and `xdg`

## Versioning

Zcash version identifiers have the format `vX.Y.Z` with the following conventions:

* Increments to the `X` component (the "major version") correspond to network
  upgrades. A network upgrade occurs only when there is a change to the
  consensus rules.
* Increments to the `Y` component (the "minor version") correspond to regular
  Zcash releases. These occur approximately every 6 weeks and may include breaking
  changes to public APIs.
* Increments to the `Z` component occur only in the case of hotfix releases.

## Release candidate & release process

Identify the commit from which the release stabilization branch will be made.
Release stabilization branches are used so that development can proceed
unblocked on the `master` branch during the release candidate testing and
bug-fixing process. By convention, release stabilization branches are named
`version-X.Y.0` where `X` and `Y` are the major and minor versions for the
release.

In the commands below, <RELEASE> and <RELEASE_PREV> must identify `git` tags
prefixed with the character `v`, i.e. `v1.0.9` (not `1.0.9`). <COMMIT_ID> is a
`git` hash identifying the commit on which a release stabilization or release
branch will be based. It is recommended to use the entire hash value to
identify the commit, although a prefix of at least 10 characters is also
permitted.

### Create the release stabilization branch

Having identified the commit from which the release will be made, the release
manager constructs the release stabilization branch as follows:

```bash
$ git checkout -b version-X.Y.0 <COMMIT_ID>
$ git push 'git@github.com:zcash/zcash' $(git rev-parse --abbrev-ref HEAD)
```

### Update the release notes

Make sure the existing [release notes](./release-notes.md) cover everything significant in the release and make any necessary changes in a PR against the release stabilization branch.

### Create the release candidate branch

Run the release script to create the first release candidate. This will create
a branch based upon the specified commit ID, then commit standard automated
changes to that branch locally:

The `APPROX_RELEASE_HEIGHT` should be some time after the current height, which can be retrieved either from another `getblockchaininfo` call against a mainnet node (the “estimatedheight” field) or some online chain explorer. A four-hour window from now is reasonable, and there are 48 blocks per hour, so generally add `200` to the current chain height.

**NB**: Hotfixes are slightly different here, refer back to the [Hotfix Release
Process](./hotfix-process.md).

**maybe merge the hotfix process doc, calling out the differences along the way**

**NB**: Ensure your working tree is clean before running this, as it will change branches and make changes that won’t work on a dirty tree.

```bash
$ ./zcutil/make-release.py <COMMIT_ID> <RELEASE> <RELEASE_PREV> <RELEASE_FROM> <APPROX_RELEASE_HEIGHT>
```

Examples:

```bash
$ ./zcutil/make-release.py 600c4acee1 v1.1.0-rc1 v1.0.0 v1.0.0 280300
$ ./zcutil/make-release.py b89b48cda1 v1.1.0 v1.1.0-rc1 v1.0.0 300600
```

### Create, Review, and Merge the release branch pull request

Review the automated changes in git:

```bash
$ git log version-X.Y.0..HEAD
```

Push the resulting branch to github:

```bash
$ git push 'git@github.com:$YOUR_GITHUB_NAME/zcash' $(git rev-parse --abbrev-ref HEAD)
```

Then create the PR on github targeting the `version-X.Y.0` branch. Complete the
standard review process and wait for CI to complete.

## Make a tag for the tip of the release candidate branch

NOTE: This has changed from the previously recommended process. The tag should
be created at the tip of the automatically-generated release branch created by
the release script; this ensures that any changes made to the release
stabilization branch since the initiation of the release process are not
accidentally tagged as being part of the release as a consequence of having
been included in a merge commit.

Check the last commit on the local and remote versions of the release branch to
make sure they are the same:

```bash
$ git log -1
```

If you haven't previously done so, set the gpg key id you intend to use for
signing (See the [pre-pre-relase section](#pre-pre-release) for PGP setup):

```bash
$ git config --global user.signingkey <keyid>
```

Then create the git tag. The `-s` means the release tag will be signed. **CAUTION:**
Remember the `v` at the beginning here:

```bash
$ git tag -m 'Release vX.Y.Z-rcN.' -s vX.Y.Z-rcN
$ git push 'git@github.com:zcash/zcash' vX.Y.Z-rcN
```

## Merge the release candidate branch to the release stabilization branch

Once CI has completed and the release candidate branch has sufficient approving
reviews, merge the release candidate branch back to the release stabilization
branch. Testing proceeds as normal. Any changes that need to be made during the
release candidate period are made by submitting PRs targeting the release
stabilization branch.

Subsequent release candidates, and the creation of the final release, follow
the same process as for release candidates, omitting the `-rcN` suffix for the
final release.

## Make and deploy deterministic builds

- Run the [Gitian deterministic build environment](https://github.com/zcash/zcash-gitian)
- Compare the uploaded [build manifests on gitian.sigs](https://github.com/zcash/gitian.sigs)
- If all is well, the DevOps engineer will build the Debian packages and update the
  [apt.z.cash package repository](https://apt.z.cash).

## Add release notes to GitHub

The following part of the release process applies only to final releases, not
release candidates.

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

### Merge the release stabilization branch

Once the final release branch has merged to the release stabilization branch, a
new PR should be opened for merging the release stabilization branch into
master. This may require fixing merge conflicts (e.g. changing the version
number in the release stabilization branch to match master, if master is
ahead). Such conflicts **MUST** be addressed with additional commits to the
release stabilization branch; specifically, the branch **MUST NOT** be rebased
on master.

Once any conflicts have been resolved, the release stabilization branch should
be merged back to the `master` branch, and then deleted.

### Deploy testnet

Notify the Zcash DevOps engineer/sysadmin that the release has been tagged. They update
some variables in the company's automation code and then run an Ansible playbook, which:

* builds Zcash based on the specified branch
* deploys it as a public service (e.g. testnet.z.cash, mainnet.z.cash)
* often the same server can be re-used, and the role idempotently handles upgrades, but if
  not then they also need to update DNS records
* possible manual steps: blowing away the `testnet3` dir, deleting old parameters,
  restarting DNS seeder.

Verify that nodes can connect to the mainnet and testnet servers.

Update the [Zcashd Full Node and CLI](https://zcash.readthedocs.io/en/latest/rtd_pages/zcashd.html)
documentation on ReadTheDocs to give the new version number.

### Publish the release announcement (blog, github, zcash-dev, slack)

### Update this process

Submit a PR against master containing any adjustments to or clarifications of this process that you needed to make to get to this point.

## Celebrate
