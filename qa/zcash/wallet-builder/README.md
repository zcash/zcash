# zcash-regtest-wallet-builder

Build a Zcash regtest chain with a `wallet.dat` containing keys and transaction history from **every network upgrade** -- Sprout through NU6.1. The result is a test fixture for integration and regression testing of Zcash wallet tools.

## What This Produces

A `wallet.dat` progressively built through **9 zcashd versions** (one per network upgrade era), simulating the real-world wallet upgrade path. The wallet contains:

- **Transparent addresses** with coinbase UTXOs from every NU era
- **Sprout shielded notes** from the Sprout through Heartwood eras (pre-Canopy)
- **Sapling shielded notes** from Sapling through NU6.1
- **Orchard shielded notes** and Unified Addresses from NU5 through NU6.1
- **Cross-pool transfers**: every active pool to every other active pool at each NU (except Orchard as source -- see [Limitations](#limitations))
- **External sends**: outgoing transactions to addresses the wallet doesn't control
- **Imported foreign keys**: `importprivkey`, `importaddress` (watch-only), `importaddress` (P2SH), `importpubkey`, `z_importkey` (Sprout/Sapling spending), `z_importviewingkey` (Sprout/Sapling viewing) -- all from keys NOT derived from the wallet's HD seed
- **Viewing keys** exported for read-only wallet testing
- **Per-phase manifests** documenting every TXID, mined height, value, fee, address, and imported key
- **Full regtest chain state** and Zcash proving parameters for resuming with future NUs

## Quick Start

```bash
# Build the Docker image (~2GB binaries + ~1.7GB params on first run)
make build

# Run the chain build
make run

# Extract artifacts to ./output/
make extract
```

Or all at once:
```bash
make all
```

The full output is in `./output/`:
```
output/
  wallet.dat              # The test fixture wallet database (Berkeley DB)
  full_manifest.json      # Aggregated manifest across all NUs
  manifests/              # Per-phase JSON manifests
  checkpoints/            # wallet.dat snapshot at each phase boundary
  exports/                # z_exportwallet output files
  regtest/                # Full regtest chain state (blocks, chainstate, wallet.dat)
  zcash-params/           # Zcash proving parameters (~1.7GB)
```

The `regtest/` directory and `zcash-params/` are needed to resume the chain for future NUs -- see [Resuming for Future NUs](#resuming-for-future-nus) below.

## Background: Zcash Network Upgrades

Zcash has undergone 9 network upgrades since its launch in October 2016. Each upgrade potentially changes the transaction format, adds new shielded pools, or modifies consensus rules. The wallet database format evolved alongside these upgrades.

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

## Version Matrix

Each NU era uses the zcashd version that was current when the NU activated on mainnet, simulating the real-world wallet upgrade path.

| Phase | NU | zcashd | Released | Branch ID | Regtest Height |
|---|---|---|---|---|---|
| 0 | Sprout | v1.0.15 | Mar 2018 | (genesis) | 0 |
| 1 | Overwinter | v1.1.1 | May 2018 | `5ba81b19` | 200 |
| 2 | Sapling | v2.0.1 | Oct 2018 | `76b809bb` | 350 |
| 3 | Blossom | v2.1.0-1 | Nov 2019 | `2bb40e60` | 500 |
| 4 | Heartwood | v3.0.0 | May 2020 | `f5b9230b` | 700 |
| 5 | Canopy | v4.1.1 | Nov 2020 | `e9ff75a6` | 850 |
| 6 | NU5 | v5.0.0 | May 2022 | `c2d6d0b4` | 1000 |
| 7 | NU6 | v6.0.0 | Oct 2024 | `c8e71055` | 1200 |
| 8 | NU6.1 | v6.10.0 | Oct 2025 | `4dec4df0` | 1400 |

All versions verified against [GitHub releases](https://github.com/zcash/zcash/releases) and [download server](https://download.z.cash/downloads/). All use Berkeley DB for `wallet.dat` (per [ZIP 400](https://zips.z.cash/zip-0400)).

### Pool availability per phase

| Phase | transparent | Sprout | Sapling | Orchard |
|---|---|---|---|---|
| 0-1 (Sprout, Overwinter) | send/receive | send/receive | -- | -- |
| 2-4 (Sapling, Blossom, Heartwood) | send/receive | send/receive | send/receive | -- |
| 5 (Canopy) | send/receive | withdraw only | send/receive | -- |
| 6-8 (NU5, NU6, NU6.1) | send/receive | withdraw only | send/receive | send/receive |

## Architecture

Each phase uses a **different zcashd version**. Between phases, we stop the current version, swap the binary, update `zcash.conf` with the new NU's activation height, and restart. **No `-reindex` is used** -- LevelDB chainstate is forward-compatible across versions.

```
Host Machine                              Docker Container
   |                                          |
   |  make build                              |
   |  ---------> [Dockerfile]                 |
   |             1. Install deps              |
   |             2. Download params           |  /opt/zcash/params/  (~1.7GB)
   |             3. Download 9 zcashd versions|  /opt/zcash/versions/
   |             4. Validate all binaries     |
   |                                          |
   |  make run                                |
   |  ---------> [build_chain.sh]             |
   |             -> [phase_runner.py]         |
   |                                          |
   |             Phase 0: Start v1.0.15       |
   |               Mine, shield to Sprout     |
   |             Phase 1: Switch to v1.1.1    |  (stop, swap, restart)
   |               Mine, Overwinter txs       |
   |             Phase 2: Switch to v2.0.1    |
   |               Sapling pool opens         |
   |             ...                          |
   |             Phase 8: Switch to v6.10.0   |
   |               Final operations           |
   |             Run verification suite       |
   |             Package artifacts            |  /artifacts/
   |                                          |
   |  make extract                            |
   |  <--------- docker cp                    |
   |  ./output/wallet.dat                     |
   |  ./output/full_manifest.json             |
   |  ./output/regtest/        (chain)        |
   |  ./output/zcash-params/   (1.7GB)        |
```

### How version switching works

Between phases, we: stop zcashd, swap the binary, write an updated `zcash.conf` with cumulative `-nuparams`, and restart. Each version only receives nuparams for the NUs it recognizes (e.g., v1.0.15 gets none, v2.0.1 gets Overwinter + Sapling).

**No `-reindex` is needed.** LevelDB chainstate and block index are forward-compatible across zcashd versions. A newer version reads databases written by an older version without issue. Using `-reindex` is actively harmful: it re-validates every block from scratch, and newer versions may apply stricter rules that reject blocks the older version accepted.

**No `-rescan` either.** While `-rescan` (re-reading the blockchain to find wallet transactions) sounds useful after a version switch, it crashes zcashd with a Sprout commitment tree assertion failure when crossing major version boundaries. Shielded notes from prior versions ARE preserved in wallet.dat and become visible to v5.0.0+ (which reads all historical wallet formats). For intermediate versions (v1.1.1-v4.1.1), shielded balances in the manifest are computed from transaction records rather than queried from zcashd.

### sprout-proving.key

The BCTV14 Sprout proving key (`sprout-proving.key`, ~910MB) is required by zcashd v1.0.15 through v2.0.1 to start (file existence check at startup). This file is NOT at the standard `/downloads/` CDN path -- it's at:

```
https://download.z.cash/zcashfinalmpc/sprout-proving.key
```

The `fetch_params.sh` script downloads it automatically.

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
    orchestrate.sh               # Host-side: build + run + extract
    download_binaries.sh         # Download all 9 zcashd versions (~2GB)
    fetch_params.sh              # Download proving parameters (~1.7GB incl. sprout-proving.key)
    validate_binaries.sh         # Verify all binaries run on Debian Bookworm
    build_chain.sh               # Container entry point
  lib/
    __init__.py
    constants.py                 # Source of truth: 9 versions, heights, pools per NU
    rpc_client.py                # JSON-RPC client for zcashd
    conf_generator.py            # Generate version-aware zcash.conf per phase
    zcashd_manager.py            # Start/stop/switch zcashd versions (no reindex)
    addresses.py                 # Version-aware address generation
    key_imports.py               # Import foreign keys (privkey, watch-only, P2SH, shielded)
    operations.py                # Pool-aware transaction operations
    manifest.py                  # Manifest creation and aggregation
    phase_runner.py              # Per-phase orchestration with version switching
    verification.py              # Post-build verification suite
```

## Manifest Schema

Each per-phase manifest (`manifests/phase_NN_<nu>.json`) contains:

| Field | Type | Description |
|---|---|---|
| `phase` | int | Phase number (0-8) |
| `network_upgrade` | string | NU identifier (e.g., "sapling") |
| `zcashd_version` | string | Version used for this phase (e.g., "2.0.1") |
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
nuparams=NEW_BRANCH_ID:1600
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

Ensure `~/.zcash-params` contains `sprout-proving.key`, `sprout-verifying.key`, `sprout-groth16.params`, `sapling-spend.params`, and `sapling-output.params`. For v1.1.1, testnet-suffixed symlinks are also needed (`sapling-spend-testnet.params`, etc.) -- `fetch_params.sh` creates these.

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
