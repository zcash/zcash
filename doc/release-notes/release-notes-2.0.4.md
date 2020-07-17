Notable changes
===============

Sprout note validation bug fixed in wallet
------------------------------------------
We include a fix for a bug in the Zcashd wallet which could result in Sprout
z-addresses displaying an incorrect balance. Sapling z-addresses are not
impacted by this issue. This would occur if someone sending funds to a Sprout
z-address intentionally sent a different amount in the note commitment of a
Sprout output than the value provided in the ciphertext (the encrypted message
from the sender).

Users should install this update and then rescan the blockchain by invoking
“zcashd -rescan”. Sprout address balances shown by the zcashd wallet should
then be correct.

Thank you to Alexis Enston for bringing this to our attention.

[Security Announcement 2019-03-19](https://z.cash/support/security/announcements/security-announcement-2019-03-19/)

[Pull request](https://github.com/zcash/zcash/pull/3897)

Miner address selection behaviour fixed
---------------------------------------
Zcash inherited a bug from upstream Bitcoin Core where both the internal miner
and RPC call `getblocktemplate` would use a fixed transparent address, until RPC
`getnewaddress` was called, instead of using a new transparent address for each
mined block.  This was fixed in Bitcoin 0.12 and we have now merged the change.

Miners who wish to use the same address for every mined block, should use the
`-mineraddress` option. 

[Mining Guide](https://zcash.readthedocs.io/en/latest/rtd_pages/zcash_mining_guide.html)


New consensus rule: Reject blocks that violate turnstile (Testnet only)
-----------------------------------------------------------------------
Testnet nodes will now enforce a consensus rule which marks blocks as invalid
if they would lead to a turnstile violation in the Sprout or Sapling value
pools. The motivations and deployment details can be found in the accompanying
[ZIP draft](https://github.com/zcash/zips/pull/210).

The consensus rule will be enforced on mainnet in a future release.

[Pull request](https://github.com/zcash/zcash/pull/3885)


Changelog
=========

Eirik Ogilvie-Wigley (14):
      Rename methods to include Sprout
      Add benchmark for decrypting sapling notes
      Move reusable Sapling test setup to utiltest
      Move test SaplingNote creation to utiltest
      Add test method for generating master Sapling extended spending keys
      Include Sapling transactions in increment note witness benchmark
      Prevent header from being included multiple times
      benchmarks do not require updating network parameters
      FakeCoinsViewDB can inherit directly from CCoinsView
      Add a method for generating a test CKey
      Change to t->z transaction and create separate benchmark for sapling
      Renaming and other minor cleanup
      Improve some error messages when building a transaction fails
      Add missing author aliases

Gareth Davies (1):
      Correcting logo on README

Jack Grigg (6):
      Add Sapling benchmarks to benchmark runner
      test: Fetch coinbase address from coinbase UTXOs
      test: Make expected_utxos optional in get_coinbase_address()
      Add comments
      Move utiltest.cpp from wallet to common
      Move payment disclosure code and tests into wallet

Jonas Schnelli (4):
      detach wallet from miner
      fix GetScriptForMining() CReserveKey::keepKey() issue
      add CReserveScript to allow modular script keeping/returning
      miner: rename UpdateRequestCount signal to ResetRequestCount

Jonathan "Duke" Leto (2):
      Backport size_on_disk to RPC call getblockchaininfo.
      Add size_on_disk test

Marius Kjærstad (1):
      Update COPYRIGHT_YEAR in clientversion.h to 2019

Paige Peterson (1):
      redirect and update source documentation

Pieter Wuille (1):
      Simplify DisconnectBlock arguments/return value

Sean Bowe (13):
      (testnet) Fall back to hardcoded shielded pool balance to avoid reorgs.
      (testnet) Reject blocks that result in turnstile violations
      (testnet/regtest) Avoid mining transactions that would violate the turnstile.
      Fix tallying for Sprout/Sapling value pools.
      Consolidate logic to enable turnstile auditing for testnet/regtest/mainnet.
      Use existing chainparams variable
      Add newlines to turntile log messages for miner
      Check blockhash of fallback block for Sprout value pool balance
      Change SproutValuePoolCheckpointEnabled to ZIP209Activated
      Only enforce Sapling turnstile if balance values have been populated.
      Do not enable ZIP209 on regtest right now.
      (minor) Remove added newline.
      (wallet) Check that the commitment matches the note plaintext provided by the sender.

Simon Liu (9):
      Update nMinimumChainWork using block 497000.
      Add checkpoint for block 497000.
      Update release notes for 2.0.4
      make-release.py: Versioning changes for 2.0.4-rc1.
      make-release.py: Updated manpages for 2.0.4-rc1.
      make-release.py: Updated release notes and changelog for 2.0.4-rc1.
      Fix typo in release notes.
      make-release.py: Versioning changes for 2.0.4.
      make-release.py: Updated manpages for 2.0.4.

Taylor Hornby (8):
      Update OpenSSL from 1.1.0h to 1.1.1a. #3786
      Update boost from v1.66.0 to v1.69.0. #3786
      Update Rust from v1.28.0 to v1.32.0. #3786
      Update Proton from 0.17.0 to 0.26.0. #3816, #3786
      Patch Proton for a minimal build. #3786
      Fix proton patch regression. #3916
      Fix OpenSSL reproducible build regression
      Patch out proton::url deprecation as workaround for build warnings

sandakersmann (1):
      Update of copyright year to 2019

zebambam (2):
      Added documentation warnings about DNS rebinding attacks, issue #3841
      Added responsible disclosure statement for issue #3869

