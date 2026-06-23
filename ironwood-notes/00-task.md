# Ironwood-in-zcashd feasibility — task notes

## Goal
Investigate feasibility of implementing Ironwood in **zcashd** (instead of the
current plan: ship Ironwood in zebra-only and block activation on zcashd
deprecation). Risk with current plan: zebra wallet functionality is incomplete
(smoke-test stage only).

This work is the **input to a writeup** + a **draft PR** demonstrating feasibility.
Treat all changes as **security-critical** (may become the real basis for Ironwood).

## What to build (a MOCK of Ironwood)
A new **network upgrade** (activates on testnet + mainnet) that:
1. Adds a **second Orchard pool** ("Ironwood") using the **same ZK circuit** as
   the existing Orchard pool (for the mock).
2. Switches the **original Orchard pool** to a **different circuit** that
   **disables spending except change**.
3. The new circuit does not exist yet → write all selection logic, but when it's
   time to select the new circuit, **print an error message and fall back to the
   existing Orchard circuit**. Ironwood always "uses" that (mocked) new circuit.

## Feasibility questions to answer
- (a) Maintain **transparent** address wallet functionality in zcashd.
      Hypothesis: once consensus rules are in, transparent wallet keeps working
      with no changes beyond the consensus branch ID.
- (b) Maintain **Sapling** shielded wallet functionality in zcashd.
- (c) Adapt the **Orchard wallet** to only flow OUT of Orchard and only send
      INTO the new Ironwood pool.
      User leans toward NOT doing (c): zcashd impl is a stopgap for
      miners/exchanges who can't move to the zebra wallet stack; likely unneeded;
      most complex piece.

## Deliverable structure
- Commit 1: consensus rule implementation
- Commit 2: wallet changes for (a)
- Commit 3: wallet changes for (b)
- Then: separate investigation/writeup of (c) feasibility

## References
- Blog: https://shieldedlabs.net/ironwood-verifying-the-soundness-of-zcashs-circulating-supply/
- Ironwood circuit: https://github.com/zcash/orchard/pull/504
