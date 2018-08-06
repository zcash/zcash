package=rust
$(package)_version=1.28.0
$(package)_download_path=https://static.rust-lang.org/dist
$(package)_file_name_linux=rust-$($(package)_version)-x86_64-unknown-linux-gnu.tar.gz
$(package)_sha256_hash_linux=2a1390340db1d24a9498036884e6b2748e9b4b057fc5219694e298bdaa37b810
$(package)_file_name_darwin=rust-$($(package)_version)-x86_64-apple-darwin.tar.gz
$(package)_sha256_hash_darwin=5d7a70ed4701fe9410041c1eea025c95cad97e5b3d8acc46426f9ac4f9f02393
$(package)_file_name_mingw32=rust-mingw-$($(package)_version)-x86_64-pc-windows-gnu.tar.gz
$(package)_sha256_hash_mingw32=17effb289f53af43c36be48635364db7eed68c5a411410216eb75a57c39219e3


define $(package)_stage_cmds
  ./install.sh --destdir=$($(package)_staging_dir) --prefix=$(host_prefix)/native --disable-ldconfig
endef
