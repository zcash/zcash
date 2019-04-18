package=libevent
$(package)_version=2.1.12
$(package)_download_path=https://github.com/libevent/libevent/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=release-$($(package)_version)-stable.tar.gz
$(package)_sha256_hash=7180a979aaa7000e1264da484f712d403fcf7679b1e9212c4e3d09f5c93efc24

define $(package)_preprocess_cmds
  ./autogen.sh
endef

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --disable-openssl --disable-libevent-regress
  $(package)_config_opts_release=--disable-debug-mode
  $(package)_config_opts_linux=--with-pic
  $(package)_config_opts_freebsd=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm lib/*.la
endef
