package=rust
$(package)_version=1.16.0
$(package)_download_path=https://static.rust-lang.org/dist
ifeq ($(build_os),darwin)
$(package)_file_name=rust-$($(package)_version)-x86_64-apple-darwin.tar.gz
$(package)_sha256_hash=2d08259ee038d3a2c77a93f1a31fc59e7a1d6d1bbfcba3dba3c8213b2e5d1926
else ifeq ($(host_os),mingw32)
$(package)_file_name=rust-$($(package)_version)-i686-unknown-linux-gnu.tar.gz
$(package)_sha256_hash=b5859161ebb182d3b75fa14a5741e5de87b088146fb0ef4a30f3b2439c6179c5
else
$(package)_file_name=rust-$($(package)_version)-x86_64-unknown-linux-gnu.tar.gz
$(package)_sha256_hash=48621912c242753ba37cad5145df375eeba41c81079df46f93ffb4896542e8fd
endif

define $(package)_stage_cmds
  ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(host_prefix)/native --disable-ldconfig
endef
