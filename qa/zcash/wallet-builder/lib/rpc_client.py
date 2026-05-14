"""
JSON-RPC client for communicating with zcashd.

This module provides a ZcashRPC class that wraps zcashd's JSON-RPC interface.
It handles:
  - Connection retry logic (zcashd may take time to start)
  - Async operation polling (z_shieldcoinbase, z_sendmany, z_mergetoaddress
    all return operation IDs that must be polled for completion)
  - Structured error reporting

The client is version-agnostic -- callers are responsible for knowing which
RPC methods are available in their zcashd version. See addresses.py and
operations.py for version-aware wrappers.

Usage:
    rpc = ZcashRPC(rpcport=18443, rpcuser="test", rpcpassword="test")
    rpc.wait_for_ready(timeout=120)
    info = rpc.call("getblockchaininfo")
    height = rpc.getblockcount()
"""

import json
import logging
import time
from urllib.request import Request, urlopen
from urllib.error import URLError

logger = logging.getLogger(__name__)

# zcashd async operations report status via z_getoperationstatus / z_getoperationresult.
# These are the possible statuses returned:
OPERATION_STATUS_SUCCESS = "success"
OPERATION_STATUS_FAILED = "failed"
OPERATION_STATUS_EXECUTING = "executing"
OPERATION_STATUS_QUEUED = "queued"


class RPCError(Exception):
    """Raised when zcashd returns a JSON-RPC error response."""

    def __init__(self, code, message):
        self.code = code
        self.message = message
        super().__init__(f"RPC error {code}: {message}")


class OperationFailed(Exception):
    """Raised when an async operation (z_sendmany, etc.) fails."""

    def __init__(self, opid, error):
        self.opid = opid
        self.error = error
        super().__init__(f"Operation {opid} failed: {error}")


class OperationTimeout(Exception):
    """Raised when an async operation exceeds the poll timeout."""

    def __init__(self, opid, elapsed):
        self.opid = opid
        self.elapsed = elapsed
        super().__init__(f"Operation {opid} timed out after {elapsed:.0f}s")


class ZcashRPC:
    """
    JSON-RPC client for zcashd.

    All RPC methods can be called either via the generic `call()` method
    or via convenience wrappers (e.g., `rpc.generate(10)`).

    Args:
        rpcport:    Port zcashd is listening on (default 18443 for regtest)
        rpcuser:    RPC username (must match zcash.conf)
        rpcpassword: RPC password (must match zcash.conf)
        rpchost:    Host zcashd is listening on (default 127.0.0.1)
    """

    def __init__(self, rpcport=18443, rpcuser="test", rpcpassword="test",
                 rpchost="127.0.0.1"):
        self.url = f"http://{rpchost}:{rpcport}"
        self.rpcuser = rpcuser
        self.rpcpassword = rpcpassword
        self._request_id = 0

    def call(self, method, *params, timeout=300):
        """
        Make a JSON-RPC call to zcashd.

        Args:
            method:  RPC method name (e.g., "getblockchaininfo")
            *params: Positional parameters for the RPC method
            timeout: HTTP request timeout in seconds

        Returns:
            The "result" field from the JSON-RPC response.

        Raises:
            RPCError: If zcashd returns an error response.
            ConnectionError: If zcashd is not reachable.
        """
        self._request_id += 1
        payload = json.dumps({
            "jsonrpc": "1.0",
            "id": self._request_id,
            "method": method,
            "params": list(params),
        }).encode("utf-8")

        # Build HTTP request with basic auth.
        # zcashd uses HTTP Basic Authentication for its JSON-RPC interface.
        import base64
        credentials = base64.b64encode(
            f"{self.rpcuser}:{self.rpcpassword}".encode()
        ).decode()

        request = Request(
            self.url,
            data=payload,
            headers={
                "Content-Type": "application/json",
                "Authorization": f"Basic {credentials}",
            },
        )

        try:
            with urlopen(request, timeout=timeout) as response:
                result = json.loads(response.read().decode("utf-8"))
        except URLError as e:
            # zcashd returns HTTP 500 for RPC errors. The JSON-RPC error
            # details are in the response body, which urllib puts in the
            # HTTPError exception. Parse it instead of treating it as a
            # connection failure.
            from urllib.error import HTTPError
            if isinstance(e, HTTPError) and e.code == 500:
                try:
                    body = e.read().decode("utf-8")
                    result = json.loads(body)
                    err = result.get("error", {})
                    raise RPCError(
                        err.get("code", -1),
                        err.get("message", str(err)),
                    ) from e
                except (json.JSONDecodeError, UnicodeDecodeError):
                    pass  # Fall through to generic ConnectionError
            raise ConnectionError(
                f"Cannot connect to zcashd at {self.url}: {e}"
            ) from e

        if result.get("error") is not None:
            err = result["error"]
            raise RPCError(err.get("code", -1), err.get("message", str(err)))

        return result.get("result")

    # ------------------------------------------------------------------
    # Connection management
    # ------------------------------------------------------------------

    def wait_for_ready(self, timeout=300, poll_interval=2):
        """
        Block until zcashd is accepting RPC connections.

        This is used after starting zcashd (especially with -reindex) to
        wait until it's ready to accept commands. During reindexing, zcashd
        may take minutes to become responsive.

        Args:
            timeout:       Maximum seconds to wait
            poll_interval: Seconds between connection attempts

        Raises:
            TimeoutError: If zcashd doesn't become ready within timeout.
        """
        start = time.time()
        last_error = None

        while time.time() - start < timeout:
            try:
                info = self.call("getblockchaininfo")
                # zcashd is ready. Log the chain state.
                logger.info(
                    "zcashd ready: chain=%s, blocks=%d",
                    info.get("chain", "unknown"),
                    info.get("blocks", -1),
                )
                return info
            except (ConnectionError, RPCError, OSError) as e:
                last_error = e
                time.sleep(poll_interval)

        raise TimeoutError(
            f"zcashd not ready after {timeout}s. Last error: {last_error}"
        )

    def is_ready(self):
        """Check if zcashd is accepting RPC connections (non-blocking)."""
        try:
            self.call("getblockchaininfo", timeout=5)
            return True
        except Exception:
            return False

    # ------------------------------------------------------------------
    # Async operation management
    # ------------------------------------------------------------------

    def wait_for_operation(self, opid, timeout=600, poll_interval=1):
        """
        Poll an async operation until it completes or fails.

        Many zcashd operations (z_sendmany, z_shieldcoinbase, z_mergetoaddress)
        are asynchronous: they return an operation ID immediately, and the actual
        work happens in the background. This method polls z_getoperationstatus
        until the operation reaches a terminal state.

        Args:
            opid:          Operation ID string (e.g., "opid-1234-5678...")
            timeout:       Maximum seconds to wait for completion
            poll_interval: Seconds between status checks

        Returns:
            The result object from the completed operation (contains txid, etc.)

        Raises:
            OperationFailed: If the operation fails.
            OperationTimeout: If the operation doesn't complete within timeout.
        """
        start = time.time()

        while time.time() - start < timeout:
            statuses = self.call("z_getoperationstatus", [opid])

            if not statuses:
                # Operation ID not found -- may have already been collected
                # by z_getoperationresult. Try that.
                results = self.call("z_getoperationresult", [opid])
                if results:
                    status = results[0]
                else:
                    # Truly unknown operation ID
                    raise OperationFailed(
                        opid, "Operation not found (unknown opid)"
                    )
            else:
                status = statuses[0]

            state = status.get("status")

            if state == OPERATION_STATUS_SUCCESS:
                # Collect the result (removes it from zcashd's internal list)
                self.call("z_getoperationresult", [opid])
                txid = status.get("result", {}).get("txid")
                logger.info("Operation %s succeeded: txid=%s", opid, txid)
                return status.get("result", {})

            if state == OPERATION_STATUS_FAILED:
                error = status.get("error", {}).get("message", str(status))
                raise OperationFailed(opid, error)

            # Still executing or queued -- keep polling
            elapsed = time.time() - start
            logger.debug(
                "Operation %s: status=%s, elapsed=%.0fs", opid, state, elapsed
            )
            time.sleep(poll_interval)

        raise OperationTimeout(opid, time.time() - start)

    # ------------------------------------------------------------------
    # Convenience wrappers for commonly used RPC methods.
    # These exist to make phase scripts more readable.
    # ------------------------------------------------------------------

    def generate(self, nblocks):
        """
        Mine blocks in regtest mode.

        Each mined block creates one coinbase UTXO sent to the wallet's
        default address. Coinbase UTXOs require 100 confirmations before
        they can be spent (the "coinbase maturity" rule).

        Args:
            nblocks: Number of blocks to mine

        Returns:
            List of block hashes for the mined blocks.
        """
        logger.info("Mining %d blocks...", nblocks)
        hashes = self.call("generate", nblocks)
        new_height = self.getblockcount()
        logger.info("Mined %d blocks, chain height now %d", nblocks, new_height)
        return hashes

    def getblockcount(self):
        """Return the current block height."""
        return self.call("getblockcount")

    def getblockchaininfo(self):
        """Return blockchain state info (chain, blocks, upgrades, etc.)."""
        return self.call("getblockchaininfo")

    def getblock(self, blockhash, verbosity=1):
        """Return block data for the given hash."""
        return self.call("getblock", blockhash, verbosity)

    def getblockhash(self, height):
        """Return the block hash at a given height."""
        return self.call("getblockhash", height)

    def getwalletinfo(self):
        """Return wallet state info."""
        return self.call("getwalletinfo")

    # -- Address generation --

    def getnewaddress(self):
        """
        Generate a new transparent (t-addr) address.

        Deprecated in zcashd v5.0.0+; requires -allowdeprecated=getnewaddress.
        """
        return self.call("getnewaddress")

    def z_getnewaddress(self, addr_type=None):
        """
        Generate a new shielded address.

        Args:
            addr_type: "sprout" or "sapling" (default varies by version).
                       In v1.x, default is sprout. In v2.0+, default is sapling.

        Deprecated in zcashd v5.0.0+; requires -allowdeprecated=z_getnewaddress.
        For v5.0+, prefer z_getnewaccount + z_getaddressforaccount.
        """
        if addr_type:
            return self.call("z_getnewaddress", addr_type)
        return self.call("z_getnewaddress")

    def z_getnewaccount(self):
        """
        Create a new HD account (zcashd v5.0+).

        Returns a dict with "account" (int): the new account number.
        Accounts are derived from the wallet's HD seed using ZIP 32.
        """
        return self.call("z_getnewaccount")

    def z_getaddressforaccount(self, account, receiver_types=None,
                               diversifier_index=None):
        """
        Generate an address for an HD account (zcashd v5.0+).

        This creates a Unified Address (UA) that bundles receivers for
        the requested pool types. UAs are the modern way to receive
        funds across multiple pools with a single address string.

        Args:
            account:          Account number from z_getnewaccount
            receiver_types:   List of receiver types, e.g. ["p2pkh", "sapling", "orchard"]
                              If omitted, zcashd picks the best defaults.
            diversifier_index: Optional diversifier index (for address re-derivation)

        Returns:
            Dict with: account, diversifier_index, receiver_types, address (UA string)
        """
        params = [account]
        if receiver_types is not None:
            params.append(receiver_types)
            if diversifier_index is not None:
                params.append(diversifier_index)
        return self.call("z_getaddressforaccount", *params)

    # -- Shielding and sending --

    def z_shieldcoinbase(self, from_address, to_address, fee=None, limit=None):
        """
        Shield transparent coinbase UTXOs to a shielded address.

        This is an async operation. Returns an operation ID that must be
        polled with wait_for_operation().

        Args:
            from_address: Transparent address or "*" for all wallet t-addrs
            to_address:   Shielded destination (Sprout, Sapling, or UA)
            fee:          Fee in ZEC (default: zcashd calculates per ZIP 317)
            limit:        Max UTXOs to shield per batch (default 50)

        Returns:
            Dict with "opid" and shielding summary.

        Note: Coinbase UTXOs must have >= 100 confirmations to be shielded.
        Note: After Canopy (NU4), shielding to Sprout addresses will fail
              because new Sprout deposits are prohibited (ZIP 211).
        """
        params = [from_address, to_address]
        if fee is not None:
            params.append(fee)
            if limit is not None:
                params.append(limit)
        elif limit is not None:
            # fee defaults to -1 (auto) when we want to specify limit
            params.append(-1)
            params.append(limit)
        result = self.call("z_shieldcoinbase", *params)
        opid = result.get("opid")
        logger.info(
            "z_shieldcoinbase: %d UTXOs, opid=%s",
            result.get("shieldingUTXOs", 0), opid,
        )
        return result

    def z_sendmany(self, from_address, recipients, minconf=None, fee=None,
                   privacy_policy=None):
        """
        Send funds from one address to one or more recipients.

        This is the primary method for moving funds between pools.
        It's an async operation -- returns an opid to poll.

        Args:
            from_address:   Source address (t-addr, z-addr, or UA)
            recipients:     List of dicts: [{"address": "...", "amount": N}, ...]
                            Each recipient can optionally include "memo" (hex string)
            minconf:        Minimum confirmations required (default 1 in old versions,
                            10 in newer versions)
            fee:            Fee in ZEC (default: calculated per ZIP 317 in v5+)
            privacy_policy: Privacy policy string (v5+ only), e.g.,
                            "AllowRevealedSenders", "AllowRevealedRecipients",
                            "AllowRevealedAmounts", "AllowFullyTransparent",
                            "AllowLinkingAccountAddresses", "NoPrivacy"

        Returns:
            Operation ID string.

        Note on cross-pool transfers: Direct transfers between different
        shielded pools (e.g., Sprout->Sapling) are prohibited. You must
        use a transparent address as an intermediary:
            Sprout -> transparent -> Sapling
        """
        params = [from_address, recipients]
        if minconf is not None:
            params.append(minconf)
        if fee is not None:
            if minconf is None:
                params.append(1)  # default minconf
            params.append(fee)
        if privacy_policy is not None:
            if fee is None:
                if minconf is None:
                    params.append(1)  # default minconf
                params.append(-1)  # default fee (auto)
            params.append(privacy_policy)
        opid = self.call("z_sendmany", *params)
        logger.info(
            "z_sendmany: from=%s, %d recipients, opid=%s",
            from_address[:16] + "...", len(recipients), opid,
        )
        return opid

    def sendtoaddress(self, address, amount):
        """
        Send ZEC to a transparent address (simple transparent->transparent send).

        Unlike z_sendmany, this is synchronous and returns a txid directly.
        """
        return self.call("sendtoaddress", address, amount)

    # -- Balance and UTXO inspection --

    def z_gettotalbalance(self, minconf=1):
        """
        Get total wallet balance split by transparent/private/total.

        Deprecated in v5.0+; use z_getbalanceforaccount instead.
        Returns dict with "transparent", "private", "total" as string amounts.
        """
        return self.call("z_gettotalbalance", minconf)

    def z_getbalanceforaccount(self, account, minconf=1):
        """
        Get balance for an HD account, broken down by pool (v5.0+).

        Returns dict with "pools" containing per-pool balances in zatoshis.
        """
        return self.call("z_getbalanceforaccount", account, minconf)

    def z_getbalance(self, address, minconf=1):
        """Get balance for a specific address (deprecated in v5.0+)."""
        return self.call("z_getbalance", address, minconf)

    def listunspent(self, minconf=1, maxconf=9999999, addresses=None):
        """
        List unspent transparent UTXOs.

        Returns list of UTXOs with txid, vout, address, amount, confirmations,
        and whether it's a coinbase output ("generated" field).
        """
        params = [minconf, maxconf]
        if addresses is not None:
            params.append(addresses)
        return self.call("listunspent", *params)

    def z_listunspent(self, minconf=1, maxconf=9999999, include_watchonly=False,
                      addresses=None):
        """
        List unspent shielded notes (Sprout, Sapling, Orchard).

        Returns list of notes with txid, pool type, amount, confirmations,
        address, and memo.
        """
        params = [minconf, maxconf, include_watchonly]
        if addresses is not None:
            params.append(addresses)
        return self.call("z_listunspent", *params)

    # -- Transaction inspection --

    def z_viewtransaction(self, txid):
        """
        Decrypt and view details of a shielded transaction.

        Returns transaction details including decrypted spends and outputs
        for any addresses the wallet knows about. This is the primary tool
        for inspecting what happened in a shielded transaction.
        """
        return self.call("z_viewtransaction", txid)

    def gettransaction(self, txid):
        """Get transaction details (transparent-focused view)."""
        return self.call("gettransaction", txid)

    def z_listreceivedbyaddress(self, address, minconf=1):
        """List amounts received by a specific shielded address."""
        return self.call("z_listreceivedbyaddress", address, minconf)

    # -- Key export (for read-only wallet testing) --

    def z_exportviewingkey(self, address):
        """
        Export the viewing key for a shielded address.

        Viewing keys allow read-only access: the holder can see incoming
        transactions to this address but cannot spend funds.

        Supported for Sprout and Sapling addresses. For Orchard, use
        unified viewing keys via z_exportviewingkey on the unified address
        (availability depends on zcashd version).
        """
        return self.call("z_exportviewingkey", address)

    def z_exportkey(self, address):
        """Export the spending key for a shielded address."""
        return self.call("z_exportkey", address)

    def z_exportwallet(self, filename):
        """
        Export all wallet keys (transparent + shielded) to a file.

        The file is written to the directory specified by -exportdir in zcash.conf.
        Contains all private keys in a human-readable format.
        """
        return self.call("z_exportwallet", filename)

    def dumpprivkey(self, address):
        """Export private key for a transparent address."""
        return self.call("dumpprivkey", address)

    # -- Wallet address listing --

    def z_listaddresses(self):
        """
        List all shielded addresses in the wallet.

        Deprecated in v5.0+; use z_listunifiedreceivers or
        z_listaccounts + z_getaddressforaccount.
        """
        return self.call("z_listaddresses")

    def getaddressesbyaccount(self, account=""):
        """List all transparent addresses (deprecated in newer versions)."""
        return self.call("getaddressesbyaccount", account)

    # -- Key import --

    def importprivkey(self, privkey, label="", rescan=False):
        """Import a transparent private key (WIF-encoded). Makes the address spendable."""
        return self.call("importprivkey", privkey, label, rescan)

    def importaddress(self, address_or_script, label="", rescan=False, p2sh=False):
        """Import a transparent address or script as watch-only (not spendable).

        The p2sh parameter was added in a later zcashd version. If it fails
        with 4 args, the caller should retry with 3 (without p2sh).
        """
        if p2sh:
            return self.call("importaddress", address_or_script, label, rescan, p2sh)
        return self.call("importaddress", address_or_script, label, rescan)

    def importpubkey(self, pubkey_hex, label="", rescan=False):
        """Import a transparent public key as watch-only (not spendable).

        Not available in very old zcashd versions (v1.0.x returns 404).
        """
        return self.call("importpubkey", pubkey_hex, label, rescan)

    def z_importkey(self, spending_key, rescan="no", start_height=0):
        """Import a shielded spending key (Sprout or Sapling). Makes the address spendable."""
        return self.call("z_importkey", spending_key, rescan, start_height)

    def z_importviewingkey(self, viewing_key, rescan="no", start_height=0):
        """Import a shielded viewing key (Sprout or Sapling). Read-only, not spendable.

        Very old versions (v1.0.x) may not accept the rescan/startHeight params.
        """
        try:
            return self.call("z_importviewingkey", viewing_key, rescan, start_height)
        except RPCError:
            # Fall back to single-arg form for old versions
            return self.call("z_importviewingkey", viewing_key)

    # -- Node control --

    def stop(self):
        """Request zcashd to shut down gracefully."""
        logger.info("Requesting zcashd shutdown...")
        try:
            return self.call("stop", timeout=10)
        except ConnectionError:
            # zcashd may close the connection before responding
            pass
