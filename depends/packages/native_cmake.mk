package=native_cmake
$(package)_version=4.2.2
$(package)_download_path=https://github.com/Kitware/CMake/releases/download/v$($(package)_version)
$(package)_file_name=cmake-$($(package)_version).tar.gz
$(package)_sha256_hash=bbda94dd31636e89eb1cc18f8355f6b01d9193d7676549fba282057e8b730f58

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
