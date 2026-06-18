# zcash-regtest-wallet-builder

Build a Zcash regtest chain with a `wallet.dat` containing keys and transaction
history from historical versions of the zcashd wallet. May generate a "full"
wallet with keys from **every network upgrade** -- Sprout through NU6.2 -- or
an "ancient" wallet with only pre-Sapling keys (no HD seed). The result is a
test fixture for integration and regression testing of Zcash wallet tools.

Build it with one of two targets:

- **`make run`** -- the multi-version wallet described below (all eras).
- **`make ancient`** -- the same build, stopped after phase 1: just the
  pre-Sapling, non-deterministic (no HD seed) eras (Sprout + Overwinter).

## What This Produces

Both targets write the same `./output/` layout (see [Quick Start](#quick-start));
only the wallet contents differ.

### `make run`: a multi-version historical zcashd wallet

A `wallet.dat` progressively built through **10 network upgrade eras** (Sprout
through NU6.2), simulating the real-world wallet upgrade path. It contains:

- **Transparent addresses** with coinbase UTXOs from every NU era
- **Sprout shielded notes** from the Sprout through Heartwood eras (pre-Canopy)
- **Sapling shielded notes** from Sapling through NU6.2
- **Orchard shielded notes** and Unified Addresses from NU5 through NU6.2
- **Cross-pool transfers**: every active pool to every other active pool at each NU (except Orchard as source -- see [Limitations](#limitations))
- **External sends**: outgoing transactions to addresses the wallet doesn't control
- **Imported foreign keys**: `importprivkey`, `importaddress` (watch-only), `importaddress` (P2SH), `importpubkey`, `z_importkey` (Sprout/Sapling spending), `z_importviewingkey` (Sprout/Sapling viewing) -- all from keys NOT derived from the wallet's HD seed
- **Viewing keys** exported for read-only wallet testing
- **Per-phase manifests** documenting every TXID, mined height, value, fee, address, and imported key
- **Full regtest chain state** and Zcash proving parameters for resuming with future NUs

### `make ancient`: a pre-Sapling, non-deterministic zcashd wallet

`make ancient` is **`make run` stopped after phase 1** -- the same build process,
run only through the pre-Sapling eras (phase 0 Sprout + phase 1 Overwinter). Both
run a real pre-Sapling zcashd (**v1.1.1**), and a v1.x wallet has **no HD seed**
at all (it is a flat keypool of independent, non-deterministically generated
keys), so this is the seedless fixture that exercises zallet's seedless import
path. The wallet contains:

- Mined **transparent coinbase UTXOs** (the wallet's own funds)
- **Sprout shielded notes** (phases 0-1 shield coinbase to Sprout)
- **Imported foreign keys** (not derived from any seed): `importprivkey`
  (spendable), `importpubkey` (watch-only), `importaddress <redeemScript>`
  (watch-only P2SH), `importaddress <addr>` (watch-only address), and Sprout
  `z_importkey` / `z_importviewingkey`
- A **manifest** in the same schema as the full build
- **Full regtest chain state**

Because phases 0-1 are pre-Sapling (BCTV14), this build -- like the full build --
requires the `sprout-proving.key` (~910MB); it is part of the `make build` image.

## Prerequisites

- **Docker** installed and running.
- **Internet access** -- the first build downloads ~2GB of zcashd binaries and
  ~2.6GB of proving parameters (including the BCTV14 `sprout-proving.key`,
  ~910MB) from `download.z.cash`.
- **~7GB of free disk space** for the image (binaries + parameters + chain state).

## Quick Start

```bash
# Build the Docker image (~2GB binaries + ~2.6GB params on first run)
make build

# Build ONE wallet -- pick a mode:
make run        # the full multi-version wallet (~30-60 min)
# or
make ancient     # the pre-Sapling, non-deterministic wallet (a few minutes)

# Extract artifacts to ./output/ (same command for either mode)
make extract
```

> **First build downloads ~4.6GB.** If `make build` fails partway through with a
> network error, just run `make build` again: already-downloaded files are
> skipped, so only the missing ones are retried. The build deliberately
> **aborts** if a required proving parameter cannot be downloaded, rather than
> producing an image that only fails later at run time (with
> `Cannot find the Zcash network parameters`). If you ever hit that error at
> run time, your image predates this check -- rebuild with `make build`.

The full output is in `./output/`:
```
output/
  wallet.dat              # The test fixture wallet database (Berkeley DB)
  full_manifest.json      # Aggregated manifest (or the ancient manifest)
  manifests/              # Per-phase JSON manifests
  checkpoints/            # wallet.dat snapshot at each phase boundary (full mode)
  exports/                # dumpwallet / z_exportwallet output files
  regtest/                # Full regtest chain state (blocks, chainstate, wallet.dat)
  zcash-params/           # Zcash proving parameters (~2.6GB)
```

The `regtest/` directory and `zcash-params/` are needed to resume the chain for future NUs -- see [Resuming for Future NUs](#resuming-for-future-nus) below.

## Background: Zcash Network Upgrades

Zcash has undergone 10 network upgrades since its launch in October 2016. Each upgrade potentially changes the transaction format, adds new shielded pools, or modifies consensus rules. The wallet database format evolved alongside these upgrades.

| # | Network Upgrade | Mainnet Date | Key Change | Regtest Height |
|---|---|---|---|---|
| 0 | **Sprout** | Oct 2016 | Genesis | 0 |
| 1 | **Overwinter** | Jun 2018 | Tx format v3, replay protection | 200 |
| 2 | **Sapling** | Oct 2018 | New shielded pool (90% faster) | 350 |
| 3 | **Blossom** | Dec 2019 | Block time 150s -> 75s | 500 |
| 4 | **Heartwood** | Jul 2020 | Coinbase to shielded (ZIP 213) | 700 |
| 5 | **Canopy** | Nov 2020 | Sprout deposits disabled (ZIP 211) | 850 |
| 6 | **NU5** | May 2022 | Orchard pool, Unified Addresses | 1000 |
| 7 | **NU6** | Nov 2024 | Hybrid Dev Fund (ZIP 2001) | 1200 |
| 8 | **NU6.1** | Nov 2025 | Lockbox disbursement (ZIP 271) | 1400 |
| 9 | **NU6.2** | 2026 | See [z.cash/upgrade/nu6.2](https://z.cash/upgrade/nu6.2/) | 1600 |

## Version Matrix

Each NU era uses a zcashd version that supports that NU's consensus rules, approximating the real-world wallet upgrade path. `config/versions.json` is the single source of truth for the version matrix.

| Phase | NU | zcashd (used) | Historical target | Branch ID | Regtest Height |
|---|---|---|---|---|---|
| 0 | Sprout | v1.1.1 | v1.0.15 | (genesis) | 0 |
| 1 | Overwinter | v1.1.1 | v1.1.1 | `5ba81b19` | 200 |
| 2 | Sapling | v2.1.0-1 | v2.0.1 | `76b809bb` | 350 |
| 3 | Blossom | v2.1.0-1 | v2.1.0-1 | `2bb40e60` | 500 |
| 4 | Heartwood | v3.0.0 | v3.0.0 | `f5b9230b` | 700 |
| 5 | Canopy | v4.1.1 | v4.1.1 | `e9ff75a6` | 850 |
| 6 | NU5 | v5.0.0 | v5.0.0 | `c2d6d0b4` | 1000 |
| 7 | NU6 | v6.0.0 | v6.0.0 | `c8e71055` | 1200 |
| 8 | NU6.1 | v6.10.0 | v6.10.0 | `4dec4df0` | 1400 |
| 9 | NU6.2 | v6.20.0 | v6.20.0 | `5437f330` | 1600 |

Phases 0-1 (Sprout, Overwinter) run the real pre-Sapling release **v1.1.1**, which has no HD seed -- this is what makes `make ancient` (phases 0-1) a seedless fixture. Because v1.1.1 uses the BCTV14 `sprout-proving.key` (~910MB), the build requires it. Phases 2-3 run v2.1.0-1, so the binary switch lands at the Sapling boundary, which is also where Sprout proofs move from BCTV14 to Groth16. All versions verified against [GitHub releases](https://github.com/zcash/zcash/releases) and [download server](https://download.z.cash/downloads/). All use Berkeley DB for `wallet.dat` (per [ZIP 400](https://zips.z.cash/zip-0400)).

### Pool availability per phase

| Phase | transparent | Sprout | Sapling | Orchard |
|---|---|---|---|---|
| 0-1 (Sprout, Overwinter) | send/receive | send/receive | -- | -- |
| 2-4 (Sapling, Blossom, Heartwood) | send/receive | send/receive | send/receive | -- |
| 5 (Canopy) | send/receive | withdraw only | send/receive | -- |
| 6-9 (NU5, NU6, NU6.1, NU6.2) | send/receive | withdraw only | send/receive | send/receive |

## Architecture

Each phase uses a **different zcashd version**. Between phases, we stop the current version, swap the binary, update `zcash.conf` with the new NU's activation height, and restart. **No `-reindex` is used** -- LevelDB chainstate is forward-compatible across versions.

```
Host Machine                              Docker Container
   |                                          |
   |  make build                              |
   |  ---------> [Dockerfile]                 |
   |             1. Install deps              |
   |             2. Download params           |  /opt/zcash/params/  (~2.6GB)
   |             3. Download zcashd versions  |  /opt/zcash/versions/
   |             4. Validate all binaries     |
   |                                          |
   |  make run                               |
   |  ---------> [build_chain.sh]             |
   |             -> [phase_runner.py]         |
   |                                          |
   |             Phase 0: Start v1.1.1        |  (`make ancient` stops here)
   |               Mine, shield to Sprout     |
   |             Phase 1: Stay on v1.1.1      |  (Overwinter activates)
   |               Mine, Overwinter txs       |
   |             Phase 2: Switch to v2.1.0-1  |
   |               Sapling pool opens         |
   |             ...                          |
   |             Phase 9: Switch to v6.20.0   |
   |               Final operations           |
   |             Run verification suite       |
   |             Package artifacts            |  /artifacts/
   |                                          |
   |  make extract                            |
   |  <--------- docker cp                    |
   |  ./output/wallet.dat                     |
   |  ./output/full_manifest.json             |
   |  ./output/regtest/        (chain)        |
   |  ./output/zcash-params/   (2.6GB)        |
```

### How version switching works

Between phases, we: stop zcashd, swap the binary, write an updated `zcash.conf` with cumulative `-nuparams`, and restart. Each version only receives nuparams for the NUs it recognizes (e.g., v1.1.1 only gets Overwinter). Phases 0-1 share the v1.1.1 binary and phases 2-3 share v2.1.0-1, so within each pair the "swap" is just a config update; the actual binary switch happens once, at the Sapling boundary (phase 1 -> 2).

**No `-reindex` is needed.** LevelDB chainstate and block index are forward-compatible across zcashd versions. A newer version reads databases written by an older version without issue. Using `-reindex` is actively harmful: it re-validates every block from scratch, and newer versions may apply stricter rules that reject blocks the older version accepted.

**No `-rescan` either.** While `-rescan` (re-reading the blockchain to find wallet transactions) sounds useful after a version switch, it crashes zcashd with a Sprout commitment tree assertion failure when crossing major version boundaries. Shielded notes from prior versions ARE preserved in wallet.dat and become visible to v5.0.0+ (which reads all historical wallet formats). For intermediate versions (v2.1.0-1 through v4.1.1), shielded balances in the manifest are computed from transaction records rather than queried from zcashd.

### sprout-proving.key

The BCTV14 Sprout proving key (`sprout-proving.key`, ~910MB) is required by the pre-Sapling binary v1.1.1 used for phases 0-1 (it is loaded at startup and used to create Sprout JoinSplit proofs). It is **not** at the standard `/downloads/` CDN path -- it's at:

```
https://download.z.cash/zcashfinalmpc/sprout-proving.key
```

The `fetch_params.sh` script downloads it automatically as part of `make build`, so both `make run` and `make ancient` have it.

## Regtest Halving Constraint

The regtest halving interval is ~150 blocks. The block subsidy starts at ~10-12.5 ZEC and halves rapidly. By NU5 (block ~1000), coinbase rewards are ~0.19 ZEC per block. By NU6.1 (block ~1400), they're ~0.024 ZEC.

**Why this is OK for testing**: The existence of UTXOs and notes matters more than their amounts. Even dust-level coinbase UTXOs are valid test fixtures. The build compensates by mining aggressively in early phases to build a large transparent balance that funds cross-pool operations in later NUs.

## Configuration

### Adjusting Versions

Edit the `zcashd_version` fields in `lib/constants.py` (the `NETWORK_UPGRADES` list), update `config/versions.json`, and update the `VERSIONS` array in `scripts/download_binaries.sh`.

### Adjusting Activation Heights

Edit `lib/constants.py` and `config/activation_heights.json`. Heights must maintain the correct order (Overwinter < Sapling < Blossom < ...).

### Adjusting Transaction Operations

Edit `lib/operations.py` to change what transactions are created at each phase (how many UTXOs to shield, send amounts, etc.). The `_calculate_send_amount()` function in `lib/phase_runner.py` controls per-phase send amounts.

### Adjusting the Ancient Build

There is no separate ancient build -- `make ancient` is `make run` stopped after
phase 1 (`build_chain.sh 1` -> `run_all_phases(stop_after_phase=1)`), i.e. the
pre-Sapling eras (phases 0-1, both v1.1.1). To change which pre-Sapling binary it
produces, edit phases 0-1's `zcashd_version` in `config/versions.json` (e.g. to
`1.0.15`, though that lacks importpubkey/importaddress-p2sh).

## File Layout

```
zcash-regtest-wallet-builder/
  Dockerfile                     # Container definition (Debian Bookworm)
  Makefile                       # Convenience targets
  README.md                      # This file
  config/
    versions.json                # Version matrix with git commits and download URLs
    activation_heights.json      # NU -> regtest activation height
  scripts/
    download_binaries.sh         # Download zcashd versions (~2GB)
    fetch_params.sh              # Download proving parameters (incl. required sprout-proving.key)
    validate_binaries.sh         # Verify all binaries run on Debian Bookworm
    build_chain.sh               # Entry point: build_chain.sh [STOP_AFTER_PHASE]
  lib/
    __init__.py
    constants.py                 # Source of truth: versions, heights, pools per NU
    rpc_client.py                # JSON-RPC client for zcashd
    conf_generator.py            # Generate version-aware zcash.conf per phase
    zcashd_manager.py            # Start/stop/switch zcashd versions (no reindex)
    addresses.py                 # Version-aware address generation
    key_imports.py               # Import foreign keys (privkey, watch-only, P2SH, shielded)
    operations.py                # Pool-aware transaction operations
    manifest.py                  # Manifest creation and aggregation
    phase_runner.py              # Orchestration; run_all_phases(stop_after_phase=...)
    verification.py              # Post-build verification suite
```

## Manifest Schema

Each per-phase manifest (`manifests/phase_NN_<nu>.json`) contains:

| Field | Type | Description |
|---|---|---|
| `phase` | int | Phase number (0-8) |
| `network_upgrade` | string | NU identifier (e.g., "sapling") |
| `zcashd_version` | string | Version used for this phase (e.g., "2.1.0-1") |
| `zcashd_commit` | string | Git commit from `getnetworkinfo` |
| `activation_height` | int | Block height where this NU activates |
| `addresses` | object | Generated addresses per pool type |
| `viewing_keys` | object | Exported viewing keys per pool type |
| `external_addresses` | object | Throwaway addresses for external sends |
| `transactions` | array | All transactions: txid, height, amount, fee, pools |
| `balances_at_phase_end` | object | Balance per pool at phase end |
| `note_counts` | object | Unspent note count per shielded pool |
| `utxo_count` | int | Total unspent transparent UTXOs |
| `coinbase_utxo_count` | int | Unspent coinbase UTXOs specifically |
| `imported_keys` | object | Foreign keys imported this phase (see below) |
| `chain_info` | object | Block height, best block hash |

### Imported key types per phase

Each phase imports foreign keys (not derived from the wallet's HD seed) to test every import code path. Availability depends on the zcashd version:

| Import Type | Method | Phases |
|---|---|---|
| Transparent private key (spendable) | `importprivkey` | 0-8 |
| Transparent address (watch-only) | `importaddress` | 0-8 |
| P2SH redeem script (watch-only) | `importaddress` + `p2sh=true` | 3-8 |
| Public key (watch-only) | `importpubkey` | 3-8 |
| Sprout spending key | `z_importkey` | 0-4 |
| Sprout viewing key | `z_importviewingkey` | 0-4 |
| Sapling spending key | `z_importkey` | 2-8 |
| Sapling viewing key | `z_importviewingkey` | 4-8 |

Transparent keys are randomly generated (no zcashd needed). Shielded keys are generated by the external zcashd wallet and exported/imported -- they have valid cryptographic structure but are foreign to the primary wallet's seed.

Orchard/Unified key import is not possible (not implemented in zcashd; see [zcash/zcash#5687](https://github.com/zcash/zcash/issues/5687)).

## Resuming for Future NUs

The output includes everything needed to extend this chain when a new network upgrade (e.g., NU7) is released:

### What you need from `output/`

| Artifact | Purpose |
|---|---|
| `regtest/` | Full chain data directory -- blocks, chainstate, and wallet.dat. zcashd reads from it directly. |
| `zcash-params/` | Proving parameters required by zcashd at startup. |
| `full_manifest.json` | Records the last chain height, all activation heights, and nuparams. |

### Resume procedure

```bash
# 1. Download the new zcashd version
NEW_VERSION="7.0.0"  # example
curl -O "https://download.z.cash/downloads/zcash-${NEW_VERSION}-linux64-debian-bookworm.tar.gz"
tar xzf "zcash-${NEW_VERSION}-linux64-debian-bookworm.tar.gz"

# 2. Point zcash-params to the saved parameters
ln -sf "$(pwd)/output/zcash-params" ~/.zcash-params

# 3. Write zcash.conf with ALL prior activation heights + new NU
cat > output/zcash.conf <<EOF
regtest=1
server=1
rpcuser=test
rpcpassword=test
rpcport=18443
listen=0
txindex=1
nuparams=5ba81b19:200
nuparams=76b809bb:350
nuparams=2bb40e60:500
nuparams=f5b9230b:700
nuparams=e9ff75a6:850
nuparams=c2d6d0b4:1000
nuparams=c8e71055:1200
nuparams=4dec4df0:1400
nuparams=5437f330:1600
nuparams=NEW_BRANCH_ID:1800
allowdeprecated=z_getnewaddress
allowdeprecated=getnewaddress
allowdeprecated=legacy_privacy
EOF

# 4. Start zcashd (no -reindex needed)
./zcash-${NEW_VERSION}/bin/zcashd -datadir="$(pwd)/output"

# 5. Mine and operate
./zcash-${NEW_VERSION}/bin/zcash-cli -datadir="$(pwd)/output" generate 200
```

### Key points

- **Do NOT use `-reindex`** unless absolutely necessary. LevelDB chainstate is forward-compatible. Reindexing re-validates all blocks and newer versions may reject old blocks.
- **Do NOT use `-rescan`**. It crashes on Sprout commitment tree assertions when crossing major version boundaries.
- **nuparams must include ALL prior NUs** at their original heights, plus the new NU above the current chain tip.
- **Branch ID** comes from `src/consensus/upgrades.cpp` in the new zcashd source.
- **Proving parameters** may need updating. Run the new version's `zcash-fetch-params`.

## Troubleshooting

### zcashd won't start: deprecation acknowledgment

zcashd v6.10.0 requires `i-am-aware-zcashd-will-be-replaced-by-zebrad-and-zallet-in-2025=1` in `zcash.conf`. The build scripts add this automatically for phases that need it.

### zcashd won't start: missing parameters

`make build` downloads the proving parameters into the image and **fails loudly**
if a required one cannot be fetched, so any image that built successfully already
has them. If you hit `Cannot find the Zcash network parameters` at run time, the
image was built before this check (or from an interrupted download) -- rebuild
with `make build`; a transient network failure is the usual cause, and re-running
resumes and retries only the missing files.

The required files are `sprout-groth16.params`, `sapling-spend.params`,
`sapling-output.params`, `sprout-verifying.key`, and the legacy BCTV14
`sprout-proving.key` (~910MB) -- the last is needed by the pre-Sapling binary
(v1.1.1) used for phases 0-1, so the build fails if it can't be fetched.
`fetch_params.sh` also creates testnet-suffixed symlinks
(`sapling-spend-testnet.params`, etc.) for compatibility with older zcashd that
expects them.

### Sprout operations fail after Canopy

Expected. After Canopy (phase 5), Sprout deposits are disabled per ZIP 211. The build only sends FROM Sprout (withdrawals) in post-Canopy phases.

### Small amounts in later phases

Expected due to regtest halving (~150 block interval). The build compensates with early mining and scaled-down send amounts.

### --version exits with code 1

Old zcashd versions (v1.0.x) exit with code 1 for `--version` even when they print correctly. `validate_binaries.sh` checks the output string, not the exit code.

## Limitations

### Orchard cross-pool transfers

Cross-pool transfers FROM Orchard (e.g. Orchard -> transparent, Orchard -> Sapling) are skipped. zcashd v5.0.0 fails with `OrchardWallet::GetSpendInfo with unknown outpoint`; v6.0.0+ crashes the node entirely. Orchard intra-pool sends and shielding TO Orchard work fine. This means the fixture does not exercise Orchard withdrawal paths.

## References

- [Zcash Protocol Specification](https://zips.z.cash/protocol/protocol.pdf) -- Branch IDs, consensus rules
- [ZIP 200: Network Upgrade Mechanism](https://zips.z.cash/zip-0200) -- How NUs work
- [ZIP 211: Disabling Sprout Deposits](https://zips.z.cash/zip-0211) -- Canopy change
- [ZIP 213: Shielded Coinbase](https://zips.z.cash/zip-0213) -- Heartwood change
- [ZIP 224: Orchard Protocol](https://zips.z.cash/zip-0224) -- NU5 Orchard pool
- [ZIP 316: Unified Addresses](https://zips.z.cash/zip-0316) -- NU5 address format
- [ZIP 400: Wallet.dat format](https://zips.z.cash/zip-0400) -- Berkeley DB wallet format
- [zcashd RPC Documentation](https://zcash.github.io/rpc/) -- RPC method reference
- [zcashd Regtest Tips](https://zcash.github.io/zcash/dev/regtest.html) -- Regtest guide
- [zcashd GitHub Releases](https://github.com/zcash/zcash/releases) -- All zcashd versions
- [zcashd Binary Downloads](https://download.z.cash/downloads/) -- Pre-built binaries
