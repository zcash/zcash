# zcashd codebase map (from research) — the touch points

## A. Adding a network upgrade (checklist, modeled on NU6.2 commit 0c8332961)
- `src/consensus/params.h:33` — `enum UpgradeIndex`: add `UPGRADE_NU6_3` before `UPGRADE_ZFUTURE`.
- `src/consensus/upgrades.cpp:11` — `NetworkUpgradeInfo[]`: add entry {branchId, name, info}.
  - branch ids are random non-zero u32 (NU6.2 = 0x5437f330).
- `src/chainparams.cpp` — mainnet (~147), testnet (~535), regtest (~871):
  set `vUpgrades[UPGRADE_NU6_3].nProtocolVersion` + `.nActivationHeight`.
- `src/chainparams.h:59` `UseFixedCircuitForProving()`, `:72` `RustNetwork()` — pass NU6.3 height.
- `src/rust/src/bridge.rs` + `src/rust/src/params.rs` — `network()` FFI gains `nu6_3` arg; `LocalNetwork`.
- `src/main.cpp` — NU-active flags (e.g. lines 894+, 2085, 3313, 5996), soft-fork logic.
- `src/util/test.cpp` + `.h` — `RegtestActivateNU6point3()` / `Deactivate`.
- `qa/rpc-tests/test_framework/util.py` — `NU6_3_BRANCH_ID`.
- `src/init.cpp:1397` auto-activation loop range; `-nuparams` parsing handles it via NetworkUpgradeInfo.
- `CurrentEpochBranchId(nHeight,params)` (upgrades.cpp:103) automatically picks newest active NU.

## B. Orchard pool internals (to mirror for the Ironwood pool)
- Tx bundle: `OrchardBundle orchardBundle` in `CTransaction` (primitives/transaction.h:461) &
  `CMutableTransaction` (:780). Serialized only in ZIP225 v5 (:603, :852).
  `OrchardBundle` wraps `rust::Box<orchard_bundle::Bundle>` (primitives/orchard.h).
- Value pool: `CBlockIndex::nOrchardValue` (chain.h:311), `nChainOrchardValue` (:315).
  `ComputePoolDeltas` (main.cpp:5114, line 5160), `AccumulateChainPoolValues` (:5335),
  turnstile check `"turnstile-violation-orchard"` (main.cpp:3432).
- Coins view: `ShieldedType::ORCHARD` (coins.h:334), `CAnchorsOrchardCacheEntry` (:306),
  `GetOrchardAnchorAt` (:374), `GetBestAnchor(ORCHARD)` (:390), `GetNullifier(nf,ORCHARD)` (:377),
  `BatchWrite(... mapOrchardAnchors, mapOrchardNullifiers ...)` (:427).
  Double-spend/anchor checks: coins.cpp:1134-1156. `CheckShieldedRequirements` via main.cpp:2706.
- Tree: `OrchardMerkleFrontier`, append in bridge.rs:321-343 `append_bundle`, used main.cpp:3788, 7032.
  `CBlockIndex::hashFinalOrchardRoot` (chain.h:336).
- Validation: `CheckTransaction` (main.cpp:1513+, flags/value/dup-nullifiers),
  `ContextualCheckTransaction` (main.cpp:867+, NU5 gating at 894). Mirror NU5 gating for Ironwood.
- Proof verify: `orchard::init_batch_validator(cache, nu6_2_active)` (main.cpp:2083, 3310),
  `QueueAuthValidation` (main.cpp:1429), `validate()` (2108, 3852).

## C. Circuit / key selection (the seam for the mock)
- Rust statics (`src/rust/src/lib.rs`): `ORCHARD_PK` OnceLock (:76), `ORCHARD_PK_INSECURE` (:81),
  `ORCHARD_VK_INSECURE` (:91), `ORCHARD_VK_FIXED` (:96). Init in `src/rust/src/init.rs:72`.
- `OrchardCircuitVersion { InsecurePreNu6_2, FixedPostNu6_2 }` in vendored orchard circuit.rs.
  **No `PostNu6_3`/Ironwood variant exists in vendored 0.14** → must mock/fallback.
- VK selection: `orchard_ffi.rs:29 orchard_batch_validation_init(cache, nu6_2_active)` →
  picks `ORCHARD_VK_FIXED` vs `ORCHARD_VK_INSECURE`.
- PK selection: `builder_ffi.rs:205` matches `bundle.circuit_version()`.
  `circuit_version_for(use_fixed)` at builder_ffi.rs:37.
- Builder creation: `transaction_builder.cpp:231` uses `Params().UseFixedCircuitForProving(nHeight)`.
- **Mock seam**: introduce an "Ironwood/restricted" circuit-version request. Where the code would
  build/select that new VK/PK, `tracing::error!`/`eprintln!` and return the existing
  `FixedPostNu6_2` keys instead.

## D. Wallet paths (feasibility a/b/c)
- Branch id computed once: `CurrentEpochBranchId(chain.Height(), consensus)` at
  wallet_tx_builder.cpp:431; flows to transparent sighash (`SignatureHash`, transaction_builder.cpp:573)
  and shielded sighash (`ProduceShieldedSignatureHash`, :564). **Transparent signing depends only on
  branch id + scriptPubKey — no pool/NU-specific logic** ⇒ supports hypothesis (a).
- Transparent: `FindSpendableInputs`, `AddTransparentInput/Output`, `ProduceSignature` — branch-id only.
- Sapling: `GetSaplingNoteWitnesses` (wallet.cpp:3992), `AddSaplingSpend/Output`,
  `apply_bundle_signatures` (transaction_builder.cpp:592). Gated only by branch id, not a specific NU.
- Orchard: `OrchardWallet` (wallet/orchard.h), note discovery `GetFilteredNotes` (wallet.cpp:7211),
  `GetOrchardSpendInfo` (wallet.cpp:4028), `AddOrchardSpend/Output`, `ProveAndSign`
  (transaction_builder.cpp:581). Change pools: `getAllowedChangePools` (wallet_tx_builder.cpp:286),
  Orchard change gated `afterNU5` (:306). Strategy: `TransactionStrategy` (wallet.h:805).
- RPCs/tests: `z_sendmany` (rpcwallet.cpp:4858), `z_getbalance*`, `getbalance`; gtests in
  wallet/gtest/test_rpc_wallet.cpp; python qa/rpc-tests.
