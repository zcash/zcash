package=native_cxxbridge
# The version needs to match cxx in Cargo.toml
$(package)_version=1.0.83
$(package)_download_path=https://github.com/dtolnay/cxx/archive/refs/tags
$(package)_file_name=native_cxxbridge-$($(package)_version).tar.gz
$(package)_download_file=$($(package)_version).tar.gz
$(package)_sha256_hash=e30cbd34fc8ec2ae78f4f9e546d29c6c92e6d714f30c3c150f7b8c6ea08ea971
$(package)_build_subdir=gen/cmd
$(package)_dependencies=native_rust
$(package)_extra_sources=$(package)-$($(package)_version)-vendored.tar.gz

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call vendor_crate_deps,$(package),$($(package)_file_name),third-party/Cargo.lock,Cargo.toml,$(package)-$($(package)_version)-vendored.tar.gz)
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  tar --no-same-owner --strip-components=1 -xf $($(package)_source) && \
  tar --no-same-owner -xf $($(package)_source_dir)/$(package)-$($(package)_version)-vendored.tar.gz
endef

define $(package)_preprocess_cmds
  cp third-party/Cargo.lock . && \
  mkdir -p .cargo && \
  echo "[source.crates-io]" >.cargo/config && \
  echo "replace-with = \"vendored-sources\"" >>.cargo/config && \
  echo "[source.vendored-sources]" >>.cargo/config && \
  echo "directory = \"$(CRATE_REGISTRY)\"" >>.cargo/config
endef

define $(package)_build_cmds
  cargo build --locked --offline --release --package=cxxbridge-cmd --bin=cxxbridge
endef

define $(package)_stage_cmds
  cargo install --locked --offline --path=. --bin=cxxbridge --root=$($(package)_staging_prefix_dir)
endef
