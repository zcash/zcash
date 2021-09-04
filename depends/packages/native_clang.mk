package=native_clang
$(package)_major_version=12
$(package)_version=12.0.1
$(package)_download_path=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
$(package)_download_path_linux=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
$(package)_download_file_linux=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-16.04.tar.xz
$(package)_file_name_linux=clang-llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-16.04.tar.xz
$(package)_sha256_hash_linux=6b3cc55d3ef413be79785c4dc02828ab3bd6b887872b143e3091692fc6acefe7
$(package)_download_path_darwin=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_major_version).0.0
$(package)_download_file_darwin=clang+llvm-$($(package)_major_version).0.0-x86_64-apple-darwin.tar.xz
$(package)_file_name_darwin=clang-llvm-$($(package)_major_version).0.0-x86_64-apple-darwin.tar.xz
$(package)_sha256_hash_darwin=7bc2259bf75c003f644882460fc8e844ddb23b27236fe43a2787870a4cd8ab50
$(package)_download_path_freebsd=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
$(package)_download_file_freebsd=clang+llvm-$($(package)_version)-amd64-unknown-freebsd11.tar.xz
$(package)_file_name_freebsd=clang-llvm-$($(package)_version)-amd64-unknown-freebsd11.tar.xz
$(package)_sha256_hash_freebsd=94dfe48d9e483283edbee968056d487a850b30de25258fa48f049cca3ede5db4

$(package)_download_file_aarch64_linux=clang+llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_file_name_aarch64_linux=clang-llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_sha256_hash_aarch64_linux=39b3d3e3b534e327d90c77045058e5fc924b1a81d349eac2be6fb80f4a0e40d4

# Ensure we have clang native to the builder, not the target host
ifneq ($(canonical_host),$(build))
$(package)_exact_download_path=$($(package)_download_path_$(build_os))
$(package)_exact_download_file=$($(package)_download_file_$(build_os))
$(package)_exact_file_name=$($(package)_file_name_$(build_os))
$(package)_exact_sha256_hash=$($(package)_sha256_hash_$(build_os))
endif

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/bin && \
  cp bin/clang-$($(package)_major_version) $($(package)_staging_prefix_dir)/bin && \
  cp bin/lld $($(package)_staging_prefix_dir)/bin && \
  cp bin/llvm-ar $($(package)_staging_prefix_dir)/bin && \
  cp bin/llvm-config $($(package)_staging_prefix_dir)/bin && \
  cp bin/llvm-nm $($(package)_staging_prefix_dir)/bin && \
  cp bin/llvm-objcopy $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/clang $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/clang++ $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/ld.lld $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/ld64.lld $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/lld-link $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/llvm-ranlib $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/llvm-strip $($(package)_staging_prefix_dir)/bin && \
  mv include/ $($(package)_staging_prefix_dir) && \
  mv lib/ $($(package)_staging_prefix_dir) && \
  mv libexec/ $($(package)_staging_prefix_dir)
endef
