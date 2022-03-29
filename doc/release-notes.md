(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Mnemonic Recovery Phrases
-------------------------

The zcashd wallet has been modified to support BIP 39, which describes how to
derive the wallet's HD seed from a mnemonic phrase.  The mnemonic phrase will
be generated on load of the wallet, or the first time the wallet is unlocked,
and is available via the `z_exportwallet` RPC call. All new addresses produced
by the wallet are now derived from this seed using the HD wallet functionality
described in ZIP 32 and ZIP 316. For users upgrading an existing Zcashd wallet,
it is recommended that the wallet be backed up prior to upgrading to the 4.7.0
Zcashd release.

Following the upgrade to 4.7.0, Zcashd will require that the user confirm that
they have backed up their new emergency recovery phrase, which may be obtained
from the output of the `z_exportwallet` RPC call. This confirmation can be
performed manually using the `zcashd-wallet-tool` utility that is supplied
with this release (built or installed in the same directory as `zcashd`).
The wallet will not allow the generation of new addresses until this
confirmation has been performed. It is recommended that after this upgrade,
that funds tied to preexisting addresses be migrated to newly generated
addresses so that all wallet funds are recoverable using the emergency
recovery phrase going forward. If you choose not to migrate funds in this
fashion, you will continue to need to securely back up the entire `wallet.dat`
file to ensure that you do not lose access to existing funds; EXISTING FUNDS
WILL NOT BE RECOVERABLE USING THE EMERGENCY RECOVERY PHRASE UNLESS THEY HAVE
BEEN MOVED TO A NEWLY GENERATED ADDRESS FOLLOWING THE 4.7.0 UPGRADE.

Wallet Updates
--------------

The zcashd wallet has been modified to alter the way that change is handled. In the case
that funds are being spent from a unified account, change is sent to a wallet-internal
change address for that account instead of sending change amounts back to the original
address where a note being spent was received.  The rationale for this change is that it
improves the security that is provided to the user of the wallet when supplying incoming
viewing keys to third parties; previously, an incoming viewing key could effectively be
used to detect when a note was spent (hence violating the "incoming" restriction) by
observing change outputs that were sent back to the address where the spent note was
originally received.

New RPC Methods
---------------

- `walletconfirmbackup` This newly created API checks a provided emergency
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
- `z_getnewaccount` This API allows for creation of new BIP 44 / ZIP 32
  accounts using HD derivation from the wallet's mnemonic seed. Each account represents a
  separate spending authority and source of funds. A single account may contain funds in
  the Sapling and Orchard shielded pools, as well as funds held in transparent addresses.
- `z_getaddressforaccount` This API allows for creation of diversified unified
  addresses under a single account. Each call to this API will, by default, create
  a new diversified unified address containing p2pkh, Sapling, and Orchard receivers.
  Additional arguments to this API may be provided to request the address to be created
  with a user-specified set of receiver types and diversifier index.
- `z_getbalanceforaccount` This API makes it possible to obtain balance information
  on a per-account basis.
- `z_getbalanceforviewingkey` This newly created API allows a user to obtain
  balance information for funds visible to a Sapling or Unified full viewing key; if a
  Sprout viewing key is provided, this method allows retrieval of the balance only in the
  case that the wallet controls the corresponding spending key.  This API has been added
  to supplement (and largely supplant) `z_getbalance`. Querying for balance by a single
  address returns only the amount received by that address, and omits value sent to other
  diversified addresses derived from the same full viewing key; by using
  `z_getbalanceforviewingkey` it is possible to obtain a correct balance that includes all
  amounts controlled by a single spending key, including both those sent to external
  diversified addresses and to wallet-internal change addresses.
- `z_listunifiedreceivers` This API allows the caller to extract the individual
  component receivers from a unified address. This is useful if one needs to
  provide a bare Sapling or p2pkh address to a service that does not yet
  support unified addresses.

RPC Changes
-----------

- The result type for the `listaddresses` endpoint has been modified:
  - The `keypool` source type has been removed; it was reserved but not used.
  - In the `sapling` address results, the `zip32AccountId` attribute has been
    removed in favor of `zip32KeyPath`. This is to allow distinct key paths to
    be reported for addresses derived from the legacy account under different
    child spending authorities, as are produced by `z_getnewaddress`.
- The results of the 'dumpwallet' and 'z_exportwallet' RPC methods have been modified
  to now include the wallet's newly generated emergency recovery phrase as part of the
  exported data.
- The results of the `getwalletinfo` RPC have been modified to return two new fields:
  `mnemonic_seedfp` and `legacy_seedfp`, the latter of which replaces the field that
  was previously named `seedfp`.
- A new `pool` attribute has been added to each element returned by `z_listunspent`
  to indicate which value pool the unspent note controls funds in.
- `z_listreceivedbyaddress` a `pool` attribute has been added to each result to indicate
  what pool the received funds are held in.
- `z_viewtransaction` has been updated to include attributes that provide
  information about Orchard components of the transaction. Also, the `type`
  attribute for spend and output values has been deprecated and replaced
  by the `pool` attribute.
- `z_getnotescount` now also returns information for Orchard notes.
- The output format of `z_exportwallet` has been changed. The exported file now
  includes the mnemonic seed for the wallet, and HD keypaths are now exported for
  transparent addresses when available.
- The result value for `z_importviewingkey` now includes an `address_type` field
  that replaces the now-deprecated `type` key.

### 'z_sendmany'

- The `z_sendmany` RPC call no longer permits Sprout recipients in the
  list of recipient addresses. Transactions spending Sprout funds will
  still result in change being sent back into the Sprout pool, but no
  other `Sprout->Sprout` transactions will be constructed by the Zcashd
  wallet.

- The restriction that prohibited `Sprout->Sapling` transactions has been
  lifted; however, since such transactions reveal the amount crossing
  pool boundaries, they must be explicitly enabled via a parameter to
  the `z_sendmany` call.

- A new string parameter, `privacyPolicy`, has been added to the list of
  arguments accepted by `z_sendmany`. This parameter enables the caller to
  control what kind of information they permit `zcashd` to reveal when creating
  the transaction. If the transaction can only be created by revealing more
  information than the given strategy permits, `z_sendmany` will return an
  error. The parameter defaults to `LegacyCompat`, which applies the most
  restrictive strategy `FullPrivacy` when a Unified Address is present as the
  sender or a recipient, and otherwise preserves existing behaviour (which
  corresponds to the `AllowFullyTransparent` policy).

- Since Sprout outputs are no longer created (with the exception of change)
  `z_sendmany` no longer generates payment disclosures (which were only
  available for Sprout outputs) when the `-paymentdisclosure` experimental
  feature flag is set.

RPC Deprecations
----------------

- `z_getnewaddress` has been deprecated in favor of `z_getnewaccount` and
  `z_getaddressforaccount`.
- `z_listaddresses` has been deprecated. Use `listaddresses` instead.
- `z_getbalance` has been deprecated. Use `z_getbalanceforviewingkey` instead.
  See the discussion of how change is now handled under the `Wallet` heading for
  additional background.
- `z_gettotalbalance` has been deprecated. Use `z_getbalanceforaccount` instead.
- `dumpwallet` has been deprecated. Use `z_exportwallet` instead.

Platform Support
----------------

- Debian 9 has been removed from the list of supported platforms.
