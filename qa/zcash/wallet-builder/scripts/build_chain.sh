#!/usr/bin/env bash
# =============================================================================
# build_chain.sh -- Container entry point: build the regtest chain
#
# PURPOSE:
#   This is the main script that runs INSIDE the Docker container.
#   It orchestrates the entire chain-building process by calling the
#   Python phase runner, which handles all zcashd version switching,
#   transaction operations, and manifest generation.
#
# PREREQUISITES:
#   - All zcashd binaries downloaded (download_binaries.sh)
#   - Zcash parameters downloaded (fetch_params.sh)
#   - Binary compatibility validated (validate_binaries.sh)
#   - Python 3 with no external dependencies (uses only stdlib + lib/)
#
# USAGE:
#   ./build_chain.sh
#   Typically called as the Docker ENTRYPOINT, not directly.
#
# ENVIRONMENT:
#   ZCASH_VERSIONS_DIR  -- Directory containing zcashd binaries (default: /opt/zcash/versions)
#   ZCASH_PARAMS_DIR    -- Directory containing proving parameters (default: /opt/zcash/params)
#   ARTIFACTS_DIR       -- Output directory for wallet.dat + manifests (default: /artifacts)
#   LOG_LEVEL           -- Python logging level (default: INFO)
#
# OUTPUT:
#   Creates artifacts in ARTIFACTS_DIR:
#     checkpoints/       -- wallet.dat at each phase boundary
#     manifests/         -- Per-phase JSON manifests
#     exports/           -- z_exportwallet output files
#     final/
#       wallet.dat       -- The final test fixture
#       regtest/         -- Full chain data directory
#       full_manifest.json
# =============================================================================
export LC_ALL=C
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"

# Default paths (can be overridden by environment)
export ZCASH_VERSIONS_DIR="${ZCASH_VERSIONS_DIR:-/opt/zcash/versions}"
export ZCASH_PARAMS_DIR="${ZCASH_PARAMS_DIR:-/opt/zcash/params}"
export ARTIFACTS_DIR="${ARTIFACTS_DIR:-/artifacts}"
export LOG_LEVEL="${LOG_LEVEL:-INFO}"

echo "============================================================"
echo "  zcash-regtest-wallet-builder: Zcash Regtest Chain Builder"
echo "============================================================"
echo ""
echo "Configuration:"
echo "  Versions directory: ${ZCASH_VERSIONS_DIR}"
echo "  Parameters directory: ${ZCASH_PARAMS_DIR}"
echo "  Artifacts directory: ${ARTIFACTS_DIR}"
echo "  Log level: ${LOG_LEVEL}"
echo ""

# Ensure required directories exist
mkdir -p "${ARTIFACTS_DIR}"/{checkpoints,manifests,exports,final}
mkdir -p /data/{primary,external}

# Ensure zcash-params symlink exists
if [[ ! -e "${HOME}/.zcash-params" ]]; then
    ln -sf "${ZCASH_PARAMS_DIR}" "${HOME}/.zcash-params"
fi

# Run the Python orchestrator.
# We add the project root to PYTHONPATH so lib/ is importable.
export PYTHONPATH="${PROJECT_DIR}:${PYTHONPATH:-}"

echo "Starting Python orchestrator..."
echo ""

python3 -c "
import logging
import sys
import os

# Configure logging
logging.basicConfig(
    level=os.environ.get('LOG_LEVEL', 'INFO'),
    format='%(asctime)s [%(levelname)s] %(name)s: %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S',
    stream=sys.stdout,
)

# Run the build
from lib.phase_runner import run_all_phases
run_all_phases()
"

exit_code=$?

if [[ ${exit_code} -eq 0 ]]; then
    echo ""
    echo "============================================================"
    echo "  BUILD SUCCESSFUL"
    echo "  Artifacts are in: ${ARTIFACTS_DIR}"
    echo "============================================================"
else
    echo ""
    echo "============================================================"
    echo "  BUILD FAILED (exit code ${exit_code})"
    echo "  Check logs above for details."
    echo "============================================================"
fi

exit ${exit_code}
