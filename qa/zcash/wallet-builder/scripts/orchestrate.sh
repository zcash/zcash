#!/usr/bin/env bash
# =============================================================================
# orchestrate.sh -- Host-side orchestrator: build Docker image and run chain build
#
# PURPOSE:
#   This is the main entry point for the entire build process, run on the
#   HOST machine (not inside Docker). It:
#     1. Builds the Docker image (downloading binaries + params)
#     2. Runs the container (which builds the regtest chain)
#     3. Extracts artifacts from the container
#
#   Equivalent to: make build && make run && make extract
#
# PREREQUISITES:
#   - Docker installed and running
#   - Internet access (for downloading zcashd binaries and params)
#   - ~5GB free disk space (binaries + params + chain data)
#
# USAGE:
#   ./scripts/orchestrate.sh [--no-cache] [--extract-only]
#
# OPTIONS:
#   --no-cache     Force Docker to rebuild without cache
#   --extract-only Skip build+run, just extract from existing container
#
# OUTPUT:
#   Creates ./output/ directory with:
#     output/wallet.dat
#     output/full_manifest.json
#     output/manifests/          -- Per-phase manifests
#     output/checkpoints/        -- wallet.dat checkpoints
# =============================================================================
export LC_ALL=C
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"
OUTPUT_DIR="${PROJECT_DIR}/output"

IMAGE_NAME="zcash-regtest-wallet-builder"
CONTAINER_NAME="zcash-regtest-wallet-builder-run"

# Parse arguments
NO_CACHE=""
EXTRACT_ONLY=false
for arg in "$@"; do
    case "${arg}" in
        --no-cache)   NO_CACHE="--no-cache" ;;
        --extract-only) EXTRACT_ONLY=true ;;
        *) echo "Unknown argument: ${arg}"; exit 1 ;;
    esac
done

cd "${PROJECT_DIR}"

if [[ "${EXTRACT_ONLY}" == true ]]; then
    echo "=== Extract-only mode ==="
else
    # ---------------------------------------------------------------
    # Step 1: Build Docker image
    # ---------------------------------------------------------------
    echo "=== Step 1/3: Building Docker image ==="
    echo "This downloads ~2GB of zcashd binaries and ~1.7GB of proving parameters."
    echo "Subsequent builds will use Docker layer cache."
    echo ""

    docker build ${NO_CACHE} -t "${IMAGE_NAME}" .

    echo ""
    echo "=== Docker image built: ${IMAGE_NAME} ==="
    echo ""

    # ---------------------------------------------------------------
    # Step 2: Run the container
    # ---------------------------------------------------------------
    echo "=== Step 2/3: Running chain build ==="
    echo "This will take approximately 30-60 minutes."
    echo ""

    # Remove any existing container with the same name
    docker rm -f "${CONTAINER_NAME}" 2>/dev/null || true

    # Run the build. We use --name so we can extract artifacts later.
    docker run \
        --name "${CONTAINER_NAME}" \
        "${IMAGE_NAME}"

    echo ""
    echo "=== Chain build complete ==="
    echo ""
fi

# ---------------------------------------------------------------
# Step 3: Extract artifacts
# ---------------------------------------------------------------
echo "=== Step 3/3: Extracting artifacts ==="

mkdir -p "${OUTPUT_DIR}"

# Copy artifacts from the container
docker cp "${CONTAINER_NAME}:/artifacts/final/wallet.dat" "${OUTPUT_DIR}/wallet.dat" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/artifacts/final/full_manifest.json" "${OUTPUT_DIR}/full_manifest.json" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/artifacts/manifests" "${OUTPUT_DIR}/manifests" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/artifacts/checkpoints" "${OUTPUT_DIR}/checkpoints" 2>/dev/null || true
docker cp "${CONTAINER_NAME}:/artifacts/exports" "${OUTPUT_DIR}/exports" 2>/dev/null || true

# Copy the full regtest chain state.
# This includes blk*.dat (raw blocks), chainstate/, blocks/index/, and wallet.dat.
# Needed to resume the chain with a future zcashd version for new NUs.
echo "Extracting regtest chain state (this may take a moment)..."
docker cp "${CONTAINER_NAME}:/artifacts/final/regtest" "${OUTPUT_DIR}/regtest" 2>/dev/null || true

# Copy Zcash proving parameters.
# These are required by zcashd to create/verify shielded transactions.
# Without them, zcashd cannot start or process shielded operations.
# Total: ~1.7GB (sprout-proving.key, sprout-groth16.params, sapling-*.params)
echo "Extracting Zcash proving parameters (~1.7GB)..."
docker cp "${CONTAINER_NAME}:/opt/zcash/params" "${OUTPUT_DIR}/zcash-params" 2>/dev/null || true

echo ""
echo "=== Artifacts extracted to ${OUTPUT_DIR} ==="
echo ""
echo "Contents:"
if [[ -f "${OUTPUT_DIR}/wallet.dat" ]]; then
    echo "  wallet.dat           $(du -h "${OUTPUT_DIR}/wallet.dat" | cut -f1)"
fi
if [[ -f "${OUTPUT_DIR}/full_manifest.json" ]]; then
    echo "  full_manifest.json   $(du -h "${OUTPUT_DIR}/full_manifest.json" | cut -f1)"
fi
if [[ -d "${OUTPUT_DIR}/manifests" ]]; then
    echo "  manifests/           $(find "${OUTPUT_DIR}/manifests/" -maxdepth 1 -type f 2>/dev/null | wc -l) files"
fi
if [[ -d "${OUTPUT_DIR}/checkpoints" ]]; then
    echo "  checkpoints/         $(find "${OUTPUT_DIR}/checkpoints/" -maxdepth 1 -type f 2>/dev/null | wc -l) files"
fi
if [[ -d "${OUTPUT_DIR}/regtest" ]]; then
    echo "  regtest/             $(du -sh "${OUTPUT_DIR}/regtest" | cut -f1) (full chain state)"
fi
if [[ -d "${OUTPUT_DIR}/zcash-params" ]]; then
    echo "  zcash-params/        $(du -sh "${OUTPUT_DIR}/zcash-params" | cut -f1) (proving parameters)"
fi
echo ""
echo "Output:"
echo "  Test fixture:   ${OUTPUT_DIR}/wallet.dat"
echo "  Chain state:    ${OUTPUT_DIR}/regtest/"
echo "  Proving params: ${OUTPUT_DIR}/zcash-params/"
echo ""
echo "To resume for a future NU, see README.md 'Resuming for Future NUs'."
