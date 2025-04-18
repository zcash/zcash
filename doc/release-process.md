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

Check that `./doc/book/src/user/installation.md` has the latest apt signing key fingerprint.

Check that there are no surprising performance regressions.

Update `src/chainparams.cpp` nMinimumChainWork with information from the getblockchaininfo rpc.

Check that dependencies are up-to-date or have been postponed. If necessary,
install `cargo-upgrades` and `cargo-audit`:

```
cargo install cargo-upgrades cargo-audit
```

Then run each of the following:

```
$ ./qa/zcash/updatecheck.py
$ cargo upgrades
$ cargo audit
```

You can optionally create a file `~/.local/share/zcash/updatecheck/token` (or
`$XDG_DATA_HOME/zcash/updatecheck/token` if the `XDG_DATA_HOME` environment
variable is set) to avoid running into GitHub rate limiting. Create an
unprivileged personal access token on GitHub and copy the value into the file.

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

    $ git checkout -b version-X.Y.0 <COMMIT_ID>
    $ git push 'git@github.com:zcash/zcash' $(git rev-parse --abbrev-ref HEAD)

### Review & update the release notes

Now is the time to make sure that all of the changes since the previous release
are property represented in `doc/release-notes.md`. Determine the tag that will
be used for the `RELEASE_FROM` value by manually searching for the tag for the
latest full release in the list obtained by running

    $ git tag -l --sort=-taggerdate

and then finding the most recent non-release-candidate version. If you have a
git remote for the upstream `bitcoin-core` repository, be aware that Bitcoin
tags will also appear in this list. Then, skim the full diff since the last
release and make certain that all user-visible changes are properly documented
in `doc/release-notes.md`.

    $ git diff <RELEASE_FROM>..HEAD

If you make any changes to the release notes, add commits to do so as
necessary, but do NOT push these commits to the `version-X.Y.Z` branch on the
upstream repository; they will be included in the release branch that will be
created in the next step.

### Create the release branch

Run the release script to create the release branch. This will create a branch
based upon the specified commit ID, then commit standard automated changes to
that branch locally:

    $ ./zcutil/make-release.py <COMMIT_ID> <RELEASE> <RELEASE_PREV> <RELEASE_FROM> <APPROX_RELEASE_HEIGHT>

Examples:

    $ ./zcutil/make-release.py 600c4acee1 v1.1.0-rc1 v1.0.0 v1.0.0 280300
    $ ./zcutil/make-release.py b89b48cda1 v1.1.0 v1.1.0-rc1 v1.0.0 300600

Ordinarily, we choose a release height that is a couple hundred blocks in
the future in order to give time for CI to run.

If it is necessary that the version you are building reach its end-of-service
halt in less time than the ordinary 16 weeks, it may be necessary to modify
the value of the `RELEASE_TO_DEPRECATION_WEEKS` constant in `deprecation.h`
prior to running `make-release.py`.

### Create, Review, and Merge the release branch pull request

Review the automated changes in git:

    $ git log version-X.Y.0..HEAD

Push the resulting branch to github:

    $ git push 'git@github.com:$YOUR_GITHUB_NAME/zcash' $(git rev-parse --abbrev-ref HEAD)

Then create the PR on github targeting the `version-X.Y.0` branch. Complete the
standard review process and wait for CI to complete.

## Make a tag for the tip of the release branch

NOTE: This has changed from the previously recommended process. The tag should
be created at the tip of the automatically-generated release branch created by
the release script; this ensures that any changes made to the release
stabilization branch since the initiation of the release process are not
accidentally tagged as being part of the release as a consequence of having
been included in a merge commit.

Check the last commit on the local and remote versions of the release branch to
make sure they are the same:

    $ git log -1

If you haven't previously done so, set the gpg key id you intend to use for
signing:

    git config --global user.signingkey <keyid>

Then create the git tag. The `-s` means the release tag will be signed.  Enter
"Release <version>." and save when prompted for a commit message.  **CAUTION:**
Remember the `v` at the beginning here:

    $ git tag -s vX.Y.Z[-rcN]

For release tags (not release candidates) copy the "Notable Changes" section of
the release notes into the tag body when creating the tag. This makes it easy
for command-line `git` users to review the changes in a release without having
to check out the tag.

    $ git push origin vX.Y.Z[-rcN]

## Merge the release branch to the release stabilization branch

Once CI has completed and the release branch has sufficient approving reviews,
merge the release branch back to the release stabilization branch. Testing
proceeds as normal. Any changes that need to be made during the release
candidate period are made by submitting PRs targeting the release stabilization
branch.

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
- Copy the "Notable Changes" section of the release notes into the release
  description, and edit to suit publication on GitHub. See previous release
  notes for examples.
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

### Publish the release announcement (blog, github, zcash-dev, slack)

## Celebrate
