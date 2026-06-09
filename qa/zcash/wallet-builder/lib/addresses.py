"""
Version-aware address generation for each zcashd era.

The RPC API for generating addresses changed significantly across zcashd versions:

  v1.0.x - v4.x:  Use legacy RPCs:
                     getnewaddress()           -> transparent (t-addr)
                     z_getnewaddress("sprout")  -> Sprout shielded (zc...)
                     z_getnewaddress("sapling") -> Sapling shielded (zs...)

  v5.0+:           Legacy RPCs deprecated. New account-based system:
                     z_getnewaccount()                          -> HD account
                     z_getaddressforaccount(acct, receivers)    -> Unified Address

                   Legacy RPCs still work if -allowdeprecated is set in zcash.conf,
                   which we do (see conf_generator.py).

This module provides a version-aware interface that uses the appropriate API
for each phase, abstracting away the version differences.
"""

import logging
from dataclasses import dataclass, field

from lib.constants import (
    Pool,
    PoolPermission,
    NETWORK_UPGRADES,
)
from lib.rpc_client import ZcashRPC

logger = logging.getLogger(__name__)


@dataclass
class GeneratedAddresses:
    """
    Addresses generated for a single build phase.

    Tracks all addresses created during a phase, organized by pool type.
    Also tracks unified address metadata (account, diversifier, receivers)
    for NU5+ phases.
    """
    transparent: list[str] = field(default_factory=list)
    sprout: list[str] = field(default_factory=list)
    sapling: list[str] = field(default_factory=list)
    orchard: list[str] = field(default_factory=list)
    unified: list[dict] = field(default_factory=list)  # Full UA metadata

    def get_by_pool(self, pool: Pool) -> list[str]:
        """Get address list for a given pool type."""
        return {
            Pool.TRANSPARENT: self.transparent,
            Pool.SPROUT: self.sprout,
            Pool.SAPLING: self.sapling,
            Pool.ORCHARD: self.orchard,
        }[pool]

    def to_dict(self) -> dict:
        """Serialize for manifest output."""
        return {
            "transparent": [{"address": a, "type": "p2pkh"} for a in self.transparent],
            "sprout": [{"address": a, "type": "sprout"} for a in self.sprout],
            "sapling": [{"address": a, "type": "sapling"} for a in self.sapling],
            "orchard": [{"address": a, "type": "orchard"} for a in self.orchard],
            "unified": self.unified,
        }


def generate_phase_addresses(rpc: ZcashRPC, phase_index: int,
                             count_per_pool: int = 2) -> GeneratedAddresses:
    """
    Generate addresses for all active pools at a given build phase.

    Creates `count_per_pool` addresses for each pool that accepts deposits
    at this phase. This gives us variety in the wallet (multiple addresses
    per pool type) and enough addresses for both internal use and as
    sources for cross-pool transfers.

    Args:
        rpc:            RPC client connected to the running zcashd
        phase_index:    Index into NETWORK_UPGRADES
        count_per_pool: Number of addresses to generate per pool type

    Returns:
        GeneratedAddresses with all created addresses.
    """
    nu = NETWORK_UPGRADES[phase_index]
    addrs = GeneratedAddresses()

    logger.info("Generating addresses for phase %d (%s)...", phase_index, nu.name)

    # Transparent addresses are always available
    for i in range(count_per_pool):
        addr = rpc.getnewaddress()
        addrs.transparent.append(addr)
        logger.debug("  transparent[%d]: %s", i, addr)

    # Sprout addresses: available phases 0-4, deposit-only through phase 4.
    # v1.0.x and v1.1.x: z_getnewaddress() with NO argument (Sprout was the
    # only shielded pool, so no type parameter existed).
    # v2.0+: z_getnewaddress("sprout") to distinguish from Sapling.
    if nu.pools.get(Pool.SPROUT) == PoolPermission.FULL:
        has_sapling = nu.pools.get(Pool.SAPLING) != PoolPermission.UNAVAILABLE
        for i in range(count_per_pool):
            addr = rpc.z_getnewaddress("sprout") if has_sapling else rpc.z_getnewaddress()
            addrs.sprout.append(addr)
            logger.debug("  sprout[%d]: %s", i, addr[:32] + "...")

    # Sapling addresses: available from phase 2 (Sapling) onwards.
    # In v2.0-v4.x: z_getnewaddress("sapling")
    # In v5.0+: z_getnewaddress("sapling") still works with -allowdeprecated,
    #           but we also create unified addresses with Sapling receivers.
    if nu.pools.get(Pool.SAPLING) == PoolPermission.FULL:
        for i in range(count_per_pool):
            addr = rpc.z_getnewaddress("sapling")
            addrs.sapling.append(addr)
            logger.debug("  sapling[%d]: %s", i, addr[:32] + "...")

    # Orchard addresses: available from phase 6 (NU5) onwards.
    # Orchard addresses are only available via Unified Addresses (UAs).
    # A UA bundles receivers for multiple pools (e.g., transparent + Sapling + Orchard).
    # We create UAs with all available receiver types.
    if nu.pools.get(Pool.ORCHARD) == PoolPermission.FULL:
        for i in range(count_per_pool):
            # Create a new HD account (or reuse account 0)
            # Each z_getnewaccount() creates a new account with a new spending key.
            # For simplicity, we create one account per address.
            account_info = rpc.z_getnewaccount()
            account_id = account_info["account"]

            # Generate a UA with all receiver types: transparent, Sapling, Orchard
            # This gives us a single address string that can receive in any pool.
            ua_info = rpc.z_getaddressforaccount(
                account_id,
                ["p2pkh", "sapling", "orchard"],
            )
            ua_address = ua_info["address"]

            addrs.orchard.append(ua_address)
            addrs.unified.append({
                "address": ua_address,
                "account": account_id,
                "diversifier_index": ua_info.get("diversifier_index"),
                "receiver_types": ua_info.get("receiver_types",
                                               ["p2pkh", "sapling", "orchard"]),
            })
            logger.debug(
                "  orchard/UA[%d]: account=%d, addr=%s",
                i, account_id, ua_address[:32] + "...",
            )

    total = (len(addrs.transparent) + len(addrs.sprout) +
             len(addrs.sapling) + len(addrs.orchard))
    logger.info(
        "Generated %d addresses: %d transparent, %d sprout, %d sapling, %d orchard/UA",
        total, len(addrs.transparent), len(addrs.sprout),
        len(addrs.sapling), len(addrs.orchard),
    )
    return addrs


def generate_external_addresses(rpc: ZcashRPC, phase_index: int) -> GeneratedAddresses:
    """
    Generate throwaway addresses from the external wallet.

    These addresses are used as recipients for "send to addresses I don't
    control" transactions in the primary wallet. The external wallet exists
    solely for address generation and is not used for any other purpose.

    The primary wallet will record outgoing transactions to these addresses,
    which is important for testing wallet tools that need to handle sends
    to external/unknown addresses.

    Args:
        rpc:          RPC client connected to the EXTERNAL zcashd instance
        phase_index:  Current build phase

    Returns:
        GeneratedAddresses with one address per active pool type.
    """
    nu = NETWORK_UPGRADES[phase_index]
    addrs = GeneratedAddresses()

    logger.info("Generating external addresses for phase %d (%s)...",
                phase_index, nu.name)

    # One transparent address
    addrs.transparent.append(rpc.getnewaddress())

    # One address per active shielded pool
    if nu.pools.get(Pool.SPROUT) == PoolPermission.FULL:
        has_sapling = nu.pools.get(Pool.SAPLING) != PoolPermission.UNAVAILABLE
        addrs.sprout.append(
            rpc.z_getnewaddress("sprout") if has_sapling else rpc.z_getnewaddress()
        )

    if nu.pools.get(Pool.SAPLING) == PoolPermission.FULL:
        addrs.sapling.append(rpc.z_getnewaddress("sapling"))

    if nu.pools.get(Pool.ORCHARD) == PoolPermission.FULL:
        account_info = rpc.z_getnewaccount()
        ua_info = rpc.z_getaddressforaccount(
            account_info["account"],
            ["p2pkh", "sapling", "orchard"],
        )
        addrs.orchard.append(ua_info["address"])

    logger.info(
        "Generated external addresses: t=%d, sprout=%d, sapling=%d, orchard=%d",
        len(addrs.transparent), len(addrs.sprout),
        len(addrs.sapling), len(addrs.orchard),
    )
    return addrs


def export_viewing_keys(rpc: ZcashRPC, addrs: GeneratedAddresses) -> dict:
    """
    Export viewing keys for all shielded addresses.

    Viewing keys provide read-only access to the wallet. They can see
    incoming transactions but cannot spend funds. This is used to test
    read-only wallet access in wallet tools.

    z_exportviewingkey is supported for:
      - Sprout addresses: exports an incoming viewing key
      - Sapling addresses: exports a full viewing key (ak, nk, ovk, dk)
      - Unified/Orchard: support varies by zcashd version; may not work
        for all address types.

    Args:
        rpc:   RPC client
        addrs: Addresses to export viewing keys for

    Returns:
        Dict mapping pool type -> list of {address, viewing_key} dicts.
    """
    result = {"sprout": [], "sapling": [], "orchard": []}

    for addr in addrs.sprout:
        try:
            vk = rpc.z_exportviewingkey(addr)
            result["sprout"].append({"address": addr, "viewing_key": vk})
        except Exception as e:
            logger.warning("Failed to export viewing key for Sprout addr %s: %s",
                           addr[:32] + "...", e)

    for addr in addrs.sapling:
        try:
            vk = rpc.z_exportviewingkey(addr)
            result["sapling"].append({"address": addr, "viewing_key": vk})
        except Exception as e:
            logger.warning("Failed to export viewing key for Sapling addr %s: %s",
                           addr[:32] + "...", e)

    # Orchard/UA viewing key export -- may not be supported in all versions.
    # z_exportviewingkey on a unified address should export a unified
    # viewing key (UFVK) in zcashd v5.4+. Earlier v5.x versions may not
    # support this.
    for addr in addrs.orchard:
        try:
            vk = rpc.z_exportviewingkey(addr)
            result["orchard"].append({"address": addr, "viewing_key": vk})
        except Exception as e:
            logger.warning(
                "Failed to export viewing key for Orchard/UA addr %s: %s "
                "(this may be expected for older zcashd versions)",
                addr[:32] + "...", e,
            )

    exported_count = sum(len(v) for v in result.values())
    logger.info("Exported %d viewing keys", exported_count)
    return result
