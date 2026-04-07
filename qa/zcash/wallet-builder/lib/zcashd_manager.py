"""
Manage zcashd process lifecycle: start, stop, version switching, and health checks.

This module handles transitioning between zcashd versions. When switching
from one version to another, we:
  1. Stop the current zcashd gracefully
  2. Back up wallet.dat (safety checkpoint)
  3. Write a new zcash.conf with the correct nuparams
  4. Start the new version (NO -reindex)
  5. Wait for RPC readiness
  6. Verify the chain height is unchanged

WHY NOT -reindex:
LevelDB chainstate and block index are forward-compatible across zcashd
versions. A newer version can read databases written by an older version.
Using -reindex is harmful: it re-validates every block from scratch, and
newer versions may apply stricter validation rules that reject blocks the
older version accepted (we observed v6.0.0 rolling the chain back from
height 1199 to 17 when reindexing blocks from v5.0.0).

wallet.dat (Berkeley DB) is also forward-compatible -- each version adds
new record types that older versions simply ignore.
"""

import logging
import os
import shutil
import signal
import subprocess
import time

from lib.constants import (
    NETWORK_UPGRADES,
    ZCASHD_BIN_DIR,
    PRIMARY_DATADIR,
    CHECKPOINTS_DIR,
    RPC_PORT_PRIMARY,
)
from lib.rpc_client import ZcashRPC
from lib.conf_generator import write_conf

logger = logging.getLogger(__name__)


def get_zcashd_binary(version: str) -> str:
    """
    Return the path to the zcashd binary for a given version.

    The download_binaries.sh script extracts each version into:
        /opt/zcash/versions/{version}/bin/zcashd

    Args:
        version: Version string, e.g., "1.0.15" or "5.10.0"

    Returns:
        Absolute path to the zcashd binary.

    Raises:
        FileNotFoundError: If the binary doesn't exist.
    """
    binary = os.path.join(ZCASHD_BIN_DIR, version, "bin", "zcashd")
    if not os.path.isfile(binary):
        # Some tarballs extract to zcash-{version}/bin/zcashd
        alt_binary = os.path.join(ZCASHD_BIN_DIR, f"zcash-{version}", "bin", "zcashd")
        if os.path.isfile(alt_binary):
            return alt_binary
        raise FileNotFoundError(
            f"zcashd binary not found at {binary} or {alt_binary}. "
            f"Run download_binaries.sh to download version {version}."
        )
    return binary


def get_zcash_cli_binary(version: str) -> str:
    """Return the path to the zcash-cli binary for a given version."""
    cli = os.path.join(ZCASHD_BIN_DIR, version, "bin", "zcash-cli")
    if not os.path.isfile(cli):
        alt_cli = os.path.join(ZCASHD_BIN_DIR, f"zcash-{version}", "bin", "zcash-cli")
        if os.path.isfile(alt_cli):
            return alt_cli
        raise FileNotFoundError(f"zcash-cli not found for version {version}")
    return cli


def start_zcashd(phase_index: int, datadir: str = PRIMARY_DATADIR,
                 rpcport: int = RPC_PORT_PRIMARY,
                 reindex: bool = False,
                 miner_address: str | None = None,
                 extra_args: list[str] | None = None) -> subprocess.Popen:
    """
    Start zcashd for a given build phase.

    This writes the appropriate zcash.conf and launches zcashd as a
    background process. Use wait_for_ready() on the returned RPC client
    to block until zcashd is accepting commands.

    Args:
        phase_index:    Index into NETWORK_UPGRADES
        datadir:        Data directory for this zcashd instance
        rpcport:        RPC port
        reindex:        If True, add -reindex flag (needed after version switch)
        miner_address:  Optional shielded mining address (Heartwood+ only)
        extra_args:     Additional command-line arguments

    Returns:
        The subprocess.Popen object for the zcashd process.
    """
    nu = NETWORK_UPGRADES[phase_index]
    binary = get_zcashd_binary(nu.zcashd_version)

    # Write zcash.conf for this phase
    write_conf(phase_index, datadir, rpcport, miner_address)

    # Build command
    cmd = [
        binary,
        f"-datadir={datadir}",
        "-daemon=0",  # Run in foreground so we can track the process
        "-printtoconsole=0",  # Don't spam stdout; use debug.log
    ]

    if reindex:
        cmd.append("-reindex")
        logger.info(
            "Starting zcashd v%s with -reindex (this may take several minutes)...",
            nu.zcashd_version,
        )
    else:
        logger.info("Starting zcashd v%s...", nu.zcashd_version)

    if extra_args:
        cmd.extend(extra_args)

    logger.debug("Command: %s", " ".join(cmd))

    # Start zcashd as a background process.
    # We redirect stdout/stderr to debug.log in the datadir.
    log_path = os.path.join(datadir, "regtest", "zcashd_stdout.log")
    os.makedirs(os.path.join(datadir, "regtest"), exist_ok=True)

    log_file = open(log_path, "a", encoding="utf8")  # noqa: SIM115
    process = subprocess.Popen(
        cmd,
        stdout=log_file,
        stderr=subprocess.STDOUT,
        start_new_session=True,  # Detach from our process group
    )

    logger.info(
        "zcashd v%s started (PID %d), logging to %s",
        nu.zcashd_version, process.pid, log_path,
    )
    return process


def stop_zcashd(rpc: ZcashRPC | None = None,
                process: subprocess.Popen | None = None,
                timeout: int = 60):
    """
    Stop zcashd gracefully.

    Tries the RPC `stop` command first (clean shutdown). If that fails
    or times out, sends SIGTERM to the process.

    Args:
        rpc:     RPC client (if available, used for clean shutdown)
        process: The Popen object (if available, used as fallback)
        timeout: Maximum seconds to wait for shutdown
    """
    # Try clean shutdown via RPC
    if rpc is not None:
        try:
            rpc.stop()
            logger.info("Sent RPC stop command, waiting for shutdown...")
        except Exception as e:
            logger.warning("RPC stop failed: %s", e)

    # Wait for the process to exit
    if process is not None:
        try:
            process.wait(timeout=timeout)
            logger.info("zcashd stopped (exit code %d)", process.returncode)
            return
        except subprocess.TimeoutExpired:
            logger.warning(
                "zcashd didn't stop within %ds, sending SIGTERM...", timeout
            )
            process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=15)
                logger.info("zcashd stopped after SIGTERM")
            except subprocess.TimeoutExpired:
                logger.error("zcashd didn't respond to SIGTERM, sending SIGKILL")
                process.kill()
                process.wait()
    else:
        # No process handle -- just wait and hope the RPC stop worked
        time.sleep(5)


def checkpoint_wallet(phase_index: int, datadir: str = PRIMARY_DATADIR):
    """
    Save a copy of wallet.dat as a checkpoint before a version transition.

    This is our safety net: if a reindex or version switch corrupts the
    wallet, we can restore from this checkpoint.

    Args:
        phase_index: Current phase number (used in checkpoint filename)
        datadir:     zcashd data directory containing wallet.dat

    Returns:
        Path to the checkpoint file.
    """
    nu = NETWORK_UPGRADES[phase_index]
    wallet_path = os.path.join(datadir, "regtest", "wallet.dat")

    if not os.path.isfile(wallet_path):
        logger.warning("No wallet.dat found at %s -- skipping checkpoint", wallet_path)
        return None

    os.makedirs(CHECKPOINTS_DIR, exist_ok=True)
    checkpoint_path = os.path.join(
        CHECKPOINTS_DIR,
        f"phase_{phase_index:02d}_{nu.id}_wallet.dat",
    )
    shutil.copy2(wallet_path, checkpoint_path)
    size_mb = os.path.getsize(checkpoint_path) / (1024 * 1024)
    logger.info(
        "Checkpointed wallet.dat for phase %d (%s): %s (%.1f MB)",
        phase_index, nu.name, checkpoint_path, size_mb,
    )
    return checkpoint_path


def restore_wallet(checkpoint_path: str, datadir: str = PRIMARY_DATADIR):
    """
    Restore wallet.dat from a checkpoint.

    Used when a version switch or reindex goes wrong.

    Args:
        checkpoint_path: Path to the checkpoint wallet.dat
        datadir:         zcashd data directory to restore into
    """
    wallet_path = os.path.join(datadir, "regtest", "wallet.dat")
    shutil.copy2(checkpoint_path, wallet_path)
    logger.info("Restored wallet.dat from %s", checkpoint_path)


def switch_version(from_phase: int, to_phase: int,
                   rpc: ZcashRPC | None = None,
                   process: subprocess.Popen | None = None,
                   datadir: str = PRIMARY_DATADIR) -> tuple[subprocess.Popen, ZcashRPC]:
    """
    Switch from one zcashd version to another.

    This is the core version-transition logic. It:
      1. Stops the current zcashd
      2. Checkpoints wallet.dat
      3. Starts the new version with -reindex
      4. Waits for the new version to be ready
      5. Verifies chain state consistency

    Args:
        from_phase: Phase index of the currently running zcashd
        to_phase:   Phase index of the new zcashd version to start
        rpc:        RPC client for the current zcashd (for clean shutdown)
        process:    Popen object for the current zcashd process
        datadir:    zcashd data directory

    Returns:
        Tuple of (new_process, new_rpc) for the new zcashd instance.
    """
    from_nu = NETWORK_UPGRADES[from_phase]
    to_nu = NETWORK_UPGRADES[to_phase]

    logger.info(
        "=== Switching zcashd: v%s (%s) -> v%s (%s) ===",
        from_nu.zcashd_version, from_nu.name,
        to_nu.zcashd_version, to_nu.name,
    )

    # Record expected chain height before shutdown
    expected_height = None
    if rpc is not None:
        try:
            expected_height = rpc.getblockcount()
            logger.info("Chain height before switch: %d", expected_height)
        except Exception as e:
            logger.warning("Couldn't get chain height before switch: %s", e)

    # 1. Stop current zcashd
    stop_zcashd(rpc=rpc, process=process)

    # 2. Checkpoint wallet.dat
    checkpoint_wallet(from_phase, datadir)

    # 3. Start the new version (no -reindex, no -rescan).
    # -reindex is harmful (re-validates all blocks, causes rollbacks).
    # -rescan is harmful (crashes on Sprout commitment tree assertion when
    # crossing major version boundaries, e.g. v1.0.15 -> v1.1.1).
    # The wallet DOES contain all notes from previous versions, but
    # intermediate versions can't always report their balances accurately
    # via z_gettotalbalance/z_listunspent. The manifest compensates by
    # computing shielded balances from transaction records.
    new_process = start_zcashd(
        phase_index=to_phase,
        datadir=datadir,
        reindex=False,
    )

    # 4. Wait for the new version to be ready.
    new_rpc = ZcashRPC(rpcport=RPC_PORT_PRIMARY)
    new_rpc.wait_for_ready(timeout=180)

    # 5. Verify chain height.
    # Without -reindex, the new version reads the existing chainstate.
    # A small rewind is acceptable: if the previous version mined a few
    # blocks past the next NU's activation height, the new version may
    # trim those blocks because they were mined under the wrong consensus
    # rules. A large drop (>50 blocks) indicates a real problem.
    if expected_height is not None:
        actual_height = new_rpc.getblockcount()
        drop = expected_height - actual_height
        if drop > 50:
            raise RuntimeError(
                f"Chain dropped {drop} blocks after version switch "
                f"({expected_height} -> {actual_height}). "
                f"The new zcashd version may have incompatible chainstate."
            )
        elif drop > 0:
            logger.warning(
                "Chain rewound %d blocks after version switch (%d -> %d). "
                "This is normal if blocks past the NU activation boundary "
                "were mined by the previous version.",
                drop, expected_height, actual_height,
            )
        else:
            logger.info("Chain height verified: %d blocks", actual_height)

    logger.info(
        "=== Successfully switched to zcashd v%s (%s) ===",
        to_nu.zcashd_version, to_nu.name,
    )
    return new_process, new_rpc


def verify_version(rpc: ZcashRPC, expected_version: str):
    """
    Verify that the running zcashd is the expected version.

    Logs a warning (not error) if the version doesn't match exactly,
    since version strings may include build metadata.
    """
    try:
        info = rpc.call("getnetworkinfo")
        version_str = info.get("subversion", "unknown")
        if expected_version not in version_str:
            logger.warning(
                "Version mismatch: expected '%s' in subversion, got '%s'",
                expected_version, version_str,
            )
        else:
            logger.info("Verified zcashd version: %s", version_str)
        return version_str
    except Exception as e:
        logger.warning("Couldn't verify zcashd version: %s", e)
        return None
