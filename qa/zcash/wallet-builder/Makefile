# =============================================================================
# Makefile -- Convenience targets for the Zcash regtest chain builder
#
# Pick which wallet to build with a distinct target:
#   make run      -- the multi-version wallet spanning every network upgrade
#                    (Sprout -> NU6.2), with an HD seed.
#   make ancient  -- a pre-Sapling, non-deterministic (no HD seed) wallet for
#                    zallet's seedless import path (zcash/wallet#384); this is
#                    `make run` stopped after phase 1 (the pre-Sapling eras).
#
# Typical flow:
#   make build && make run     && make extract
#   make build && make ancient && make extract
#
# Development:
#   make shell   -- Open a shell inside the container (for debugging)
#   make clean   -- Remove container and output directory
#   make nuke    -- Remove everything (container, image, output)
# =============================================================================

IMAGE_NAME    := zcash-regtest-wallet-builder
CONTAINER_NAME := zcash-regtest-wallet-builder-run
OUTPUT_DIR    := output

.PHONY: build run ancient extract shell clean nuke help

help: ## Ask for help!
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

build: ## Build Docker image (~4.6GB download on first run)
	docker build -t $(IMAGE_NAME) .

run: ## Build the full multi-version wallet (Sprout -> NU6.2, HD seed; ~30-60 min)
	-docker rm -f $(CONTAINER_NAME) 2>/dev/null
	docker run --name $(CONTAINER_NAME) $(IMAGE_NAME)

# `make ancient` is the same build as `make run`, stopped after phase 1 -- the
# trailing "1" is build_chain.sh's STOP_AFTER_PHASE argument. Phases 0-1 (Sprout,
# Overwinter) both run the pre-Sapling v1.1.1, so the wallet has no HD seed;
# phase 2 (Sapling) is the first that would add one.
ancient: ## Build the pre-Sapling, non-deterministic wallet (= phases 0-1 of the full build)
	-docker rm -f $(CONTAINER_NAME) 2>/dev/null
	docker run --name $(CONTAINER_NAME) $(IMAGE_NAME) 1

extract: ## Copy artifacts to ./output/ (same layout for either wallet)
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

shell: ## Open a debug shell in the container
	docker run --rm -it --entrypoint /bin/bash $(IMAGE_NAME)

clean: ## Remove container and output
	-docker rm -f $(CONTAINER_NAME) 2>/dev/null
	rm -rf $(OUTPUT_DIR)
	@echo "Cleaned container and output."

nuke: clean ## Remove container, image, and output
	-docker rmi $(IMAGE_NAME) 2>/dev/null
	@echo "Removed Docker image."
