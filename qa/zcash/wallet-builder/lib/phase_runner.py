"""
Per-phase orchestration: the main build loop.

This module implements the core logic for building the regtest chain.
It runs each phase in sequence:

  For each network upgrade (phase 0..8):
    1. Start zcashd (with version switch + reindex if needed)
    2. Generate addresses for available pools
    3. Mine blocks for coinbase creation and maturity
    4. Execute transaction operations (shield, send, cross-pool, external)
    5. Mine to the next NU activation height
    6. Record manifest (balances, TXIDs, addresses, viewing keys)
    7. Save wallet checkpoint
    8. Stop zcashd

The external (throwaway) wallet is started separately at each phase to
generate addresses the primary wallet doesn't control.

Usage (called by scripts/build_chain.sh):
    from lib.phase_runner import run_all_phases
    run_all_phases()
"""

import logging
import os
import shutil
import subprocess

from lib.constants import (
    NETWORK_UPGRADES,
    PRIMARY_DATADIR,
    EXTERNAL_DATADIR,
    ARTIFACTS_DIR,
    FINAL_DIR,
    RPC_PORT_PRIMARY,
    RPC_PORT_EXTERNAL,
    COINBASE_MATURITY,
    TARGET_COINBASE_PER_PHASE,
)
from lib.rpc_client import ZcashRPC
from lib.zcashd_manager import (
    start_zcashd,
    stop_zcashd,
    switch_version,
    checkpoint_wallet,
)
from lib.addresses import (
    generate_phase_addresses,
    generate_external_addresses,
    export_viewing_keys,
)
from lib.operations import (
    mine_and_mature,
    execute_phase_operations,
    mine_to_height,
)
from lib.manifest import (
    create_phase_manifest,
    write_phase_manifest,
    write_full_manifest,
)
from lib.verification import verify_all
from lib.key_imports import import_keys_for_phase

logger = logging.getLogger(__name__)


def run_all_phases():
    """
    Run all build phases to create the complete regtest chain.

    This is the top-level entry point. It iterates through all network
    upgrades, executing the build operations for each one, and produces
    the final artifacts (wallet.dat, manifests, exports).
    """
    logger.info("=" * 70)
    logger.info("Starting regtest chain build: %d phases", len(NETWORK_UPGRADES))
    logger.info("=" * 70)

    phase_manifests = []
    all_transactions = []  # Accumulated tx records for computed balances
    process = None
    rpc = None

    try:
        for i, nu in enumerate(NETWORK_UPGRADES):
            logger.info("")
            logger.info("=" * 70)
            logger.info(
                "PHASE %d/%d: %s (zcashd v%s, activation height %d)",
                i, len(NETWORK_UPGRADES) - 1,
                nu.name, nu.zcashd_version, nu.activation_height,
            )
            logger.info("=" * 70)

            manifest = run_single_phase(
                phase_index=i,
                prev_process=process,
                prev_rpc=rpc,
                all_prior_transactions=all_transactions,
            )
            phase_manifests.append(manifest)

            # Update process/rpc handles for next phase
            process = manifest.pop("_process", None)
            rpc = manifest.pop("_rpc", None)

        # ---------------------------------------------------------------
        # Final steps: verification and artifact packaging
        # ---------------------------------------------------------------

        logger.info("")
        logger.info("=" * 70)
        logger.info("ALL PHASES COMPLETE -- Running verification")
        logger.info("=" * 70)

        if rpc is not None:
            errors = verify_all(rpc, phase_manifests)
            if errors:
                logger.warning(
                    "Verification found %d issues (see above)", len(errors)
                )
            else:
                logger.info("All verification checks passed!")

        # Write the aggregated full manifest
        write_full_manifest(phase_manifests)

        # Copy the final wallet.dat to the artifacts directory
        _package_final_artifacts()

        logger.info("")
        logger.info("=" * 70)
        logger.info("BUILD COMPLETE")
        logger.info("  Artifacts: %s", ARTIFACTS_DIR)
        logger.info("  Final wallet: %s/wallet.dat", FINAL_DIR)
        logger.info("  Full manifest: %s/full_manifest.json", FINAL_DIR)
        logger.info("=" * 70)

    finally:
        # Always stop zcashd when we're done
        if process is not None or rpc is not None:
            logger.info("Stopping zcashd...")
            stop_zcashd(rpc=rpc, process=process)


def run_single_phase(phase_index: int,
                     prev_process: subprocess.Popen | None = None,
                     prev_rpc: ZcashRPC | None = None,
                     all_prior_transactions: list | None = None) -> dict:
    """
    Execute a single build phase.

    This handles the full lifecycle for one NU era: starting the correct
    zcashd version, executing operations, and recording results.

    Args:
        phase_index:   Index into NETWORK_UPGRADES
        prev_process:  Popen for the previously running zcashd (None for phase 0)
        prev_rpc:      RPC client for the previous zcashd (None for phase 0)

    Returns:
        The phase manifest dict, with extra "_process" and "_rpc" keys
        containing the running zcashd state for the next phase.
    """
    nu = NETWORK_UPGRADES[phase_index]

    # ---------------------------------------------------------------
    # 1. Start zcashd (fresh start or version switch)
    # ---------------------------------------------------------------
    if phase_index == 0:
        process = start_zcashd(phase_index=0, datadir=PRIMARY_DATADIR)
        rpc = ZcashRPC(rpcport=RPC_PORT_PRIMARY)
        rpc.wait_for_ready(timeout=180)
    else:
        # Stop previous version, swap binary, update conf, restart.
        # No -reindex: LevelDB chainstate is forward-compatible.
        process, rpc = switch_version(
            from_phase=phase_index - 1,
            to_phase=phase_index,
            rpc=prev_rpc,
            process=prev_process,
            datadir=PRIMARY_DATADIR,
        )

    from lib.zcashd_manager import verify_version
    verify_version(rpc, nu.zcashd_version)

    # ---------------------------------------------------------------
    # 2. Generate addresses for this phase
    # ---------------------------------------------------------------
    addrs = generate_phase_addresses(rpc, phase_index)

    # ---------------------------------------------------------------
    # 3. Start external wallet, generate external addresses + import keys
    # ---------------------------------------------------------------
    ext_process, ext_rpc = _start_external_wallet(phase_index)
    external_addrs = generate_external_addresses(ext_rpc, phase_index)
    imported_keys = import_keys_for_phase(rpc, ext_rpc, phase_index)
    stop_zcashd(rpc=ext_rpc, process=ext_process)

    # ---------------------------------------------------------------
    # 5. Mine blocks for coinbase creation + maturity
    # ---------------------------------------------------------------
    if phase_index < len(NETWORK_UPGRADES) - 1:
        next_activation = NETWORK_UPGRADES[phase_index + 1].activation_height
    else:
        next_activation = None

    current_height = rpc.getblockcount()
    # Reserve blocks for tx confirmations + inter-operation mining gaps.
    # v6+ phases need more room due to extra mining for Orchard note indexing.
    operation_room = 80 if phase_index >= 6 else 50

    if next_activation is not None:
        max_mine = next_activation - 1 - current_height - operation_room
    else:
        max_mine = TARGET_COINBASE_PER_PHASE + COINBASE_MATURITY

    desired = 200 if phase_index == 0 else TARGET_COINBASE_PER_PHASE + COINBASE_MATURITY
    actual_mine = min(desired, max(0, max_mine))

    if actual_mine > 0:
        mine_and_mature(rpc, num_blocks=actual_mine)
    else:
        logger.warning("No room to mine in phase %d (height %d, next NU at %s)",
                        phase_index, current_height, next_activation)

    # ---------------------------------------------------------------
    # 5. Execute transaction operations
    # ---------------------------------------------------------------
    send_amount = _calculate_send_amount(phase_index)

    transactions = execute_phase_operations(
        rpc=rpc,
        phase_index=phase_index,
        addrs=addrs,
        external_addrs=external_addrs,
        send_amount=send_amount,
    )

    # ---------------------------------------------------------------
    # 6. Mine to one block BELOW the next NU activation height
    # ---------------------------------------------------------------
    # We stop at next_activation - 1 so the next phase's version (with
    # the new nuparams) mines the activation block itself.
    if next_activation is not None:
        mine_to_height(rpc, next_activation - 1)
    else:
        # Final phase: mine a few extra blocks for good measure
        rpc.generate(10)

    # ---------------------------------------------------------------
    # 7. Export viewing keys
    # ---------------------------------------------------------------
    viewing_keys = export_viewing_keys(rpc, addrs)

    # ---------------------------------------------------------------
    # 8. Export full wallet (for reference/debugging)
    # ---------------------------------------------------------------
    _export_wallet(rpc, phase_index)

    # ---------------------------------------------------------------
    # 9. Create and write phase manifest
    # ---------------------------------------------------------------
    # Add this phase's transactions to the accumulator for computed balances
    if all_prior_transactions is not None:
        all_prior_transactions.append(transactions)

    manifest = create_phase_manifest(
        phase_index=phase_index,
        rpc=rpc,
        addrs=addrs,
        external_addrs=external_addrs,
        viewing_keys=viewing_keys,
        transactions=transactions,
        all_prior_transactions=all_prior_transactions,
    )
    if imported_keys is not None:
        manifest["imported_keys"] = imported_keys.to_dict()
    write_phase_manifest(manifest, phase_index)

    # Checkpoint wallet.dat for this phase
    checkpoint_wallet(phase_index, PRIMARY_DATADIR)

    # Pass the running zcashd state to the next phase
    manifest["_process"] = process
    manifest["_rpc"] = rpc

    logger.info(
        "Phase %d (%s) complete: %d transactions, chain height %d",
        phase_index, nu.name, len(transactions), rpc.getblockcount(),
    )

    return manifest


def _start_external_wallet(phase_index: int) -> tuple[subprocess.Popen, ZcashRPC]:
    """Start the external zcashd wallet for address/key generation."""
    ext_datadir = os.path.join(EXTERNAL_DATADIR, f"phase_{phase_index:02d}")
    logger.info("Starting external zcashd for address/key generation...")
    ext_process = start_zcashd(
        phase_index=phase_index,
        datadir=ext_datadir,
        rpcport=RPC_PORT_EXTERNAL,
        reindex=False,
    )
    ext_rpc = ZcashRPC(rpcport=RPC_PORT_EXTERNAL)
    ext_rpc.wait_for_ready(timeout=60)
    return ext_process, ext_rpc


def _calculate_send_amount(phase_index: int) -> float:
    """
    Calculate appropriate send amounts based on the phase.

    Later phases have smaller coinbase rewards due to the regtest halving
    schedule (~150 blocks per halving). We reduce send amounts to match
    available balances.

    Returns:
        Send amount in ZEC.
    """
    # Phase 0 (Sprout): coinbase is 10-12.5 ZEC, use 5 ZEC sends
    # Phase 1-2: coinbase is ~3-6 ZEC, use 2 ZEC sends
    # Phase 3-4: coinbase is ~0.5-1.5 ZEC, use 0.5 ZEC sends
    # Phase 5-6: coinbase is ~0.1-0.4 ZEC, use 0.1 ZEC sends
    # Phase 7-8: coinbase is ~0.02-0.05 ZEC, use 0.01 ZEC sends
    amounts = [5.0, 2.0, 2.0, 0.5, 0.5, 0.1, 0.1, 0.01, 0.01]
    if phase_index < len(amounts):
        return amounts[phase_index]
    return 0.01


def _export_wallet(rpc: ZcashRPC, phase_index: int):
    """Export the full wallet for reference (z_exportwallet)."""
    nu = NETWORK_UPGRADES[phase_index]
    # Old zcashd versions only accept alphanumeric filenames (no underscores,
    # dots, or hyphens). Use a simple concatenated name.
    filename = f"walletexportphase{phase_index:02d}{nu.id}"

    try:
        rpc.z_exportwallet(filename)
        logger.info("Exported wallet to %s", filename)
    except Exception as e:
        # z_exportwallet may fail in early zcashd versions or if exportdir
        # is not configured. This is non-fatal.
        logger.warning("z_exportwallet failed (non-fatal): %s", e)


def _package_final_artifacts():
    """
    Copy the final wallet.dat and chain data to the artifacts directory.

    This creates the final deliverable: a wallet.dat and regtest chain
    data directory that can be used by downstream wallet tools.
    """
    os.makedirs(FINAL_DIR, exist_ok=True)

    # Copy wallet.dat
    wallet_src = os.path.join(PRIMARY_DATADIR, "regtest", "wallet.dat")
    wallet_dst = os.path.join(FINAL_DIR, "wallet.dat")
    if os.path.isfile(wallet_src):
        shutil.copy2(wallet_src, wallet_dst)
        size_mb = os.path.getsize(wallet_dst) / (1024 * 1024)
        logger.info("Copied final wallet.dat (%.1f MB) to %s", size_mb, wallet_dst)
    else:
        logger.error("wallet.dat not found at %s!", wallet_src)

    # Copy the regtest data directory (for full chain state)
    regtest_src = os.path.join(PRIMARY_DATADIR, "regtest")
    regtest_dst = os.path.join(FINAL_DIR, "regtest")
    if os.path.isdir(regtest_src):
        if os.path.exists(regtest_dst):
            shutil.rmtree(regtest_dst)
        shutil.copytree(regtest_src, regtest_dst)
        logger.info("Copied regtest data directory to %s", regtest_dst)
