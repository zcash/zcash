#!/usr/bin/env bash
# =============================================================================
# download_binaries.sh -- Download all required zcashd versions
#
# PURPOSE:
#   Downloads pre-built zcashd binaries from https://download.z.cash/downloads/
#   for each network upgrade phase. Each version is extracted into its own
#   subdirectory under the target directory.
#
# PREREQUISITES:
#   - curl must be available
#   - Sufficient disk space (~2GB for all versions)
#
# USAGE:
#   ./download_binaries.sh [TARGET_DIR]
#   TARGET_DIR defaults to /opt/zcash/versions
#
# OUTPUT:
#   Creates TARGET_DIR/{version}/bin/zcashd for each version.
#   Also creates TARGET_DIR/{version}/bin/zcash-cli.
#
# NOTES:
#   - The download URL format varies by version:
#       Old (v1.x-v3.x): zcash-{version}-linux64.tar.gz
#       Newer (v4.x+):   zcash-{version}-linux64-debian-{codename}.tar.gz
#   - This script probes multiple URL patterns to find the correct one.
#   - Downloaded tarballs are cached in /tmp/zcash-downloads/ to avoid
#     re-downloading on repeated runs.
# =============================================================================
export LC_ALL=C
set -euo pipefail

TARGET_DIR="${1:-/opt/zcash/versions}"
CACHE_DIR="/tmp/zcash-downloads"
BASE_URL="https://download.z.cash/downloads"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSIONS_JSON="${SCRIPT_DIR}/../config/versions.json"

# Version list is the single source of truth in config/versions.json. We extract
# the deduplicated set of zcashd_version values across all phases. lib/constants.py
# reads the same file at import time, so the two cannot drift.
if [[ ! -f "${VERSIONS_JSON}" ]]; then
    echo "[ERROR] versions.json not found at ${VERSIONS_JSON}" >&2
    exit 1
fi

mapfile -t VERSIONS < <(python3 -c "
import json, sys
with open('${VERSIONS_JSON}') as f:
    data = json.load(f)
seen = []
for entry in data['phases']:
    v = entry['zcashd_version']
    if v not in seen:
        seen.append(v)
for v in seen:
    print(v)
")

if [[ ${#VERSIONS[@]} -eq 0 ]]; then
    echo "[ERROR] No versions found in ${VERSIONS_JSON}" >&2
    exit 1
fi

# URL patterns to try for each version.
# Older versions use just "-linux64", newer ones add a Debian codename.
URL_PATTERNS=(
    "{BASE_URL}/zcash-{VERSION}-linux64.tar.gz"
    "{BASE_URL}/zcash-{VERSION}-linux64-debian-stretch.tar.gz"
    "{BASE_URL}/zcash-{VERSION}-linux64-debian-buster.tar.gz"
    "{BASE_URL}/zcash-{VERSION}-linux64-debian-bullseye.tar.gz"
    "{BASE_URL}/zcash-{VERSION}-linux64-debian-bookworm.tar.gz"
)

mkdir -p "${TARGET_DIR}" "${CACHE_DIR}"

echo "=== Downloading zcashd binaries ==="
echo "Target directory: ${TARGET_DIR}"
echo "Versions to download: ${VERSIONS[*]}"
echo ""

download_version() {
    local version="$1"
    local dest_dir="${TARGET_DIR}/${version}"

    # Check if already downloaded
    if [[ -f "${dest_dir}/bin/zcashd" ]]; then
        echo "[SKIP] v${version} already exists at ${dest_dir}/bin/zcashd"
        return 0
    fi

    echo "[DOWNLOAD] v${version}..."

    # Try each URL pattern until one succeeds
    local tarball=""
    local url=""
    for pattern in "${URL_PATTERNS[@]}"; do
        url="${pattern//\{BASE_URL\}/${BASE_URL}}"
        url="${url//\{VERSION\}/${version}}"
        local filename
        filename=$(basename "${url}")
        tarball="${CACHE_DIR}/${filename}"

        # Check if cached
        if [[ -f "${tarball}" ]]; then
            echo "  Using cached: ${filename}"
            break
        fi

        # Try to download (HEAD request first to check existence). --retry guards
        # against transient network failures on these ~300MB+ tarballs; --fail
        # ensures a failed download is not mistaken for success.
        if curl --silent --fail --head "${url}" > /dev/null 2>&1; then
            echo "  Downloading: ${url}"
            if curl --silent --show-error --fail --location --retry 3 --retry-delay 5 \
                    --output "${tarball}" "${url}"; then
                echo "  Downloaded: ${filename} ($(du -h "${tarball}" | cut -f1))"
                break
            fi
        fi

        tarball=""
    done

    if [[ -z "${tarball}" || ! -f "${tarball}" ]]; then
        echo "  [ERROR] Could not download v${version} from any URL pattern!"
        echo "  Tried patterns:"
        for pattern in "${URL_PATTERNS[@]}"; do
            local try_url="${pattern//\{BASE_URL\}/${BASE_URL}}"
            try_url="${try_url//\{VERSION\}/${version}}"
            echo "    ${try_url}"
        done
        return 1
    fi

    # Extract the tarball
    echo "  Extracting to ${dest_dir}..."
    mkdir -p "${dest_dir}"

    # zcashd tarballs typically extract to zcash-{version}/bin/
    # We extract to a temp dir first, then move the contents.
    local tmp_extract="${CACHE_DIR}/extract_${version}"
    rm -rf "${tmp_extract}"
    mkdir -p "${tmp_extract}"
    tar xzf "${tarball}" -C "${tmp_extract}"

    # Find the extracted directory (could be zcash-{version} or just bin/)
    local extracted_dir
    extracted_dir=$(find "${tmp_extract}" -name "zcashd" -type f | head -1 | xargs dirname | xargs dirname)

    if [[ -z "${extracted_dir}" ]]; then
        echo "  [ERROR] Could not find zcashd binary in extracted tarball!"
        rm -rf "${tmp_extract}"
        return 1
    fi

    # Move bin/ to the target directory
    if [[ -d "${extracted_dir}/bin" ]]; then
        cp -r "${extracted_dir}/bin" "${dest_dir}/"
    else
        mkdir -p "${dest_dir}/bin"
        cp "${extracted_dir}/zcashd" "${dest_dir}/bin/" 2>/dev/null || true
        cp "${extracted_dir}/zcash-cli" "${dest_dir}/bin/" 2>/dev/null || true
    fi

    # Also copy zcash-fetch-params if present
    if [[ -f "${extracted_dir}/bin/zcash-fetch-params" ]]; then
        cp "${extracted_dir}/bin/zcash-fetch-params" "${dest_dir}/bin/"
    elif [[ -f "${extracted_dir}/zcash-fetch-params" ]]; then
        cp "${extracted_dir}/zcash-fetch-params" "${dest_dir}/bin/"
    fi

    rm -rf "${tmp_extract}"

    # Verify the binary exists
    if [[ ! -f "${dest_dir}/bin/zcashd" ]]; then
        echo "  [ERROR] zcashd binary not found after extraction!"
        return 1
    fi

    chmod +x "${dest_dir}/bin/zcashd"
    chmod +x "${dest_dir}/bin/zcash-cli" 2>/dev/null || true

    echo "  [OK] v${version} installed to ${dest_dir}/bin/zcashd"
}

# Download each version
errors=0
for version in "${VERSIONS[@]}"; do
    if ! download_version "${version}"; then
        errors=$((errors + 1))
    fi
    echo ""
done

if [[ ${errors} -gt 0 ]]; then
    echo "=== FAILED: ${errors} version(s) could not be downloaded ==="
    exit 1
fi

echo "=== All ${#VERSIONS[@]} versions downloaded successfully ==="
