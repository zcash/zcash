# Ironwood in zcashd — feasibility findings (input to writeup)

This is the analysis you asked for: feasibility of implementing Ironwood in zcashd as
an alternative to "zebra-only + block activation on zcashd deprecation." It is grounded
in a working MOCK on this branch (a new NU6.3 network upgrade adding a second
Orchard-style "Ironwood" pool, with the original Orchard pool switched to a restricted
circuit). The mock's zcashd changes are written to be **what the real implementation
would contain**, modulo the shared Rust crates (which are mocked — see below).

## EMPIRICAL RESULTS (this branch, regtest)
A working mock is implemented and committed on branch `ironwood-mock` (5 commits) and
builds cleanly (zcashd, zcash-gtest, test_bitcoin). A regtest test
(`qa/rpc-tests/ironwood.py`) **passes**, demonstrating end to end:
- NU6.3 activates and is reported by `getblockchaininfo` with branch id 0x725d9e30.
- **(a) transparent** send works across NU6.3 (built, accepted, mined, balance updates).
- **(b) Sapling** transparent->Sapling shielding AND Sapling->Sapling spend work across NU6.3.
- Wallet transactions are v5 (ZIP225) committing to the NU6.3 branch id, validated under
  the NU6.3 consensus rules (value pool, turnstile, version gating, etc.).

CONFIRMED: (a) and (b) needed **no wallet-logic changes** — only relay-policy version
acceptance + the wallet's version selection. The send/sign/build paths are unchanged
because they key off the consensus branch id, which flows through automatically.

Two empirical findings worth putting in the writeup:
1. The legacy transparent-wallet RPCs (e.g. `getnewaddress`) are ALREADY disabled by
   default in zcashd (the deprecation campaign); the test re-enables them with
   `-allowdeprecated=getnewaddress`. Transparent wallet functionality is intact but gated
   behind that flag — so "maintaining transparent wallet functionality" partly means
   "keep the deprecation gate openable," which it already is.
2. The v5 txid/auth-digest/ZIP-244 sighash are computed in the shared crates and reached
   through TWO different FFIs (a builder-side `shielded_signature_digest` and a
   verification-side `zcash_transaction_zip244_signature_digest`). In this MOCK they could
   not be made to agree for a v6 transaction's Sapling binding signature (transparent
   agreed; Sapling did not). The real implementation has a single coherent ZIP-244 v6
   digest, so it has no such mismatch; the mock sidesteps it by building v5 wallet txs at
   NU6.3 (zcashd never needs v6 because it never builds Ironwood bundles). This is purely a
   mock artifact, not a zcashd-complexity finding.

## TL;DR / recommendations
- **Overall: implementing Ironwood's consensus rules + transparent/Sapling wallet in
  zcashd is FEASIBLE and not enormous.** The consensus work is "yet another shielded
  pool," for which zcashd has a well-worn template (Orchard). It is mechanical but large.
- **(a) Transparent wallet: feasible with essentially no wallet changes** beyond the new
  consensus branch id flowing through — your hypothesis holds. CONFIRMED by code paths.
- **(b) Sapling wallet: feasible, small changes** — Sapling building/signing is gated only
  on the branch id, not on any specific NU. Main work is tests + change-pool defaults.
- **(c) Orchard→Ironwood wallet flow: feasible but by far the most complex piece; we
  recommend NOT doing it in zcashd.** It requires standing up a second full Orchard-style
  wallet (note discovery/decryption/trial-decryption, a second note-commitment tree,
  spend-info/witness generation, change handling) — essentially duplicating the entire
  `OrchardWallet` for Ironwood — for a stopgap audience (miners/exchanges who can't move
  to the Zebra wallet stack). The cost/benefit is poor.

## CRUCIAL cross-cutting finding (affects the whole comparison)
The v5+ **transaction id, auth digest, and ZIP-244 signature hash are computed in the
shared Rust crates** (`zcash_primitives`), reached from zcashd via
`zcash_transaction_digests` / `zcash_transaction_zip244_signature_digest`
(src/rust/src/transaction_ffi.rs). Those functions **reparse the serialized transaction**,
and `zcash_primitives` **rejects unknown consensus branch ids**.

Consequence: **every** NU6.3 transaction — even a purely transparent one — cannot have its
txid/sighash computed unless the shared crates know the NU6.3 branch id. So:
- The shared crates (`zcash_protocol`, `zcash_primitives`, `orchard`, `sapling-crypto`)
  MUST gain Ironwood/NU6.3 support regardless of which client (zcashd or Zebra) ships a
  wallet. This is shared work that has to happen anyway. It is **not** extra cost specific
  to "Ironwood in zcashd."
- From the **zcashd** side, transparent/Sapling support is "just the branch id" — which is
  exactly why (a)/(b) are cheap *in zcashd*. The branch-id knowledge they depend on lives
  in the shared crates (others' work).

This is the single most important point for the writeup: **doing Ironwood's wallet in
zcashd does not avoid the shared-crate work; that work is required for any wallet. zcashd
adds only a thin, well-understood layer on top.**

## What the mock implements today (compiles into a working zcashd + zcash-gtest)
- `UPGRADE_NU6_3` network upgrade (branch id 0x725d9e30), activating on mainnet + testnet
  (placeholder heights) and overridable on regtest via `-nuparams`. PROTOCOL_VERSION bumped.
- A real **v6 transaction** (`IRONWOOD_VERSION_GROUP_ID`, `IRONWOOD_TX_VERSION = 6`) whose
  body is the ZIP225 v5 body + a second ("Ironwood") Orchard bundle. Full C++
  serialization, `CurrentTxVersionInfo` returns v6 from NU6.3, wallet builds v6.
- Block-index value-pool fields for the Ironwood pool (`nIronwoodValue`,
  `nChainIronwoodValue`, `hashFinalIronwoodRoot`) incl. disk persistence (NU6_3_DATA_VERSION).
- The circuit selection seam (Ironwood / restricted-Orchard "new circuit"): per your
  instruction it logs an error and falls back to the existing fixed Orchard circuit.
- The shared crates are forked via cargo `[patch.crates-io]` → local clones in
  `./ironwood-crates/` (so they survive `./distclean.sh`). Mocked minimally: NU6.3 branch
  id in `zcash_protocol`; `zcash_primitives` routes the v6 Ironwood header through the v5
  digest machinery (the Ironwood bundle is not yet committed in the digest — that is
  shared-crate ZIP-244 work others will do).

## Remaining consensus work (mapped, mechanical — mirrors Orchard exactly)
The remaining pieces are voluminous but are a near-mechanical mirror of the Orchard pool
(see ironwood-notes/05-cpp-edit-plan.md and 02-codebase-map.md for exact file:line):
- Value-pool population + ZIP-209 turnstile: thread `ironwoodValue` through
  `ComputePoolDeltas` / `AccumulateChainPoolValues` / the turnstile check in `ConnectBlock`
  (5 sites in main.cpp). This is the "circulating-supply soundness" enforcement.
- Nullifier set + note-commitment tree + anchors for the Ironwood pool: a new
  `ShieldedType::IRONWOOD` across coins.h/coins.cpp/txdb (reusing `OrchardMerkleFrontier`),
  plus double-spend/anchor checks in `CheckShieldedRequirements`, and tree append +
  `hashFinalIronwoodRoot` in `ConnectBlock`. This is the single biggest mechanical chunk.
- Validation gating: `ContextualCheckTransaction` allow v6 only from NU6.3; reject an
  Ironwood bundle before NU6.3; `CheckTransaction` structural checks for the Ironwood
  bundle (mirroring Orchard); queue the Ironwood bundle into the batch validator.
- The "old Orchard pool spends-except-change" restriction is enforced by the new circuit
  (mocked here); zcashd just selects that circuit (the error-logging seam).

Estimated effort for the consensus side: large but bounded and low-novelty — it is the
fourth time Zcash has added a shielded pool to this codebase (Sprout, Sapling, Orchard).

## (a) Transparent wallet — FEASIBLE, ~no wallet changes. CONFIRMED.
Evidence (src/transaction_builder.cpp, src/wallet/wallet_tx_builder.cpp):
- The consensus branch id is computed once via `CurrentEpochBranchId(height, params)`
  (wallet_tx_builder.cpp:431) and again in `TransactionBuilder::Build`
  (transaction_builder.cpp:557).
- Transparent signing is `SignatureHash(scriptCode, mtx, ..., consensusBranchId, txdata)`
  (transaction_builder.cpp:573) → `ProduceSignature` (615-631). It depends ONLY on the
  branch id and the scriptPubKey. There is **no pool-specific or NU-specific branching** in
  the transparent path.
- Input selection (`FindSpendableInputs`/`AvailableCoins`), change, and broadcast are
  pool-agnostic.
Conclusion: once NU6.3's consensus rules are in (and the shared crate knows the branch id),
transparent send/receive/balance keep working unchanged. The only zcashd-side change is
that the wallet now builds v6 transactions (handled centrally by `CurrentTxVersionInfo`).
Risk: none material. Work: confirmation + a regtest test spanning NU6.3 activation.

## (b) Sapling wallet — FEASIBLE, small changes.
Evidence:
- Sapling note selection/witnesses: `GetSaplingNoteWitnesses` (wallet.cpp:3992); spends/
  outputs added to the builder; finalized via `sapling::apply_bundle_signatures(...,
  dataToBeSigned)` (transaction_builder.cpp:592), where `dataToBeSigned` is the ZIP-244
  sighash keyed on the branch id. No NU6.3-specific coupling.
- Sapling change/anchor are gated on Sapling activity + branch id, not on a specific NU.
Risks / small work:
- The transaction-strategy "allowed change pools" logic (`getAllowedChangePools`,
  wallet_tx_builder.cpp:286-314) gates Orchard change on `afterNU5`. Ensure NU6.3 doesn't
  perturb Sapling change selection, and that any "prefer the newest pool" defaults still
  produce Sapling change when appropriate.
- v6 tx building must include the Sapling bundle (it does — v6 body == v5 body).
Conclusion: feasible; primary work is regtest tests + auditing change-pool defaults.

## (c) Orchard → Ironwood wallet flow — FEASIBLE but most complex; recommend AGAINST in zcashd.
The ask: make the wallet flow value OUT of the old Orchard pool only, and INTO the new
Ironwood pool only.
What it requires (evidence: src/wallet/orchard.h, wallet.cpp:7211/4028, transaction_builder.cpp:581):
1. **A second Orchard-style wallet for Ironwood.** Orchard notes are discovered by
   trial-decryption inside the Rust `OrchardWallet` (orchard_wallet_* FFI), which maintains
   the wallet's Orchard note set, nullifier tracking, and an in-wallet note-commitment tree
   for witness/anchor generation (`GetOrchardSpendInfo`, wallet.cpp:4028). Ironwood notes
   live in a *separate* pool/tree, so spending from Ironwood needs a parallel
   `IronwoodWallet` with its own decryption, note set, tree, and spend-info generation.
   This is the bulk of the cost and is **not** a thin wrapper — it mirrors a large,
   security-critical subsystem.
2. **Restrict Orchard to outflow-only.** The wallet must stop producing Orchard payment
   outputs (only migration to Ironwood / change as the new circuit allows), and route new
   value exclusively into Ironwood. This touches recipient-pool resolution
   (`Payments::AddPayment`), change-pool selection (`getAllowedChangePools`), and the
   transaction strategy/privacy-policy lattice.
3. **Build txs that spend Orchard and create Ironwood** in one v6 transaction (the
   migration shape) — needs both the Orchard spend path and a new Ironwood output/prove path
   wired into `TransactionBuilder` + the Rust builder FFI.
4. **Note management**: scanning, balance reporting (`z_getbalanceforaccount` per-pool),
   `z_listunspent`, rescan, and wallet DB schema all need an Ironwood pool dimension.

Why recommend against (your reasoning, corroborated):
- The zcashd wallet is explicitly a **stopgap** for operators who cannot move to the Zebra
  wallet stack. Ironwood's user-facing migration is a wallet concern that the Zebra stack
  is meant to own.
- (c) is the only piece that is genuinely large and novel on the *wallet* side (vs. the
  consensus side, which is mechanical). It roughly doubles the Orchard wallet surface.
- Most operators (miners/exchanges) primarily need (a) transparent (and maybe (b) Sapling)
  to keep functioning across activation; they are not the ones doing shielded
  Orchard→Ironwood migrations.
- If migration-from-zcashd is ever required, a far cheaper option is a one-shot
  "sweep Orchard → transparent → (re-shield via Zebra/Ironwood)" using existing Orchard
  *spend* support, rather than a full in-wallet Ironwood pool. (Whether the restricted
  Orchard circuit permits Orchard→transparent outflow is a design question for the circuit.)

## Comparison to the current plan (zebra-only + block on zcashd deprecation)
- The risk you flagged — Zebra wallet functionality is only at smoke-test stage — is real
  and is precisely the (c)-shaped work. That work is hard *wherever* it is done.
- Implementing Ironwood **consensus + (a)/(b)** in zcashd is comparatively cheap and
  de-risks activation timing without waiting on zcashd deprecation. It does NOT require the
  hardest piece (c).
- The shared-crate work (branch id, circuit, ZIP-244 digest commitment to the Ironwood
  bundle) is required for any client and is the natural place for the real cryptographic
  Ironwood implementation. zcashd consuming it is straightforward.

## Security note
Treat the consensus pieces as security-critical: the value-pool turnstile (supply
soundness — Ironwood's entire purpose), the nullifier-set isolation between Orchard and
Ironwood, the v6 validation gating, and (in the real impl, not the mock) committing the
Ironwood bundle in the txid/sighash so it is bound to the transaction. The mock deliberately
defers the cryptographic bindings to the shared crates and documents each such gap.
