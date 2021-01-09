package=libcxx
$(package)_version=$(native_clang_version)

ifneq ($(canonical_host),$(build))
ifneq ($(host_os),mingw32)
# Clang is provided pre-compiled for a bunch of targets; fetch the one we need
# and stage its copies of the static libraries.
$(package)_download_path=https://releases.llvm.org/$($(package)_version)
$(package)_download_file_aarch64_linux=clang+llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_file_name_aarch64_linux=clang-llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_sha256_hash_aarch64_linux=998e9ae6e89bd3f029ed031ad9355c8b43441302c0e17603cf1de8ee9939e5c9
$(package)_download_file_linux=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-14.04.tar.xz
$(package)_file_name_linux=clang-llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-14.04.tar.xz
$(package)_sha256_hash_linux=9ef854b71949f825362a119bf2597f744836cb571131ae6b721cd102ffea8cd0

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib && \
  cp lib/libc++.a $($(package)_staging_prefix_dir)/lib && \
  cp lib/libc++abi.a $($(package)_staging_prefix_dir)/lib
endef

else
# For Windows cross-compilation, use the MSYS2 binaries.
$(package)_download_path=https://repo.msys2.org/mingw/x86_64
$(package)_download_file=mingw-w64-x86_64-libc++-9.0.1-1-any.pkg.tar.xz
$(package)_file_name=mingw-w64-x86_64-libcxx-9.0.1-1-any.pkg.tar.xz
$(package)_sha256_hash=04e77c5d0e3a9efc9cc8ca3b6549af9a9eef6e20d53076295efbdfba76c5f5de

$(package)_libcxxabi_download_file=mingw-w64-x86_64-libc++abi-9.0.1-1-any.pkg.tar.xz
$(package)_libcxxabi_file_name=mingw-w64-x86_64-libc++abi-9.0.1-1-any.pkg.tar.xz
$(package)_libcxxabi_sha256_hash=e8a38084dc05c9f6bd4ded4fe1cdbbe16f7280d66426a76b2c3c23d0575aad5c

$(package)_extra_sources += $($(package)_libcxxabi_file_name)

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_libcxxabi_download_file),$($(package)_libcxxabi_file_name),$($(package)_libcxxabi_sha256_hash))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_libcxxabi_sha256_hash)  $($(package)_source_dir)/$($(package)_libcxxabi_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir -p libcxxabi && \
  tar --no-same-owner --strip-components=1 -C libcxxabi -xf $($(package)_source_dir)/$($(package)_libcxxabi_file_name) && \
  tar --no-same-owner --strip-components=1 -xf $($(package)_source)
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib && \
  mv include/ $($(package)_staging_prefix_dir) && \
  cp lib/libc++.a $($(package)_staging_prefix_dir)/lib && \
  cp libcxxabi/lib/libc++abi.a $($(package)_staging_prefix_dir)/lib
endef
endif

else
# For native compilation, use the static libraries from native_clang.
# We explicitly stage them so that subsequent dependencies don't link to the
# shared libraries distributed with Clang.
define $(package)_fetch_cmds
endef

define $(package)_extract_cmds
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib && \
  cp $(build_prefix)/lib/libc++.a $($(package)_staging_prefix_dir)/lib && \
  if [ -f "$(build_prefix)/lib/libc++abi.a" ]; then cp $(build_prefix)/lib/libc++abi.a $($(package)_staging_prefix_dir)/lib; fi
endef

endif
