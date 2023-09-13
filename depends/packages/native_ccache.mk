package=native_ccache
$(package)_version=4.8.3
$(package)_download_path=https://github.com/ccache/ccache/releases/download/v$($(package)_version)
$(package)_file_name=ccache-$($(package)_version).tar.gz
$(package)_sha256_hash=d59dd569ad2bbc826c0bc335c8ebd73e78ed0f2f40ba6b30069347e63585d9ef
$(package)_build_subdir=build
$(package)_dependencies=native_cmake native_zstd

define $(package)_set_vars
$(package)_config_opts += -DCMAKE_BUILD_TYPE=Release
$(package)_config_opts += -DZSTD_LIBRARY=$(build_prefix)/lib/libzstd.a
$(package)_config_opts += -DREDIS_STORAGE_BACKEND=OFF
endef

define $(package)_preprocess_cmds
  mkdir $($(package)_build_subdir)
endef

define $(package)_config_cmds
  $($(package)_cmake) .. $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf lib include
endef
