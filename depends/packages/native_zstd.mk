package=native_zstd
$(package)_version=1.5.2
$(package)_download_path=https://github.com/facebook/zstd/releases/download/v$($(package)_version)
$(package)_file_name=zstd-$($(package)_version).tar.gz
$(package)_sha256_hash=7c42d56fac126929a6a85dbc73ff1db2411d04f104fae9bdea51305663a83fd0
$(package)_build_subdir=build/cmake
$(package)_dependencies=native_cmake

define $(package)_set_vars
$(package)_config_opts += -DCMAKE_BUILD_TYPE=Release
$(package)_config_opts += -DCMAKE_INSTALL_LIBDIR=lib
$(package)_config_opts += -DZSTD_BUILD_CONTRIB=OFF
$(package)_config_opts += -DZSTD_BUILD_PROGRAMS=ON
$(package)_config_opts += -DZSTD_BUILD_SHARED=OFF
$(package)_config_opts += -DZSTD_BUILD_STATIC=ON
$(package)_config_opts += -DZSTD_BUILD_TESTS=OFF
$(package)_config_opts += -DZSTD_LEGACY_SUPPORT=OFF
$(package)_config_opts += -DZSTD_LZ4_SUPPORT=OFF
$(package)_config_opts += -DZSTD_LZMA_SUPPORT=OFF
$(package)_config_opts += -DZSTD_MULTITHREAD_SUPPORT=ON
$(package)_config_opts += -DZSTD_PROGRAMS_LINK_SHARED=OFF
$(package)_config_opts += -DZSTD_ZLIB_SUPPORT=OFF
endef

define $(package)_config_cmds
  $($(package)_cmake) $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
