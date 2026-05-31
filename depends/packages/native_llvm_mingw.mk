package=native_llvm_mingw
# llvm-mingw is a self-consistent UCRT toolchain (Clang, LLD, mingw-w64, libc++,
# libc++abi, libunwind, and winpthreads all built together). We use it for the
# Windows cross build instead of the host's system mingw-w64 so that the whole
# tree shares one CRT (UCRT) and one libc++ ABI, avoiding the CRT/ABI mismatches
# that arise from stitching a prebuilt libc++ onto a separately-provided mingw.
#
# Keep the bundled LLVM version aligned with native_clang (and therefore with the
# Rust compiler's LLVM) so that LTO between the C/C++ and Rust objects keeps
# working. 20260324 ships LLVM 22.1.2, matching native_clang.
$(package)_version=20260324
$(package)_llvm_version=22.1.2
$(package)_download_path=https://github.com/mstorsjo/llvm-mingw/releases/download/$($(package)_version)
$(package)_download_file=llvm-mingw-$($(package)_version)-ucrt-ubuntu-22.04-x86_64.tar.xz
$(package)_file_name=$($(package)_download_file)
$(package)_sha256_hash=f92b02c4f835470deb5ac5fb92ddb458239e80ddff9ce8867155679ee5f57ffc

# Stage the toolchain under a dedicated subdirectory of the native prefix so its
# binaries (clang, clang++, lld, llvm-ar, ...) do not collide with native_clang's
# identically-named binaries in $(build_prefix)/bin. hosts/mingw32.mk references
# the target-prefixed wrappers here by absolute path, and config.site.in adds this
# bin directory to PATH so that configure can find x86_64-w64-mingw32-windres.
#
# llvm-mingw shares one set of headers (libc++ and the mingw-w64 CRT) across all
# targets in generic-w64-mingw32/, with each <target>/include being a symlink into
# it, so generic-w64-mingw32 must be staged alongside the x86_64 sysroot or the
# symlink dangles and no headers are found. The other target sysroots (aarch64,
# armv7, i686, arm64ec) are not needed.
define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/llvm-mingw && \
  cp -a bin include lib generic-w64-mingw32 x86_64-w64-mingw32 $($(package)_staging_prefix_dir)/llvm-mingw/ && \
  mkdir -p $($(package)_staging_prefix_dir)/bin && \
  cd $($(package)_staging_prefix_dir)/bin && \
  ln -s ../llvm-mingw/bin/x86_64-w64-mingw32-* .
endef
