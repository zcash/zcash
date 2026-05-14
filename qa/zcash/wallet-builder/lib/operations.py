"""
Pool-aware transaction operations for building wallet state.

This module implements the per-phase transaction operations:
  1. Shield coinbase UTXOs to each available shielded pool
  2. Cross-pool transfers (every active pool -> every other active pool)
  3. External sends (to addresses the wallet doesn't control)

All operations are pool-aware: they check which pools are available at the
current NU and only perform operations that are valid. For example, after
Canopy (phase 5), Sprout deposits are prohibited (ZIP 211), so we skip
shielding to Sprout.

CROSS-POOL TRANSFER RULES:
  Direct transfers between different shielded pools are PROHIBITED by zcashd.
  For example, you cannot z_sendmany from a Sprout address directly to a
  Sapling address. The workaround is to use a transparent address as an
  intermediary:
    1. Sprout -> transparent   (z_sendmany)
    2. transparent -> Sapling  (z_sendmany)
  This applies to all cross-pool transfers (Sprout<->Sapling, Sapling<->Orchard, etc.).
"""

import logging
from dataclasses import dataclass

from lib.constants import (
    Pool,
    NetworkUpgrade,
    NETWORK_UPGRADES,
    COINBASE_MATURITY,
    SHIELD_BATCH_LIMIT,
    SHIELD_COUNT_PER_POOL,
    get_active_pools,
    get_withdrawable_pools,
)
from lib.rpc_client import ZcashRPC, RPCError, OperationFailed
from lib.addresses import GeneratedAddresses


def _version_supports_privacy_policy(phase_index: int) -> bool:
    """Check if the zcashd version for this phase supports the privacyPolicy parameter.

    The privacyPolicy parameter was added in zcashd v5.0.0. Passing it to
    older versions causes an RPC error (they dump the help text).
    """
    major = NETWORK_UPGRADES[phase_index].zcashd_version.split(".")[0]
    return int(major) >= 5


def _default_privacy_policy(phase_index: int) -> str | None:
    """Return the most permissive privacy policy for this phase's version.

    v5+/v6+ require explicit privacy policies for operations that cross
    pool boundaries or reveal amounts. We use the weakest policy that
    allows all our operations to succeed.

    Returns None for versions that don't support the parameter.
    """
    if not _version_supports_privacy_policy(phase_index):
        return None
    # NoPrivacy allows everything: revealed senders, recipients, amounts,
    # linking accounts, and fully transparent transactions.
    return "NoPrivacy"

logger = logging.getLogger(__name__)


@dataclass
class TransactionRecord:
    """Record of a transaction created during a build phase."""
    txid: str
    mined_height: int | None  # None until mined
    tx_type: str              # e.g., "shieldcoinbase", "z_sendmany", "sendtoaddress"
    from_pool: str            # Pool name the funds came from
    to_pool: str              # Pool name the funds went to
    from_address: str
    to_address: str
    amount_zec: str           # String to avoid float precision issues
    fee_zec: str
    description: str

    def to_dict(self) -> dict:
        return {
            "txid": self.txid,
            "mined_height": self.mined_height,
            "type": self.tx_type,
            "from_pool": self.from_pool,
            "to_pool": self.to_pool,
            "from_address": self.from_address,
            "to_address": self.to_address,
            "amount_zec": self.amount_zec,
            "fee_zec": self.fee_zec,
            "description": self.description,
        }


def mine_and_mature(rpc: ZcashRPC, num_blocks: int = 110) -> int:
    """
    Mine blocks to create coinbase UTXOs and wait for maturity.

    Coinbase UTXOs require 100 confirmations before they can be spent
    (the "coinbase maturity" rule). So to get N spendable coinbase UTXOs,
    we need to mine N + 100 blocks total.

    This function mines `num_blocks` blocks. The first `num_blocks - 100`
    coinbase UTXOs will be mature after mining completes.

    Args:
        rpc:        RPC client
        num_blocks: Total blocks to mine (should be >= COINBASE_MATURITY + desired UTXOs)

    Returns:
        New chain height after mining.
    """
    height_before = rpc.getblockcount()
    rpc.generate(num_blocks)
    height_after = rpc.getblockcount()
    mature_count = max(0, num_blocks - COINBASE_MATURITY)
    logger.info(
        "Mined %d blocks (%d -> %d). ~%d coinbase UTXOs now mature.",
        num_blocks, height_before, height_after, mature_count,
    )
    return height_after


def shield_coinbase(rpc: ZcashRPC, to_address: str, to_pool: str,
                    phase_index: int,
                    num_utxos: int = SHIELD_COUNT_PER_POOL,
                    fee: float = 0.0001) -> list[TransactionRecord]:
    """
    Shield transparent coinbase UTXOs to a shielded address.

    z_shieldcoinbase shields up to `limit` UTXOs per call. If we want
    more than SHIELD_BATCH_LIMIT, we make multiple calls.

    Args:
        rpc:          RPC client
        to_address:   Destination shielded address
        to_pool:      Pool name for the record (e.g., "sprout", "sapling")
        phase_index:  Current build phase (for version-aware parameter passing)
        num_utxos:    Target number of UTXOs to shield
        fee:          Fee per shielding transaction

    Returns:
        List of TransactionRecords for the shielding operations.
    """
    records = []
    remaining = num_utxos
    policy = _default_privacy_policy(phase_index)
    # Use smaller batches for v5+ to stay within ZIP 317 fee limits.
    # ZIP 317 conventional fee is 5000 zatoshis per action. With batch=5
    # that's 25000 zatoshis (0.00025 ZEC). Use 0.001 to have headroom.
    max_batch = 5 if policy is not None else SHIELD_BATCH_LIMIT
    if policy is not None:
        fee = 0.0004  # Override fee for v5+ to cover ZIP 317 action costs
        # (conventional fee is ~0.0001/action, zcashd rejects > 4x conventional)

    while remaining > 0:
        batch_size = min(remaining, max_batch)

        try:
            # z_shieldcoinbase params vary by version:
            #   v1.x-v4.x: fromaddr, toaddr [, fee, limit]
            #   v5.0.0:    fromaddr, toaddr [, fee, limit]  (no memo/policy)
            #   v6.0+:     fromaddr, toaddr [, fee, limit, memo, privacyPolicy]
            # v5.0.0 doesn't support memo/policy params. v6+ does.
            major = int(NETWORK_UPGRADES[phase_index].zcashd_version.split(".")[0])
            if major >= 6 and policy is not None:
                result = rpc.call("z_shieldcoinbase", "*", to_address,
                                  fee, batch_size, "", policy)
            else:
                result = rpc.call("z_shieldcoinbase", "*", to_address,
                                  fee, batch_size)
            opid = result.get("opid")
            logger.info("z_shieldcoinbase: %d UTXOs, opid=%s",
                        result.get("shieldingUTXOs", 0), opid)
        except RPCError as e:
            # Common failure: no mature coinbase UTXOs available
            if "Could not find any coinbase funds" in str(e):
                logger.warning(
                    "No more coinbase UTXOs to shield (wanted %d more)", remaining
                )
                break
            raise

        opid = result.get("opid")
        shielded_count = result.get("shieldingUTXOs", 0)
        shielded_value = result.get("shieldingValue", "0")

        if shielded_count == 0:
            logger.info("No UTXOs shielded in this batch, stopping")
            break

        # Wait for the async operation to complete.
        # Retry once on CommitTransaction failures (sporadic race condition).
        try:
            op_result = rpc.wait_for_operation(opid)
        except OperationFailed as e:
            if "CommitTransaction" in str(e) or "bad-txns" in str(e):
                logger.warning("Retrying shield after CommitTransaction failure")
                rpc.generate(2)
                major = int(NETWORK_UPGRADES[phase_index].zcashd_version.split(".")[0])
                if major >= 6 and policy is not None:
                    result = rpc.call("z_shieldcoinbase", "*", to_address,
                                      fee, batch_size, "", policy)
                else:
                    result = rpc.call("z_shieldcoinbase", "*", to_address,
                                      fee, batch_size)
                opid = result.get("opid")
                op_result = rpc.wait_for_operation(opid)
            else:
                raise
        txid = op_result.get("txid", "unknown")

        # Mine a block to confirm the shielding transaction
        rpc.generate(1)
        height = rpc.getblockcount()

        records.append(TransactionRecord(
            txid=txid,
            mined_height=height,
            tx_type="z_shieldcoinbase",
            from_pool="transparent",
            to_pool=to_pool,
            from_address="*",
            to_address=to_address,
            amount_zec=str(shielded_value),
            fee_zec=str(fee),
            description=f"Shield {shielded_count} coinbase UTXOs to {to_pool}",
        ))

        remaining -= shielded_count
        logger.info(
            "Shielded %d coinbase UTXOs to %s (txid=%s, height=%d)",
            shielded_count, to_pool, txid[:16] + "...", height,
        )

    return records


def send_between_pools(rpc: ZcashRPC, from_address: str, to_address: str,
                       from_pool: str, to_pool: str, amount: float,
                       phase_index: int,
                       fee: float = 0.0001) -> TransactionRecord:
    """
    Send funds from one address to another, potentially across pools.

    This uses z_sendmany for shielded sends and sendtoaddress for
    transparent-to-transparent. For cross-pool transfers between shielded
    pools, the CALLER is responsible for routing through transparent
    (see execute_cross_pool_transfers).

    The parameter list passed to z_sendmany is version-aware:
      v1.x-v4.x: z_sendmany fromaddr recipients  (just 2 params)
      v5.0+:     z_sendmany fromaddr recipients minconf fee privacyPolicy

    Args:
        rpc:          RPC client
        from_address: Source address
        to_address:   Destination address
        from_pool:    Source pool name
        to_pool:      Destination pool name
        amount:       Amount in ZEC
        phase_index:  Current build phase (for version-aware parameter passing)
        fee:          Fee in ZEC

    Returns:
        TransactionRecord for the completed send.
    """
    # Round to 8 decimal places (zatoshi precision) to avoid "Invalid amount"
    # errors from floating-point artifacts like 0.008000000000000001.
    amount = round(amount, 8)
    description = f"{from_pool} -> {to_pool}: {amount} ZEC"
    actual_fee = fee  # May be overridden by the v5+ path below.

    if from_pool == "transparent" and to_pool == "transparent":
        # Simple transparent-to-transparent send.
        # Retry once on rejection (stale UTXO state after version switch).
        try:
            txid = rpc.sendtoaddress(to_address, amount)
        except RPCError as e:
            if "rejected" in str(e) or "bad-txns" in str(e):
                logger.warning("Retrying transparent send after rejection")
                rpc.generate(2)
                txid = rpc.sendtoaddress(to_address, amount)
            else:
                raise
    else:
        # Use z_sendmany for any send involving a shielded pool
        recipients = [{"address": to_address, "amount": amount}]
        policy = _default_privacy_policy(phase_index)

        if policy is not None:
            # v5+: pass all parameters including privacy policy.
            # ZIP 317 conventional fee: ~0.0001 ZEC per action for simple txs.
            # zcashd rejects fees > 4x conventional. Use 0.0004 (just under 4x
            # for single-action sends, sufficient for multi-action sends).
            v5_fee = 0.0004
            actual_fee = v5_fee
            opid = rpc.z_sendmany(from_address, recipients, 1, v5_fee, policy)
        else:
            # v1.x-v4.x: just fromaddr and recipients, no extra params.
            # Old versions don't accept minconf/fee/privacyPolicy as
            # positional args and will dump the help text if you try.
            opid = rpc.z_sendmany(from_address, recipients)

        try:
            op_result = rpc.wait_for_operation(opid)
        except OperationFailed as e:
            if "CommitTransaction" in str(e) or "bad-txns" in str(e):
                # Sporadic race condition. Mine a block to clear mempool
                # state and retry once.
                logger.warning("Retrying after CommitTransaction failure: %s", e)
                rpc.generate(2)
                opid = rpc.z_sendmany(from_address, recipients) if policy is None \
                    else rpc.z_sendmany(from_address, recipients, 1,
                                        v5_fee if policy else fee, policy)
                op_result = rpc.wait_for_operation(opid)
            else:
                raise
        txid = op_result.get("txid", "unknown")

    # Mine a block to confirm
    rpc.generate(1)
    height = rpc.getblockcount()

    record = TransactionRecord(
        txid=txid,
        mined_height=height,
        tx_type="z_sendmany" if from_pool != "transparent" or to_pool != "transparent" else "sendtoaddress",
        from_pool=from_pool,
        to_pool=to_pool,
        from_address=from_address,
        to_address=to_address,
        amount_zec=str(amount),
        fee_zec=str(actual_fee),
        description=description,
    )

    logger.info(
        "%s (txid=%s, height=%d)",
        description, txid[:16] + "...", height,
    )
    return record


def execute_phase_operations(rpc: ZcashRPC, phase_index: int,
                             addrs: GeneratedAddresses,
                             external_addrs: GeneratedAddresses,
                             send_amount: float = 1.0) -> list[TransactionRecord]:
    """
    Execute all transaction operations for a build phase.

    This is the main operation sequence called by phase_runner.py.
    It performs:
      1. Shield coinbase to each active shielded pool
      2. Same-pool sends (e.g., Sapling -> Sapling)
      3. Cross-pool sends (routed through transparent)
      4. External sends (to addresses the wallet doesn't control)

    The amount for sends is adjusted downward in later phases where
    balances are smaller due to the regtest halving schedule.

    Args:
        rpc:            RPC client
        phase_index:    Current build phase
        addrs:          Addresses generated for this phase (from addresses.py)
        external_addrs: External throwaway addresses
        send_amount:    Base amount for sends (adjusted per phase)

    Returns:
        List of all TransactionRecords created during this phase.
    """
    nu = NETWORK_UPGRADES[phase_index]
    records = []

    logger.info("=== Executing operations for phase %d (%s) ===", phase_index, nu.name)

    # ---------------------------------------------------------------
    # 1. Shield coinbase to each active shielded pool
    # ---------------------------------------------------------------
    active_pools = get_active_pools(nu)
    shielded_active = [p for p in active_pools if p != Pool.TRANSPARENT]

    for pool in shielded_active:
        pool_addrs = addrs.get_by_pool(pool)
        if not pool_addrs:
            logger.warning("No %s addresses available, skipping shielding", pool.value)
            continue

        target_addr = pool_addrs[0]
        logger.info("Shielding coinbase to %s: %s", pool.value, target_addr[:32] + "...")

        try:
            shield_records = shield_coinbase(
                rpc, target_addr, pool.value,
                phase_index=phase_index,
                num_utxos=SHIELD_COUNT_PER_POOL,
            )
            records.extend(shield_records)
        except Exception as e:
            logger.error("Failed to shield coinbase to %s: %s", pool.value, e)

    # Mine extra blocks after shielding to ensure notes are confirmed
    # and the wallet has scanned them. Without this, subsequent sends
    # from shielded addresses may fail with "unknown outpoint" or
    # "duplicate nullifier" (which can CRASH zcashd v6.x).
    # v6.x Orchard wallet needs significantly more confirmations to
    # fully index shielded notes before they can be spent.
    if shielded_active:
        mine_gap = 10 if phase_index >= 6 else 3
        rpc.generate(mine_gap)

    # ---------------------------------------------------------------
    # 2. Fund the transparent address for cross-pool sends.
    # ---------------------------------------------------------------
    # Mining goes to the wallet's default address, not our generated
    # t-addrs. Send some funds to addrs.transparent[0] so cross-pool
    # transparent->shielded sends have UTXOs to spend.
    if addrs.transparent:
        try:
            rpc.sendtoaddress(addrs.transparent[0], send_amount * 10)
            rpc.generate(2)
        except Exception as e:
            logger.warning("Failed to fund t-addr for cross-pool sends: %s", e)

    # ---------------------------------------------------------------
    # 3. Same-pool sends (within each pool)
    # ---------------------------------------------------------------
    for pool in active_pools:
        pool_addrs = addrs.get_by_pool(pool)
        if len(pool_addrs) < 2:
            continue

        from_addr = pool_addrs[0]
        to_addr = pool_addrs[1]
        pool_name = pool.value

        logger.info("Same-pool send: %s -> %s", pool_name, pool_name)
        try:
            amount = send_amount if pool != Pool.TRANSPARENT else send_amount * 2
            record = send_between_pools(
                rpc, from_addr, to_addr, pool_name, pool_name, amount,
                phase_index=phase_index,
            )
            records.append(record)
        except Exception as e:
            logger.error("Same-pool send failed (%s): %s", pool_name, e)

    # Mine between operation groups to prevent double-spend / note-not-found issues
    mine_gap = 5 if phase_index >= 6 else 2
    rpc.generate(mine_gap)

    # ---------------------------------------------------------------
    # 4. Cross-pool sends (routed through transparent)
    # ---------------------------------------------------------------
    records.extend(
        _execute_cross_pool_transfers(rpc, nu, addrs, send_amount, phase_index)
    )

    rpc.generate(mine_gap)

    # ---------------------------------------------------------------
    # 5. External sends (to throwaway addresses)
    # ---------------------------------------------------------------
    records.extend(
        _execute_external_sends(rpc, nu, addrs, external_addrs, send_amount, phase_index)
    )

    logger.info(
        "=== Phase %d operations complete: %d transactions ===",
        phase_index, len(records),
    )
    return records


def _execute_cross_pool_transfers(rpc: ZcashRPC, nu: NetworkUpgrade,
                                  addrs: GeneratedAddresses,
                                  send_amount: float,
                                  phase_index: int) -> list[TransactionRecord]:
    """
    Execute cross-pool transfers for all valid pool pairs.

    Cross-pool transfers between shielded pools must be routed through
    transparent: Source_pool -> transparent -> Dest_pool. This generates
    TWO transactions per cross-pool transfer.
    """
    records = []
    withdrawable = get_withdrawable_pools(nu)
    active = get_active_pools(nu)
    # Use 80% of send_amount for cross-pool sends to leave room for fees.
    # Floor at 0.01 to avoid "Invalid amount" errors from dust amounts.
    xpool_amount = max(send_amount * 0.8, 0.01)

    for from_pool in withdrawable:
        for to_pool in active:
            if from_pool == to_pool:
                continue
            if from_pool == Pool.TRANSPARENT and to_pool == Pool.TRANSPARENT:
                continue
            if from_pool == Pool.TRANSPARENT and to_pool == Pool.SPROUT:
                continue
            # zcashd crashes when spending Orchard notes in cross-pool
            # transfers: "OrchardWallet::GetSpendInfo with unknown outpoint"
            # in v5.0.0, full node crash in v6.0.0+. Skip Orchard as a
            # cross-pool source until this is fixed upstream.
            if from_pool == Pool.ORCHARD:
                continue

            from_addrs = addrs.get_by_pool(from_pool)
            to_addrs = addrs.get_by_pool(to_pool)
            t_addrs = addrs.transparent

            if not from_addrs or not to_addrs or not t_addrs:
                continue

            from_addr = from_addrs[0]
            to_addr = to_addrs[0]
            t_addr = t_addrs[0]

            from_name = from_pool.value
            to_name = to_pool.value

            try:
                if from_pool == Pool.TRANSPARENT:
                    logger.info("Cross-pool: transparent -> %s", to_name)
                    record = send_between_pools(
                        rpc, from_addr, to_addr, "transparent", to_name,
                        xpool_amount, phase_index=phase_index,
                    )
                    records.append(record)

                elif to_pool == Pool.TRANSPARENT:
                    logger.info("Cross-pool: %s -> transparent", from_name)
                    record = send_between_pools(
                        rpc, from_addr, to_addr, from_name, "transparent",
                        xpool_amount, phase_index=phase_index,
                    )
                    records.append(record)

                else:
                    # shielded -> shielded (different pools): route via transparent
                    logger.info(
                        "Cross-pool: %s -> transparent -> %s",
                        from_name, to_name,
                    )
                    record1 = send_between_pools(
                        rpc, from_addr, t_addr, from_name, "transparent",
                        xpool_amount, phase_index=phase_index,
                    )
                    records.append(record1)

                    record2 = send_between_pools(
                        rpc, t_addr, to_addr, "transparent", to_name,
                        xpool_amount * 0.8,
                        phase_index=phase_index,
                    )
                    records.append(record2)

            except Exception as e:
                logger.error(
                    "Cross-pool transfer %s -> %s failed: %s",
                    from_name, to_name, e,
                )

            # Mine between each cross-pool pair in v6+ to prevent
            # double-spend crashes from rapid note spending.
            if phase_index >= 6:
                try:
                    rpc.generate(3)
                except Exception:
                    pass  # zcashd may have crashed; let the outer handler deal with it

    return records


def _execute_external_sends(rpc: ZcashRPC, nu: NetworkUpgrade,
                            addrs: GeneratedAddresses,
                            external_addrs: GeneratedAddresses,
                            send_amount: float,
                            phase_index: int) -> list[TransactionRecord]:
    """
    Send funds to external (throwaway) addresses.

    These transactions test that the wallet correctly records outgoing
    transactions to addresses it doesn't control. Each active pool type
    sends to the corresponding external address.
    """
    records = []
    active = get_active_pools(nu)

    for pool in active:
        from_addrs = addrs.get_by_pool(pool)
        ext_addrs = external_addrs.get_by_pool(pool)

        if not from_addrs or not ext_addrs:
            continue

        pool_name = pool.value
        from_addr = from_addrs[0]
        ext_addr = ext_addrs[0]

        logger.info("External send: %s -> external_%s", pool_name, pool_name)
        try:
            record = send_between_pools(
                rpc, from_addr, ext_addr,
                pool_name, pool_name,
                send_amount * 0.5,
                phase_index=phase_index,
            )
            record.to_pool = f"external_{pool_name}"
            record.description = f"External send: {pool_name} to unknown address"
            records.append(record)
        except Exception as e:
            logger.error("External send from %s failed: %s", pool_name, e)

    return records


def mine_to_height(rpc: ZcashRPC, target_height: int):
    """
    Mine blocks until the chain reaches the target height.

    Used to advance the chain to the next NU activation height.

    Args:
        rpc:           RPC client
        target_height: Desired chain height
    """
    current = rpc.getblockcount()
    if current >= target_height:
        logger.info(
            "Already at height %d (target %d), no mining needed",
            current, target_height,
        )
        return

    blocks_needed = target_height - current
    logger.info(
        "Mining %d blocks to reach height %d (currently %d)...",
        blocks_needed, target_height, current,
    )
    rpc.generate(blocks_needed)
    logger.info("Reached target height %d", rpc.getblockcount())
