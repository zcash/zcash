package=native_cmake
$(package)_version=3.25.2
$(package)_download_path=https://github.com/Kitware/CMake/releases/download/v$($(package)_version)
$(package)_file_name=cmake-$($(package)_version).tar.gz
$(package)_sha256_hash=c026f22cb931dd532f648f087d587f07a1843c6e66a3dfca4fb0ea21944ed33c

define $(package)_set_vars
$(package)_config_opts += -DCMAKE_BUILD_TYPE:STRING=Release
$(package)_config_opts += -DCMAKE_USE_OPENSSL:BOOL=OFF
endef

define $(package)_config_cmds
  ./bootstrap --prefix=$($(package)_staging_prefix_dir) -- $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE) cmake
endef

define $(package)_stage_cmds
  $(MAKE) install
endef
