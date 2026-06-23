# Implementation approach (refined per user guidance)

## Guiding principle (user)
- **Measure the zcashd (C++ / src/rust FFI) complexity.** The shared Rust crates
  (orchard, zcash_primitives, zcash_protocol, sapling-crypto) are shared with
  Zebra and will be implemented for real by others → **mock them minimally**.
- Forking (vs duplicating) is correct because it unearths the real complexity.
- Forks need NOT actually implement Ironwood; make a good-guess API and mock.

## Key findings driving the design
1. The v5 **txid / auth digest / ZIP-244 sighash are computed in shared Rust**
   (`zcash_primitives`) via `zcash_transaction_digests` /
   `zcash_transaction_zip244_signature_digest` (src/rust/src/transaction_ffi.rs),
   which **reparse the serialized tx**. `Transaction::read` reads from a `&[u8]`
   and does NOT check for trailing bytes.
2. `zcash_primitives` rejects unknown consensus branch IDs → **every** NU6.3 tx
   (even transparent-only) needs the shared crates to know the NU6.3 branch id.
   This is why (a)/(b) hold *in zcashd* (only the branch id flows through) but
   still depend on a shared-crate change underneath (others' work).

## Chosen Ironwood tx encoding: a real v6 transaction (NO zcashd hacks)
Per user: **zcashd changes must be exactly as the real Ironwood implementation
would have them**, modulo crate API differences. So we use a proper new tx
version, exactly as Sapling→v4 and Orchard→v5 did historically:
- `IRONWOOD_VERSION_GROUP_ID` + `IRONWOOD_TX_VERSION = 6`, with the Ironwood
  `OrchardBundle` as a real serialized field after the Orchard bundle in v6.
- `CurrentTxVersionInfo` returns v6 once NU6.3 is active; ContextualCheck allows
  v6 only from NU6.3.
- `UpdateHash`/sighash call the digest FFI normally with the full v6 bytes — no
  "v5 view", no branch-id gating, no trailing-append tricks.
- The MOCK lives entirely in the shared crate (`zcash_primitives`): it learns to
  parse the v6 header + Ironwood bundle and produce ZIP-244 digests. Committing
  the Ironwood bundle in the digest is mocked (reuse the Orchard bundle
  commitment); "modulo API differences between the real crate and our mock."

## Circuit selection mock (zcashd-owned, satisfies "print an error + fall back")
- Ironwood bundles + post-NU6.3 old-Orchard "restricted" bundles should use the
  *new* circuit. We don't have it → at selection time **log an error** and use
  the existing fixed Orchard VK/PK (`ORCHARD_VK_FIXED` / `ORCHARD_PK`).
- Verification already selects the fixed VK when NU6.2 is active (true at NU6.3),
  so Ironwood verifies with the fixed VK; we add an explicit error log to mark
  the missing circuit selection.

## Minimal crate mocks (NOT the measured complexity)
- `zcash_protocol`: add `Nu6_3` to NetworkUpgrade + BranchId (0x725d9e30) +
  LocalNetwork.nu6_3; fix in-crate matches. (~29 mechanical edits.)
- `zcash_primitives`: add `Nu6_3` match arms (BranchId) so it compiles
  (suggested_for_branch→V5, valid_in_branch V5 allowed, read_v5 proof
  enforcement→Strict, proptest arbitraries).
- Empty each edited crate's `.cargo-checksum.json` "files" map to allow edits.
- orchard crate: **untouched** (circuit mock lives in src/rust).

## zcashd C++/FFI = the measured complexity (full + complete regtest tests)
- transaction.h/.cpp: `ironwoodBundle` field + gated trailing serialization +
  `GetIronwoodBundle()` + constructors.
- chain.h: nIronwoodValue / nChainIronwoodValue / hashFinalIronwoodRoot + disk.
- coins.h/.cpp + txdb: ShieldedType::IRONWOOD anchors + nullifiers + tree.
- main.cpp: pool deltas, chain value accumulation, ZIP209 turnstile,
  ContextualCheckTransaction gating (Ironwood valid only ≥ NU6.3), batch
  validation + QueueAuthValidation, tree append, hashFinalIronwoodRoot, circuit
  selection + old-Orchard restriction error log.
- src/rust FFI: params.rs/bridge.rs thread nu6_3; orchard_ffi/builder circuit log.
- Complete regtest RPC tests for every added behavior.
