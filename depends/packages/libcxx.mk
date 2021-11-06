package=libcxx
$(package)_version=$(native_clang_version)

ifneq ($(canonical_host),$(build))
ifneq ($(host_os),mingw32)
# Clang is provided pre-compiled for a bunch of targets; fetch the one we need
# and stage its copies of the static libraries.
$(package)_download_path=$(native_clang_download_path)
$(package)_download_file_aarch64_linux=clang+llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_file_name_aarch64_linux=clang-llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_sha256_hash_aarch64_linux=968d65d2593850ee9b37fcda074fb7641529bd45d2f976af6c8197de3c22612f
$(package)_download_file_linux=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-16.04.tar.xz
$(package)_file_name_linux=clang-llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-16.04.tar.xz
$(package)_sha256_hash_linux=76d0bf002ede7a893f69d9ad2c4e101d15a8f4186fbfe24e74856c8449acd7c1

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib && \
  cp lib/libc++.a $($(package)_staging_prefix_dir)/lib && \
  cp lib/libc++abi.a $($(package)_staging_prefix_dir)/lib
endef

else
# For Windows cross-compilation, use the MSYS2 binaries.
$(package)_download_path=https://repo.msys2.org/mingw/x86_64
$(package)_download_file=mingw-w64-x86_64-libc++-13.0.0-3-any.pkg.tar.zst
$(package)_file_name=mingw-w64-x86_64-libcxx-13.0.0-3-any.pkg.tar.zst
$(package)_sha256_hash=0f8819e88273579f7c9262456c6b8f4d73e1693095c2364d1192c61c5f6a1a4f

$(package)_libcxxabi_download_file=mingw-w64-x86_64-libc++abi-13.0.0-3-any.pkg.tar.zst
$(package)_libcxxabi_file_name=mingw-w64-x86_64-libcxxabi-13.0.0-3-any.pkg.tar.zst
$(package)_libcxxabi_sha256_hash=7224a7252a566938afe91ea8f130682abd29b10e13c9a3c2347af523ca0d7c42

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
