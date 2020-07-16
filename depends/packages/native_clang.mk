package=native_clang
$(package)_major_version=8
$(package)_version=8.0.0
$(package)_download_path=https://releases.llvm.org/$($(package)_version)
$(package)_download_file_linux=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-14.04.tar.xz
$(package)_file_name_linux=clang-llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-14.04.tar.xz
$(package)_sha256_hash_linux=9ef854b71949f825362a119bf2597f744836cb571131ae6b721cd102ffea8cd0
$(package)_download_file_darwin=clang+llvm-$($(package)_version)-x86_64-apple-darwin.tar.xz
$(package)_file_name_darwin=clang-llvm-$($(package)_version)-x86_64-apple-darwin.tar.xz
$(package)_sha256_hash_darwin=94ebeb70f17b6384e052c47fef24a6d70d3d949ab27b6c83d4ab7b298278ad6f

# Ensure we have clang native to the builder, not the target host
ifneq ($(canonical_host),$(build))
$(package)_exact_download_file=$($(package)_download_file_$(build_os))
$(package)_exact_file_name=$($(package)_file_name_$(build_os))
$(package)_exact_sha256_hash=$($(package)_sha256_hash_$(build_os))
endif

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/bin && \
  cp bin/clang-$($(package)_major_version) $($(package)_staging_prefix_dir)/bin && \
  cp bin/llvm-ar $($(package)_staging_prefix_dir)/bin && \
  cp bin/llvm-nm $($(package)_staging_prefix_dir)/bin && \
  cp bin/llvm-objcopy $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/clang $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/clang++ $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/llvm-ranlib $($(package)_staging_prefix_dir)/bin && \
  cp -P bin/llvm-strip $($(package)_staging_prefix_dir)/bin && \
  mv include/ $($(package)_staging_prefix_dir) && \
  mv lib/ $($(package)_staging_prefix_dir) && \
  mv libexec/ $($(package)_staging_prefix_dir)
endef
