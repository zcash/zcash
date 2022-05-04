Zcashd Dependency Management
============================

Subtrees
----------

Several parts of the repository are git subtrees containing software maintained elsewhere.

Some of these are maintained by active developers of Zcash or Bitcoin Core, in
which case changes should probably go directly upstream without being PRed
directly against the project. They will be merged back in the next subtree
merge.

Others are external projects without a tight relationship with our project.
Changes to these should also be sent upstream, but bugfixes may also be prudent
to PR against Zcash and/or Bitcoin Core so that they can be integrated quickly.
Cosmetic changes should be purely taken upstream.

There is a tool in `test/lint/git-subtree-check.sh`
([instructions](../test/lint#git-subtree-checksh)) to check a subtree directory
for consistency with its upstream repository.

Current subtrees include:

- src/leveldb
  - Upstream at https://github.com/google/leveldb ; Maintained by Google, but
    open important PRs to Core to avoid delay.
  - **Note**: Follow the instructions in [Upgrading LevelDB](#upgrading-leveldb) when
    merging upstream changes to the LevelDB subtree.

- src/secp256k1
  - Upstream at https://github.com/bitcoin-core/secp256k1/ ; actively maintained by Core contributors.

- src/crypto/ctaes
  - Upstream at https://github.com/bitcoin-core/ctaes ; actively maintained by Core contributors.

- src/univalue
  - Upstream at https://github.com/bitcoin-core/univalue ; actively maintained
    by Core contributors, deviates from upstream https://github.com/jgarzik/univalue

