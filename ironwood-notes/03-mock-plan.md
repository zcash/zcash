# Mock plan: Ironwood-in-zcashd

## What the mock is
A new network upgrade (testnet + mainnet) that:
1. Adds a second Orchard-style pool, "**Ironwood**", reusing the existing Orchard
   `Bundle` type + circuit (the real Ironwood/`PostNu6_3` circuit isn't in our
   vendored `orchard` 0.14, so we reuse the fixed Orchard circuit).
2. Switches the **original Orchard pool** to a "restricted" circuit (spends except
   change). Enforcement is the circuit's job (same-address constraint) → in the
   mock the circuit-selection prints an error and falls back to the existing
   fixed Orchard circuit. Ironwood always "uses" that (mocked) new circuit.

## Commit plan
- **Commit 1 — consensus rules:**
  - New `UPGRADE_NU6_3` (Ironwood) NU across params.h / upgrades.cpp / chainparams
    (main+test+regtest) / rust bridge+params / test helpers / qa util.
  - Second pool plumbing mirroring Orchard: tx `ironwoodBundle` field + serialization,
    `nIronwoodValue`/`nChainIronwoodValue` + turnstile, Ironwood nullifier/anchor/tree
    namespace in coins view + DB, Check/ContextualCheck gating (Ironwood valid only
    from NU6.3), batch proof verification.
  - Circuit-selection seam: a "restricted Orchard"/"Ironwood" circuit-version request
    that, where the real new VK/PK would be built/selected, logs an error and returns
    the existing `FixedPostNu6_2` keys.
- **Commit 2 — wallet (a) transparent:** confirm transparent wallet works post-NU6.3;
  changes expected to be ~none beyond branch id flowing through (verify, add tests).
- **Commit 3 — wallet (b) Sapling:** confirm/keep Sapling shielded wallet working post-NU6.3.

## Feasibility hypotheses (to confirm during implementation)
- (a) **Transparent**: should "just work". Transparent build/sign depends only on
  `CurrentEpochBranchId` + scriptPubKey (transaction_builder.cpp:573); no pool/NU
  coupling. Expect: only the branch id changes. LIKELY TRUE.
- (b) **Sapling**: Sapling build/sign gated only by branch id, not a specific NU
  (apply_bundle_signatures, transaction_builder.cpp:592). Expect: works unchanged.
  Risk: anything that assumes "latest pool = Orchard" or default change pool logic.
- (c) **Orchard→Ironwood wallet flow** (out of Orchard only, into Ironwood only):
  most complex — requires the wallet to (i) discover/spend old-Orchard notes under
  the restricted circuit, (ii) build Ironwood outputs/bundles, (iii) decrypt/track
  Ironwood notes (new OrchardWallet-equivalent), (iv) change-pool logic. User leans
  NOT to do (c) — stopgap only; investigate feasibility & cost separately.

## Open decisions (asking the user)
1. Fidelity of the Ironwood pool (faithful compiling scaffold vs. full crate fork vs. lean).
2. NU naming: `UPGRADE_NU6_3` ("NU6.3", matches real Ironwood vehicle) vs `UPGRADE_IRONWOOD`.
