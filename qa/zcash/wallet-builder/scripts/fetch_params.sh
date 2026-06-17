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

# Shared curl options. --retry/--retry-delay matter for the ~700MB-900MB files,
# which are the ones most likely to be cut off by a transient network blip; the
# previous lack of retries is what produced silently incomplete images. --silent
# suppresses the progress bar but --show-error still prints the reason on
# failure, and --fail avoids writing an HTTP error page into the output file.
CURL_OPTS=(--fail --location --silent --show-error --retry 3 --retry-delay 5)

# Return 0 if $1 exists and is at least 90% of the expected size $2. The 10%
# tolerance allows for minor size differences between parameter revisions while
# still catching truncated/partial downloads.
verify_size() {
    local filepath="$1" expected="$2" actual min
    actual=$(stat -c%s "${filepath}" 2>/dev/null || stat -f%z "${filepath}" 2>/dev/null || echo "0")
    min=$((expected * 90 / 100))
    [[ ${actual} -ge ${min} ]]
}

# All parameter files here (PARAMS plus sprout-proving.key, below) are REQUIRED:
# without them zcashd cannot start, so any failure must abort the build rather
# than ship a broken image.
essential_errors=0

for filename in "${!PARAMS[@]}"; do
    expected_size="${PARAMS[${filename}]}"
    filepath="${TARGET_DIR}/${filename}"

    # Skip if already downloaded and roughly the right size.
    if [[ -f "${filepath}" ]] && verify_size "${filepath}" "${expected_size}"; then
        actual_size=$(stat -c%s "${filepath}" 2>/dev/null || stat -f%z "${filepath}" 2>/dev/null || echo "0")
        echo "[SKIP] ${filename} already exists ($(numfmt --to=iec "${actual_size}" 2>/dev/null || echo "${actual_size} bytes"))"
        continue
    fi

    echo "[DOWNLOAD] ${filename} (~$(numfmt --to=iec "${expected_size}" 2>/dev/null || echo "${expected_size} bytes"))..."
    url="${PARAMS_BASE}/${filename}"

    if curl "${CURL_OPTS[@]}" --output "${filepath}" "${url}" && verify_size "${filepath}" "${expected_size}"; then
        actual_size=$(stat -c%s "${filepath}" 2>/dev/null || stat -f%z "${filepath}" 2>/dev/null || echo "0")
        echo "  [OK] Downloaded ($(numfmt --to=iec "${actual_size}" 2>/dev/null || echo "${actual_size} bytes"))"
    else
        echo "  [ERROR] Could not download ${filename} from ${url}"
        echo "          This parameter is REQUIRED; refusing to build a broken image."
        rm -f "${filepath}"  # drop any partial file so a re-run retries cleanly
        essential_errors=$((essential_errors + 1))
    fi
done

# sprout-proving.key (BCTV14) is REQUIRED: phases 0-1 run the pre-Sapling
# binary v1.1.1, which loads it at startup (existence check) and uses it to
# create Sprout JoinSplit proofs. Different URL path too (under /zcashfinalmpc/,
# not /downloads/).
filepath="${TARGET_DIR}/sprout-proving.key"
if [[ -f "${filepath}" ]] && verify_size "${filepath}" "${SPROUT_PROVING_KEY_SIZE}"; then
    actual_size=$(stat -c%s "${filepath}" 2>/dev/null || stat -f%z "${filepath}" 2>/dev/null || echo "0")
    echo "[SKIP] sprout-proving.key already exists ($(numfmt --to=iec "${actual_size}" 2>/dev/null || echo "${actual_size} bytes"))"
else
    echo "[DOWNLOAD] sprout-proving.key (~$(numfmt --to=iec "${SPROUT_PROVING_KEY_SIZE}" 2>/dev/null || echo "${SPROUT_PROVING_KEY_SIZE} bytes"))..."
    if curl "${CURL_OPTS[@]}" --output "${filepath}" "${SPROUT_PROVING_KEY_URL}" && verify_size "${filepath}" "${SPROUT_PROVING_KEY_SIZE}"; then
        actual_size=$(stat -c%s "${filepath}" 2>/dev/null || stat -f%z "${filepath}" 2>/dev/null || echo "0")
        echo "  [OK] Downloaded ($(numfmt --to=iec "${actual_size}" 2>/dev/null || echo "${actual_size} bytes"))"
    else
        rm -f "${filepath}"  # drop any partial file so a re-run retries cleanly
        echo "  [ERROR] Could not download sprout-proving.key, which is REQUIRED by"
        echo "          the pre-Sapling binary used for phases 0-1."
        essential_errors=$((essential_errors + 1))
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
#
# Safety: this script is reachable directly on developer hosts (it is a
# documented standalone tool and is invoked by `make shell`), so we must not
# silently clobber an existing ~/.zcash-params that the developer is using
# for their normal zcashd install.
if [[ ! -e "${PARAMS_HOME}" && ! -L "${PARAMS_HOME}" ]]; then
    echo ""
    echo "Creating symlink: ${PARAMS_HOME} -> ${TARGET_DIR}"
    ln -s "${TARGET_DIR}" "${PARAMS_HOME}"
elif [[ -L "${PARAMS_HOME}" ]]; then
    existing_target="$(readlink "${PARAMS_HOME}")"
    if [[ "${existing_target}" == "${TARGET_DIR}" ]]; then
        : # Already points where we want; nothing to do.
    else
        echo ""
        echo "[WARN] ${PARAMS_HOME} is a symlink to ${existing_target}, not ${TARGET_DIR}."
        echo "       Refusing to clobber it. Set PARAMS_HOME to a different path or"
        echo "       remove the existing symlink manually if you intended to replace it."
    fi
elif [[ -d "${PARAMS_HOME}" ]]; then
    echo ""
    echo "[WARN] ${PARAMS_HOME} is an existing directory (not a symlink)."
    echo "       Refusing to replace it. Set PARAMS_HOME to a different path if you"
    echo "       intended to use the params downloaded to ${TARGET_DIR}."
fi

echo ""
if [[ ${essential_errors} -gt 0 ]]; then
    echo "=== FAILED: ${essential_errors} required parameter file(s) could not be downloaded ==="
    echo "Refusing to produce an image that cannot start zcashd. This is usually a"
    echo "transient network error -- just re-run the build; already-downloaded files"
    echo "are skipped, so only the missing ones are retried."
    exit 1
fi
echo "=== All required Zcash parameters present ==="
echo "Total size: $(du -sh "${TARGET_DIR}" | cut -f1)"
