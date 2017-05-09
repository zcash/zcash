package=libcurl
$(package)_version=7.54.0
$(package)_download_path=https://curl.haxx.se/download
$(package)_file_name=curl-$($(package)_version).tar.gz
$(package)_sha256_hash=a84b635941c74e26cce69dd817489bec687eb1f230e7d1897fc5b5f108b59adf
$(package)_config_opts=--disable-shared --enable-static --prefix=$(host_prefix)
$(package)_cflags=
$(package)_conf_tool=./configure

define $(package)_set_vars
endef

define $(package)_config_cmds
  $($(package)_conf_tool) $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
