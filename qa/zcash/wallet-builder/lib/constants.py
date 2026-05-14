"""
Central source of truth for the regtest chain build.

This module defines ALL configuration constants used across the build pipeline:
  - Network upgrade definitions (branch IDs, activation heights)
  - zcashd version matrix (which version to use for each NU)
  - Pool availability per NU (what shielded pools exist)
  - Version-specific RPC quirks (deprecated flags, API differences)

All values are documented with references to the Zcash protocol specification,
relevant ZIPs, and zcashd release notes.

NOTE ON VERSION NUMBERS: The version strings below represent the target
"latest zcashd release before each NU activated on mainnet." These should
be verified against https://github.com/zcash/zcash/releases before use.
If a version is unavailable for download, use the closest available version.
"""

from enum import Enum, auto
from dataclasses import dataclass, field


# =====================================================================
# Pool Types
# =====================================================================
# Zcash has evolved through multiple shielded pool types. Each pool uses
# different cryptography and has different capabilities:
#
#   transparent: Standard Bitcoin-like UTXOs. On-chain visible.
#   sprout:      Original shielded pool (2016). Uses BCTV14/Groth16 proofs.
#                Deprecated; deposits disabled after Canopy (ZIP 211).
#   sapling:     Second-generation shielded pool (2018). Much faster proof
#                generation. Uses Groth16 with a specific circuit.
#   orchard:     Third-generation shielded pool (2022). Uses Halo2 proofs
#                (no trusted setup). Introduced with Unified Addresses.
# =====================================================================

class Pool(Enum):
    """Zcash value pool types."""
    TRANSPARENT = "transparent"
    SPROUT = "sprout"
    SAPLING = "sapling"
    ORCHARD = "orchard"


class PoolPermission(Enum):
    """Whether a pool allows deposits, withdrawals, or both."""
    FULL = auto()          # Can send to and from this pool
    WITHDRAW_ONLY = auto() # Can only send FROM this pool (Sprout after Canopy)
    UNAVAILABLE = auto()   # Pool doesn't exist yet at this NU


# =====================================================================
# Network Upgrade Definitions
# =====================================================================
# Each network upgrade is identified by a branch ID (a 32-bit value defined
# in the Zcash protocol spec). In regtest mode, NUs are activated at specific
# block heights via the -nuparams configuration option.
#
# Branch IDs are from the Zcash protocol specification, section 3.12
# "Mainnet and Testnet Network Upgrade Activation Sequence":
#   https://zips.z.cash/protocol/protocol.pdf
#
# The activation heights below are chosen for our regtest build. The spacing
# of ~150-200 blocks per NU is tight to minimize the impact of the regtest
# halving interval (~150 blocks), which rapidly reduces coinbase rewards.
# =====================================================================

@dataclass
class NetworkUpgrade:
    """Definition of a Zcash network upgrade for our regtest build."""

    # Human-readable name
    name: str

    # Short identifier used in file paths and manifest keys
    id: str

    # Branch ID as lowercase hex string (without 0x prefix).
    # This is the value passed to -nuparams in zcash.conf.
    # None for Sprout (genesis, no branch ID needed).
    branch_id: str | None

    # Activation height in our regtest chain.
    # Sprout activates at genesis (0).
    activation_height: int

    # The zcashd version to use for this phase.
    # This is the "latest version released before the NU activated on mainnet."
    # The version should support this NU's consensus rules.
    zcashd_version: str

    # Pool availability at this NU.
    # Maps Pool -> PoolPermission for each pool that exists.
    pools: dict[Pool, PoolPermission]

    # NUs that this zcashd version knows about (for zcash.conf generation).
    # Each version only gets nuparams for the NUs it recognizes. Passing an
    # unknown branch ID will cause zcashd to error or behave unpredictably.
    known_nus: list[str]  # list of NU ids (e.g., ["overwinter", "sapling"])

    # Extra zcash.conf options needed for this version.
    # Primarily -allowdeprecated flags for v5.0+ which deprecated many RPCs.
    extra_conf: list[str] = field(default_factory=list)

    # Phase number (0-indexed, matches file naming)
    phase: int = 0

    # Notes about this phase for documentation
    notes: str = ""


# The complete sequence of network upgrades for our regtest build.
# This is the master list that drives the entire pipeline.
#
# VERSION NUMBERS: Verified against https://github.com/zcash/zcash/releases
# and https://download.z.cash/downloads/. Each version is the latest release
# BEFORE the corresponding NU activated on mainnet.
# All versions use Berkeley DB for wallet.dat (ZIP 400). No SQLite migration.

NETWORK_UPGRADES = [
    NetworkUpgrade(
        name="Sprout",
        id="sprout",
        branch_id=None,  # Genesis -- no branch ID
        activation_height=0,
        # Commit: 93332d6e84590cdcb8c64282f9b525152c3f1b2f (Mar 28, 2018)
        # Download: zcash-1.0.15-linux64.tar.gz
        zcashd_version="1.0.15",
        pools={
            Pool.TRANSPARENT: PoolPermission.FULL,
            Pool.SPROUT: PoolPermission.FULL,
            Pool.SAPLING: PoolPermission.UNAVAILABLE,
            Pool.ORCHARD: PoolPermission.UNAVAILABLE,
        },
        # v1.0.x predates the -nuparams mechanism (added in v1.1.0 for
        # Overwinter). It runs regtest as pure Sprout with no NU awareness.
        known_nus=[],
        extra_conf=[],
        phase=0,
        notes=(
            "Genesis era. Mine aggressively here to build the transparent "
            "'bank' (~1500 ZEC) that funds operations in later NUs where "
            "coinbase rewards are small due to halving."
        ),
    ),
    NetworkUpgrade(
        name="Overwinter",
        id="overwinter",
        # ZIP 200: Network Upgrade Mechanism
        branch_id="5ba81b19",
        activation_height=200,
        # Commit: 047b0bf94a2018a5f842219d3d5cdc157f9a52da (May 29, 2018)
        # Download: zcash-1.1.1-linux64.tar.gz
        zcashd_version="1.1.1",
        pools={
            Pool.TRANSPARENT: PoolPermission.FULL,
            Pool.SPROUT: PoolPermission.FULL,
            Pool.SAPLING: PoolPermission.UNAVAILABLE,
            Pool.ORCHARD: PoolPermission.UNAVAILABLE,
        },
        known_nus=["overwinter"],
        extra_conf=[],
        phase=1,
        notes=(
            "Overwinter introduces transaction format v3 and replay protection. "
            "No new pools, but the transaction format change is significant "
            "for wallet database testing."
        ),
    ),
    NetworkUpgrade(
        name="Sapling",
        id="sapling",
        # ZIP 243: Transaction Signature Validation for Sapling
        branch_id="76b809bb",
        activation_height=350,
        # Commit: e8f5e592b864b341391c9becf80bbe3e0e930a33 (Oct 15, 2018)
        # Download: zcash-2.0.1-linux64.tar.gz
        zcashd_version="2.0.1",
        pools={
            Pool.TRANSPARENT: PoolPermission.FULL,
            Pool.SPROUT: PoolPermission.FULL,
            Pool.SAPLING: PoolPermission.FULL,
            Pool.ORCHARD: PoolPermission.UNAVAILABLE,
        },
        known_nus=["overwinter", "sapling"],
        extra_conf=[],
        phase=2,
        notes=(
            "Sapling introduces the second shielded pool with dramatically "
            "improved performance (90% faster proving, 97% less memory). "
            "First NU where cross-pool transfers are possible "
            "(Sprout -> transparent -> Sapling)."
        ),
    ),
    NetworkUpgrade(
        name="Blossom",
        id="blossom",
        # ZIP 206: Deployment of the Blossom Network Upgrade
        branch_id="2bb40e60",
        activation_height=500,
        # Commit: 253fcaa997d3d5e30c3789c825a82e1ed3e7a3fe (Nov 12, 2019)
        # Download: zcash-2.1.0-1-linux64-debian-stretch.tar.gz
        zcashd_version="2.1.0-1",
        pools={
            Pool.TRANSPARENT: PoolPermission.FULL,
            Pool.SPROUT: PoolPermission.FULL,
            Pool.SAPLING: PoolPermission.FULL,
            Pool.ORCHARD: PoolPermission.UNAVAILABLE,
        },
        known_nus=["overwinter", "sapling", "blossom"],
        extra_conf=[],
        phase=3,
        notes=(
            "Blossom halves the target block time from 150s to 75s. "
            "No new pools, but the halving schedule changes."
        ),
    ),
    NetworkUpgrade(
        name="Heartwood",
        id="heartwood",
        # ZIP 250: Deployment of the Heartwood Network Upgrade
        branch_id="f5b9230b",
        activation_height=700,
        # Commit: de2e1160db45fe3f6c252b699cf56000c9104d76 (May 27, 2020)
        # Download: zcash-3.0.0-linux64-debian-stretch.tar.gz
        zcashd_version="3.0.0",
        pools={
            Pool.TRANSPARENT: PoolPermission.FULL,
            Pool.SPROUT: PoolPermission.FULL,
            Pool.SAPLING: PoolPermission.FULL,
            Pool.ORCHARD: PoolPermission.UNAVAILABLE,
        },
        known_nus=["overwinter", "sapling", "blossom", "heartwood"],
        extra_conf=[],
        phase=4,
        notes=(
            "Heartwood enables mining coinbase directly to shielded addresses "
            "(ZIP 213) and FlyClient support (ZIP 221)."
        ),
    ),
    NetworkUpgrade(
        name="Canopy",
        id="canopy",
        # ZIP 251: Deployment of the Canopy Network Upgrade
        branch_id="e9ff75a6",
        activation_height=850,
        # Commit: 6d856869e9c4cb9e6f3332db6fb04b956bb9fd3d (Nov 17, 2020)
        # Download: zcash-4.1.1-linux64-debian-stretch.tar.gz
        zcashd_version="4.1.1",
        pools={
            Pool.TRANSPARENT: PoolPermission.FULL,
            # ZIP 211: no new Sprout deposits after Canopy.
            Pool.SPROUT: PoolPermission.WITHDRAW_ONLY,
            Pool.SAPLING: PoolPermission.FULL,
            Pool.ORCHARD: PoolPermission.UNAVAILABLE,
        },
        known_nus=["overwinter", "sapling", "blossom", "heartwood", "canopy"],
        extra_conf=[],
        phase=5,
        notes=(
            "Canopy disables Sprout deposits (ZIP 211) and introduces the "
            "development fund (ZIP 1014)."
        ),
    ),
    NetworkUpgrade(
        name="NU5",
        id="nu5",
        # ZIP 252: Deployment of the NU5 Network Upgrade
        branch_id="c2d6d0b4",
        activation_height=1000,
        # Commit: 16b49eadd56086aae10f2907e5b8e77b773f1813 (May 11, 2022)
        # Download: zcash-5.0.0-linux64-debian-buster.tar.gz
        zcashd_version="5.0.0",
        pools={
            Pool.TRANSPARENT: PoolPermission.FULL,
            Pool.SPROUT: PoolPermission.WITHDRAW_ONLY,
            Pool.SAPLING: PoolPermission.FULL,
            Pool.ORCHARD: PoolPermission.FULL,  # ZIP 224
        },
        known_nus=[
            "overwinter", "sapling", "blossom", "heartwood", "canopy", "nu5",
        ],
        extra_conf=[
            "allowdeprecated=z_getnewaddress",
            "allowdeprecated=z_listaddresses",
            "allowdeprecated=z_getbalance",
            "allowdeprecated=z_gettotalbalance",
            "allowdeprecated=getnewaddress",
            "allowdeprecated=legacy_privacy",
        ],
        phase=6,
        notes=(
            "NU5 introduces the Orchard shielded pool (ZIP 224), Unified "
            "Addresses (ZIP 316), v5 transaction format, and Halo2 proofs."
        ),
    ),
    NetworkUpgrade(
        name="NU6",
        id="nu6",
        # ZIP 253: Deployment of the NU6 Network Upgrade
        branch_id="c8e71055",
        activation_height=1200,
        # Commit: c61e5473ae1fee18ac57e14123d3943323f0f04c (Oct 4, 2024)
        # Download: zcash-6.0.0-linux64-debian-bullseye.tar.gz
        zcashd_version="6.0.0",
        pools={
            Pool.TRANSPARENT: PoolPermission.FULL,
            Pool.SPROUT: PoolPermission.WITHDRAW_ONLY,
            Pool.SAPLING: PoolPermission.FULL,
            Pool.ORCHARD: PoolPermission.FULL,
        },
        known_nus=[
            "overwinter", "sapling", "blossom", "heartwood", "canopy",
            "nu5", "nu6",
        ],
        extra_conf=[
            "allowdeprecated=z_getnewaddress",
            "allowdeprecated=z_listaddresses",
            "allowdeprecated=z_getbalance",
            "allowdeprecated=z_gettotalbalance",
            "allowdeprecated=getnewaddress",
            "allowdeprecated=legacy_privacy",
        ],
        phase=7,
        notes="NU6: Hybrid Dev Fund, lockbox mechanism (ZIP 2001).",
    ),
    NetworkUpgrade(
        name="NU6.1",
        id="nu6_1",
        branch_id="4dec4df0",
        activation_height=1400,
        # Commit: 385c6d907cc09e4f21e2d5ad2d1ce4ce14d05aa0 (Oct 4, 2025)
        # Download: zcash-6.10.0-linux64-debian-bookworm.tar.gz
        zcashd_version="6.10.0",
        pools={
            Pool.TRANSPARENT: PoolPermission.FULL,
            Pool.SPROUT: PoolPermission.WITHDRAW_ONLY,
            Pool.SAPLING: PoolPermission.FULL,
            Pool.ORCHARD: PoolPermission.FULL,
        },
        known_nus=[
            "overwinter", "sapling", "blossom", "heartwood", "canopy",
            "nu5", "nu6", "nu6_1",
        ],
        extra_conf=[
            "allowdeprecated=z_getnewaddress",
            "allowdeprecated=z_listaddresses",
            "allowdeprecated=z_getbalance",
            "allowdeprecated=z_gettotalbalance",
            "allowdeprecated=getnewaddress",
            "allowdeprecated=legacy_privacy",
            "i-am-aware-zcashd-will-be-replaced-by-zebrad-and-zallet-in-2025=1",
        ],
        phase=8,
        notes="NU6.1: deferred lockbox disbursement (ZIP 271).",
    ),
]


# =====================================================================
# Derived lookup structures
# =====================================================================

def get_nu_by_id(nu_id: str) -> NetworkUpgrade:
    """Look up a network upgrade by its short id (e.g., 'sapling')."""
    for nu in NETWORK_UPGRADES:
        if nu.id == nu_id:
            return nu
    raise KeyError(f"Unknown network upgrade: {nu_id}")


def get_nu_by_phase(phase: int) -> NetworkUpgrade:
    """Look up a network upgrade by its phase number."""
    for nu in NETWORK_UPGRADES:
        if nu.phase == phase:
            return nu
    raise KeyError(f"Unknown phase: {phase}")


def get_active_pools(nu: NetworkUpgrade) -> list[Pool]:
    """
    Return pools that allow deposits at this NU.

    Only pools with FULL permission are returned. WITHDRAW_ONLY pools
    (like Sprout after Canopy) are excluded because you can't send TO them.
    """
    return [
        pool for pool, perm in nu.pools.items()
        if perm == PoolPermission.FULL
    ]


def get_withdrawable_pools(nu: NetworkUpgrade) -> list[Pool]:
    """
    Return pools that allow withdrawals (sending FROM) at this NU.

    Includes both FULL and WITHDRAW_ONLY pools.
    """
    return [
        pool for pool, perm in nu.pools.items()
        if perm in (PoolPermission.FULL, PoolPermission.WITHDRAW_ONLY)
    ]


def get_all_pools_at_nu(nu: NetworkUpgrade) -> list[Pool]:
    """Return all pools that exist (are not UNAVAILABLE) at this NU."""
    return [
        pool for pool, perm in nu.pools.items()
        if perm != PoolPermission.UNAVAILABLE
    ]


# =====================================================================
# RPC Configuration
# =====================================================================

# Default RPC credentials for our regtest setup.
# These must match the values in zcash.conf (generated by conf_generator.py).
RPC_USER = "test"
RPC_PASSWORD = "test"
RPC_PORT_PRIMARY = 18443
RPC_PORT_EXTERNAL = 18444

# Coinbase maturity: the number of confirmations required before a coinbase
# UTXO can be spent. This is a consensus rule in Zcash (inherited from Bitcoin).
# In regtest, this is the same as mainnet: 100 blocks.
COINBASE_MATURITY = 100

# Number of coinbase UTXOs to create per NU phase by mining blocks.
# We mine more than this because additional blocks are needed for maturity
# and transaction confirmations.
TARGET_COINBASE_PER_PHASE = 110

# Maximum UTXOs to shield per z_shieldcoinbase call.
# zcashd limits this to prevent transaction size from exceeding limits.
SHIELD_BATCH_LIMIT = 50

# Number of coinbase UTXOs to shield per pool per phase.
# We shield a subset and keep the rest for future NUs.
SHIELD_COUNT_PER_POOL = 20


# =====================================================================
# File paths (inside Docker container)
# =====================================================================

# Base directory for zcashd binaries (each version in a subdirectory)
ZCASHD_BIN_DIR = "/opt/zcash/versions"

# Directory for zcash proving parameters
ZCASH_PARAMS_DIR = "/opt/zcash/params"

# Primary wallet data directory
PRIMARY_DATADIR = "/data/primary"

# External (throwaway) wallet data directory
EXTERNAL_DATADIR = "/data/external"

# Output artifacts directory
ARTIFACTS_DIR = "/artifacts"
CHECKPOINTS_DIR = f"{ARTIFACTS_DIR}/checkpoints"
MANIFESTS_DIR = f"{ARTIFACTS_DIR}/manifests"
EXPORTS_DIR = f"{ARTIFACTS_DIR}/exports"
FINAL_DIR = f"{ARTIFACTS_DIR}/final"


# =====================================================================
# Download URLs
# =====================================================================
# Binary download base URL. The actual filename varies by version:
#   Old versions (v1.x-v3.x): zcash-{version}-linux64.tar.gz
#   Newer versions (v4.x+):   zcash-{version}-linux64-debian-{codename}.tar.gz
#
# The download_binaries.sh script handles probing for the correct filename.
DOWNLOAD_BASE_URL = "https://download.z.cash/downloads"
