package=native_tinfo
#$(package)_major_version=13
#$(package)_version=13.0.0
#$(package)_download_path=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
$(package)_download_path_linux=http://ftp.debian.org/debian/pool/main/n/ncurses/
$(package)_download_file_linux=libtinfo5_6.0+20161126-1+deb9u2_amd64.deb
$(package)_file_name_linux=libtinfo5_6.0+20161126-1+deb9u2_amd64.deb
$(package)_sha256_hash_linux=1d249a3193568b5ef785ad8993b9ba6d6fdca0eb359204c2355532b82d25e9f5
#$(package)_download_path_darwin=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_major_version).0.0
#$(package)_download_file_darwin=clang+llvm-$($(package)_major_version).0.0-x86_64-apple-darwin.tar.xz
#$(package)_file_name_darwin=clang-llvm-$($(package)_major_version).0.0-x86_64-apple-darwin.tar.xz
#$(package)_sha256_hash_darwin=d051234eca1db1f5e4bc08c64937c879c7098900f7a0370f3ceb7544816a8b09
#$(package)_download_path_freebsd=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
#$(package)_download_file_freebsd=clang+llvm-$($(package)_version)-amd64-unknown-freebsd12.tar.xz
#$(package)_file_name_freebsd=clang-llvm-$($(package)_version)-amd64-unknown-freebsd12.tar.xz
#$(package)_sha256_hash_freebsd=e579747a36ff78aa0a5533fe43bc1ed1f8ed449c9bfec43c358d953ffbbdcf76
#$(package)_download_file_aarch64_linux=clang+llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
#$(package)_file_name_aarch64_linux=clang-llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
#$(package)_sha256_hash_aarch64_linux=968d65d2593850ee9b37fcda074fb7641529bd45d2f976af6c8197de3c22612f

## Ensure we have clang native to the builder, not the target host
#ifneq ($(canonical_host),$(build))
#$(package)_exact_download_path=$($(package)_download_path_$(build_os))
#$(package)_exact_download_file=$($(package)_download_file_$(build_os))
#$(package)_exact_file_name=$($(package)_file_name_$(build_os))
#$(package)_exact_sha256_hash=$($(package)_sha256_hash_$(build_os))
#endif

define $(package)_stage_cmds
	pwd
	ls -l
	ls -l
	ls -l
endef
#mkdir -p $($(package)_staging_prefix_dir)/bin && \
#cp bin/clang-$($(package)_major_version) $($(package)_staging_prefix_dir)/bin && \
#cp bin/lld $($(package)_staging_prefix_dir)/bin && \
#cp bin/llvm-ar $($(package)_staging_prefix_dir)/bin && \
#cp bin/llvm-config $($(package)_staging_prefix_dir)/bin && \
#cp bin/llvm-nm $($(package)_staging_prefix_dir)/bin && \
#cp bin/llvm-objcopy $($(package)_staging_prefix_dir)/bin && \
#cp -P bin/clang $($(package)_staging_prefix_dir)/bin && \
#cp -P bin/clang++ $($(package)_staging_prefix_dir)/bin && \
#cp -P bin/ld.lld $($(package)_staging_prefix_dir)/bin && \
#cp -P bin/ld64.lld $($(package)_staging_prefix_dir)/bin && \
#cp -P bin/lld-link $($(package)_staging_prefix_dir)/bin && \
#cp -P bin/llvm-ranlib $($(package)_staging_prefix_dir)/bin && \
#cp -P bin/llvm-strip $($(package)_staging_prefix_dir)/bin && \
#mv include/ $($(package)_staging_prefix_dir) && \
#mv lib/ $($(package)_staging_prefix_dir) && \
#mv libexec/ $($(package)_staging_prefix_dir)
