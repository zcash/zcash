Release Process
====================
Meta: There should always be a single release engineer to disambiguate responsibility.

## Pre-release

### Github Milestone

Ensure all goals for the github milestone are met. If not, remove tickets
or PRs with a comment as to why it is not included. (Running out of time
is a common reason.)

### Pre-release checklist:

Check that dependencies are properly hosted by looking at the `check-depends` builder:

  https://ci.z.cash/#/builders/1

Check that there are no surprising performance regressions:

  https://speed.z.cash

Ensure that new performance metrics appear on that site.

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

## Release process

Run the release script, which will verify you are on the latest clean
checkout of master, create a branch, then commit standard automated
changes to that branch locally:

    $ ./zcutil/make-release.py <RELEASE> <RELEASE_PREV> <APPROX_RELEASE_HEIGHT>

Example:

    $ ./zcutil/make-release.py v1.0.9 v1.0.8-1 120000

### Create, Review, and Merge the release branch pull request

Review the automated changes in git:

    $ git log master..HEAD

Push the resulting branch to github:

    $ git push 'git@github.com:$YOUR_GITHUB_NAME/zcash' $(git rev-parse --abbrev-ref HEAD)

Then create the PR on github. Complete the standard review process,
then merge, then wait for CI to complete.

## Make tag for the newly merged result

Checkout master and pull the latest version to ensure master is up to date with the release PR which was merged in before.

    $ git checkout master
    $ git pull --ff-only

Check the last commit on the local and remote versions of master to make sure they are the same:

    $ git log -1

The output should include something like, which is created by Homu:

    Auto merge of #4242 - nathan-at-least:release-v1.0.9, r=nathan-at-least

Then create the git tag. The `-s` means the release tag will be
signed. **CAUTION:** Remember the `v` at the beginning here:

    $ git tag -s v1.0.9
    $ git push origin v1.0.9

## Make and deploy deterministic builds

- Run the [Gitian deterministic build environment](https://github.com/zcash/zcash-gitian)
- Compare the uploaded [build manifests on gitian.sigs](https://github.com/zcash/gitian.sigs)
- If all is well, the DevOps engineer will build the Debian packages and update the
  [apt.z.cash package repository](https://apt.z.cash).

## Post Release Task List

### Deploy testnet

Notify the Zcash DevOps engineer/sysadmin that the release has been tagged. They update some variables in the company's automation code and then run an Ansible playbook, which:

* builds Zcash based on the specified branch
* deploys it as a public service (e.g. betatestnet.z.cash, mainnet.z.cash)
* often the same server can be re-used, and the role idempotently handles upgrades, but if not then they also need to update DNS records
* possible manual steps: blowing away the `testnet3` dir, deleting old parameters, restarting DNS seeder

Then, verify that nodes can connect to the testnet server, and update the guide on the wiki to ensure the correct hostname is listed in the recommended zcash.conf.

### Update the 1.0 User Guide

### Publish the release announcement (blog, zcash-dev, slack)

## Celebrate
