package=native_ccache
$(package)_version=4.8.2
$(package)_download_path=https://github.com/ccache/ccache/releases/download/v$($(package)_version)
$(package)_file_name=ccache-$($(package)_version).tar.gz
$(package)_sha256_hash=75eef15b8b9da48db9c91e1d0ff58b3645fc70c0e4ca2ef1b6825a12f21f217d
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
