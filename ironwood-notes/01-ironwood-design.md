# Ironwood — the real design (from blog + orchard PR #504)

## The problem
A counterfeiting vulnerability was discovered in the **Orchard** pool. Because
Orchard is private, users **cannot verify the soundness of the circulating
supply** for themselves. Ironwood restores that verifiability.

## The mechanism (three parts, per the Shielded Labs post)
1. **Create a new shielded pool** ("Ironwood") using the **fixed Orchard
   circuit** (i.e. a *new circuit version*, not the historical one).
2. **Disable new outputs in the old Orchard pool**: *"Reject as invalid any
   transaction that creates a new output in the old Orchard pool."* Combined with
   turnstile accounting (ZIP 209-style), the old pool can only be **drained**
   outward — value cannot circulate within it.
3. Higher assurance via AI-assisted auditing + formal verification.

Wallets are expected to **migrate user funds** from old Orchard to Ironwood. ZEC
sent to a pre-Ironwood Orchard receiver is *automatically received in the new
pool*. Transparent and Sapling pools are **not changed**.

## The circuit (orchard PR #504, branch `feat/ironwood`, targets NU6.3)
- Adds an **Ironwood pool circuit** nearly identical to Orchard, plus a
  **cross-address restriction**.
- New circuit version **`PostNu6_3`**; new public input **`disableCrossAddress`**.
  - `disableCrossAddress = 1` ⇒ circuit enforces `(g_d,pk_d)^old = (g_d,pk_d)^new`
    (spent note and output note share the **same receiver**). i.e. you may only
    "spend to yourself" = **change/migration only, no payments**.
  - Flag **bit 2** is repurposed as `enableCrossAddress` (was reserved 0).
- New **`BundleProtocol`** enum: `OrchardPreNu6_2`, `OrchardPreNu6_3`,
  `OrchardPostNu6_3`, `IronwoodPostNu6_3`. Each maps to an `OrchardCircuitVersion`.
  - **Old Orchard pool post-NU6.3** → `OrchardPostNu6_3` (cross-address **forbidden**,
    bit2 = 0): only same-address change allowed → "disable spending aside from change".
  - **Ironwood pool** → `IronwoodPostNu6_3` (cross-address **allowed**): normal payments.
  - Both share the **same circuit version `PostNu6_3`** (same VK/PK); they differ
    only by the `disableCrossAddress` public input / flag bit.
- Builder pairs each spend with a **fabricated same-address output** when
  cross-address is disabled (`OutputInfo::fabricated_for_spend()`), randomized
  ciphertexts to avoid linkability. New `Builder::add_change_output()`.
- `BatchValidator::new(&VerifyingKey)` now binds the VK at construction.

## Key takeaway for the mock
- "Disable spending aside from change" in the old Orchard pool is enforced **by
  the new circuit's same-address constraint** (`OrchardPostNu6_3`), NOT by a
  coarse C++ consensus rule. The blog's "reject new outputs in old pool" is the
  coarser framing; the PR refines it to same-address change via the circuit.
- The new circuit (`PostNu6_3` VK/PK, `BundleProtocol`) is **NOT in our vendored
  `orchard` 0.14** (PR #504 is unmerged, on `feat/ironwood`). So a faithful mock
  **cannot actually build/verify against the real circuit** → this is exactly why
  the user wants: write the selection logic, but at circuit-selection time print
  an error and fall back to the existing fixed Orchard circuit.
