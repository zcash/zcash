package=rust
$(package)_version=beta
$(package)_download_path=https://static.rust-lang.org/dist/2018-04-09
$(package)_file_name_linux=rust-$($(package)_version)-x86_64-unknown-linux-gnu.tar.gz
$(package)_sha256_hash_linux=e9a22f2c732e9e3e0774627fb565116d4871e5551464af9b22f170eb6cc79222
$(package)_file_name_darwin=rust-$($(package)_version)-x86_64-apple-darwin.tar.gz
$(package)_sha256_hash_darwin=b31b671c54ed1923ee2b98c3939b4097e43df21f2c44aa12d656f567b86d2387
$(package)_file_name_mingw32=rust-mingw-$($(package)_version)-x86_64-pc-windows-gnu.tar.gz
$(package)_sha256_hash_mingw32=fed89bdca8d926587ea670b20e84e5ebea0024f38375d608c9f0bfc8654a18cb


define $(package)_stage_cmds
  ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(host_prefix)/native --disable-ldconfig
endef
