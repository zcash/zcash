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

# Version list: each entry is "version" as it appears in the download filename.
# These are the zcashd versions used for each build phase.
#
# IMPORTANT: These version strings must match the download server filenames
# exactly. If a version is not available, the script will report an error.
# Verify availability at: https://download.z.cash/downloads/
#
# Phase 0 (Sprout):     v1.0.15   -- latest 1.0.x before Overwinter
# Phase 1 (Overwinter): v1.1.1    -- released May 29, 2018, before activation Jun 25
# Phase 2 (Sapling):    v2.0.1    -- released Oct 15, 2018, before activation Oct 28
# Phase 3 (Blossom):    v2.1.0-1  -- security hotfix (Nov 12, 2019), before Dec 11
# Phase 4 (Heartwood):  v3.0.0    -- released May 27, 2020, before activation Jul 16
# Phase 5 (Canopy):     v4.1.1    -- hotfix (Nov 17, 2020), 1 day before activation
# Phase 6 (NU5):        v5.0.0    -- released May 11, 2022, before activation May 31
# Phase 7 (NU6):        v6.0.0    -- released Oct 4, 2024, before activation Nov 23
# Phase 8 (NU6.1):      v6.10.0   -- released Oct 4, 2025, before activation Nov 24
VERSIONS=(
    "1.0.15"
    "1.1.1"
    "2.0.1"
    "2.1.0-1"
    "3.0.0"
    "4.1.1"
    "5.0.0"
    "6.0.0"
    "6.10.0"
)

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

        # Try to download (HEAD request first to check existence)
        if curl --silent --fail --head "${url}" > /dev/null 2>&1; then
            echo "  Downloading: ${url}"
            if curl --silent --fail --location --output "${tarball}" "${url}"; then
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
