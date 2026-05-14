# =============================================================================
# Dockerfile -- Build environment for the Zcash regtest chain builder
#
# This container includes:
#   - All 9 zcashd versions (v1.0.15 through v6.2.0) for each network upgrade
#   - Zcash proving parameters (~1.7GB)
#   - Python orchestration scripts
#
# WHY DEBIAN BOOKWORM:
#   glibc is backward-compatible: binaries built against old glibc (2016-era)
#   can run on newer glibc versions. Debian Bookworm (glibc 2.36) is the base
#   because it can run both old (v1.0.x) and new (v6.x) zcashd binaries.
#   If old binaries fail (e.g., due to libstdc++ ABI breaks), use the
#   multi-container fallback described in README.md.
#
# BUILD LAYERS (optimized for caching):
#   1. System dependencies (rarely changes)
#   2. Zcash proving parameters (~1.7GB, never changes)
#   3. zcashd binaries (~2GB, changes when version list changes)
#   4. Build scripts (changes frequently)
#
# USAGE:
#   docker build -t zcash-regtest-wallet-builder
#   docker run --name zrwb-run zcash-regtest-wallet-builder
#   docker cp zrwb-run:/artifacts/final/wallet.dat ./output/
# =============================================================================

FROM debian:bookworm-slim

# Layer 1: System dependencies
# python3 is used for the RPC orchestration scripts.
# curl is used for downloading binaries and parameters.
# libgomp1 is a runtime dependency for some zcashd versions.
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    curl \
    ca-certificates \
    libgomp1 \
    procps \
    && rm -rf /var/lib/apt/lists/*

# Create directory structure
RUN mkdir -p \
    /opt/zcash/versions \
    /opt/zcash/params \
    /data/primary \
    /data/external \
    /artifacts/checkpoints \
    /artifacts/manifests \
    /artifacts/exports \
    /artifacts/final

# Layer 2: Zcash proving parameters (~1.7GB)
# Downloaded once and cached. These NEVER change between builds.
COPY scripts/fetch_params.sh /opt/zcash/scripts/fetch_params.sh
RUN chmod +x /opt/zcash/scripts/fetch_params.sh \
    && /opt/zcash/scripts/fetch_params.sh /opt/zcash/params

# Layer 3: zcashd binaries (~2GB)
# Downloaded once per version list change.
COPY scripts/download_binaries.sh /opt/zcash/scripts/download_binaries.sh
RUN chmod +x /opt/zcash/scripts/download_binaries.sh \
    && /opt/zcash/scripts/download_binaries.sh /opt/zcash/versions

# Validate that all binaries can execute on this system.
# This catches glibc/library incompatibilities early.
COPY scripts/validate_binaries.sh /opt/zcash/scripts/validate_binaries.sh
RUN chmod +x /opt/zcash/scripts/validate_binaries.sh \
    && /opt/zcash/scripts/validate_binaries.sh /opt/zcash/versions

# Layer 4: Build scripts and Python library (changes frequently)
COPY scripts/ /opt/zcash/scripts/
COPY lib/ /opt/zcash/lib/
COPY config/ /opt/zcash/config/
RUN chmod +x /opt/zcash/scripts/*.sh

WORKDIR /opt/zcash
ENV PYTHONPATH=/opt/zcash

# The entry point runs the chain-building process.
# Output artifacts are written to /artifacts/ inside the container.
# Use `docker cp` to extract them after the run completes.
ENTRYPOINT ["/opt/zcash/scripts/build_chain.sh"]
