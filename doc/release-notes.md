(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Mnemonic Recovery Phrases
-------------------------

The zcashd wallet has been modified to support ZIP 339 (to be compatible with BIP 39)
which describes how to derive the wallet's HD seed from a mnemonic phrase.
The mnemonic phrase will be generated on load of the wallet, or the first time
the wallet is unlocked, and is available via the `z_exportwallet` RPC call. All
new addresses produced by the wallet are now derived from this seed using the
HD wallet functionality described in ZIP 32 and ZIP 316. For users upgrading an
existing Zcashd wallet, it is recommended that the wallet be backed up prior to
upgrading to the 4.5.2 Zcashd release.

Following the upgrade to 4.5.2, Zcashd will require that the user confirm that
they have backed up their new emergency recovery phrase, which may be obtained
from the output of the `z_exportwallet` RPC call. This confirmation can be
performed manually using the `zcashd-wallet-tool` utility that is supplied with
this release.  The wallet will not allow the generation of new addresses until
this confirmation has been performed. It is recommended that after this
upgrade, that funds tied to preexisting addresses be migrated to newly
generated addresses so that all wallet funds are recoverable using the
emergency recovery phrase going forward.  If you choose not to migrate funds in
this fashion, you will continue to need to securely back up the entire
`wallet.dat` file to ensure that you do not lose access to existing funds;
EXISTING FUNDS WILL NOT BE RECOVERABLE USING THE EMERGENCY RECOVERY PHRASE
UNLESS THEY HAVE BEEN MOVED TO A NEWLY GENERATED ADDRESS FOLLOWING THE 4.5.2
UPGRADE.

New RPC Methods
---------------

- 'walletconfirmbackup' This newly created API checks a provided emergency
  recovery phrase against the wallet's emergency recovery phrase; if the phrases
  match then it updates the wallet state to allow the generation of new addresses.
  This backup confirmation workflow can be disabled by starting zcashd with 
  `-requirewalletbackup=false` but this is not recommended unless you know what
  you're doing (and have otherwise backed up the wallet's recovery phrase anyway).
  For security reasons, this RPC method is not intended for use via zcash-cli 
  but is provided to enable `zcashd-wallet-tool` and other third-party wallet 
  interfaces to satisfy the backup confirmation requirement. Use of the 
  `walletconfirmbackup` API via zcash-cli would risk that the recovery phrase 
  being confirmed might be leaked via the user's shell history or the system
  process table; `zcashd-wallet-tool` is specifically provided to avoid this
  problem.

RPC Changes
-----------

- The results of the 'dumpwallet' and 'z_exportwallet' RPC methods have been modified
  to now include the wallet's newly generated emergency recovery phrase as part of the
  exported data.

- The results of the 'getwalletinfo' RPC have been modified to return two new fields:
  `mnemonic_seedfp` and `legacy_seedfp`, the latter of which replaces the field that
  was previously named `seedfp`. 
