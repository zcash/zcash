Changelog
=========

Ariel (1):
      add examples to z_getoperationresult

Ariel Gabizon (1):
      add load-wallet benchmark

Bjorn Hjortsberg (2):
      Do not warn on built in declaration mismatch
      Remove deprecated exception specification

Jack Grigg (20):
      ci-workers: Enable pipelining, and use root to set admin and host details
      Variable overrides for Arch Linux
      Rationalize currency unit to "ZEC"
      ci-workers: Fail if Python is not version 2.7
      ci-workers: Variable overrides and process tweaks for CentOS 7
      Add build progress to the release script if progressbar module is available
      Add hotfix support to release script
      Document the hotfix release process
      Enforce sequential hotfix versioning
      Benchmark time to call sendtoaddress with many UTXOs
      Fix bug in benchmark data generation script
      Adjust instructions for UTXO dataset creation
      Add GitHub release notes to release process
      Clarify branching and force-building operations in hotfix process
      Update user guide translations as part of release process
      make-release.py: Send stderr to stdout
      List dependencies for release script in release process doc
      Additional test cases for importprivkey RPC test
      make-release.py: Versioning changes for 1.0.12-rc1.
      make-release.py: Updated manpages for 1.0.12-rc1.

Jason Davies (1):
      Fix deprecation policy comment.

Nathan Wilcox (5):
      key_import_export rpc-test: verify that UTXO view co-evolves for nodes sharing a key.
      Add a new rpc-test-specified requirement: `importprivkey` outputs the associated address. (Test fails.)
      [tests pass] Output address on new key import.
      Add a new requirement that `importprivkey` API is idempotent.
      [tests pass] Ensure `importprivkey` outputs the address in case key is already imported.

Ross Nicoll (1):
      Rationalize currency unit to "BTC"

Simon Liu (3):
      Closes #2583. Exclude watch-only utxos from z_sendmany coin selection.
      Set up a clean chain.     Delete redundant method wait_until_miner_sees() via use of sync_all().
      Implement RPC shield_coinbase #2448.

kpcyrd (2):
      Fetch params from ipfs if possible
      Prefer wget over ipfs

