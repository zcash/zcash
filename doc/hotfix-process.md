Hotfix Release Process
======================

Hotfix releases are versioned by incrementing the patch number of the latest
release. For example:

    First hotfix:  v1.0.0 -> v1.0.1
    Second hotfix: v1.0.1 -> v1.0.2

In the commands below, <RELEASE> and <RELEASE_PREV> are prefixed with a v, ie.
v1.0.2 (not 1.0.2).

## Create a hotfix branch

Create a hotfix branch from the previous release tag, and push it to the main
repository:

    $ git branch hotfix-<RELEASE> <RELEASE_PREV>
    $ git push 'git@github.com:zcash/zcash' hotfix-<RELEASE>

## Implement hotfix changes

Hotfix changes are implemented the same way as regular changes (developers work
in separate branches per change, and push the branches to their own repositories),
except that the branches are based on the hotfix branch instead of master:

    $ git checkout hotfix-<RELEASE>
    $ git checkout -b <BRANCH_NAME>

## Merge hotfix PRs

Hotfix PRs are created like regular PRs, except using the hotfix branch as the
base instead of `master`. Each PR should be reviewed and merged as normal.

## Release process

The majority of this process is identical to the standard release process.
Release candidates for hotfixes should be created and tested as normal, using
the `hotfix-<RELEASE>` branch in place of the release stabilization branch,
with a couple of minor differences:

- When running the release script, use the `--hotfix` flag. Provide the hash of 
  the commit to be released as the first argument:

    $ ./zcutil/make-release.py --hotfix <COMMIT_ID> <RELEASE> <RELEASE_PREV> <APPROX_RELEASE_HEIGHT>

- To review the automated changes in git:

    $ git log hotfix-<RELEASE>..HEAD
