# =============================================================================
# Makefile -- Convenience targets for the Zcash regtest chain builder
#
# Quick start:
#   make build   -- Build the Docker image (downloads ~4GB on first run)
#   make run     -- Run the chain build (~30-60 minutes)
#   make extract -- Copy artifacts from the container to ./output/
#
# Or all at once:
#   make all     -- Build + run + extract
#
# Development:
#   make shell   -- Open a shell inside the container (for debugging)
#   make clean   -- Remove container and output directory
#   make nuke    -- Remove everything (container, image, output)
# =============================================================================

IMAGE_NAME    := zcash-regtest-wallet-builder
CONTAINER_NAME := zcash-regtest-wallet-builder-run
OUTPUT_DIR    := output

.PHONY: all build run extract shell clean nuke help

# Default target
help:
	@echo "Zcash Regtest Chain Builder"
	@echo ""
	@echo "Targets:"
	@echo "  make build    - Build Docker image (~4GB download on first run)"
	@echo "  make run      - Run the chain build (~30-60 min)"
	@echo "  make extract  - Copy artifacts to ./output/"
	@echo "  make all      - Build + run + extract"
	@echo ""
	@echo "  make shell    - Open a debug shell in the container"
	@echo "  make clean    - Remove container and output"
	@echo "  make nuke     - Remove container, image, and output"
	@echo ""

all: build run extract

# Build the Docker image.
# The first build downloads ~4GB (zcashd binaries + proving parameters).
# Subsequent builds use Docker layer cache and are fast unless
# scripts or config change.
build:
	docker build -t $(IMAGE_NAME) .

# Run the chain build inside Docker.
# This creates the regtest chain with wallet state across all NUs.
# Artifacts are stored inside the container at /artifacts/.
run:
	-docker rm -f $(CONTAINER_NAME) 2>/dev/null
	docker run --name $(CONTAINER_NAME) $(IMAGE_NAME)

# Extract artifacts from the completed container to ./output/.
# Includes the full regtest chain state and zcash proving parameters
# so the chain can be resumed for future NUs.
extract:
	@mkdir -p $(OUTPUT_DIR)
	-docker cp $(CONTAINER_NAME):/artifacts/final/wallet.dat $(OUTPUT_DIR)/wallet.dat
	-docker cp $(CONTAINER_NAME):/artifacts/final/full_manifest.json $(OUTPUT_DIR)/full_manifest.json
	-docker cp $(CONTAINER_NAME):/artifacts/manifests $(OUTPUT_DIR)/manifests
	-docker cp $(CONTAINER_NAME):/artifacts/checkpoints $(OUTPUT_DIR)/checkpoints
	-docker cp $(CONTAINER_NAME):/artifacts/exports $(OUTPUT_DIR)/exports
	@echo "Extracting regtest chain state..."
	-docker cp $(CONTAINER_NAME):/artifacts/final/regtest $(OUTPUT_DIR)/regtest
	@echo "Extracting zcash proving parameters (~1.7GB)..."
	-docker cp $(CONTAINER_NAME):/opt/zcash/params $(OUTPUT_DIR)/zcash-params
	@echo ""
	@echo "Artifacts extracted to $(OUTPUT_DIR)/"
	@ls -lh $(OUTPUT_DIR)/wallet.dat 2>/dev/null || true
	@ls -lh $(OUTPUT_DIR)/full_manifest.json 2>/dev/null || true
	@du -sh $(OUTPUT_DIR)/regtest 2>/dev/null || true
	@du -sh $(OUTPUT_DIR)/zcash-params 2>/dev/null || true

# Open a shell inside a fresh container for debugging.
# Useful for:
#   - Testing zcashd binaries manually
#   - Inspecting the chain state
#   - Running individual phase scripts
shell:
	docker run --rm -it --entrypoint /bin/bash $(IMAGE_NAME)

# Clean up the run container and output directory.
clean:
	-docker rm -f $(CONTAINER_NAME) 2>/dev/null
	rm -rf $(OUTPUT_DIR)
	@echo "Cleaned container and output."

# Remove everything: container, image, and output.
nuke: clean
	-docker rmi $(IMAGE_NAME) 2>/dev/null
	@echo "Removed Docker image."
