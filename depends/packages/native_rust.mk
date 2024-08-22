package=native_rust
# To update the Rust compiler, change the version below and then run the script
# ./contrib/devtools/update-rust-hashes.sh
# The Rust compiler should use the same LLVM version as the Clang compiler; you
# can check this with `rustc --version -v`.
$(package)_version=1.80.1
$(package)_download_path=https://static.rust-lang.org/dist
$(package)_file_name_linux=rust-$($(package)_version)-x86_64-unknown-linux-gnu.tar.gz
$(package)_sha256_hash_linux=9eedaea9719914a0f4673899aa11693607f25486569913fcca5905ef1da784ec
$(package)_file_name_darwin=rust-$($(package)_version)-x86_64-apple-darwin.tar.gz
$(package)_sha256_hash_darwin=4fcc0dad8b47066e13877e2839760ef1f40754a90a8fe83ecd4a1f14bf63c71a
$(package)_file_name_freebsd=rust-$($(package)_version)-x86_64-unknown-freebsd.tar.gz
$(package)_sha256_hash_freebsd=d7f4f66c3cc97616bcd37d9f63ed14c3c22c72a467f308453df2f1b128ffe0bc
$(package)_file_name_aarch64_linux=rust-$($(package)_version)-aarch64-unknown-linux-gnu.tar.gz
$(package)_sha256_hash_aarch64_linux=4ae791da82be6e7223f4f6cd477799624481aa1011e17c37753a57d257f89198

# Mapping from GCC canonical hosts to Rust targets
# If a mapping is not present, we assume they are identical, unless $host_os is
# "darwin", in which case we assume x86_64-apple-darwin.
$(package)_rust_target_x86_64-pc-linux-gnu=x86_64-unknown-linux-gnu
$(package)_rust_target_x86_64-w64-mingw32=x86_64-pc-windows-gnu

# Mapping from Rust targets to SHA-256 hashes
$(package)_rust_std_sha256_hash_aarch64-unknown-linux-gnu=2465f0df2ee35d32bd9690e05dd84d9c38bf81e8a5e9fd940d7347b66447c97f
$(package)_rust_std_sha256_hash_x86_64-apple-darwin=8fe1bd5ac9fb8741d3049b753a6eabec0e88d9c2c0276fdff34f868b35edda41
$(package)_rust_std_sha256_hash_x86_64-pc-windows-gnu=4fb82d57b0422ad31894529d3b45f84c41dec92009e643aa66fc4f418178b475
$(package)_rust_std_sha256_hash_x86_64-unknown-freebsd=b293f4ad4baa7298b2ccff0da3224b1e9bb64e34d5d1158d0a03fea37adb0402

define rust_target
$(if $($(1)_rust_target_$(2)),$($(1)_rust_target_$(2)),$(if $(findstring darwin,$(3)),x86_64-apple-darwin,$(if $(findstring freebsd,$(3)),x86_64-unknown-freebsd,$(2))))
endef

define $(package)_set_vars
$(package)_stage_opts=--disable-ldconfig
$(package)_stage_build_opts=--without=rust-docs-json-preview,rust-docs
endef

ifneq ($(canonical_host),$(build))
$(package)_rust_target=$(call rust_target,$(package),$(canonical_host),$(host_os))
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
  bash ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(build_prefix) $($(package)_stage_opts) $($(package)_stage_build_opts) && \
  ../$(canonical_host)/install.sh --destdir=$($(package)_staging_dir) --prefix=$(build_prefix) $($(package)_stage_opts)
endef
else

define $(package)_stage_cmds
  bash ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(build_prefix) $($(package)_stage_opts) $($(package)_stage_build_opts)
endef
endif
