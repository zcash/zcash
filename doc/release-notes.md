(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Changes to Testnet NU5 Consensus Rules
--------------------------------------

- In order to better support hardware wallets, transparent signature hash construction as
  defined in [ZIP 244](https://zips.z.cash/zip-0244) has been modified to include a hash
  of the serialization of the amounts of all outputs being spent, along with a hash of all
  spent outputs `scriptPubKeys` values, except in the case that the `ANYONECANPAY` flag is
  set. This allows hardware wallet devices to verify the UTXO amounts without having to
  stream all the previous transactions containing the outputs being spent to the device.
  Also as part of these changes, the transparent signature hash digest now commits
  directly, rather than implicitly, to the sighash type, and the sighash type is
  restricted to a fixed set of valid values. The change to ZIP 244 can be seen 
  [here](https://github.com/zcash/zips/commit/ac9dd97f77869b9191dc9592faa3ebf45475c341).
- This release fixes a bug in `v4.6.0` that caused a consensus failure on the Zcash
  testnet at height `1,779,200`. For details see
  <https://github.com/zcash/zcash/commit/5990853de3e9f04f28b7a4d59ef9e549eb4b4cca>.
- There have been changes to the Halo2 proving system to improve consistency between the
  specification and the implementation, and these may break compatibility. See for example
  <https://github.com/zcash/halo2/commit/247cd620eeb434e7f86dc8579774b30384ae02b2>.
- There have been numerous changes to the Orchard circuit impementation since `v4.6.0`.
  See <https://github.com/zcash/orchard/issues?q=label%3AM-consensus-change-since-0.1.0-beta-1>
  for a complete list.
- A potential Faerie Gold vulnerability affecting the previous activation of NU5 on
  testnet and existing since `v4.6.0` has been mitigated.

NU5 Testnet Reactivation
------------------------

To support the aforementioned testnet consensus changes, the following changes are made in
`zcashd v4.7.0`:

- The consensus branch ID for NU5 is changed to `0xC2D6D0B4`.
- The protocol version indicating NU5-aware testnet nodes is set to `170050`.
- The testnet reactivation height for NU5 is set to **1,840,000**.

Testnet nodes that upgrade to `zcashd v4.7.0` prior to block height `1,840,000` will follow
the new testnet network upgrade. Testnet nodes that are running `zcashd v4.6.0-2` at that
height will need to upgrade to `v4.7.0` and then run with `-reindex`.

Mnemonic Recovery Phrases
-------------------------

The zcashd wallet has been modified to support BIP 39, which describes how to derive the
wallet's HD seed from a mnemonic phrase.  The mnemonic phrase will be generated on load of
the wallet, or the first time the wallet is unlocked, and is available via the
`z_exportwallet` RPC call. All new addresses produced by the wallet are now derived from
this seed using the HD wallet functionality described in ZIP 32 and ZIP 316. For users
upgrading an existing Zcashd wallet, it is recommended that the wallet be backed up prior
to upgrading to the 4.7.0 Zcashd release.

Following the upgrade to 4.7.0, Zcashd will require that the user confirm that they have
backed up their new emergency recovery phrase, which may be obtained from the output of
the `z_exportwallet` RPC call. This confirmation can be performed manually using the
`zcashd-wallet-tool` utility that is supplied with this release (built or installed in the
same directory as `zcashd`).  The wallet will not allow the generation of new addresses
until this confirmation has been performed. It is recommended that after this upgrade,
that funds tied to preexisting addresses be migrated to newly generated addresses so that
all wallet funds are recoverable using the emergency recovery phrase going forward. If you
choose not to migrate funds in this fashion, you will continue to need to securely back up
the entire `wallet.dat` file to ensure that you do not lose access to existing funds;
EXISTING FUNDS WILL NOT BE RECOVERABLE USING THE EMERGENCY RECOVERY PHRASE UNLESS THEY
HAVE BEEN MOVED TO A NEWLY GENERATED ADDRESS FOLLOWING THE 4.7.0 UPGRADE.

In the case that your wallet previously contained a Sapling HD seed, the mnemonic phrase
is constructed using the bytes of that seed, such that it is possible to reconstruct 
keys generated using that legacy seed if you know the mnemonic phrase. HOWEVER, THIS 
RECONSTRUCTION DOES NOT FOLLOW THE NORMAL PROCESS OF DERIVATION FROM THE MNEMONIC PHRASE.
Instead, to recover a legacy Sapling key from the mnemonic seed, it is necessary to 
reconstruct the bytes of the legacy seed by conversion of the mnemonic phrase back to
its source randomness instead of by hashing as is specified in BIP 39. Only keys and
addresses produced after the upgrade can be obtained by normal ZIP 32 or BIP 39 derivation.

Wallet Updates
--------------

The zcashd wallet now supports the Orchard shielded protocol.

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

- `walletconfirmbackup` This newly created API checks a provided emergency recovery phrase 
  against the wallet's emergency recovery phrase; if the phrases match then it updates the
  wallet state to allow the generation of new addresses.  This backup confirmation
  workflow can be disabled by starting zcashd with `-requirewalletbackup=false` but this
  is not recommended unless you know what you're doing (and have otherwise backed up the
  wallet's recovery phrase anyway).  For security reasons, this RPC method is not intended
  for use via zcash-cli but is provided to enable `zcashd-wallet-tool` and other
  third-party wallet interfaces to satisfy the backup confirmation requirement. Use of the
  `walletconfirmbackup` API via zcash-cli would risk that the recovery phrase being
  confirmed might be leaked via the user's shell history or the system process table;
  `zcashd-wallet-tool` is specifically provided to avoid this problem.
- `z_getnewaccount` This API allows for creation of new BIP 44 / ZIP 32 accounts using HD
  derivation from the wallet's mnemonic seed. Each account represents a separate spending
  authority and source of funds. A single account may contain funds in the Sapling and
  Orchard shielded pools, as well as funds held in transparent addresses.
- `z_getaddressforaccount` This API allows for creation of diversified unified addresses 
  under a single account. Each call to this API will, by default, create a new diversified
  unified address containing p2pkh, Sapling, and Orchard receivers.  Additional arguments
  to this API may be provided to request the address to be created with a user-specified
  set of receiver types and diversifier index.
- `z_getbalanceforaccount` This API makes it possible to obtain balance information on a 
  per-account basis.
- `z_getbalanceforviewingkey` This newly created API allows a user to obtain balance
  information for funds visible to a Sapling or Unified full viewing key; if a Sprout
  viewing key is provided, this method allows retrieval of the balance only in the case
  that the wallet controls the corresponding spending key.  This API has been added to
  supplement (and largely supplant) `z_getbalance`. Querying for balance by a single
  address returns only the amount received by that address, and omits value sent to other
  diversified addresses derived from the same full viewing key; by using
  `z_getbalanceforviewingkey` it is possible to obtain a correct balance that includes all
  amounts controlled by a single spending key, including both those sent to external
  diversified addresses and to wallet-internal change addresses.
- `z_listunifiedreceivers` This API allows the caller to extract the individual component
  receivers from a unified address. This is useful if one needs to provide a bare Sapling
  or p2pkh address to a service that does not yet support unified addresses.

RPC Changes
-----------

- The result type for the `listaddresses` endpoint has been modified:
  - The `keypool` source type has been removed; it was reserved but not used.
  - In the `sapling` address results, the `zip32AccountId` attribute has been
    removed in favor of `zip32KeyPath`. This is to allow distinct key paths to
    be reported for addresses derived from the legacy account under different
    child spending authorities, as are produced by `z_getnewaddress`.
  - Addresses derived from the wallet's mnemonic seed are now included in
    `listaddresses` output.
- The results of the `dumpwallet` and `z_exportwallet` RPC methods have been modified
  to now include the wallet's newly generated emergency recovery phrase as part of the
  exported data. Also, the seed fingerprint and HD keypath information are now included in
  the output of these methods for all HD-derived keys.
- The results of the `getwalletinfo` RPC have been modified to return two new fields:
  `mnemonic_seedfp` and `legacy_seedfp`, the latter of which replaces the field that
  was previously named `seedfp`.
- A new `pool` attribute has been added to each element returned by `z_listunspent`
  to indicate which value pool the unspent note controls funds in.
- `z_listreceivedbyaddress` 
  - A `pool` attribute has been added to each result to indicate what pool the received
    funds are held in. 
  - A boolean-valued `change` attribute has been added to indicate whether the output is
    change.
  - Block metadata attributes `blockheight`, `blockindex`, and `blocktime` have been
    added to the result.
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
- `z_listunspent` has been updated to render unified addresses for Sapling and 
  Orchard outputs when those outputs are controlled by unified spending keys.
  Outputs received by unified internal addresses do not include the `address` field.
- Legacy transparent address generation using `getnewaddress` no longer uses a 
  preallocated keypool, but instead performs HD derivation from the wallet's mnemonic
  seed according to BIP 39 and BIP 44 under account ID `0x7FFFFFFF`.

### 'z_sendmany'

- The `z_sendmany` RPC call no longer permits Sprout recipients in the list of recipient
  addresses. Transactions spending Sprout funds will still result in change being sent
  back into the Sprout pool, but no other `Sprout->Sprout` transactions will be
  constructed by the Zcashd wallet.
- The restriction that prohibited `Sprout->Sapling` transactions has been lifted; however, 
  since such transactions reveal the amount crossing pool boundaries, they must be
  explicitly enabled via a parameter to the `z_sendmany` call.
- A new string parameter, `privacyPolicy`, has been added to the list of arguments
  accepted by `z_sendmany`. This parameter enables the caller to control what kind of
  information they permit `zcashd` to reveal when creating the transaction. If the
  transaction can only be created by revealing more information than the given strategy
  permits, `z_sendmany` will return an error. The parameter defaults to `LegacyCompat`,
  which applies the most restrictive strategy `FullPrivacy` when a Unified Address is
  present as the sender or a recipient, and otherwise preserves existing behaviour (which
  corresponds to the `AllowFullyTransparent` policy). In cases where it is possible to do
  so without revealing additional information, and where it is permitted by the privacy
  policy, the wallet will now opportunistically shield funds to the most current pool.
- Since Sprout outputs are no longer created (with the exception of change)
  `z_sendmany` no longer generates payment disclosures (which were only available for
  Sprout outputs) when the `-paymentdisclosure` experimental feature flag is set.
- Outgoing viewing keys used for shielded outputs are now produced as 
  described in [ZIP 316](https://zips.z.cash/zip-0316#usage-of-outgoing-viewing-keys)
- When sending from or to one or more unified addresses, change outputs
  are now always sent to addresses controlled by the wallet's internal spending keys, as
  described in [ZIP 316](https://zips.z.cash/zip-0316#deriving-internal-keys).  These
  addresses are not returned by any RPC API, as they are intended to never be shared with
  any third party, and are for wallet-internal use only. This change improves the privacy
  properties that may be maintained when sharing a unified internal viewing key for an
  account in the wallet.
- In cases where `z_sendmany` might produce transparent change UTXOs, those UTXOs are
  sent to addresses derived from the wallet's mnemonic seed via the BIP 44 `change`
  derivation path.

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

Build System
------------

- Clang has been updated to use LLVM 13.0.1.
- libcxx has been updated to use LLVM 13.0.1, except on Windows where it uses 13.0.0-3.
- The Rust toolchain dependency has been updated to version 1.59.0.

Platform Support
----------------

- Debian 9 has been removed from the list of supported platforms.
- A build issue (a missing header file) has been fixed for macOS targets.
- On Arch Linux only, use Debian's libtinfo5_6.0 to fix a build regression.

Mining
------

- Mining to Orchard recipients is now supported on testnet. 
- It is now possible to mine to a Sapling receiver of a unified address.
- Concurrency bugs related to `getblocktemplate` have been fixed via backports from
  Bitcoin Core.

Licenses
--------

- License information in `contrib/debian/copyright` has been updated to be more accurate.
