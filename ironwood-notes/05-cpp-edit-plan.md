# Commit 1 — zcashd C++ edit plan (the MEASURED complexity)

Status legend: [ ] todo  [x] done  [~] partial

## PROGRESS SNAPSHOT (this session)
DONE + building (commit 1 = 82ef33137, plus follow-up edits to be committed):
- [x] NU6.3 network upgrade (1a)
- [x] crate mocks via [patch] local clones (1b)
- [x] v6 Ironwood tx format (field + serialization + CurrentTxVersionInfo + version gating)
- [x] value pool: nIronwoodValue/nChainIronwoodValue/hashFinalIronwoodRoot + disk persistence
- [x] ComputePoolDeltas + AccumulateChainPoolValues + ZIP-209 turnstile (supply soundness)
- [x] ContextualCheckTransaction version-group gating (v6 only from NU6.3)
- [x] Ironwood proof-validation queue + circuit-selection error/fallback (1d)
REMAINING consensus (per user: consensus = validation rules incl. spend validation, which
a node must enforce on network blocks regardless of whether zcashd's wallet creates them):
- [x] ShieldedType::IRONWOOD nullifier set + note-commitment tree + anchors
      (coins.h/coins.cpp/txdb.h/txdb.cpp: anchor cache + map + GetIronwoodAnchorAt +
      GetBestAnchor/GetNullifier/PopAnchor/PushIronwoodAnchor + BatchWrite + DB keys
      W/N/w; CheckShieldedRequirements double-spend + anchor checks; SetNullifiers).
      ConnectBlock: ironwood_tree load/append + PushIronwoodAnchor + hashFinalIronwoodRoot.
      DisconnectBlock: PopAnchor(IRONWOOD). Mock CCoinsView subclasses updated.
- [ ] CheckTransaction structural mirror for the Ironwood bundle (num actions, value-balance
      range, dup nullifiers within tx, coinbase rules) -- defense-in-depth, in progress.
- [ ] Secondary/alt paths left Orchard-only (documented): reindex orchard-tree rebuild
      (~main.cpp:7088), disconnect best-anchor sanity (~4499/4511), block-index validity
      check (~4390). Not on the primary ConnectBlock validation path.


## Constants / IDs (decided)
- NU6.3 branch id: `0x725d9e30` (upgrades.cpp, util.py, zcash_protocol mock).
- Ironwood tx: `IRONWOOD_VERSION_GROUP_ID = 0x77777777`, `IRONWOOD_TX_VERSION = 6`.
  Body layout == v5 body + trailing Ironwood `OrchardBundle`.
- Crate mocks via `[patch.crates-io]` → local clones in `./ironwood-crates/`
  (zcash_protocol, zcash_primitives). See ironwood-notes/crate-mocks/apply.py for the
  exact edits (to be re-homed into the clones). zcash_primitives maps the v6 Ironwood
  header → V5 for ZIP-244 digests (trailing Ironwood bytes ignored).

## 1a NETWORK UPGRADE — DONE (builds)
[x] params.h enum UPGRADE_NU6_3; upgrades.cpp NUInfo; chainparams.cpp main/test/regtest;
    version.h PROTOCOL_VERSION 170160; util/test.cpp+.h RegtestActivateNU6point3;
    qa util.py NU6_3_BRANCH_ID.

## 1b CRATE MOCKS — designed (apply via [patch] clones after build.sh)
[~] zcash_protocol: Nu6_3 NetworkUpgrade+BranchId(0x725d9e30)+LocalNetwork.nu6_3.
[~] zcash_primitives: Nu6_3 arms + IRONWOOD v6 header → V5 in TxVersion::read.

## 1c IRONWOOD POOL — zcashd C++

### transaction format (v6)  [ ]
- consensus/consensus.h: add `IRONWOOD_MIN_TX_VERSION=6`, `IRONWOOD_MAX_TX_VERSION=6`.
- primitives/transaction.h:
  - after ZFUTURE consts (~L64-70): add `IRONWOOD_VERSION_GROUP_ID=0x77777777`,
    `IRONWOOD_TX_VERSION=6` + static_asserts.
  - CTransaction: add `OrchardBundle ironwoodBundle;` after `orchardBundle` (L461, private).
    Add current-version consts: `NU6_3_MIN_CURRENT_VERSION=4`, `NU6_3_MAX_CURRENT_VERSION=6`
    (+ static_asserts mirroring NU5_*).
  - SerializationOp (CTransaction ~L538 and CMutableTransaction ~L790):
    - add `bool isIronwoodV6 = fOverwintered && nVersionGroupId==IRONWOOD_VERSION_GROUP_ID
      && nVersion==IRONWOOD_TX_VERSION;`
    - add `isIronwoodV6` to the "Unknown transaction format" allow-list (L578/L827).
    - change `if (isZip225V5)` → `if (isZip225V5 || isIronwoodV6)`; at the END of that
      block (after `READWRITE(orchardBundle)` L603/L852) add
      `if (isIronwoodV6) { READWRITE(ironwoodBundle); }`.
  - add `const OrchardBundle& GetIronwoodBundle() const { return ironwoodBundle; }` (~L717).
  - CMutableTransaction: add `OrchardBundle ironwoodBundle;` after orchardBundle (L780).
- primitives/transaction.cpp: copy `ironwoodBundle` in all 4 CTransaction ctors/operator=
  (L165, L177, L189, L202) and CMutableTransaction(const CTransaction&) (~L90-100).
- primitives/tx_version_info.cpp CurrentTxVersionInfo: add, before the NU5 branch:
  `else if (NetworkUpgradeActive(nHeight, UPGRADE_NU6_3) && !requireV4) { return {true,
  IRONWOOD_VERSION_GROUP_ID, IRONWOOD_TX_VERSION}; }`.

### version validity (main.cpp ContextualCheckTransaction)  [ ]
- The NU5 block (~L1134-1170): allow IRONWOOD_VERSION_GROUP_ID when NU6.3 active
  (mirror ZIP225 handling; version must be IRONWOOD_TX_VERSION). Reject Ironwood vgid
  before NU6.3. Reject Ironwood bundle present unless NU6.3 active + v6.
- future-version switch (~L1307): add `case IRONWOOD_VERSION_GROUP_ID`.
- CheckTransaction (main.cpp ~L1513): mirror Orchard bundle structural checks for the
  Ironwood bundle (num actions, value-balance range, dup nullifiers within tx, coinbase
  rules: coinbase Ironwood spends disabled, value balance non-positive, etc.).

### value pool + turnstile  [ ]
- chain.h CBlockIndex: add `CAmount nIronwoodValue{0};` (after nOrchardValue L311),
  `std::optional<CAmount> nChainIronwoodValue;` (after nChainOrchardValue L315),
  `uint256 hashFinalIronwoodRoot;` (after hashFinalOrchardRoot L342). Init in SetNull
  (L405/390) + ResetBodyState (L460/468).
- chain.h CDiskBlockIndex SerializationOp (~L614-696): persist nIronwoodValue +
  hashFinalIronwoodRoot under a NEW client-version gate (see CLIENT_VERSION / the
  `if (nVersion >= ...)` guards around L666-696). Add a new DISK index version constant.
- main.cpp ComputePoolDeltas (~L5114-5161): `ironwoodValue -= tx.GetIronwoodBundle()
  .GetValueBalance();` + MoneyDeltaRange check; set pindex->nIronwoodValue.
- main.cpp AccumulateChainPoolValues (~L5335-5386): accumulate nChainIronwoodValue from
  parent (genesis = nIronwoodValue) + MoneyRange checks.
- main.cpp turnstile (~L3432): add MoneyRange(nChainIronwoodValue) check →
  "turnstile-violation-ironwood"; include in the pool-balance error report.
- chainparams.h + chainparams.cpp: ChainSupply checkpoint may need an Ironwood field
  (mirror nChainSupplyCheckpointOrchardValue) — likely 0 (pool empty at checkpoint).

### nullifiers / anchors / note-commitment tree  [ ]
- coins.h: add `ShieldedType::IRONWOOD` (enum L334, value 0x04). Add
  `CAnchorsIronwoodCacheEntry` (mirror CAnchorsOrchardCacheEntry L306) +
  `CAnchorsIronwoodMap` typedef (L344). Reuse `OrchardMerkleFrontier` as the tree type.
- CCoinsView + CCoinsViewCache + CCoinsViewBacked + dummy: add
  `GetIronwoodAnchorAt`, extend `GetBestAnchor/GetNullifier(...IRONWOOD)`,
  `BatchWrite(... hashIronwoodAnchor, mapIronwoodAnchors, mapIronwoodNullifiers ...)`.
  Members: `hashIronwoodAnchor`, `cacheIronwoodAnchors`, `cacheIronwoodNullifiers` (L633-639).
  PushAnchor/PopAnchor/AbstractPush/PopAnchor handle Tree=OrchardMerkleFrontier+IRONWOOD.
- coins.cpp: implement all of the above (mirror Orchard). CheckShieldedRequirements
  (~L1070-1159): add Ironwood nullifier double-spend + anchor checks.
- txdb.h/.cpp: DB key prefixes DB_IRONWOOD_ANCHOR / DB_IRONWOOD_NULLIFIER + best-anchor
  key; read/write in CCoinsViewDB::BatchWrite + GetIronwoodAnchorAt/GetNullifier/GetBestAnchor.
- main.cpp ConnectBlock: append Ironwood bundles to the Ironwood tree (mirror
  orchard_tree.AppendBundle ~L3788, L7032); set hashFinalIronwoodRoot; spend Ironwood
  nullifiers; check anchors.

### proof verification + circuit selection (1d)  [ ]
- main.cpp: queue Ironwood bundle auth into the Orchard batch validator
  (tx.GetIronwoodBundle().QueueAuthValidation(...)) alongside Orchard (~L1429).
- src/rust orchard_ffi.rs / builder_ffi.rs: when NU6.3 active (Ironwood / restricted
  Orchard circuit requested), `tracing::error!("Ironwood/restricted Orchard circuit not
  implemented; falling back to fixed Orchard circuit")` and use ORCHARD_VK_FIXED/ORCHARD_PK.
- chainparams.h: add a helper expressing "old Orchard pool is restricted at nHeight"
  (NU6.3 active) for the wallet/builder; transaction_builder.cpp logs the fallback.
- src/rust params.rs + chainparams.h RustNetwork: thread nu6_3 into LocalNetwork.

## 1c TESTS (regtest) — REQUIRED for everything added  [ ]
- qa/rpc-tests/ironwood_pool.py: regtest, -nuparams=0x725d9e30:<h>; verify:
  - getblockchaininfo shows NU6.3 upgrade + correct branch id at/after activation.
  - a v6 Ironwood tx is accepted; getblock/gettransaction shows it; pool value tracked.
  - turnstile: cannot remove more from Ironwood than entered.
  - circuit-selection fallback error is logged (debug.log) when NU6.3 active.
- gtest: tx_version_info, serialization round-trip of v6 Ironwood tx, CheckTransaction.

## Commit 2 (a) transparent + Commit 3 (b) sapling — see 03-mock-plan.md
- Expect minimal/no zcashd changes beyond branch id flowing through; add regtest tests
  proving transparent & Sapling wallet still work across NU6.3 activation.
