Hotfix Release Process
======================

Hotfix releases are versioned by incrementing the build number of the latest
release. For example:

    First hotfix:  v1.0.11   -> v1.0.11-1
    Second hotfix: v1.0.11-1 -> v1.0.11-2

In the commands below, <RELEASE> and <RELEASE_PREV> are prefixed with a v, ie.
v1.0.11 (not 1.0.11).

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
base instead of master. Each PR should be reviewed as normal, and then the
following process should be used to merge:

- A CI merge build is manually run by logging into the CI server, going to the
  pr-merge builder, clicking the "force" button, and entering the following
  values:

  - Repository: https://github.com/<DevUser>/zcash
    - <DevUser> must be in the set of "safe" users as-specified in the CI
      config.
  - Branch: name of the hotfix PR branch (not the hotfix release branch).

- A link to the build and its result is manually added to the PR as a comment.

- If the build was successful, the PR is merged via the GitHub button.

## Release process

The majority of this process is identical to the standard release process.
However, there are a few notable differences:

- When running the release script, use the `--hotfix` flag:

    $ ./zcutil/make-release.py --hotfix <RELEASE> <RELEASE_PREV> <APPROX_RELEASE_HEIGHT>

- To review the automated changes in git:

    $ git log hotfix-<RELEASE>..HEAD

- After the standard review process, use the hotfix merge process outlined above
  instead of the regular merge process.

- When making the tag, check out the hotfix branch instead of master.

## Post-release

Once the hotfix release has been created, a new PR should be opened for merging
the hotfix release branch into master. This may require fixing merge conflicts
(e.g. changing the version number in the hotfix branch to match master, if
master is ahead). Such conflicts **MUST** be addressed with additional commits
to the hotfix branch; specifically, the branch **MUST NOT** be rebased on
master.
