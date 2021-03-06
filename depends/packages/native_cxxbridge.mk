package=native_cxxbridge
# The version needs to match cxx in Cargo.toml
$(package)_version=1.0.68
$(package)_download_path=https://github.com/dtolnay/cxx/archive/refs/tags
$(package)_file_name=native_cxxbridge-$($(package)_version).tar.gz
$(package)_download_file=$($(package)_version).tar.gz
$(package)_sha256_hash=6fed9ef1c64a37c343727368b38c27fa4e15b27ca9924c6672a6a5496080c832
$(package)_build_subdir=gen/cmd
$(package)_dependencies=native_rust

define $(package)_build_cmds
  cargo build --release --package=cxxbridge-cmd --bin=cxxbridge
endef

define $(package)_stage_cmds
  cargo install --offline --path=. --bin=cxxbridge --root=$($(package)_staging_prefix_dir)
endef
