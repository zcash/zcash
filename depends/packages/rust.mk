package=rust
$(package)_version=1.42.0
$(package)_download_path=https://static.rust-lang.org/dist
$(package)_file_name_linux=rust-$($(package)_version)-x86_64-unknown-linux-gnu.tar.gz
$(package)_sha256_hash_linux=7d1e07ad9c8a33d8d039def7c0a131c5917aa3ea0af3d0cc399c6faf7b789052
$(package)_file_name_darwin=rust-$($(package)_version)-x86_64-apple-darwin.tar.gz
$(package)_sha256_hash_darwin=db1055c46e0d54b99da05e88c71fea21b3897e74a4f5ff9390e934f3f050c0a8
$(package)_file_name_freebsd=rust-$($(package)_version)-x86_64-unknown-freebsd.tar.gz
$(package)_sha256_hash_freebsd=230bcf17e4383fba85d3c87fe25d17737459fe561a5f4668fe70dcac2da4e17c

# Mapping from GCC canonical hosts to Rust targets
# If a mapping is not present, we assume they are identical
$(package)_rust_target_x86_64-apple-darwin11=x86_64-apple-darwin
$(package)_rust_target_x86_64-w64-mingw32=x86_64-pc-windows-gnu

# Mapping from Rust targets to SHA-256 hashes
$(package)_rust_std_sha256_hash_aarch64-unknown-linux-gnu=1343f51fc87049327233cee8941629c3d7dfdc425d359385f93665de3d46711b
$(package)_rust_std_sha256_hash_x86_64-apple-darwin=1d61e9ed5d29e1bb4c18e13d551c6d856c73fb8b410053245dc6e0d3b3a0e92c
$(package)_rust_std_sha256_hash_x86_64-pc-windows-gnu=8a8389f3860df6f42fbf8b76a62ddc7b9b6fe6d0fb526dcfc42faab1005bfb6d

ifneq ($(canonical_host),$(build))
$(package)_rust_target=$(if $($(package)_rust_target_$(canonical_host)),$($(package)_rust_target_$(canonical_host)),$(canonical_host))
$(package)_exact_file_name=rust-std-$($(package)_version)-$($(package)_rust_target).tar.gz
$(package)_exact_sha256_hash=$($(package)_rust_std_sha256_hash_$($(package)_rust_target))
$(package)_build_subdir=buildos
$(package)_extra_sources=$($(package)_file_name_$(build_os))

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_file_name_$(build_os)),$($(package)_file_name_$(build_os)),$($(package)_sha256_hash_$(build_os)))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_sha256_hash_$(build_os))  $($(package)_source_dir)/$($(package)_file_name_$(build_os))" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir $(canonical_host) && \
  tar --strip-components=1 -xf $($(package)_source) -C $(canonical_host) && \
  mkdir buildos && \
  tar --strip-components=1 -xf $($(package)_source_dir)/$($(package)_file_name_$(build_os)) -C buildos
endef

define $(package)_stage_cmds
  bash ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(host_prefix)/native --disable-ldconfig && \
  ../$(canonical_host)/install.sh --destdir=$($(package)_staging_dir) --prefix=$(host_prefix)/native --disable-ldconfig
endef
else

define $(package)_stage_cmds
  bash ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(host_prefix)/native --disable-ldconfig
endef
endif
