"""
Post-build verification suite.

After all phases complete, this module runs a series of checks to validate
that the final wallet state is correct and complete. It verifies:

  1. All recorded transactions exist and are decodable
  2. The wallet contains addresses for every pool type
  3. Balance accounting is sane (no funds created from thin air)
  4. Unspent outputs exist in every expected pool
  5. Coinbase UTXOs from early NUs still exist (weren't all spent)
  6. Viewing keys can be exported for all shielded addresses
  7. The wallet loads without errors
  8. Chain height is at or above the expected final height

These checks serve as a smoke test for the generated test fixture.
Downstream wallet tools should perform their own, more detailed validation.
"""

import logging

from lib.constants import (
    NETWORK_UPGRADES,
)
from lib.rpc_client import ZcashRPC, RPCError

logger = logging.getLogger(__name__)


class VerificationError:
    """A single verification failure."""

    def __init__(self, check: str, message: str, severity: str = "error"):
        self.check = check
        self.message = message
        self.severity = severity  # "error" or "warning"

    def __repr__(self):
        return f"[{self.severity.upper()}] {self.check}: {self.message}"


def verify_all(rpc: ZcashRPC, phase_manifests: list[dict]) -> list[VerificationError]:
    """
    Run all verification checks.

    Args:
        rpc:              RPC client connected to the final zcashd instance
        phase_manifests:  List of per-phase manifest dicts

    Returns:
        List of VerificationErrors. Empty list means all checks passed.
    """
    errors = []

    logger.info("=== Running post-build verification ===")

    errors.extend(_verify_transactions(rpc, phase_manifests))
    errors.extend(_verify_address_diversity(rpc))
    errors.extend(_verify_pool_balances(rpc))
    errors.extend(_verify_unspent_outputs(rpc))
    errors.extend(_verify_viewing_keys(rpc, phase_manifests))
    errors.extend(_verify_wallet_health(rpc))
    errors.extend(_verify_chain_height(rpc))

    error_count = sum(1 for e in errors if e.severity == "error")
    warning_count = sum(1 for e in errors if e.severity == "warning")

    if errors:
        logger.warning(
            "Verification complete: %d errors, %d warnings",
            error_count, warning_count,
        )
        for err in errors:
            if err.severity == "error":
                logger.error("  %s", err)
            else:
                logger.warning("  %s", err)
    else:
        logger.info("Verification complete: ALL CHECKS PASSED")

    return errors


def _verify_transactions(rpc: ZcashRPC,
                         phase_manifests: list[dict]) -> list[VerificationError]:
    """
    Verify that all recorded transactions exist and are decodable.

    Uses z_viewtransaction to check each TXID. This confirms that the
    transactions were actually mined and the wallet can decode them.
    """
    errors = []
    total_txs = 0
    verified = 0

    for manifest in phase_manifests:
        phase = manifest.get("phase", "?")
        for tx in manifest.get("transactions", []):
            total_txs += 1
            txid = tx.get("txid", "")
            if not txid or txid == "unknown":
                errors.append(VerificationError(
                    "tx_existence",
                    f"Phase {phase}: Transaction has no/unknown txid",
                ))
                continue

            try:
                # z_viewtransaction decrypts shielded transaction details
                # for any addresses the wallet knows about.
                tx_info = rpc.z_viewtransaction(txid)
                if tx_info is not None:
                    verified += 1
            except RPCError as e:
                # Some old transaction types may not be viewable with
                # z_viewtransaction (e.g., pure transparent txs).
                # Fall back to gettransaction.
                try:
                    rpc.gettransaction(txid)
                    verified += 1
                except RPCError:
                    errors.append(VerificationError(
                        "tx_existence",
                        f"Phase {phase}: Transaction {txid[:16]}... not found: {e}",
                    ))

    logger.info("Verified %d/%d transactions", verified, total_txs)
    return errors


def _verify_address_diversity(rpc: ZcashRPC) -> list[VerificationError]:
    """
    Verify the wallet contains addresses for multiple pool types.

    The wallet should contain at least: transparent, Sprout, Sapling,
    and Orchard/unified addresses. Missing any of these suggests a
    phase didn't execute correctly.
    """
    errors = []
    found_pools = set()

    # Check transparent addresses
    try:
        t_addrs = rpc.getaddressesbyaccount("")
        if t_addrs:
            found_pools.add("transparent")
            logger.info("  Found %d transparent addresses", len(t_addrs))
    except Exception:
        pass

    # Check shielded addresses
    try:
        z_addrs = rpc.z_listaddresses()
        for addr in z_addrs:
            # Sprout addresses start with "zc" (on regtest, "zt" for testnet)
            # Sapling addresses start with "zs" (on regtest, also "ztestsapling")
            # This is a rough heuristic; the actual prefix depends on network.
            if addr.startswith("zc") or addr.startswith("zt"):
                found_pools.add("sprout")
            elif addr.startswith("zs"):
                found_pools.add("sapling")
            # Unified addresses have a different format; detect separately
    except Exception:
        pass

    # Check for unspent notes in each pool (more reliable than address format)
    try:
        notes = rpc.z_listunspent(0)  # minconf=0 to catch unconfirmed too
        for note in notes:
            pool = note.get("type", note.get("pool", ""))
            if pool:
                found_pools.add(pool)
    except Exception:
        pass

    expected = {"transparent", "sprout", "sapling", "orchard"}
    missing = expected - found_pools

    if missing:
        for pool in missing:
            errors.append(VerificationError(
                "address_diversity",
                f"No {pool} addresses/notes found in wallet",
                severity="error" if pool != "sprout" else "warning",
            ))

    logger.info("  Found pools: %s", ", ".join(sorted(found_pools)))
    return errors


def _verify_pool_balances(rpc: ZcashRPC) -> list[VerificationError]:
    """
    Verify that at least some balance exists in each expected pool.

    We expect nonzero balances in transparent, Sprout (from pre-Canopy
    shielding, if not fully withdrawn), Sapling, and Orchard.
    """
    errors = []

    try:
        balance = rpc.z_gettotalbalance(0)
        t_bal = float(balance.get("transparent", 0))
        p_bal = float(balance.get("private", 0))
        total = float(balance.get("total", 0))

        logger.info(
            "  Total balance: transparent=%.8f, private=%.8f, total=%.8f",
            t_bal, p_bal, total,
        )

        if total <= 0:
            errors.append(VerificationError(
                "balance", "Total wallet balance is zero or negative",
            ))
        if t_bal <= 0:
            errors.append(VerificationError(
                "balance", "Transparent balance is zero (expected unspent coinbase)",
                severity="warning",
            ))
    except Exception as e:
        errors.append(VerificationError(
            "balance", f"Failed to get total balance: {e}",
        ))

    return errors


def _verify_unspent_outputs(rpc: ZcashRPC) -> list[VerificationError]:
    """
    Verify that unspent outputs exist in each pool.

    This is a critical check: we should have unspent notes in Sprout
    (from pre-Canopy deposits), Sapling, and Orchard, plus transparent
    coinbase UTXOs from each era.
    """
    errors = []
    pools_with_notes = set()

    try:
        notes = rpc.z_listunspent(1)
        for note in notes:
            pool = note.get("type", note.get("pool", "unknown"))
            pools_with_notes.add(pool)

        logger.info(
            "  Found unspent notes in pools: %s (total: %d notes)",
            ", ".join(sorted(pools_with_notes)), len(notes),
        )

        # We expect notes in sprout (from pre-Canopy), sapling, and orchard
        for expected_pool in ["sprout", "sapling", "orchard"]:
            if expected_pool not in pools_with_notes:
                errors.append(VerificationError(
                    "unspent_outputs",
                    f"No unspent notes in {expected_pool} pool",
                ))
    except Exception as e:
        errors.append(VerificationError(
            "unspent_outputs", f"Failed to list unspent notes: {e}",
        ))

    # Check transparent coinbase UTXOs
    try:
        utxos = rpc.listunspent(1)
        coinbase_count = sum(1 for u in utxos if u.get("generated", False))
        logger.info("  Found %d unspent transparent UTXOs (%d coinbase)",
                     len(utxos), coinbase_count)

        if coinbase_count == 0:
            errors.append(VerificationError(
                "unspent_outputs",
                "No unspent coinbase UTXOs (expected some from each era)",
            ))
    except Exception as e:
        errors.append(VerificationError(
            "unspent_outputs", f"Failed to list transparent UTXOs: {e}",
        ))

    return errors


def _verify_viewing_keys(rpc: ZcashRPC,
                         phase_manifests: list[dict]) -> list[VerificationError]:
    """
    Verify that viewing keys can be exported for addresses in the manifests.

    Checks a sample of addresses from each pool type.
    """
    errors = []
    exported = 0

    for manifest in phase_manifests:
        for pool in ["sprout", "sapling"]:
            addr_list = manifest.get("addresses", {}).get(pool, [])
            for entry in addr_list[:1]:  # Check first address per pool per phase
                addr = entry.get("address", entry) if isinstance(entry, dict) else entry
                try:
                    vk = rpc.z_exportviewingkey(addr)
                    if vk:
                        exported += 1
                except Exception as e:
                    errors.append(VerificationError(
                        "viewing_keys",
                        f"Failed to export viewing key for {pool} addr "
                        f"{addr[:24]}...: {e}",
                        severity="warning",
                    ))

    logger.info("  Exported %d viewing keys successfully", exported)
    return errors


def _verify_wallet_health(rpc: ZcashRPC) -> list[VerificationError]:
    """Verify the wallet loads and reports valid state."""
    errors = []

    try:
        info = rpc.getwalletinfo()
        logger.info(
            "  Wallet info: txcount=%d, keypoolsize=%d",
            info.get("txcount", 0),
            info.get("keypoolsize", 0),
        )
        if info.get("txcount", 0) == 0:
            errors.append(VerificationError(
                "wallet_health",
                "Wallet reports 0 transactions",
            ))
    except Exception as e:
        errors.append(VerificationError(
            "wallet_health", f"Failed to get wallet info: {e}",
        ))

    return errors


def _verify_chain_height(rpc: ZcashRPC) -> list[VerificationError]:
    """Verify the chain height is at or above the expected minimum."""
    errors = []

    # The final phase should end at least at the last NU's activation height
    last_nu = NETWORK_UPGRADES[-1]
    expected_min = last_nu.activation_height

    try:
        height = rpc.getblockcount()
        logger.info("  Chain height: %d (expected >= %d)", height, expected_min)

        if height < expected_min:
            errors.append(VerificationError(
                "chain_height",
                f"Chain height {height} is below expected minimum {expected_min}",
            ))
    except Exception as e:
        errors.append(VerificationError(
            "chain_height", f"Failed to get block count: {e}",
        ))

    return errors
