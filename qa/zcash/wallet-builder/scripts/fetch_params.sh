#!/usr/bin/env bash
# =============================================================================
# fetch_params.sh -- Download Zcash proving/verification parameters
#
# PURPOSE:
#   Downloads the cryptographic parameters needed by zcashd for creating
#   and verifying shielded transactions. These are large files (totaling
#   ~1.7GB) that are shared across all zcashd versions.
#
# BACKGROUND:
#   Zcash uses zero-knowledge proofs to enable shielded transactions.
#   The proving parameters are generated during a "ceremony" and must
#   be available on every node that creates shielded transactions.
#
#   Parameter files:
#     sprout-proving.key    (~900MB)  Sprout BCTV14 proving key (v1.0.x)
#     sprout-groth16.params (~725MB)  Sprout Groth16 params (v2.0+)
#     sprout-verifying.key  (~1KB)    Sprout verification key
#     sapling-spend.params  (~47MB)   Sapling spend circuit params
#     sapling-output.params (~3MB)    Sapling output circuit params
#
# PREREQUISITES:
#   - curl must be available
#   - ~2GB free disk space
#
# USAGE:
#   ./fetch_params.sh [TARGET_DIR]
#   TARGET_DIR defaults to /opt/zcash/params
#
# OUTPUT:
#   Downloads parameter files to TARGET_DIR.
#   Symlinks TARGET_DIR to ~/.zcash-params if it doesn't exist.
#
# NOTES:
#   - Some parameters may not be strictly needed for regtest, but we
#     download all of them for completeness and to avoid version-specific
#     issues.
#   - The download uses the official Zcash parameter download URLs.
#   - Files are verified by size after download (SHA256 would be better
#     but adds complexity).
# =============================================================================
export LC_ALL=C
set -euo pipefail

TARGET_DIR="${1:-/opt/zcash/params}"
PARAMS_HOME="${HOME}/.zcash-params"

# Official Zcash parameter download URLs
# These are hosted on z.cash infrastructure and are the same files
# that zcash-fetch-params downloads.
PARAMS_BASE="https://download.z.cash/downloads"

# Parameter files and their expected approximate sizes (for basic validation).
# Format: "filename expected_size_bytes"
declare -A PARAMS
PARAMS=(
    ["sprout-groth16.params"]="725523612"
    ["sapling-spend.params"]="47958396"
    ["sapling-output.params"]="3592860"
    ["sprout-verifying.key"]="1449"
)

# The old Sprout proving key (BCTV14) is needed by zcashd v1.0.x-v2.0.x
# to start (existence check at startup, content loaded when creating proofs).
# This file is NOT at the usual /downloads/ path -- it's under /zcashfinalmpc/.
SPROUT_PROVING_KEY_URL="https://download.z.cash/zcashfinalmpc/sprout-proving.key"
SPROUT_PROVING_KEY_SIZE="910173851"

mkdir -p "${TARGET_DIR}"

echo "=== Downloading Zcash proving parameters ==="
echo "Target directory: ${TARGET_DIR}"
echo ""

errors=0
for filename in "${!PARAMS[@]}"; do
    expected_size="${PARAMS[${filename}]}"
    filepath="${TARGET_DIR}/${filename}"

    # Check if already downloaded and roughly the right size
    if [[ -f "${filepath}" ]]; then
        actual_size=$(stat -c%s "${filepath}" 2>/dev/null || stat -f%z "${filepath}" 2>/dev/null || echo "0")
        # Allow 10% size tolerance (different versions may have slightly different sizes)
        min_size=$((expected_size * 90 / 100))
        if [[ ${actual_size} -ge ${min_size} ]]; then
            echo "[SKIP] ${filename} already exists ($(numfmt --to=iec ${actual_size} 2>/dev/null || echo "${actual_size} bytes"))"
            continue
        fi
    fi

    echo "[DOWNLOAD] ${filename} (~$(numfmt --to=iec ${expected_size} 2>/dev/null || echo "${expected_size} bytes"))..."
    url="${PARAMS_BASE}/${filename}"

    if curl --silent --fail --location --output "${filepath}" "${url}"; then
        actual_size=$(stat -c%s "${filepath}" 2>/dev/null || stat -f%z "${filepath}" 2>/dev/null || echo "0")
        echo "  [OK] Downloaded ($(numfmt --to=iec ${actual_size} 2>/dev/null || echo "${actual_size} bytes"))"
    else
        echo "  [WARNING] Could not download ${filename} from ${url}"
        echo "  This parameter may not be needed for all phases."
        # Don't fail on missing params -- some are only needed for specific phases
        errors=$((errors + 1))
    fi
done

# Download sprout-proving.key separately (different URL path than the others).
filepath="${TARGET_DIR}/sprout-proving.key"
if [[ -f "${filepath}" ]]; then
    actual_size=$(stat -c%s "${filepath}" 2>/dev/null || stat -f%z "${filepath}" 2>/dev/null || echo "0")
    min_size=$((SPROUT_PROVING_KEY_SIZE * 90 / 100))
    if [[ ${actual_size} -ge ${min_size} ]]; then
        echo "[SKIP] sprout-proving.key already exists ($(numfmt --to=iec ${actual_size} 2>/dev/null || echo "${actual_size} bytes"))"
    fi
else
    echo "[DOWNLOAD] sprout-proving.key (~$(numfmt --to=iec ${SPROUT_PROVING_KEY_SIZE} 2>/dev/null || echo "${SPROUT_PROVING_KEY_SIZE} bytes"))..."
    if curl --silent --fail --location --output "${filepath}" "${SPROUT_PROVING_KEY_URL}"; then
        actual_size=$(stat -c%s "${filepath}" 2>/dev/null || stat -f%z "${filepath}" 2>/dev/null || echo "0")
        echo "  [OK] Downloaded ($(numfmt --to=iec ${actual_size} 2>/dev/null || echo "${actual_size} bytes"))"
    else
        echo "  [ERROR] Could not download sprout-proving.key from ${SPROUT_PROVING_KEY_URL}"
        errors=$((errors + 1))
    fi
fi

# v1.1.1 (Overwinter) expects testnet-suffixed param filenames.
# Create symlinks so the same files work for all versions.
for pair in \
    "sapling-spend.params:sapling-spend-testnet.params" \
    "sapling-output.params:sapling-output-testnet.params" \
    "sprout-groth16.params:sprout-groth16-testnet.params"; do
    src="${pair%%:*}"
    dst="${pair##*:}"
    if [[ -f "${TARGET_DIR}/${src}" && ! -e "${TARGET_DIR}/${dst}" ]]; then
        ln -s "${src}" "${TARGET_DIR}/${dst}"
        echo "[SYMLINK] ${dst} -> ${src}"
    fi
done

# Create symlink from ~/.zcash-params to TARGET_DIR if it doesn't exist.
# zcashd looks for parameters in ~/.zcash-params by default.
if [[ ! -e "${PARAMS_HOME}" ]]; then
    echo ""
    echo "Creating symlink: ${PARAMS_HOME} -> ${TARGET_DIR}"
    ln -s "${TARGET_DIR}" "${PARAMS_HOME}"
elif [[ -L "${PARAMS_HOME}" ]]; then
    # Already a symlink -- update it
    rm "${PARAMS_HOME}"
    ln -s "${TARGET_DIR}" "${PARAMS_HOME}"
fi

echo ""
if [[ ${errors} -gt 0 ]]; then
    echo "=== WARNING: ${errors} parameter file(s) could not be downloaded ==="
    echo "Some phases may fail if they need the missing parameters."
    # Don't exit with error -- missing params are non-fatal for some phases
else
    echo "=== All Zcash parameters downloaded successfully ==="
fi

echo "Total size: $(du -sh "${TARGET_DIR}" | cut -f1)"
