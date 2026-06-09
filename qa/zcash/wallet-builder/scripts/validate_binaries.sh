#!/usr/bin/env bash
# =============================================================================
# validate_binaries.sh -- Verify all zcashd binaries can execute
#
# PURPOSE:
#   Runs each downloaded zcashd binary with --version to verify it works
#   on the current system. This catches glibc/library incompatibilities
#   before we start the long chain-building process.
#
# BACKGROUND:
#   Old zcashd binaries (v1.0.x from 2016) were compiled against older
#   system libraries. glibc is backward-compatible (old binaries run on
#   new glibc), but other libraries (libstdc++, libgomp, etc.) may have
#   breaking changes. This script detects these issues early.
#
# PREREQUISITES:
#   - download_binaries.sh must have been run first
#
# USAGE:
#   ./validate_binaries.sh [VERSIONS_DIR]
#   VERSIONS_DIR defaults to /opt/zcash/versions
#
# OUTPUT:
#   Prints PASS/FAIL for each version. Exits 0 if all pass, 1 if any fail.
#   For failed versions, prints the error message (typically a dynamic
#   linker error like "version GLIBC_2.XX not found").
# =============================================================================
export LC_ALL=C
set -euo pipefail

VERSIONS_DIR="${1:-/opt/zcash/versions}"

echo "=== Validating zcashd binary compatibility ==="
echo "Versions directory: ${VERSIONS_DIR}"
echo "System: $(uname -m) $(cat /etc/debian_version 2>/dev/null || echo 'unknown')"
echo "glibc: $(ldd --version 2>&1 | head -1 || echo 'unknown')"
echo ""

pass=0
fail=0
failed_versions=()

for version_dir in "${VERSIONS_DIR}"/*/; do
    version=$(basename "${version_dir}")
    binary="${version_dir}bin/zcashd"

    if [[ ! -f "${binary}" ]]; then
        echo "[SKIP] v${version}: binary not found at ${binary}"
        continue
    fi

    printf "[TEST] v%-10s ... " "${version}"

    # Try to run zcashd --version. This should print version info and exit.
    # NOTE: Old zcashd versions (v1.0.x) exit with code 1 for --version
    # even when they work fine. So we check the OUTPUT for a version string
    # rather than relying on the exit code. A real failure (missing libs)
    # will produce a dynamic linker error, not a version string.
    output=$(timeout 10 "${binary}" --version 2>&1) || true

    if echo "${output}" | grep -qi "zcash"; then
        # Output contains "Zcash" -- the binary runs on this system.
        version_line=$(echo "${output}" | head -1)
        echo "PASS (${version_line})"
        pass=$((pass + 1))
    else
        echo "FAIL"
        # Print the error (usually a dynamic linker message)
        echo "  Error output:"
        # shellcheck disable=SC2001
        echo "${output}" | sed 's/^/    /'
        fail=$((fail + 1))
        failed_versions+=("${version}")
    fi
done

echo ""
echo "=== Results: ${pass} passed, ${fail} failed ==="

if [[ ${fail} -gt 0 ]]; then
    echo ""
    echo "Failed versions: ${failed_versions[*]}"
    echo ""
    echo "These versions cannot run on this system. You have two options:"
    echo "  1. Use the multi-container fallback (see Dockerfile.stretch)"
    echo "  2. Build these versions from source (complex, not recommended)"
    echo ""
    echo "The most common cause is missing shared libraries. Check with:"
    echo "  ldd ${VERSIONS_DIR}/<version>/bin/zcashd"
    exit 1
fi
