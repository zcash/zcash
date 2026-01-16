#!/usr/bin/env bash
#
# This script generates man pages (manual pages) for Zcash executables
# using the help2man tool, ensuring version consistency and proper copyright footers.

# Stop immediately if any command fails.
set -euo pipefail

# Set Locale to C for consistent output (as requested by original code)
export LC_ALL=C

# --- CONFIGURATION AND PATHS ---

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

# Define executable paths. Use defaults if environment variables are not set.
ZC_DAEMON="${SRCDIR}/zcashd"
ZC_WALLET_TOOL="${SRCDIR}/zcashd-wallet-tool"
ZC_CLI="${SRCDIR}/zcash-cli"
ZC_TX_UTIL="${SRCDIR}/zcash-tx"

# --- VALIDATION ---

REQUIRED_COMMANDS=(
    "$ZC_DAEMON"
    "$ZC_CLI"
    "$ZC_TX_UTIL"
    "$ZC_WALLET_TOOL"
)

for cmd in "${REQUIRED_COMMANDS[@]}"; do
    if [ ! -x "$cmd" ]; then
        echo "Error: Command not found or not executable: $cmd" >&2
        exit 1
    fi
done

# --- VERSION PARSING ---

# Extract the full version string (e.g., "Zcash Daemon version v5.6.0-1")
FULL_VER_STR="$("$ZC_CLI" --version | head -n1 | awk '{ print $NF }')"

# Separate the base version (e.g., v5.6.0) from the commit suffix (e.g., 1)
# ZECVER: Base version string (e.g., v5.6.0)
# ZECCOMMIT: Commit suffix (e.g., 1)
# Use 'read' with IFS for array splitting based on the last hyphen.
IFS='-' read -r -a ZEC_PARTS <<< "$FULL_VER_STR"
ZEC_COMMIT="${ZEC_PARTS[-1]}"
ZEC_VER="${FULL_VER_STR%-*}" # Strips the last hyphen and everything after it.

echo "Detected Zcash Version: $ZEC_VER"
echo "Detected Commit Suffix: $ZEC_COMMIT"

# --- FOOTER CREATION ---

FOOTER_ALL="footer.h2m"
FOOTER_NO_BTC="footer-no-bitcoin-copyright.h2m"

# Create a full footer file with copyright content based on zcashd --version output.
echo "[COPYRIGHT]" > "$FOOTER_ALL"
"$ZC_DAEMON" --version | sed -n '1!p' >> "$FOOTER_ALL"

# Create a restricted footer for zcashd-wallet-tool (which lacks Bitcoin Core Developers copyright).
grep -v "Bitcoin Core Developers" "$FOOTER_ALL" > "$FOOTER_NO_BTC"


# --- MAN PAGE GENERATION (GROUP 1: Full Copyright) ---

# Includes zcashd, zcash-cli, zcash-tx
for cmd in "$ZC_DAEMON" "$ZC_CLI" "$ZC_TX_UTIL"; do
    cmdname="${cmd##*/}"
    output_file="${MANDIR}/${cmdname}.1"
    
    # Generate man page using help2man.
    help2man -N --version-string="$ZEC_VER" --include="$FOOTER_ALL" -o "$output_file" "$cmd"
    
    # Remove the commit suffix from the version string in the man page content.
    # The sed command uses a backslash to match the literal hyphen in the output.
    sed -i "s/\\\-${ZEC_COMMIT}//g" "$output_file"
    echo "Generated $output_file"
done

# --- MAN PAGE GENERATION (GROUP 2: Restricted Copyright) ---

# Includes zcashd-wallet-tool
for cmd in "$ZC_WALLET_TOOL"; do
    cmdname="${cmd##*/}"
    output_file="${MANDIR}/${cmdname}.1"
    
    # Generate man page using the restricted footer.
    help2man -N --version-string="$ZEC_VER" --include="$FOOTER_NO_BTC" -o "$output_file" "$cmd"
    
    # Remove the commit suffix from the version string.
    sed -i "s/\\\-${ZEC_COMMIT}//g" "$output_file"
    echo "Generated $output_file"
done

# --- CLEANUP ---
rm -f "$FOOTER_ALL"
rm -f "$FOOTER_NO_BTC"

echo "Man page generation complete."
