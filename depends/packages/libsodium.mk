package=libsodium
$(package)_version=1.0.8
$(package)_download_path=https://download.libsodium.org/libsodium/releases/
$(package)_file_name=libsodium-1.0.8.tar.gz
$(package)_sha256_hash=c0f191d2527852641e0a996b7b106d2e04cbc76ea50731b2d0babd3409301926
$(package)_dependencies=
$(package)_config_opts=

define $(package)_preprocess_cmds
  cd $($(package)_build_subdir); ./autogen.sh
endef

define $(package)_config_cmds
  $($(package)_autoconf) --enable-static --disable-shared
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
