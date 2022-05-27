package=native_cxxbridge
# The version needs to match cxx in Cargo.toml
$(package)_version=1.0.68
$(package)_download_path=https://github.com/dtolnay/cxx/archive/refs/tags
$(package)_file_name=native_cxxbridge-$($(package)_version).tar.gz
$(package)_download_file=$($(package)_version).tar.gz
$(package)_sha256_hash=6fed9ef1c64a37c343727368b38c27fa4e15b27ca9924c6672a6a5496080c832
$(package)_build_subdir=gen/cmd
$(package)_dependencies=native_rust
# This file is somewhat annoying to update, but can be done like so from the repo base:
# $ rm .cargo/config .cargo/.configured-for-offline
# $ mkdir tmp
# $ cd tmp
# $ tar xf ../depends/sources/native_cxxbridge-1.0.68.tar.gz
# $ cd cxx-1.0.68
# $ cargo build --release --package=cxxbridge-cmd --bin=cxxbridge
# $ cargo clean
# $ cd ..
# $ mv cxx-1.0.68 cxx-1.0.68-locked
# $ tar xf ../depends/sources/native_cxxbridge-1.0.68.tar.gz
# $ diff -urN cxx-1.0.68 cxx-1.0.68-locked >../depends/patches/native_cxxbridge/lockfile.diff
$(package)_patches=lockfile.diff
$(package)_extra_sources=$(package)-$($(package)_version)-vendored.tar.gz

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call vendor_crate_deps,$(package),$($(package)_file_name),lockfile.diff,Cargo.toml,$(package)-$($(package)_version)-vendored.tar.gz)
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  tar --no-same-owner --strip-components=1 -xf $($(package)_source) && \
  tar --no-same-owner -xf $($(package)_source_dir)/$(package)-$($(package)_version)-vendored.tar.gz
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/lockfile.diff && \
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
