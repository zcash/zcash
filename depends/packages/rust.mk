package=rust
$(package)_version=1.26.0
$(package)_download_path=https://static.rust-lang.org/dist
$(package)_file_name_linux=rust-$($(package)_version)-x86_64-unknown-linux-gnu.tar.gz
$(package)_sha256_hash_linux=13691d7782577fc9f110924b26603ade1990de0b691a3ce2dc324b4a72a64a68
$(package)_file_name_darwin=rust-$($(package)_version)-x86_64-apple-darwin.tar.gz
$(package)_sha256_hash_darwin=38708803c3096b8f101d1919ee2d7e723b0adf1bc1bb986b060973b57d8c7c28
$(package)_file_name_mingw32=rust-mingw-$($(package)_version)-x86_64-pc-windows-gnu.tar.gz
$(package)_sha256_hash_mingw32=156f99baf34a7a52867879cde17b762481c49aa280ca1b8fee3c4c5b7b43a408


define $(package)_stage_cmds
  ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(host_prefix)/native --disable-ldconfig
endef
