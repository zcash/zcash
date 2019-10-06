Changelog
=========

Braydon Fuller (1):
      tests: adds unit test for IsPayToPublicKeyHash method

Dimitris Apostolou (1):
      Electric Coin Company

Eirik0 (22):
      Split test in to multiple parts
      Use a custom error type if creating joinsplit descriptions fails
      Rename and update comment
      Add rpc to enable and disable Sprout to Sapling migration
      Move migration logic to ChainTip
      Documentation cleanup
      Additional locking and race condition prevention
      Refactor wait_and_assert_operationid_status to allow returning the result
      Set min depth when selecting notes to migrate
      Check for full failure message in test case
      Add migration options to conf file
      Create method for getting HD seed in RPCs
      Add rpc to get Sprout to Sapling migration status
      Fix help message
      Test migration using both the parameter and the default Sapling address
      Fix typos and update documentation
      use -valueBalance rather than vpub_new to calculate migrated amount
      Do not look at vin/vout when determining migration txs and other cleanup
      Calculate the number of confimations in the canonical way
      Do not throw an exception if HD Seed is not found when exporting wallet
      make-release.py: Versioning changes for 2.0.5-rc1.
      make-release.py: Updated manpages for 2.0.5-rc1.

Gareth Davies (1):
      Adding addressindex.h to Makefile.am

Jack Grigg (9):
      Add Sprout support to TransactionBuilder
      depends: Use full path to cargo binary
      depends: Generalise the rust package cross-compilation functions
      depends: Add rust-std hash for aarch64-unknown-linux-gnu
      depends: Compile bdb with --disable-atomics on aarch64
      depends: Update .gitignore
      configure: Guess -march for libsnark OPTFLAGS instead of hard-coding
      Add Blossom to upgrade list
      init: Fix new HD seed generation for previously-encrypted wallets

Larry Ruane (6):
      fix enable-debug build DB_COINS undefined
      add -addressindex changes for bitcore insight block explorer
      add -spentindex changes for bitcore insight block explorer
      Update boost from v1.69.0 to v1.70.0. #3947
      add -timestampindex for bitcore insight block explorer
      3873 z_setmigration cli bool enable arg conversion

Marius Kjærstad (1):
      Update _COPYRIGHT_YEAR in configure.ac to 2019

Mary Moore-Simmons (1):
      Creates checklist template for new PRs being opened and addresses Str4d's suggestion for using GitHub handles

Simon Liu (4):
      Add testnet and regtest experimental feature: -developersetpoolsizezero
      Add qa test for experimental feature: -developersetpoolsizezero
      Enable ZIP209 on mainnet and set fallback Sprout pool balance.
      Enable experimental feature -developersetpoolsizezero on mainnet.

Jack Grigg (1):
      remove extra hyphen

zebambam (1):
      Minor speling changes

