package=libcxx
$(package)_version=$(if $(native_clang_version_$(host_arch)_$(host_os)),$(native_clang_version_$(host_arch)_$(host_os)),$(if $(native_clang_version_$(host_os)),$(native_clang_version_$(host_os)),$(native_clang_default_version)))
$(package)_msys2_version=17.0.6-1

ifneq ($(canonical_host),$(build))
ifneq ($(host_os),mingw32)
# Clang is provided pre-compiled for a bunch of targets; fetch the one we need
# and stage its copies of the static libraries.
$(package)_download_path=$(native_clang_download_path)
$(package)_download_file_aarch64_linux=clang+llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_file_name_aarch64_linux=clang-llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_sha256_hash_aarch64_linux=6dd62762285326f223f40b8e4f2864b5c372de3f7de0731cb7cd55ca5287b75a
$(package)_download_file_linux=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-22.04.tar.xz
$(package)_file_name_linux=clang-llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-22.04.tar.xz
$(package)_sha256_hash_linux=884ee67d647d77e58740c1e645649e29ae9e8a6fe87c1376be0f3a30f3cc9ab3

# Starting from LLVM 14.0.0, some Clang binary tarballs store libc++ in a
# target-specific subdirectory.
define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib && \
  (test ! -f lib/*/libc++.a    || cp lib/*/libc++.a    $($(package)_staging_prefix_dir)/lib) && \
  (test ! -f lib/*/libc++abi.a || cp lib/*/libc++abi.a $($(package)_staging_prefix_dir)/lib) && \
  (test ! -f lib/libc++.a      || cp lib/libc++.a      $($(package)_staging_prefix_dir)/lib) && \
  (test ! -f lib/libc++abi.a   || cp lib/libc++abi.a   $($(package)_staging_prefix_dir)/lib)
endef

else
# For Windows cross-compilation, use the MSYS2 binaries.
# Starting from LLVM 15.0.0, libc++abi is provided by libc++.
$(package)_download_path=https://repo.msys2.org/mingw/x86_64
$(package)_download_file=mingw-w64-x86_64-libc++-$($(package)_msys2_version)-any.pkg.tar.zst
$(package)_file_name=mingw-w64-x86_64-libcxx-$($(package)_msys2_version)-any.pkg.tar.zst
$(package)_sha256_hash=ba719056a30eca2684bf816cf1864177792df54fe30d82b410c2d2c84f0b8277

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib && \
  mv include/ $($(package)_staging_prefix_dir) && \
  cp lib/libc++.a $($(package)_staging_prefix_dir)/lib && \
  cp lib/libc++abi.a $($(package)_staging_prefix_dir)/lib
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
  (test ! -f $(build_prefix)/lib/*/libc++.a    || cp $(build_prefix)/lib/*/libc++.a    $($(package)_staging_prefix_dir)/lib) && \
  (test ! -f $(build_prefix)/lib/*/libc++abi.a || cp $(build_prefix)/lib/*/libc++abi.a $($(package)_staging_prefix_dir)/lib) && \
  (test ! -f $(build_prefix)/lib/libc++.a      || cp $(build_prefix)/lib/libc++.a      $($(package)_staging_prefix_dir)/lib) && \
  (test ! -f $(build_prefix)/lib/libc++abi.a   || cp $(build_prefix)/lib/libc++abi.a   $($(package)_staging_prefix_dir)/lib)
endef

endif
