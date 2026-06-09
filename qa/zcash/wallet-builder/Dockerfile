# =============================================================================
# Dockerfile -- Build environment for the Zcash regtest chain builder
#
# This container includes:
#   - All zcashd versions referenced by config/versions.json
#   - Zcash proving parameters (~1.7GB)
#   - Python orchestration scripts
#
# PLATFORM: pinned to linux/amd64 because every zcashd release tarball under
# https://download.z.cash/downloads/ is x86_64-only. On Apple Silicon hosts
# without this pin, docker build defaults to linux/arm64 and the binaries
# fail at exec with "rosetta error: failed to open elf at /lib64/ld-linux-x86-64.so.2".
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

# The --platform pin is intentional and load-bearing: every zcashd release
# tarball under https://download.z.cash/downloads/ is x86_64-only, so without
# it docker build defaults to the host arch (e.g. arm64 on Apple Silicon) and
# the binaries fail at exec with a rosetta/ld-linux error. See the header above.
# hadolint ignore=DL3029
FROM --platform=linux/amd64 debian:bookworm-slim

# Layer 1: System dependencies
# python3 is used for the RPC orchestration scripts.
# curl is used for downloading binaries and parameters.
# libgomp1 is a runtime dependency for some zcashd versions.
# Versions are pinned (DL3008) to the current Debian Bookworm point release for
# reproducibility; bump them when the base image's security updates roll over.
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3=3.11.2-1+b1 \
    curl=7.88.1-10+deb12u14 \
    ca-certificates=20230311+deb12u1 \
    libgomp1=12.2.0-14+deb12u1 \
    procps=2:4.0.2-3 \
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
# Downloaded once per version list change. config/versions.json is the single
# source of truth for which versions to fetch, so it must be copied in before
# download_binaries.sh runs (otherwise the build fails with
# "versions.json not found"). Placing it here also means a change to the
# version matrix correctly invalidates this expensive download layer.
COPY config/ /opt/zcash/config/
COPY scripts/download_binaries.sh /opt/zcash/scripts/download_binaries.sh
RUN chmod +x /opt/zcash/scripts/download_binaries.sh \
    && /opt/zcash/scripts/download_binaries.sh /opt/zcash/versions

# Validate that all binaries can execute on this system.
# This catches glibc/library incompatibilities early.
COPY scripts/validate_binaries.sh /opt/zcash/scripts/validate_binaries.sh
RUN chmod +x /opt/zcash/scripts/validate_binaries.sh \
    && /opt/zcash/scripts/validate_binaries.sh /opt/zcash/versions

# Layer 4: Build scripts and Python library (changes frequently)
# config/ was already copied above (Layer 3) so download_binaries.sh could
# read versions.json; it is re-copied implicitly via the cached layer.
COPY scripts/ /opt/zcash/scripts/
COPY lib/ /opt/zcash/lib/
RUN chmod +x /opt/zcash/scripts/*.sh

WORKDIR /opt/zcash
ENV PYTHONPATH=/opt/zcash

# The entry point runs the chain-building process.
# Output artifacts are written to /artifacts/ inside the container.
# Use `docker cp` to extract them after the run completes.
ENTRYPOINT ["/opt/zcash/scripts/build_chain.sh"]
