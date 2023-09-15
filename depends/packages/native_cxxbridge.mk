package=native_cxxbridge
# The version needs to match cxx in Cargo.toml
$(package)_version=1.0.107
$(package)_download_path=https://github.com/dtolnay/cxx/archive/refs/tags
$(package)_file_name=native_cxxbridge-$($(package)_version).tar.gz
$(package)_download_file=$($(package)_version).tar.gz
$(package)_sha256_hash=961256a942c2369d84db29f6f7d09bce7fa7de221ec729856216a87b0970b1df
$(package)_build_subdir=gen/cmd
$(package)_dependencies=native_rust
# This file is somewhat annoying to update, but can be done like so from the repo base:
# $ export VERSION=1.0.107
# $ rm .cargo/config .cargo/.configured-for-offline
# $ mkdir tmp
# $ cd tmp
# $ tar xf ../depends/sources/native_cxxbridge-$VERSION.tar.gz
# $ cd cxx-$VERSION
# $ cargo check --release --package=cxxbridge-cmd --bin=cxxbridge
# $ cp Cargo.lock ../../depends/patches/native_cxxbridge/
$(package)_patches=Cargo.lock
$(package)_extra_sources=$(package)-$($(package)_version)-vendored.tar.gz

define $(package)_fetch_cmds
$(call fetch_file,$(1),$($(1)_download_path),$($(1)_download_file),$($(1)_file_name),$($(1)_sha256_hash)) && \
$(call vendor_crate_deps,$(1),$($(1)_file_name),$(PATCHES_PATH)/$(1)/Cargo.lock,Cargo.toml,$(1)-$($(1)_version)-vendored.tar.gz)
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  tar --no-same-owner --strip-components=1 -xf $($(package)_source) && \
  tar --no-same-owner -xf $($(package)_source_dir)/$(package)-$($(package)_version)-vendored.tar.gz
endef

define $(package)_preprocess_cmds
  cp $($(package)_patch_dir)/Cargo.lock . && \
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
