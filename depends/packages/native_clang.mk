package=native_clang
$(package)_major_version_linux=8
$(package)_major_version_darwin=10
$(package)_version_linux=8.0.0
$(package)_version_darwin=10.0.0
$(package)_major_version=$($(package)_major_version_$(build_os))
$(package)_version=$($(package)_version_$(build_os))
$(package)_download_path=https://releases.llvm.org/$($(package)_version)
$(package)_download_file_linux=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-14.04.tar.xz
$(package)_file_name_linux=clang-llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-14.04.tar.xz
$(package)_sha256_hash_linux=9ef854b71949f825362a119bf2597f744836cb571131ae6b721cd102ffea8cd0
$(package)_download_file_darwin=clang+llvm-$($(package)_version)-x86_64-apple-darwin.tar.xz
$(package)_file_name_darwin=clang-llvm-$($(package)_version)-x86_64-apple-darwin.tar.xz
$(package)_sha256_hash_darwin=633a833396bf2276094c126b072d52b59aca6249e7ce8eae14c728016edb5e61

# Ensure we have clang native to the builder, not the target host
ifneq ($(canonical_host),$(build))
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
