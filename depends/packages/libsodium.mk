package=libsodium
$(package)_version=1.0.15
$(package)_download_path=https://download.libsodium.org/libsodium/releases/old/unsupported/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=fb6a9e879a2f674592e4328c5d9f79f082405ee4bb05cb6e679b90afe9e178f4
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
